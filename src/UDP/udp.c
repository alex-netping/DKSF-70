/*
v1.3
22.02.2010
v2.0
8.05.2013
  rewrite for by ref nic.c
*/

#include "platform_setup.h"
#ifdef UDP_MODULE
#include <string.h>

//Описание процедур модуля

struct udp_header udp_rx_head;
struct udp_header udp_tx_head;
unsigned udp_rx_body_pointer; // offset in rx udp packet payload
unsigned udp_tx_body_pointer; // offset in tx udp packet payload


unsigned udp_create_packet_sized(unsigned size)
{
  unsigned pkt = ip_create_packet_sized(size, UDP_PROTO);
  if(pkt == 0xff) return 0xff;
  void *p = ip_ref(pkt, 0);
  memset(p, 0, sizeof (struct udp_hdr_s));
  memset(&udp_tx_head, 0, sizeof udp_tx_head); // legacy
  udp_tx_body_pointer = 0; // legacy
  return pkt;
}

unsigned udp_create_packet(void)
{
  return udp_create_packet_sized(6 * 256);
}

void udp_send_packet(unsigned pkt, unsigned payload_len)
{
  unsigned udplen = payload_len + sizeof (struct udp_hdr_s);
  struct ip_hdr_s  *ih = mac_ref(pkt, 0);
  struct udp_hdr_s *uh = ip_ref(pkt, 0);
  if(ih == 0) return;
  uh->total_len = htons(udplen);
  checksum_reset(&uh->checksum);
  checksum_incremental_pseudo(ih->dst_ip, ih->src_ip, UDP_PROTO, udplen);
  checksum_incremental_calc(uh, udplen);
  checksum_place(&uh->checksum);
  ip_send_packet(pkt, udplen, 1);
}

void udp_send_packet_to(unsigned pkt, unsigned char *ip, unsigned dst_port, unsigned src_port, unsigned payload_len)
{
  unsigned udplen = payload_len + sizeof (struct udp_hdr_s);
  struct udp_hdr_s *uh = ip_ref(pkt, 0);
  if(uh == 0) return;
  uh->dst_port = htons(dst_port);
  uh->src_port = htons(src_port);
  uh->total_len = htons(udplen);
  checksum_reset(&uh->checksum);
  checksum_incremental_pseudo(ip, sys_setup.ip, UDP_PROTO, udplen);
  checksum_incremental_calc(uh, udplen);
  checksum_place(&uh->checksum);
  ip_send_packet_to(pkt, ip, udplen, 1);
}

void udp_parsing(void)
{
  if(ip_head_rx.protocol == UDP_PROTO)
  {
    udp_get_rx_header();
    UDP_PARSING();
  }
}

void udp_init(void)
{
  udp_rx_body_pointer = 0;
  udp_tx_body_pointer = 0;
}

void udp_get_rx_header(void)
{
  char *p = ip_ref(NIC_RX_PACKET, 0);
  memcpy(&udp_rx_head, p, sizeof udp_rx_head);
}

void udp_get_tx_header(unsigned packet_id)
{
  char *p = ip_ref(packet_id, 0);
  if(p == 0) return;
  memcpy(&udp_tx_head, p, sizeof udp_tx_head);
}

void udp_put_tx_header(unsigned packet_id)
{
  char *p = ip_ref(packet_id, 0);
  if(p == 0) return;
  memcpy(p, &udp_tx_head, sizeof udp_tx_head);
}

void udp_get_rx_body(void *buf, unsigned len)
{
  char *p = udp_ref(NIC_RX_PACKET, udp_rx_body_pointer);
  memcpy(buf, p, len);
  udp_rx_body_pointer += len;
}

void udp_get_tx_body(unsigned packet_id, void *buf, unsigned len)
{
  char *p = udp_ref(packet_id, udp_tx_body_pointer);
  if(p == 0) return;
  memcpy(buf, p, len);
  udp_tx_body_pointer += len;
}

void udp_put_tx_body(unsigned packet_id, void *buf, unsigned len)
{
  char *p = udp_ref(packet_id, udp_tx_body_pointer);
  if(p == 0) return;
  memcpy(p, buf, len);
  udp_tx_body_pointer += len;
}


void udp_event(enum event_e event)
{
  // empty
}


#endif



