/*
v1.2-48
25.10.2013
  uart rate 150000 bps, 2 stop bits and WR0 = 0x80 instead of 0x00, sampling is 2nd bit (13.3 to 20us)
v2.1
24.01.2014
  RH sensor added
  ow_timeout bugfix
  minor bugfix
v2.2-54
12.09.2014
  web page for new sensor 1w id
v2.3-70
26.11.2014
  rh_real_t_100 also set to 0 in case of RH sensor faill
*/

#include "platform_setup.h"

#ifdef OW_MODULE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

/*

restart - drive low, start new slot
match 0 - undive on write 1 slot / read slot / 8us
match 1 - sample on read slot = 10us minus interr latency
match 2 - undrive on write 0 slot / 62us
match 3 - restart / 70us

*/

#define U3_IRQ 8

unsigned ow_idx;
unsigned ow_tx_len;
unsigned ow_rx_len;
unsigned char ow_tx_buf[16];
unsigned char ow_rx_buf[10];
unsigned char ow_direction;
unsigned char ow_reset_pulse;
volatile unsigned char ow_completed;
unsigned char ow_present;
short         ow_error;
short         ow_ch;
unsigned      ow_scan_time;
unsigned short ow_repeat;
unsigned char ow_read_rom_buf[8];
systime_t     ow_timeout = ~0ULL;

enum ow_thermo_state_e {
  OW_START,
  OW_READ_ROM,
  OW_READ_ROM_CHK,
  OW_RH_INIT,
  OW_RH_SAVE,
  OW_RH_CONV_T,
  OW_RH_CONV_V,
  OW_RH_RECALL,
  OW_RH_READ,
  OW_RH_CHECK,
  OW_RH_DONE,
  OWT_START,
  OWT_CONVERT,
  OWT_CONV_CHK,
  OWT_READ,
  OWT_READ_CHK,
  OWT_NEXT
} ow_state;

void ow_pin_init(void)
{
  pindir(0,0,1);
}

void ow_drive(void)
{
  pinclr(0,0);
}

void ow_float(void)
{
  pinset(0,0);
}

void ow_set(int state)
{
  pinwrite(0,0,state);
}

int ow_read(void)
{
  return pinread(0,1);
}

/*

struct lpc_timer_s {
  volatile unsigned IR;
  volatile unsigned TCR;
  volatile unsigned TC;
  volatile unsigned PR;
  volatile unsigned PC;
  volatile unsigned MCR;
  volatile unsigned MR0;
  volatile unsigned MR1;
  volatile unsigned MR2;
  volatile unsigned MR3;
  volatile unsigned CCR;
  volatile const unsigned CR0;
  volatile const unsigned CR1;
           unsigned RESERVED0[2];
  volatile unsigned EMR;
           unsigned RESERVED1[12];
  volatile unsigned CTCR;
};

struct lpc_timer_s *owt = (void*)&T2IR;


void ow_reset_pulse_setup(void)
{
  owt->MR0 = 500;
  owt->MR1 = 500 + 62;
  owt->MR2 = 500 + 500;
}

void ow_bit_setup(void)
{
  owt->MR0 = 6;
  owt->MR1 = 11;
  owt->MR2 = 62;
  owt->MR3 = 70;
}

void TMR2_IRQHandler(void)
{
  switch(owt->IR)
  {
  case 1: // MR0, float on read/write bit on write
    if(ow_direction)
      ow_set(ow_buf[ow_byte_idx] >> ow_bit_idx) & 1);
    else
      ow_float();
    break;
  case 2: // MR1, sample on read
    ram_bit_write(&ow_buf[ow_byte_idx], ow_bit_idx, ow_read());
    break;
  case 4: // MR2, float before end of bit timeslot
    ow_float();
    if(++ow_bit_idx == 8)
    {
      ow_bit_idx = 0;
      if(++ow_byte_idx == ow_byte_count)
      {
        owt->CR = 2; // stop and reset
      }
    }
    break;
  case 16: // MR3, start next bit
    ow_drive();
    break;
  }
  owt->IR = 0x0f; // reset all MRx interrupt bits
}

void ow_rw(int direction, void *data, unsigned len)
{
  if(len == 0)
  {
    ow_completed = 1;
    return;
  }
  ow_completed = 0;
  ow_direction = direction;
  memcpy(ow_buf, data, len);
  if(len > sizeof ow_buf) len = sizeof ow_buf;
  ow_byte_count = len;
  ow_byte_idx = ow_bit_idx = 0;
  ow_bit_setup();
  ahb_bit_write(&owt->MCR, 3, direction ? 0 : 1);
  owt->CR = owt->MR3 - 1;
  owt->TCR = 1; // start
}

void ow_init(void) // board dependant!
{
  PCONP_bits.PCTIM2 = 1;
  owt->PR = SYSTEM_PCLK / 1000000; // 1us tick
  owt->TCR = 2; // stop and reset
  delay_us(10);
  owt->MCR =
    1 <<  0 | // MR0 int
    1 <<  3 | // MR1 int
    1 <<  6 | // MR2 int
    3 <<  9 ; // MR3 int and reset
}

*/


const unsigned char crc8_table[256] = {
    0, 94,188,226, 97, 63,221,131,194,156,126, 32,163,253, 31, 65,
  157,195, 33,127,252,162, 64, 30, 95,  1,227,189, 62, 96,130,220,
   35,125,159,193, 66, 28,254,160,225,191, 93,  3,128,222, 60, 98,
  190,224,  2, 92,223,129, 99, 61,124, 34,192,158, 29, 67,161,255,
   70, 24,250,164, 39,121,155,197,132,218, 56,102,229,187, 89,  7,
  219,133,103, 57,186,228,  6, 88, 25, 71,165,251,120, 38,196,154,
  101, 59,217,135,  4, 90,184,230,167,249, 27, 69,198,152,122, 36,
  248,166, 68, 26,153,199, 37,123, 58,100,134,216, 91,  5,231,185,
  140,210, 48,110,237,179, 81, 15, 78, 16,242,172, 47,113,147,205,
   17, 79,173,243,112, 46,204,146,211,141,111, 49,178,236, 14, 80,
  175,241, 19, 77,206,144,114, 44,109, 51,209,143, 12, 82,176,238,
   50,108,142,208, 83, 13,239,177,240,174, 76, 18,145,207, 45,115,
  202,148,118, 40,171,245, 23, 73,  8, 86,180,234,105, 55,213,139,
   87,  9,235,181, 54,104,138,212,149,203, 41,119,244,170, 72, 22,
  233,183, 85, 11,136,214, 52,106, 43,117,151,201, 74, 20,246,168,
  116, 42,200,150, 21, 75,169,247,182,232, 10, 84,215,137,107, 53
};

char ow_crc(void *buf, unsigned len)
{
  unsigned char crc = 0;
  unsigned char *p = buf;
  while(len--)
    crc = crc8_table[crc ^ *p++];
  return crc;
}

// uart-based ow stuff
// timebase

//const unsigned ow_data_divider = SYSTEM_PCLK / ( 115200 * 16 );
const unsigned ow_data_divider = SYSTEM_PCLK / ( 150000 * 16 );
const unsigned ow_reset_divider = SYSTEM_PCLK / ( 9600 * 16 );

// timeslot character defs
// ttl uart format: lsb first, idle=1, start=0, data=positive (as is), stop=1

#define OW_WR1_R 0xff
#define OW_WR0   0x80 // 28.10.2013


void ow_wr_bit(int bit)
{
  unsigned s=proj_disable_interrupt();
  ow_drive();
  delay_us(8);
  ow_set(bit);
  delay_us(55);
  ow_float();
  delay_us(10);
  proj_restore_interrupt(s);
}

unsigned ow_rd_bit(void)
{
  unsigned s=proj_disable_interrupt();
  ow_drive();
  delay_us(6);
  ow_float();
  delay_us(6);
  unsigned data = ow_read();
  delay_us(60);
  proj_restore_interrupt(s);
  return data;
}

char ow_rd_byte(void)
{
  char data;
  int i;
  for(i=0;i<8;++i)
  {
    data>>=1;
    if(ow_rd_bit()) data |= 0x80;
  }
  return data;
}

void ow_wr_byte(char byte)
{
  char data = byte;
  int i;
  for(i=0;i<8;++i)
  {
    ow_wr_bit(data&1);
    data>>=1;
  }
}

void ow_res_pulse(void)
{
  unsigned s=proj_disable_interrupt();
  ow_drive();
  delay_us(500);
  ow_float();
  delay_us(100);
  ow_present = !ow_rd_bit();
  delay_us(400);
  proj_restore_interrupt(s);
}

void ow_test(void)
{
  ow_res_pulse();
  ow_wr_byte(0x33);
  for(int i=0;i<8;++i)
    ow_rx_buf[i] = ow_rd_byte();
}


void ow_octet(void);

void ow_clear_and_enable_uart(unsigned fifo_trig_lvl)
{
  U3FCR = 1<<0 | 3<<1 | fifo_trig_lvl<<6; // enable fifoss, clear rx tx fifos, 8 byte rx interr trig level
  IOCON_P0_01_bit.FUNC = 2; // connect rx pin
}

void ow_disable_and_clear_uart(void)
{
  IOCON_P0_01_bit.FUNC = 0; // disconnect rx pin
  U3FCR = 1<<0 | 3<<1 | 2<<6; // enable fifoss, clear rx tx fifos, 8 byte rx interr trig level
}

void UART3_IRQHandler(void)
{
  volatile char temp, lsr;
  unsigned char data = 0;
  switch(U3IIR & 15)
  {
  case 6: // rx line error
    temp = U3LSR;
    ow_disable_and_clear_uart();
    ow_timeout = ~0ULL;
    ow_completed = 1;
    ow_error = 7;
    return;

  case 4: // rx interr, programmed in FCR for 8 byte in rx queue
    if(ow_reset_pulse)
    {
      ow_reset_pulse = 0;
      data = U3RBR;
      ow_present = (data != 0xf0);
      // prepare 1w bit timing and 8 bits in one chumk
      ow_disable_and_clear_uart();
      U3LCR_bit.DLAB = 1;
      U3DLL = ow_data_divider & 0xff;
      U3DLM = ow_data_divider >> 8;
      U3LCR_bit.DLAB = 0;
      ow_clear_and_enable_uart(2);
      // initiate 1st byte of 1w data transfer
      ow_octet();
      return;
    }
    // not reset, regular 1w 8 bits in FIFO
    for(int i=0; i<8; ++i) // proces 8 timeslots
    {
      data >>= 1;
      temp = U3RBR;
      lsr = U3LSR;
      // rx processing
      // 2nd bit after start bit is sampled, 13.3 to 20.0us @ 150000 bps // 25.10.2013
      if(temp & 0x02)
        data |= 0x80;
    }
    if(ow_direction)
    { // tx phase
      if(ow_idx == ow_tx_len)
      {
        if(ow_rx_len)
        { // start rx phase
          ow_direction = 0;
          ow_idx = 0;
        }
        else
        { // stop
          ow_disable_and_clear_uart();
          ow_error = 0;
          ow_timeout = ~0ULL;
          ow_completed = 1;
          return;
        }
      }
    }
    else
    { // rx phase
      ow_rx_buf[ow_idx++] = data;
      if(ow_idx == ow_rx_len)
      { // stop
        ow_disable_and_clear_uart();
        ow_error = 0;
        ow_timeout = ~0ULL;
        ow_completed = 1;
        return;
      }
    }
    ow_octet(); // start next ow byte (8 uart bytes)
    break;

  case 12: // rx timeout, very specific processing for uart 1w
    do { temp = U3RBR; }
    while(U3LSR_bit.DR); // purge rx to clear interrupt
    ow_timeout = ~0ULL;
    ow_error = 1;
    ow_completed = 1;
    break;
  }
}

void ow_octet(void)
{ // 1 byte of 1w data = 8 bytes of uart data (1w timeslots)
  ow_clear_and_enable_uart(2);
  unsigned char data = 0xff; // aka 8 read slots
  if(ow_direction)
    data = ow_tx_buf[ow_idx++]; // on tx, tx procedure is responsible for increment, on rx - rh handler
  for(int i=0; i<8; ++i)
    U3THR = ((data >> i) & 1) ? OW_WR1_R : OW_WR0;
}

int ow_t_valid_data(void)
{
  if(ow_rx_len == 0) return 1;
  if(ow_rx_buf[0] == 0x00 || ow_rx_buf[0] == 0xff) // 25.10.2013
  {
    for(int i=0; i<ow_rx_len; ++i)
      if(ow_rx_buf[i] != ow_rx_buf[0])
        return 1;
  }
  return 0;
}

// put data in ow_tx_buf[], call ow_start, wait ow_completed
// if ow_error == 0, rx data can be found in ow_rx_buf[]
void ow_start(unsigned tx_len, unsigned rx_len)
{
  if(tx_len + rx_len == 0)
  {
    ow_error = 1;
    ow_completed = 1;
    return;
  }
  ow_disable_and_clear_uart(); // 23.01.2014 ensure uart disabled before touching buffer index
  ow_tx_len = tx_len;
  ow_rx_len = rx_len;
  ow_direction = tx_len ? 1 : 0;
  ow_idx = ow_error = ow_completed = ow_present = 0;
  // prepare 1w reset timing and uart mode
  U3LCR_bit.DLAB = 1;
  U3DLL = ow_reset_divider & 0xff;
  U3DLM = ow_reset_divider >> 8;
  U3LCR_bit.DLAB = 0;
  // start reset pulse
  ow_reset_pulse = 1;
  ow_clear_and_enable_uart(0); // connect U3RX (enable rx)
  U3THR = 0xF0; // tx reset pulse
  ow_timeout = sys_clock() + 50; // 25->50 24.01.2012
}

/*
если появляется новый датчик между оцифровкой и считыванием,
система сбойнёт, может прочитаться неоцифрованный датчик
- сделать маску оцифрованных датчиков?
- возможен ли контроль срабатывания 0x55 ConvT?

= ow перезапускается при каждом сохранении любого ow_addr
*/

int ow_next(void)
{
  int more_flag = 1;
  int i;
  for(i=0; i<TERMO_N_CH; ++i)
  {
    if(++ow_ch == TERMO_N_CH)
    {
      ow_ch = 0; // loop scan
      more_flag = 0; // scan pass completed
    }
    if(termo_setup[ow_ch].ow_addr[0] == 0x28) // DS18B20
      break; // next ow t sensor found
  }
  if(i == TERMO_N_CH)
  { // no one ow sensor was found
    ow_ch = -1; // no ow sensors in system
    return 0; // no next sensor
  }
  return more_flag;
}

void ow_exec(void)
{
  if(sys_clock() > ow_timeout)
  {
    ow_timeout = ~0ULL;
    ow_disable_and_clear_uart();
    ow_error = 13;
    ow_completed = 1;
  }
}

void ow_restart(void)
{
  ow_ch = -1;
  ow_next(); // select first ow t channel, or still ch = -1
  ow_disable_and_clear_uart();
  ow_completed = 1;
  ow_error = 0;
  ow_repeat = 0;
  ow_scan_time = 11;
  ow_state = OW_START;
  ow_timeout = ~0ULL;
}

void ow_init(void) // ATTN call *after* termo_init()! if no, ow_ch == -1 and dont works
{
  ow_init_pin(); //17.06.2014

  PCONP_bit.PCUART3 = 1;
  IOCON_P0_00_bit.FUNC = 2; // U3TX
  // IOCON_P0_01_bit.FUNC = 2; // U3RX, disconnected by default!
  // U3LCR = 3; // 8-N-1 format
  U3LCR = 7; // 8-N-2 format // 25.10.2013
  U3IER = 1<<0 | 1<<2; // rx interr, rx error interr
  nvic_set_pri(U3_IRQ, 16);
  nvic_enable_irq(U3_IRQ);

  ow_restart();
}

void ow_scan(void)
{
  if(sys_clock_100ms < ow_scan_time) return;
  if(ow_completed == 0) return;
  switch(ow_state)
  {
  case OW_START:
  case OW_READ_ROM:
    ow_tx_buf[0] = 0x33; // read rom
    ow_start(1, 8);
    ow_state = OW_READ_ROM_CHK;
    break;
  case OW_READ_ROM_CHK:
    if(ow_error == 0 && ow_crc(ow_rx_buf, 7) == ow_rx_buf[7])
      memcpy(ow_read_rom_buf, ow_rx_buf, 8);
    else
      memset(ow_read_rom_buf, 0, sizeof ow_read_rom_buf);
    // проваливаемся
  case OW_RH_INIT:
    ow_repeat = 0;
    if(relhum_setup.ow_addr[0] == 0)
    {
      rh_status = RH_STATUS_FAILED;
      rh_real_t_100 = rh_real_t = rh_real_h = 0;
      ow_state = OW_RH_DONE;
    }
    else
    {
      ow_tx_buf[0] = 0x55; // match ROM
      memcpy(ow_tx_buf + 1, relhum_setup.ow_addr, 8);
      ow_tx_buf[9] = 0x4E; // Write scratchpad
      ow_tx_buf[10] = 0; // Page 0
      ow_tx_buf[11] = 0; // offset 0, Config - disable charge integrator, switch ADC to AD input
      ow_start(12,0); // write partial page (only 1 byte)
      ow_state = OW_RH_CONV_T; // skipping saving to sensor's EEPROM, it works good from RAM page
    }
    break;
  case OW_RH_SAVE:
    ow_tx_buf[0] = 0x55; // match ROM
    memcpy(ow_tx_buf + 1, relhum_setup.ow_addr, 8);
    ow_tx_buf[9] = 0x48; // Save page
    ow_tx_buf[10] = 0; // Page 0
    ow_start(11, 0);
    ow_scan_time = sys_clock_100ms + 2; // 18.10.2014 EEPROM prog time
    ow_state = OW_RH_CONV_T;
    break;
  case OW_RH_CONV_T:
    ow_tx_buf[0] = 0x55; // match ROM
    memcpy(ow_tx_buf + 1, relhum_setup.ow_addr, 8);
    ow_tx_buf[9] = 0x44; // conv T
    ow_start(10,0);
    ow_scan_time = sys_clock_100ms + 2;
    ow_state = OW_RH_CONV_V;
    break;
  case OW_RH_CONV_V:
    ow_tx_buf[0] = 0x55; // match ROM
    memcpy(ow_tx_buf + 1, relhum_setup.ow_addr, 8);
    ow_tx_buf[9] = 0xB4; // conv V
    ow_start(10,0);
    ow_repeat = 0;
    ow_scan_time = sys_clock_100ms + 2;
    ow_state = OW_RH_RECALL;
    break;
  case OW_RH_RECALL:
    ow_tx_buf[0] = 0x55; // match ROM
    memcpy(ow_tx_buf + 1, relhum_setup.ow_addr, 8);
    ow_tx_buf[9] = 0xB8; // Recall to SP
    ow_tx_buf[10] = 0; // Page 0
    ow_start(11, 0);
    ow_state = OW_RH_READ;
    break;
  case OW_RH_READ:
    ow_tx_buf[0] = 0x55; // match ROM
    memcpy(ow_tx_buf + 1, relhum_setup.ow_addr, 8);
    ow_tx_buf[9] = 0xBE; // Read scratchpad
    ow_tx_buf[10] = 0; // Page 0;
    ow_start(11, 9);
    /// ow_scan_time = sys_clock_100ms + 2; // 18.10.2014 - what this for?
    ow_state = OW_RH_CHECK;
    break;
  case OW_RH_CHECK:
    if(ow_error == 0)
      if(ow_crc(ow_rx_buf, 8) != ow_rx_buf[8])
        ow_error = 8;
    if(ow_error == 0)
    {
      double h;
      int raw_h;
      ow_repeat = 0;
      rh_real_t = ow_rx_buf[2];
      if(ow_rx_buf[1] & 0x80) ++rh_real_t; // rounding
      // rh_real_t_100 = (int)ceil((ow_rx_buf[2]<<8 | ow_rx_buf[0]) * (100.0/256.0)); // wrong? conv to signed!
      rh_real_t_100 = ((int)(ow_rx_buf[2]<<8 | ow_rx_buf[1]) * 25600) >> 16;  // fixed 8.8 first *(2^8) to be fxp 16.16, then *100, then drop (2^16) frac part; checked for 384 (1.5C) -> 150.
      raw_h = ow_rx_buf[4] << 8 | ow_rx_buf[3];
      h = ((double)raw_h / 500.0 - 0.16) / 0.0062; // 500.0 for 5.00V Vdd
      h = h / (1.0546 - 0.00216*(double)rh_real_t);
      rh_real_h = (int)ceil(h);
      if(rh_real_h < 0) rh_real_h = 0;
      if(rh_real_h > 100) rh_real_h = 100;
      rh_status = RH_STATUS_OK; // comm status
      ow_state = OW_RH_DONE;
    }
    else
    {
      if(++ow_repeat <= 3)
      {
        ow_state = OW_RH_READ;
        break; // repeat
      }
      else
      { // unrecoverable error
        rh_status = RH_STATUS_FAILED;
        rh_real_t_100 = rh_real_t = rh_real_h = 0;
        ow_state = OW_RH_DONE;
      }
    }
    rh_check_status();
    break;
  case OW_RH_DONE:
    if(relhum_setup.ow_addr[0] != 0)
    { // translate T data of RH to termo channel
      for(int t_ch=0; t_ch<TERMO_N_CH; ++t_ch)
      {
        if(memcmp(termo_setup[t_ch].ow_addr, relhum_setup.ow_addr, 8) == 0)
        {
          if(rh_status == RH_STATUS_FAILED)
          {
            termo_state[t_ch].status = 0;
            termo_state[t_ch].value = 0;
          }
          else
          {
            termo_state[t_ch].value = rh_real_t;
            check_termo_status(t_ch);
          }
        } // ow_addr match
      } // for
    } // if rh ow_addr present
    ow_state = OWT_START;
    break;
  case OWT_START:
    if(ow_ch == -1)
    {
      ow_state = OW_START;
      ow_scan_time = sys_clock_100ms + 40;
    }
    else
      ow_state = OWT_CONVERT;
    /// break; // 18.10.2014 проваливаемся, выходить в суперлуп нежелательно, чтобы не поменялось ow_ch != -1
  case OWT_CONVERT:
    if(ow_ch != -1)
    {
      ow_tx_buf[0] = 0x55; // match rom
      memcpy(ow_tx_buf + 1, termo_setup[ow_ch].ow_addr, 8);
      ow_tx_buf[9] = 0x44; // conv T
      ow_start(10,0);
      ow_state = OWT_CONV_CHK;
    }
    break;
  case OWT_CONV_CHK:
    if(ow_error != 0)
    { // 1w error
      if(++ow_repeat <= 3)
      {
        ow_state = OWT_CONVERT; // repeat on next cycle
      }
      else
      {
        termo_state[ow_ch].status = 0;
        termo_state[ow_ch].value = 0;
      }
    }
    else
    { // no 1w error
      ow_repeat = 0;
      if(ow_next())
        ow_state = OWT_CONVERT;
      else
      { // pass to read after 1s pause
        ow_scan_time = sys_clock_100ms + 10;
        ow_state = OWT_READ;
      }
    }
    break;
  case OWT_READ:
    if(ow_ch != -1)
    {
      ow_tx_buf[0] = 0x55; // match rom
      memcpy(ow_tx_buf+1, termo_setup[ow_ch].ow_addr, 8);
      ow_tx_buf[9] = 0xbe; // read s-pad
      ow_start(10, 9);
      ow_state = OWT_READ_CHK;
    }
    break;
  case OWT_READ_CHK:
    if(ow_error == 0)
      if(ow_crc(ow_rx_buf, 8) != ow_rx_buf[8])
        ow_error = 8;
    if(ow_error == 0)
    {
      ow_repeat = 0;
      signed short t = ow_rx_buf[1] << 8 | ow_rx_buf[0];
      if(t & 8) t += 16; // round (is it ok for neg value?)
      termo_state[ow_ch].value = t >> 4;
      check_termo_status(ow_ch);
    }
    else
    {
      if(++ow_repeat <= 3 && ow_t_valid_data())
      {
        ow_state = OWT_READ;
        break; // repeat with current ow_ch on the next main cycle
      }
      else
      { // unrecoverable error
        termo_state[ow_ch].status = 0;
        termo_state[ow_ch].value = 0;
      }
    }
    if(ow_next())
      ow_state = OWT_READ;
    else
    {
      ow_scan_time = sys_clock_100ms + TERMO_READ_PERIOD / 100 - 10; // -10 is T conv pause!
      ow_state = OW_START;
    }
    break;
  } // switch
}


unsigned ow_http_get_addr_cgi(unsigned pkt, unsigned more_data)
{
  char buf[256];
  char *dest = buf;
  unsigned char *owa = ow_read_rom_buf;
  dest += sprintf(dest, "%02x%02x %02x%02x %02x%02x %02x%02x",
              owa[0], owa[1], owa[2], owa[3], owa[4], owa[5], owa[6], owa[7]);
  tcp_put_tx_body(pkt, (void*)buf, strlen(buf));
  return 0;
}

unsigned ow_http_get_addr_data(unsigned pkt, unsigned more_data)
{
  char buf[256];
  char *dest = buf;
  unsigned char *owa = ow_read_rom_buf;
  dest += sprintf(dest, "var data={addr:'%02x%02x %02x%02x %02x%02x %02x%02x'};",
              owa[0], owa[1], owa[2], owa[3], owa[4], owa[5], owa[6], owa[7]);
  tcp_put_tx_body(pkt, (void*)buf, strlen(buf));
  return 0;
}

unsigned ow_http_sensor_init_cgi(unsigned pkt, unsigned more_data)
{
  char buf[256];
  char *dest = buf;
  switch(more_data)
  {
  case 0:
    dest += sprintf(dest, "<html><head></head><body>");
    // проваливаемся
  case 111:
    if(ow_state != OW_START || ow_completed == 0)
    {
      *dest++ = ' ';
      delay(30);
      more_data = 111;
    }
    else
    {
      ow_scan_time = sys_clock_100ms + 10;
      more_data = 1;
    }
    break;
  case 1:
    if(ow_completed)
    {
      if(memcmp(req_args, "w=rh,", 5) == 0)
      {
        ow_tx_buf[0] = 0xcc; // skip rom
        ow_tx_buf[1] = 0x4e; // write scratchpad
        ow_tx_buf[2] = 3; // page 3
        ow_tx_buf[3] = 1; // humidity sensor
        ow_tx_buf[4] = atoi(req_args+5); // version
        memset(ow_tx_buf + 5, 0, 6);
        ow_start(11, 0);
        more_data = 2;
      }
      else if(req_args[0] == 0)
      {
        more_data = 3;
      }
      else
      {
        dest += sprintf(dest, "<p>Wrong CGI arguments!</p></body></html>");
        more_data = 0;
      }
    }
    else
    {
      *dest++ = ' ';
      delay(30);
    }
    break;
  case 2:
    if(ow_completed)
    {
      ow_tx_buf[0] = 0xcc; // skip rom
      ow_tx_buf[1] = 0x48; // copy scratchpad to eeprom
      ow_tx_buf[2] = 3; // page 3
      ow_start(3, 0);
      more_data = 3;
    }
    else
    {
      *dest++ = ' ';
      delay(30);
    }
    break;
  case 3:
    if(ow_completed)
    {
      delay(30);
      ow_tx_buf[0] = 0xcc; // skip rom
      ow_tx_buf[1] = 0xb8; //  recall eeprom to scratchpad
      ow_tx_buf[2] = 3; // page 3
      ow_start(3, 0);
      more_data = 4;
    }
    else
    {
      *dest++ = ' ';
      delay(30);
    }
    break;
  case 4:
    if(ow_completed)
    {
      ow_tx_buf[0] = 0xcc; // skip rom
      ow_tx_buf[1] = 0xbe; // read scratchpad
      ow_tx_buf[2] = 3; // page 3
      ow_start(3, 2);
      more_data = 5;
    }
    else
    {
      *dest++ = ' ';
      delay(30);
    }
    break;
  case 5:
    if(ow_completed)
    {
      dest += sprintf(dest, "<p>%s id=%u (%s), ver=%u</p></body></html>",
                        req_args[0] == 0 ? "Read" : "DONE, Readback",
                        ow_rx_buf[0],
                        ow_rx_buf[0] == 1 ? "RH" : "??",
                        ow_rx_buf[1] );
      more_data = 0;
    }
    else
    {
      *dest++ = ' ';
      delay(30);
    }
    break;
  }
  tcp_put_tx_body(pkt, (void*)buf, dest - buf);
  return more_data;
}

HOOK_CGI(ow_new,            (void*)ow_http_get_addr_cgi,    mime_js,   HTML_FLG_GET | HTML_FLG_NOCACHE );
HOOK_CGI(ow_init,           (void*)ow_http_sensor_init_cgi, mime_html, HTML_FLG_GET | HTML_FLG_NOCACHE );
HOOK_CGI(ow_get_addr_data,  (void*)ow_http_get_addr_data,   mime_js,   HTML_FLG_GET | HTML_FLG_NOCACHE );

void ow_event(enum event_e event)
{
  switch(event)
  {
  case E_EXEC:
    ow_scan();
    ow_exec();
    break;
  case E_INIT:
    ow_init();
    break;
  }
}

#endif //  OW_MODULE
