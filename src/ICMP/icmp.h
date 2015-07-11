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

#ifndef ICMP_H
#define ICMP_H

#define ICMP_VER     1
#define ICMP_BUILD   8


///Значение поля protocol, IP заголовка, для протокола ICMP
#define ICMP_PROT         0x01
///Значение поля type, ICMP заголовка, для пакетов Echo
#define ICMP_ECHO         0x08
///Значение поля type, ICMP заголовка, для пакетов Echo reply
#define ICMP_ECHO_REPLY   0x00
///Значение поля type, ICMP заголовка, для пакетов TTL exceed
#define ICMP_TTL_EXCEED   0x0B


struct icmp_header_s {
  unsigned char type;
  unsigned char opcode;
  unsigned char checksum[2];
  unsigned char id[2];
  unsigned char seq[2];
};


extern struct icmp_header_s icmp_rx_header;
extern struct icmp_header_s icmp_tx_header;

extern unsigned icmp_rx_body_pointer;
extern unsigned icmp_tx_body_pointer;

void icmp_init(void);
void icmp_parsing(void);
unsigned icmp_create_packet(void);
unsigned icmp_create_packet_sized(unsigned size);
void icmp_send_packet(unsigned packet_id, unsigned len);

void icmp_get_rx_header(void);
void icmp_get_tx_header(unsigned packet_id);
void icmp_put_tx_header(unsigned packet_id);
void icmp_get_rx_body(void *buf, unsigned len);
void icmp_get_tx_body(unsigned packet_id, void *buf, unsigned len);
void icmp_put_tx_body(unsigned packet_id, void *buf, unsigned len);

void icmp_echo_reply(void);

#endif

