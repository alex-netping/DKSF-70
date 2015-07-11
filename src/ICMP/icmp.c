/*
*\autor - modified by LBS
*version 1.5
*\date 17.02.2010
*v1.6-50
*25.05.2012
* packet creation check in icmp_echo_reply()
* fault protection in icmp_echo_reply() while copying data
*v1.7-50
*11.11.2012
*  removed 'total_lenght', cosmetic rewrite
*v1.8-48
*26.03.2013
*  cosmetic rewrite
*/

#include "platform_setup.h"

#ifdef ICMP_MODULE

#include <string.h>

//Описание переменных модуля

_Pragma("data_alignment=4") struct icmp_header_s icmp_rx_header;
_Pragma("data_alignment=4") struct icmp_header_s icmp_tx_header;

unsigned icmp_rx_body_pointer;
unsigned icmp_tx_body_pointer;

//Описание процедур модуля
void icmp_init(void)
{
  icmp_rx_body_pointer = 0;
  icmp_tx_body_pointer = 0;
}


void icmp_parsing(void)
{
  if (ip_head_rx.protocol == ICMP_PROT)
  {
    icmp_get_rx_header();
    switch(icmp_rx_header.type)
    {
    case ICMP_ECHO:
      icmp_echo_reply();
      break;
    case ICMP_ECHO_REPLY:
      ICMP_PARSING;
      break;
    }
  }
}

unsigned icmp_create_packet_sized(unsigned size)
{
  unsigned pkt = ip_create_packet_sized(size, ICMP_PROT); // 2.02.2014 changed api
  if(pkt != 0xff)
  {
    ip_put_tx_header(pkt);
    memset(&icmp_tx_header, 0, sizeof icmp_tx_header);
    icmp_put_tx_header(pkt);
  }
  return pkt;
}

unsigned icmp_create_packet(void)
{
  return icmp_create_packet_sized(256*6);
}

void icmp_send_packet(unsigned packet_id, unsigned icmp_payload_len)
{
  unsigned buf[64];

  icmp_get_tx_header(packet_id);
  checksum_reset(icmp_tx_header.checksum);
  checksum_incremental_calc(&icmp_tx_header, sizeof icmp_tx_header);
  unsigned len, chunk;
  icmp_tx_body_pointer = 0;
  for(len=icmp_payload_len; len>0; len-=chunk)
  {
    chunk = sizeof buf;
    if(chunk > len) chunk = len;
    icmp_get_tx_body(packet_id, buf, chunk);
    checksum_incremental_calc(buf, chunk);
  }
  checksum_place(icmp_tx_header.checksum);
  icmp_put_tx_header(packet_id);
  ip_send_packet(packet_id, sizeof icmp_tx_header + icmp_payload_len, 1);
}


void icmp_get_rx_header(void)
{
  ip_rx_body_pointer = 0;
  ip_get_rx_body((void*)&icmp_rx_header, sizeof icmp_rx_header);
}

void icmp_get_tx_header(unsigned packet_id)
{
  ip_tx_body_pointer = 0;
  ip_get_tx_body(packet_id, (void*)&icmp_tx_header, sizeof icmp_tx_header);
}

void icmp_put_tx_header(unsigned packet_id)
{
  ip_tx_body_pointer = 0;
  ip_put_tx_body(packet_id, (void*)&icmp_tx_header, sizeof icmp_tx_header);
}

void icmp_get_rx_body(void *buf, unsigned len)
{
  ip_rx_body_pointer = icmp_rx_body_pointer + sizeof icmp_rx_header;
  ip_get_rx_body(buf, len);
  icmp_rx_body_pointer += len;
}

void icmp_get_tx_body(unsigned packet_id, void *buf, unsigned len)
{
  ip_tx_body_pointer = icmp_tx_body_pointer + sizeof icmp_tx_header;
  ip_get_tx_body(packet_id, buf, len);
  icmp_tx_body_pointer += len;
}

void icmp_put_tx_body(unsigned packet_id, void *buf, unsigned len)
{
  ip_tx_body_pointer = icmp_tx_body_pointer + sizeof icmp_tx_header;
  ip_put_tx_body(packet_id, buf, len);
  icmp_tx_body_pointer += len;
}


void icmp_echo_reply(void)
{
  unsigned buf[64];

  unsigned pkt = icmp_create_packet();
  if(pkt == 0xff) return;

  ip_get_tx_header(pkt);
  memcpy(ip_head_tx.dest_ip, ip_head_rx.src_ip, 4);
  ip_put_tx_header(pkt);

  icmp_get_tx_header(pkt);
  icmp_tx_header.type = ICMP_ECHO_REPLY;
  *(short*)icmp_tx_header.id  = *(short*)icmp_rx_header.id;
  *(short*)icmp_tx_header.seq = *(short*)icmp_rx_header.seq;
  icmp_put_tx_header(pkt);

  icmp_tx_body_pointer = 0;
  icmp_rx_body_pointer = 0;
  unsigned icmp_payload_len = ip_rx_body_length - sizeof icmp_rx_header;
  unsigned len, chunk;
  for(len=icmp_payload_len; len > 0; len -= chunk)
  {
    chunk = sizeof buf;
    if(chunk > len) chunk = len;
    icmp_get_rx_body(buf, chunk);
    icmp_put_tx_body(pkt, buf, chunk);
  }
  icmp_send_packet(pkt, icmp_payload_len);
}


#endif
