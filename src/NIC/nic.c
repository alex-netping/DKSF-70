/*
* v1.3
* 23.02.2009
*v1.4-50
*21.05.2012 by LBS
* nic_free_packet(), nic_resise_packet() packet_id check modified
* nic_put_tx_head() etc. simplified (all fields copied at once)
*v2.0-60
*8.05.2013
*  byref api
*v2.1-70
*  modified byfer api, header pointers, tpc_ref()
*/

#include "platform_setup.h"


#ifdef NIC_MODULE

#ifndef NIC_DEBUG
	
	#undef DEBUG_SHOW_TIME_LINE
	#undef DEBUG_MSG			
	#undef DEBUG_PROC_START
	#undef DEBUG_PROC_END
        #undef DEBUG_INPUT_PARAM
        #undef DEBUG_OUTPUT_PARAM	

	#define DEBUG_SHOW_TIME_LINE
	#define DEBUG_MSG(...)			
	#define DEBUG_PROC_START(msg)
	#define DEBUG_PROC_END(msg)
        #define DEBUG_INPUT_PARAM(msg,val)	
        #define DEBUG_OUTPUT_PARAM(msg,val)	
	
#endif

//Описание переменных модуля

int nic_pkt_count;

unsigned char nic_mem_allocation[NIC_RAM_SIZE];
upointer nic_rx_body_pointer;
upointer nic_tx_body_pointer;
struct mac_header mac_head_tx={0};
struct mac_header mac_head_rx={0};

/*
//Описание констант модуля
const struct exec_queue_rec nic_init_table[]={{(upointer)nic_init,NIC_INIT1_PRI|LAST_REC}};
const struct exec_queue_rec nic_exec_table[]={{(upointer)nic_exec,NIC_EXEC1_PRI|LAST_REC}};
const struct module_rec nic_struct={(upointer)nic_init_table, (upointer)nic_exec_table, NULL};
*/

//Описание процедур модуля

void nic_init(void)                              //Процедура инициализации
{
  uword cnt;
  DEBUG_PROC_START("nic_init");
  nic_rx_body_pointer=0;
  nic_tx_body_pointer=0;
  //nic_mem_allocation[0] = NIC_RAM_SIZE&0x7f;
  //for(cnt=1;cnt<NIC_RAM_SIZE;cnt++)nic_mem_allocation[cnt]=0;
  for(cnt=0; cnt<NIC_RAM_SIZE; cnt++) nic_mem_allocation[cnt] = 1;
  nic_merge_blocks();
  DEBUG_PROC_END("nic_init");
}

void nic_exec(void)
{
  DEBUG_PROC_START("nic_exec");
  NIC_GET_PACKET;
  if(NIC_RX_FLAG)
  {
    nic_get_rx_head();
    nic_rx_body_pointer = 0;
    NIC_PACKET_PARSING;
    NIC_PACKET_REMOVE;
  }
  DEBUG_PROC_END("nic_exec");
}


// LBS 11.2009
uword nic_create_packet_sized(unsigned size)
{
  DEBUG_PROC_START("nic_create_packet");

  unsigned chunk_len;
  unsigned required_len;
  unsigned n;

  required_len = size >> 8; // 256 байт блоки. Неправильно, т.к. размер блока задан маросом!
  if(size & 0xFF) required_len += 1;

  for(n=0; n<NIC_RAM_SIZE; n+=chunk_len)
  {
    chunk_len = nic_mem_allocation[n] & 0x7f;
    if((nic_mem_allocation[n] & 0x80) == 0)
    {
      if(chunk_len >= required_len)
      {
        nic_mem_allocation[n] = required_len | 0x80;
        if(chunk_len > required_len)
        {
          nic_mem_allocation[n + required_len] = (chunk_len - required_len) & 0x7F;
        }
        util_fill(mac_head_tx.dest_mac, 6, 0xff);
        util_cpy(NIC_MAC, mac_head_tx.src_mac, 6);
        nic_put_tx_head(n);
        DEBUG_OUTPUT_PARAM(":%x", n);
        DEBUG_PROC_END("nic_create_packet");
        ++ nic_pkt_count;
        return n;
      }
    }
  }
  //ERROR(NICK_ERROR1);
  DEBUG_OUTPUT_PARAM(":%x", 0xff);
  DEBUG_PROC_END("nic_create_packet");
  return 0xff;
}

uword nic_create_packet(void)
{
  return nic_create_packet_sized(6*256);
}

void nic_send_packet(uword packet_id, unsigned short len, uword packet_free)
{
  DEBUG_PROC_START("nic_send_packet");
  DEBUG_INPUT_PARAM("packet_id:%u", packet_id);
  DEBUG_INPUT_PARAM("len:%u", len);
  DEBUG_INPUT_PARAM("packet_free:%u", packet_free);
  /*
  nic_get_tx_head(packet_id);
  nic_put_tx_head(packet_id);
  */
  if(packet_id == 0xff) return;
  NIC_SEND_PACKET(packet_id+NIC_RAM_START, len);
  if(packet_free) nic_free_packet(packet_id);
  else nic_resize_packet(packet_id, len);
  DEBUG_PROC_END("nic_send_packet");
}

extern void nic_resize_packet(uword packet_id, unsigned short len)
{
  uword tmp, str;

  if(packet_id == 0xff) return;
  if(packet_id >= NIC_RAM_SIZE)
    while(1) {} // panic
  tmp = len>>8;
  if(len & 0xff) ++tmp;
  str = nic_mem_allocation[packet_id]&0x7f;
  if(str <= tmp) return;
  nic_mem_allocation[packet_id] = 0x80|tmp;
  nic_mem_allocation[packet_id+tmp] = (str-tmp)&0x7f;
  nic_merge_blocks();
}

void nic_free_packet(uword packet_id)
{
  if(packet_id == 0xff) return;
  if(packet_id >= NIC_RAM_SIZE)
    while(1) {} // panic
  nic_mem_allocation[packet_id] &= 0x7f;
  nic_merge_blocks();
  -- nic_pkt_count;
}

void nic_get_rx_head(void)
{
  NIC_READ_BUF(NIC_RX_ADDR, (void*)&mac_head_rx, 6 + 6 + 2);
}

void nic_get_tx_head(uword packet_id)
{
  NIC_READ_BUF((packet_id+NIC_RAM_START)<<8, (void*)&mac_head_tx, 6 + 6 + 2);
}

void nic_put_tx_head(uword packet_id)
{
  if(packet_id >= NIC_RAM_SIZE) return; // LBS 07.2009
  NIC_WRITE_BUF((packet_id+NIC_RAM_START)<<8, (void*)&mac_head_tx, 6 + 6 + 2);
}

void nic_get_rx_body(unsigned char* buf, uword len)
{
  DEBUG_PROC_START("nic_get_rx_body");
  DEBUG_INPUT_PARAM("*buf:%x", (upointer)buf);
  DEBUG_INPUT_PARAM("len:%u", len);

  NIC_READ_BUF(NIC_RX_ADDR+nic_rx_body_pointer+14, buf, len);
  nic_rx_body_pointer+=len;

  DEBUG_PROC_END("nic_get_rx_body");
}

void nic_get_tx_body(uword packet_id, unsigned char* buf, uword len)
{
  DEBUG_PROC_START("nic_get_tx_body");
  DEBUG_INPUT_PARAM("packet_id:%u", packet_id);
  DEBUG_INPUT_PARAM("*buf:%x", (upointer)buf);
  DEBUG_INPUT_PARAM("len:%u", len);

  NIC_READ_BUF(((NIC_RAM_START+packet_id)<<8)+nic_tx_body_pointer+14, buf, len);
  nic_tx_body_pointer+=len;

  DEBUG_PROC_END("nic_get_tx_body");
}


void nic_put_tx_body(uword packet_id,unsigned char* buf, uword len)
{
  DEBUG_PROC_START("nic_put_tx_body");
  DEBUG_INPUT_PARAM("packet_id:%u", packet_id);
  DEBUG_INPUT_PARAM("*buf:%x", (upointer)buf);
  DEBUG_INPUT_PARAM("len:%u", len);

  if(packet_id >= NIC_RAM_SIZE) return; // LBS 07.2009
  NIC_WRITE_BUF(((NIC_RAM_START+packet_id)<<8)+nic_tx_body_pointer+14, buf, len);
  nic_tx_body_pointer+=len;

  DEBUG_PROC_END("nic_put_tx_body");
}

void nic_merge_blocks(void)
{
  int n, sp, span;
  for(n=0; n<NIC_RAM_SIZE; )
  {
    span = nic_mem_allocation[n] & 0x7f;
    if(span==0) span = 1;
    if((nic_mem_allocation[n] & 0x80)==0) // free span
    {
      while(n+span < NIC_RAM_SIZE && (nic_mem_allocation[n+span] & 0x80)==0) // next is free
      {
        sp = nic_mem_allocation[n+span];
        if(sp==0) sp = 1;
        nic_mem_allocation[n] += sp;
        span += sp;
      }
    }
    n += span;
  }
}

unsigned short pntohs(void *p)
{
  char *q = p;
  return q[0] << 8 | q[1];
}

unsigned long pntohl(void *p)
{
  char *q = p;
  return q[0] << 24 | q[1] << 16 | q[2] << 8 | q[3];
}

int htonps(void *p, unsigned short d)
{
  char *q = p;
  *q++ = (char)(d >> 8);
  *q   = (char)(d >> 0);
  return 2;
}

int htonpl(void *p, unsigned d)
{
  char *q = p;
  *q++ = (char)(d >> 24);
  *q++ = (char)(d >> 16);
  *q++ = (char)(d >>  8);
  *q   = (char)(d >>  0);
   return 4;
}

unsigned short htons(unsigned short d)
{
  return d >> 8 | d << 8;
}

unsigned long htonl(unsigned d)
{
  return
    (d)              >> 24 |
    (d & 0x00ff0000) >>  8 |
    (d & 0x0000ff00) <<  8 |
    (d)              << 24 ;
}

void *nic_ref(unsigned pkt, unsigned offset)
{
  char *p;
  if(pkt == NIC_RX_PACKET)
  {
    p = (char*)&mac_rx_buf[mac236x_parse_struct.packet_desc];
  }
  else
  {
    if(pkt >= MAC236X_MAC_TX_PAGE_NUM) return 0;
    p = (char*)&mac_tx_buf[pkt];
  }
  return p + offset;
}

void *mac_ref(unsigned pkt, int body_offset)
{
  char *p = nic_ref(pkt, 12);
  if(p == 0) return 0;
  if(body_offset == -1) return p; // return mac header (start of eth packet)
  if(pntohs(p) == 0x8100) p += 2; // skip vlan tag
  return p + 2 + body_offset; // skip ethertype
}

void *ip_ref(unsigned pkt, int body_offset)
{
  char *p = mac_ref(pkt, 0);
  if(p == 0) return 0;
  if(body_offset == -1) return p; // return header
  p += (*p & 0x0f) * 4; // use IVH field, skip ip header
  return p + body_offset;
}

void *udp_ref(unsigned pkt, int body_offset)
{
  if(body_offset == -1)
    return ip_ref(pkt, 0); // return header
  else
    return ip_ref(pkt, sizeof udp_rx_head + body_offset);
}

void *tcp_ref(unsigned pkt, int body_offset)
{
  char *p = ip_ref(pkt, 0);
  if(body_offset == -1) return (void*)p;
  p += (p[12] & 0xf0) >> 2; // offset field of top 4 bits, words->byte
  p += body_offset;
  return (void*)p;
}

unsigned nic_event(enum event_e event)
{
  switch(event)
  {
  case E_EXEC:
    nic_exec();
    break;
  case E_INIT:
    nic_init();
    break;
  }
  return 0;
}

#endif

