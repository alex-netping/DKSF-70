/*
*\autor - modified by LBS
*version 1.5
*\date 22.02.2010
v1.6
by LBS
26.03.2010
v1.7
by LBS
5.04.2010  phy_reset() code
v1.8
by LBS
5.05.2010  mac236x_send_packet()
v 1.9
by LBS
1.06.2010
 phy_reset() moved to separate file
3.06.2010
 lpc23xx_smi_init() bugfix
v1.10-51 by LBS
18.09.2010
 void mac236x_init_rmii_pins(void)
v1.11-52
30.05.2012 by LBS
  additional protection from wrong arguments in module api calls
v1.12-48
4.03.2013
  LPC17xx support
  set_hardware_mac_address() bugfix, promisc. mode disabled!
v1.2-60
8.05.2013
  some rewrite, ethermem alignment, pointer access
v1.13-201
  some rewrite of EtherMem read-write
v1.14-60
20.11.2014
  __root-ed (against optimization) buffer align variable (ip etc header from 32 bit boundary)
*/

#include "platform_setup.h"
#ifdef MAC236X_MODULE

#include "emac.h"
#include <string.h>

#ifndef MAC236X_DEBUG
	#undef DEBUG_SHOW_TIME_LINE
	#undef DEBUG_MSG			
	#undef DEBUG_PROC_START
	#undef DEBUG_PROC_END
	#undef DEBUG_INPUT_PARAM
  #undef DEBUG_OUTPUT_PARAM
	#define DEBUG_SHOW_TIME_LINE
	#define DEBUG_MSG			
	#define DEBUG_PROC_START(msg)
	#define DEBUG_PROC_END(msg)
	#define DEBUG_INPUT_PARAM(msg,val)	
  #define DEBUG_OUTPUT_PARAM(msg,val)	
#endif

#if CPU_FAMILY == LPC17xx
#define POWERDOWN          PowerDown
#define COMMAND            Command
#define RXDESCRIPTOR       RxDescriptor
#define RXSTATUS           RxStatus
#define RXDESCRIPTORNUMBER RxDescriptorNumber
#define INTENABLE          IntEnable
#define TXDESCRIPTOR       TxDescriptor
#define TXSTATUS           TxStatus
#define TXDESCRIPTORNUMBER TxDescriptorNumber
#define RXPRODUCEINDEX     RxProduceIndex
#define RXCONSUMEINDEX     RxConsumeIndex
#define TXPRODUCEINDEX     TxProduceIndex
#define TXCONSUMEINDEX     TxConsumeIndex
#endif

void phy_reset(void);

/* Module ID reg (RO) */
#define MAC_BASE_ADDR 0xFFE00000
#define MAC_MODULEID (*(volatile unsigned long *)(MAC_BASE_ADDR + 0xFFC))
#define OLD_EMAC_MODULE_ID ((0x3902 << 16) | 0x2000)

/* Ethernet Memory */

#pragma section="ETHER_MEM"
#pragma data_alignment = 8
__no_init struct mac236x_packet_descr mac_rx_table[MAC236X_MAC_RX_PAGE_NUM] @ "ETHER_MEM"; // must be 8 byte aligned!
#pragma data_alignment = 8
__no_init unsigned mac_rx_status[MAC236X_MAC_RX_PAGE_NUM][2] @ "ETHER_MEM";                // must be 8 byte aligned!

#pragma data_alignment = 8
__no_init struct mac236x_packet_descr mac_tx_table[MAC236X_MAC_TX_DESCR_NUM] @ "ETHER_MEM"; // must be 8 byte aligned!
#pragma data_alignment = 4
__no_init unsigned mac_tx_status[MAC236X_MAC_TX_DESCR_NUM] @ "ETHER_MEM";                   // must be 4 byte aligned!
#pragma data_alignment = 4
__no_init __root unsigned char packet_align[2] @ "ETHER_MEM"; // align IP headers and so on to word boundary
#pragma data_alignment = 2
__no_init unsigned char mac_tx_buf[MAC236X_MAC_TX_PAGE_NUM][256] @ "ETHER_MEM";  // tx buf first, rx second (nic.c/mac236x.c algorithm)
#pragma data_alignment = 2
__no_init unsigned char mac_rx_buf[MAC236X_MAC_RX_PAGE_NUM + 5][256] @ "ETHER_MEM";

/*
// it's fixed in 201.8 by correct .icf and 0xEE00 0000 EEPROM move
// linker warns if section overflows
#if MAC236X_MAC_RX_PAGE_NUM * (16 + 256) + MAC236X_MAC_TX_DESCR_NUM * 12 + MAC236X_MAC_TX_PAGE_NUM * 256 + 4 + 5 * 256 > 1024 * 16
#error "Adjust Ether Mem allocations!"
#endif
*/

/* RX and TX descriptor and status definitions. */
/*
#define RX_DESC_PACKET(i)   (*(unsigned int *)(MAC236X_MAC_RX_TBL_START   + 8*i))
#define RX_DESC_CTRL(i)     (*(unsigned int *)(MAC236X_MAC_RX_TBL_START+4 + 8*i))
#define RX_STAT_INFO(i)     (*(unsigned int *)(MAC236X_MAC_RX_ST_TBL_START   + 8*i))
#define RX_STAT_HASHCRC(i)  (*(unsigned int *)(MAC236X_MAC_RX_ST_TBL_START+4 + 8*i))
#define TX_DESC_PACKET(i)   (*(unsigned int *)(MAC236X_MAC_TX_TBL_START   + 8*i))
#define TX_DESC_CTRL(i)     (*(unsigned int *)(MAC236X_MAC_TX_TBL_START+4 + 8*i))
#define TX_STAT_INFO(i)     (*(unsigned int *)(MAC236X_MAC_TX_ST_TBL_START+ 4*i))
#define RX_BUF(i)           (MAC236X_MAC_RX_MEM_START + MAC236X_RX_PACKET_SIZE*i)
#define TX_BUF(i)           (MAC236X_PACKETS_START + MAC236X_TX_PACKET_SIZE*i)
*/

#define RX_DESC_PACKET(i)   mac_rx_table[i].packet
#define RX_DESC_CTRL(i)     mac_rx_table[i].control
#define RX_STAT_INFO(i)     mac_rx_status[i][0]
#define RX_STAT_HASHCRC(i)  mac_rx_status[i][1]
#define TX_DESC_PACKET(i)   mac_tx_table[i].packet
#define TX_DESC_CTRL(i)     mac_tx_table[i].control
#define TX_STAT_INFO(i)     mac_tx_status[i]
#define RX_BUF(i)           ((unsigned)&mac_rx_buf[i])
#define TX_BUF(i)           ((unsigned)&mac_tx_buf[i])

struct mac236x_parse_rec mac236x_parse_struct;

#if CPU_FAMILY == LPC23xx

void mac236x_init_rmii_pins(void)
{
  // RMII pins
  PINSEL2_bit.P1_0  = 1; // ENET_TXD0
  PINSEL2_bit.P1_1  = 1; // ENET_TXD1
  PINSEL2_bit.P1_4  = 1; // ENET_TX_EN
  PINSEL2_bit.P1_8  = 1; // ENET_CRS
  PINSEL2_bit.P1_9  = 1; // ENET_RXD0
  PINSEL2_bit.P1_10 = 1; // ENET_RXD1
  PINSEL2_bit.P1_14 = 1; // ENET_RX_ER
  PINSEL2_bit.P1_15 = 1; // ENET_REF_CLK
}

void lpc23xx_smi_init(void)
{
  PINSEL3_bit.P1_16 = 1; // ENET_MDC
  PINSEL3_bit.P1_17 = 1; // ENET_MDIO
  PINMODE3_bit.P1_16 = 3; // pull MDC down
  PINMODE3_bit.P1_17 = 0; // pull MDIO up
  MCFG = 1<<15 /* reset */ | 6<<2 /* clock = host:20 */ ;
  MCFG &=~ (1<<15); // clear reset // LBS 03.06.2010
}

#elif CPU_FAMILY == LPC17xx

void mac236x_init_rmii_pins(void)
{
  IOCON_P1_00_bit.FUNC = 1; // ENET_TXD0
  IOCON_P1_01_bit.FUNC = 1; // ENET_TXD1
  IOCON_P1_04_bit.FUNC = 1; // ENET_TX_EN
  IOCON_P1_08_bit.FUNC = 1; // ENET_CRS
  IOCON_P1_09_bit.FUNC = 1; // ENET_RXD0
  IOCON_P1_10_bit.FUNC = 1; // ENET_RXD1
  IOCON_P1_14_bit.FUNC = 1; // ENET_RX_ER
  IOCON_P1_15_bit.FUNC = 1; // ENET_REF_CLK
}

void lpc23xx_smi_init(void)
{
  IOCON_P1_16_bit.FUNC = 1; // ENET MDC
  IOCON_P1_17_bit.FUNC = 1; // ENET_MDIO
  IOCON_P1_16_bit.MODE = 3; // pull MDC down
  IOCON_P1_17_bit.MODE = 0; // pull MDIO up
  MCFG = 1<<15 /* reset */ | 8<<2 /* clock = CCLK:36 */ ;
  MCFG &=~ (1<<15); // clear reset
}

#else
#error "Undefined CPU!"
#endif

void lpc23xx_smi_write(unsigned fulladdr, unsigned short data)
{
  MCMD = 0;
  MADR = fulladdr & 0x00001f1f; // bits 12:8 = devaddr, 4:0 = register
  MWTD = data;
  while(MIND & 1) ;
}

unsigned short lpc23xx_smi_read(unsigned fulladdr)
{
  MCMD = 1;
  MADR = fulladdr & 0x00001f1f; // bits 12:8 = devaddr, 4:0 = register
  while(MIND & (1|4)) ; // loop while BUSY or NOT_VALID
  MCMD = 0; // as per LPC23xx User Manual
  return MRDD;
}

void set_hardware_mac_address(unsigned char *mac)
{
#if CPU_FAMILY == LPC23xx
  // as in lpc23xx doc, order of sys_setup.mac on wire is correct (checked 11/11/2013), set of mac via snmp was not checked
  SA0 = (mac[0]<<8) | mac[1];
  SA1 = (mac[2]<<8) | mac[3];
  SA2 = (mac[4]<<8) | mac[5];
#elif CPU_FAMILY == LPC17xx
  // LBS 6.03.2013
  SA0 = (mac[5]<<8) | mac[4]; // it's ok, tested, perhaps, LPC17xx feature (not the same byte order as on lpc23xx
  SA1 = (mac[3]<<8) | mac[2];
  SA2 = (mac[1]<<8) | mac[0];
#else
#error Undefined CPU!
#endif
}

void  mac236x_init(void)
{
  uword i;
//  struct mac236x_packet_descr descr;
  DEBUG_PROC_START("mac236x_init");
  // Pins assignment
#if CPU_FAMILY == LPC23xx
  PINMODE2 = 0xA02A220A;  // P1[0,1,4,6,8,9,10,14,15] disable pu/pd
  // PINMODE3 = 0x0000000A;  // P1[17:16] disable pu/pd
  // (Errat)
  i = MAC_MODULEID;
  if ( i == OLD_EMAC_MODULE_ID ) PINSEL2 = 0x50151105; //Rev. '-' selects P1[0,1,4,6,8,9,10,14,15]
  else PINSEL2 = 0x50150105; //Rev. 'A' selects P1[0,1,4,8,9,10,14,15] //
#elif CPU_FAMILY == LPC17xx
  // func 1, disable pu/pd on pins before PHY reset for PHY strapping
  unsigned enetpin = 1<<0 | 0<<3 | 1<<5 ; // FUNC=1, MODE=0 no pull, Hys = 1
  IOCON_P1_00 = enetpin; // ENET_TXD0
  IOCON_P1_01 = enetpin; // ENET_TXD1
  IOCON_P1_04 = enetpin; // ENET_TX_EN
  IOCON_P1_08 = enetpin; // ENET_CRS
  IOCON_P1_09 = enetpin; // ENET_RXD0
  IOCON_P1_10 = enetpin; // ENET_RXD1
  IOCON_P1_14 = enetpin | 1<<3 ; // ENET_RX_ER, pull-down, pin N/C when used with KSZ8863RLL switch
  IOCON_P1_15 = enetpin; // ENET_REF_CLK
#endif
  /*
  // SMI bus (MDIO) connected in phy_init()
  // it's board-dependant
  PINSEL3 = 0x00000005;// selects P1[17:16]
  */
  /* moved after PHY reset, RMII ref clock problem!
  // clk enable
  PCONP |=(1<<30);
  POWERDOWN &=~(1<<31);
  // Reset entire MAC
  COMMAND = 0x0038; // reset all control registers
  */
  ///
#warning "attn RMII clock problem"
/// reset PHY, need RMII clock ???
  phy_reset_line_init();
  phy_reset_line_clear();
  delay(20);
  phy_reset_line_set();
  delay(20);
////

  // clk enable
  PCONP |=(1<<30);
  for(int i=0; i<1000; ++i) ;
  ///POWERDOWN &=~(1<<31);



	/* Reset all EMAC internal modules */
	MAC1    = EMAC_MAC1_RES_TX | EMAC_MAC1_RES_MCS_TX | EMAC_MAC1_RES_RX |
					EMAC_MAC1_RES_MCS_RX | EMAC_MAC1_SIM_RES | EMAC_MAC1_SOFT_RES;

for (int tout = 100; tout; tout--);
MAC1 = EMAC_MAC1_PASS_ALL;

	COMMAND = EMAC_CR_RMII |
          EMAC_CR_REG_RES | EMAC_CR_TX_RES | EMAC_CR_RX_RES | EMAC_CR_PASS_RUNT_FRM;

	/* A short delay after reset. */
	for (int tout = 100; tout; tout--);

	/* Enable Reduced MII interface. */
	COMMAND =  EMAC_CR_RMII | EMAC_CR_PASS_RUNT_FRM;

	/* Reset Reduced MII Logic. */
	SUPP = EMAC_SUPP_RES_RMII | 1<<8;

	for (int tout = 100; tout; tout--);
	SUPP = 1<<8;



	/* Initialize MAC control registers. */
	MAC1 = EMAC_MAC1_PASS_ALL;
	MAC2 = EMAC_MAC2_CRC_EN | EMAC_MAC2_PAD_EN
          | EMAC_MAC2_FULL_DUP; //lbs
	MAXF = EMAC_ETH_MAX_FLEN;
	
	// Write to MAC configuration register and reset
	MCFG = /*EMAC_MCFG_CLK_SEL(tout)*/ 8<<2 /* :36*/ | EMAC_MCFG_RES_MII;
	// release reset
	MCFG &= ~(EMAC_MCFG_RES_MII);
	CLRT = EMAC_CLRT_DEF;
	IPGR = EMAC_IPGR_P2_DEF;
        set_hardware_mac_address(sys_setup.mac);

        RxFilterCtrl = EMAC_RFC_MCAST_EN | EMAC_RFC_BCAST_EN | EMAC_RFC_PERFECT_EN;

#if 0 // -------------------------------------------------
// debug cut-out


  ///// 6.03.2012---- LPC17xx
  MAC1 = 0; // remove soft reset before any ops
  SUPP = 1<<8; // select 100mbit RMII mode
  COMMAND |= 1<<9; // select RMII
  for(int i=0; i<100; ++i);
  ///----

  /*
  // Reset entire MAC
  COMMAND = 0x0038; // reset all control registers
  */

  MAC1 = 0;
  COMMAND |=(1<<9);
  /* 1778
  SUPP = 0;
  */
  TEST = 0;
  // write the station address registers
  set_hardware_mac_address(sys_setup.mac);

  MAXF = 0x600;
  MCFG = (1<<15) /* reset */ | (7<<2); /* div 28 */

  /////MCFG = (6<<2); /* div 20 */
  /////MCFG |= (1<<15); // reset

  for(i=0;i<1000;++i);
  // MCMD = 0;
  MCFG&=~(1<<15);

  /* 1778???
  MAC1=0x00000F00; //Сброс Tx, MCS/Tx, Rx, Rx/MCS

  COMMAND&=~(1<<1);
  COMMAND|=(1<<4);

  COMMAND&=~(1<<0);
  COMMAND|=(1<<5);
  MAC1=0;

  COMMAND|=(1<<6); // pass runt frames (<64byte)
//  COMMAND|=(1<<7); // it's promisc.mode! it was set by legacy coder! LBS 6.03.2013

  MAC1|=(1<<1);
  LBS - */
  MAC2 = 0x31;

  CLRT|=0xF;
  CLRT|=(0x37<<8);

  #ifdef MAC236X_PHY_INIT
    MAC236X_PHY_INIT;
  #else
    #warning "MAC236X_PHY_INIT is not defined!"
  #endif

  IPGT = 0x12;

  SUPP|=(1<<8); // 100base-TX
  //SUPP = 0; // 10base-T

// end of debug cut-out
#endif // 0 --------------------------------------------------------------

  i=0;
  //Инициализация Rx буфферов и дискрипторов
  for (i = 0; i < MAC236X_MAC_RX_PAGE_NUM; i++)
  {
      RX_DESC_PACKET(i)  = /*(unsigned char*)*/RX_BUF(i); //Записываем адрес пространства для хранения принятых пакетов
      RX_DESC_CTRL(i)    = (MAC236X_RX_PACKET_SIZE-1); //длина пространства для одного пакета -1
      RX_STAT_INFO(i)    = 0;
      RX_STAT_HASHCRC(i) = 0;
  }
  RXDESCRIPTOR = (unsigned int)MAC236X_MAC_RX_TBL_START;
  RXSTATUS = (unsigned int)MAC236X_MAC_RX_ST_TBL_START;
  RXDESCRIPTORNUMBER = MAC236X_MAC_RX_PAGE_NUM-1; //Кол-во приемных дескрипторов
  INTENABLE=0;
  TXDESCRIPTOR = (unsigned int)MAC236X_MAC_TX_TBL_START;
  TXSTATUS = (unsigned int)MAC236X_MAC_TX_ST_TBL_START;
  TXDESCRIPTORNUMBER = MAC236X_MAC_TX_DESCR_NUM-1;//Кол-во передающих дескрипторов

  RXCONSUMEINDEX = TXPRODUCEINDEX = 0;
  util_fill((unsigned char*)&mac236x_parse_struct, sizeof(mac236x_parse_struct), 0);

  //Включаем приём
  COMMAND|=(1<<0);
  MAC1|=(1<<0);
  //Включаем передачу
  COMMAND|=(1<<1);
  DEBUG_PROC_END("mac236x_init");
}

const unsigned MAC_ERRRORS =
            MAC236X_RX_CTRL_FARME_MSK |
            MAC236X_RX_VLAN_MSK | /////////////  VLAN not supported!
            MAC236X_RX_FAIL_FILTER_MSK |
            MAC236X_RX_CRC_ERR_MSK |
            MAC236X_RX_SYM_ERR_MSK |
            // MAC236X_RX_LEN_ERR_MSK |
            // MAC236X_RX_RANGE_ERR_MSK |
            MAC236X_RX_ALIGN_ERR_MSK |
            MAC236X_RX_OVERRUN_MSK |
            MAC236X_RX_NODESCR_MSK |
            // MAC236X_RX_ERR_MSK |
            0;

void  mac236x_get_packet(void) // LBS 12.2009
{
  unsigned first, rxlen, n;
  for(;;)
  {
    first = RXCONSUMEINDEX;
    n = first;
    rxlen = 0;
    for(;;) // scan packet to the last fragment
    {
      if(n == RXPRODUCEINDEX)
      { // no completed packets yet
        return;
      }
      rxlen += (RX_STAT_INFO(n) & MAC236X_RX_LEN_MSK) + 1;
      if(RX_STAT_INFO(n) & MAC236X_RX_LAST_MSK)
      {
        if(RX_STAT_INFO(n) & MAC_ERRRORS)
        { // drop this packet and get next one
          ++n; if(n > RXDESCRIPTORNUMBER) n = 0;
          RXCONSUMEINDEX = n;
          break;
        }
        // packet is ok
        mac236x_parse_struct.packet_desc = first; // LBS 05.05.2010
        mac236x_parse_struct.packet_addr = RX_BUF(first) - MAC236X_PACKETS_START; // адрес пакета относительно MAC236X_PACKETS_START
        ++n; if(n > RXDESCRIPTORNUMBER) n = 0;
        mac236x_parse_struct.next_descr = n;
        mac236x_parse_struct.packet_len = rxlen;
        mac236x_parse_struct.rcv_flag = 1;
        // call self-test watchdog receive routune
#ifdef SELF_TEST_WATCHDOG
        check_self_watchdog_pkt((void*)RX_BUF(first)); // LBS 26.03.2010
#endif
        // unwrap rx packet if necessary
        unsigned n_of_buf = (rxlen + 255) >> 8; // round up to 256 byte chunks (data buffers)
        int wrapped_n = first + n_of_buf - MAC236X_MAC_RX_PAGE_NUM;
        if(wrapped_n > 0)
        { // unwrap to spare ether mem space (+5 buffers)
          memcpy(mac_rx_buf[MAC236X_MAC_RX_PAGE_NUM], mac_rx_buf[0], wrapped_n * 256);
        }
        return;
      } // if last frag
      ++n; if(n > RXDESCRIPTORNUMBER) n = 0;
    } // for current packet
  } // for all rx packets in loop dma buffer
}


void  mac236x_remove_packet(void)
{
  RXCONSUMEINDEX = mac236x_parse_struct.next_descr;
  mac236x_parse_struct.rcv_flag = 0;
}

void  mac236x_send_packet(unsigned int packet_id, unsigned short len)
{
  unsigned n;

  if(packet_id == 0xff) return;
  if(len < 60) len = 60;
  n = TXPRODUCEINDEX;
  if(packet_id & 0x80000000) // LBS 05.05.2010
  { // tx from rx buffer
    if(packet_id >= MAC236X_MAC_RX_PAGE_NUM) return;
    if(len > 256) return; // *** w/o wrap to start of loop buffer!!! only 64..256 byte packets ***
    TX_DESC_PACKET(n) = RX_BUF(packet_id & 0x7fffffff);
  }
  else
  {
    if(packet_id >= MAC236X_MAC_TX_PAGE_NUM) return;
    if(len > 256*6) return; // check packet length
    TX_DESC_PACKET(n) = TX_BUF(packet_id);
  }
  TX_DESC_CTRL(n)=((len-1)&MAC236X_TX_LEN_MSK)
                          | MAC236X_TX_PAD_MSK
                          | MAC236X_TX_CRC_MSK
                          | MAC236X_TX_LAST_MSK
                          | MAC236X_TX_INT_MSK;
  // Инкрементируем для начала передачи
  n += 1;
  if(n >= MAC236X_MAC_TX_DESCR_NUM) n = 0;
  TXPRODUCEINDEX = n;
  // Ждём завершение передачи
  do { n = TXPRODUCEINDEX; } while(n != TXCONSUMEINDEX);
}


void  mac236x_read_buf(upointer addr, unsigned char *buf, uword len) // addr is offset from start of mac memory
{
  if(len > 256 * 6) len = 256 * 6; // limit max packet len
  if( MAC236X_PACKETS_START + addr < (unsigned)__section_begin("ETHER_MEM")
  ||  MAC236X_PACKETS_START + addr + len > (unsigned)__section_end("ETHER_MEM") )
  {
    memset(buf, 0, len);
  }
  else
  {
    memcpy(buf, (void*)(MAC236X_PACKETS_START + addr), len);
  }
}

void  mac236x_write_buf(upointer addr, unsigned char *buf, uword len) // addr is offset from start of mac memory
{
  if(len > 256 * 6) len = 256 * 6; // limit max packet len
  if( MAC236X_PACKETS_START + addr < (unsigned)__section_begin("ETHER_MEM")
  ||  MAC236X_PACKETS_START + addr + len > (unsigned)__section_end("ETHER_MEM") )
  {
    return;
  }
  else
  {
    memcpy((void*)(MAC236X_PACKETS_START + addr), buf, len);
  }
}


void mac236x_event(enum event_e event)
{
}

#warning "TODO cleanup code!!!"

#endif

