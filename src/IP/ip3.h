/*
* IP module
* author P.V.Lyubasov
*v2.2
* 22.10.2010 - ???
* 3.06.2010 - Gratious ARP
*v2.3-50
*5.03.2012
* void ip_free_packet(uword packet_id) added and used outside instead of nic_free_packet()
v3.0-50
13.11.2012
 major revrite
v3.1-50
7.06.2013
  accept misused ARP responces with wrong target ip
  (responce to web camera incident)
v3.1-60
8.05.2013
  ip_send_packet_to() [byref]
  ip_creatce_packet_sized(size, protocol)
v3.1-48
8.07.2013
  removed __no_init from ip_head_rx etc. declarations
v3.1-52
30.09.2013
  'late use of rx packet ip-mac pair for making reply packet' bugfix
  bugfix og logic if no arp rec is found in ip_send_packet()
*/

#include "platform_setup.h"
#ifndef  IP_H
#define  IP_H

///������ ������
#define  IP_VER	3
///������ ������
#define  IP_BUILD 1

///�������� ���� type MAC ��������� ��� ��������� ARP
#define IP_PROT_TYPE_ARP 0x0806
///�������� ���� type MAC ��������� ��� ��������� IP
#define IP_PROT_TYPE_IP  0x0800

/*! �������� ��������� �������� ��������� ARP
���� ������������ � ������������ � ARP ���������� ������ RFC826
\param hw_type ��� ����������
\param prot_type ��� ���������
\param hw_size ������ ����������� ������
\param prot_size ������ ������ ���������
\param opcode ��� ARP �������
\param sender_mac MAC ����� �����������
\param sender_ip IP ����� �����������
\param target_mac MAC ����� ����������
\param target_ip IP ����� ����������
*/
struct arp_header_s {
  unsigned char hw_type[2];
  unsigned char prot_type[2];
  unsigned char hw_size;
  unsigned char prot_size;
  unsigned char opcode[2];
  unsigned char sender_mac[6];
  unsigned char sender_ip[4];
  unsigned char target_mac[6];
  unsigned char target_ip[4];
};

/*! �������� ��������� �������� ��������� IP
���� ������ ��������� ������������ � ����������� � RFC 791
\param ver_ihl ������ ���������
\param tos ��� �������
\param total_lenght ������ ������
\param id ������������� ������
\param fragment_offset �������� ���������
\param ttl ����� ����� ������
\param protocol ��� ���������� ���������
\param crc ����������� �����
\param src_ip  ip ����� ���������
\param dest_ip ip ����� ���������
*/
struct ip_header_s {
  unsigned char ver_ihl;
  unsigned char tos;
  unsigned char total_len[2]; // 11.11.2012 renamed from 'lenght'
  unsigned char id[2];
  unsigned char fragment_offset[2];
  unsigned char ttl;
  unsigned char protocol;
  unsigned char checksum[2];  // 11.11.2012 renamed from 'crc'
  unsigned char src_ip[4];
  unsigned char dest_ip[4];
};

struct ip_hdr_s { // 8.05.2013
  unsigned char  ver_ihl;
  unsigned char  tos;
  unsigned short total_len;
  unsigned short id;
  unsigned short fragment_offset;
  unsigned char  ttl;
  unsigned char  protocol;
  unsigned short checksum;
  unsigned char  src_ip[4];
  unsigned char  dst_ip[4];
};

//---------------- ������, ��� ����� ������������ ���������� ���������� ������ -------------
///��������� ��� �������������� �������� ��� ��������� ARP ������������� ������
extern struct arp_header_s arp_head_tx;
///��������� ��� �������������� �������� ��� ��������� ARP ��������� ������
extern struct arp_header_s arp_head_rx;
///��������� ��� �������������� �������� ��� ��������� IP ������������� ������
extern struct ip_header_s  ip_head_tx;
///��������� ��� �������������� �������� ��� ��������� IP ��������� ������
extern struct ip_header_s ip_head_rx;
///��������� ������� � ���� ��������� IP ������ (�� ��������� ip_get_rx_body)
extern unsigned ip_rx_body_pointer;
///��������� ������� � ���� �������������� IP ������ (�� ��������� ip_get_tx_bod, ip_put_tx_body)
extern unsigned ip_tx_body_pointer;


/*! ��������� �������� IP ������
* ��������� ��������� ��������� ��������:
*  1. ������� ����� � NIC (NIC_CREATE_PACKET), ���� ����� �� ������ ���������� ���������� 0xFF
*  2. ������������ ���� ��������� MAC type, ��������� � ���� 0x08,0x00
*  3. ������� � ���� ������ ��������� IP ������ �������� ���� ���� �������:
*   ver_ihl 0x45
*   ttl     0x80
*   src_ip  - IP ������ (arp_table[IP_ARP_SELF].ip)
*  4. ���������� ���������� ������.
\return ���������� ������.
*/
unsigned ip_create_packet(void);
unsigned ip_create_packet_sized(unsigned size, unsigned protocol);

/*! ��������� �������� IP ������
* ������� ����� �� ������� ��������� ���������� ARP (���� �� ��� �����),
* ����� �������� nic_free_packet()
*/
void ip_free_packet(uword packet_id); // 5.03.2012

/*! ��������� �������� IP ������
*/
/////uword ip_send_packet(uword packet_id,unsigned short len,uword flag);
//v3
void ip_send_packet(unsigned pkt, unsigned ip_payload_len, int remove_flag);
void ip_send_packet_to(unsigned pkt, unsigned char *ip, unsigned ip_payload_len, int remove_flag);

/*! ��������� ��������� ��������� ��������� ������ � ��������� ip_head_rx
*/
void ip_get_rx_header(void);

/*! ��������� ��������� IP ��������� ������ c ������������ packet_id � ��������� ip_head_tx
*/
void ip_get_tx_header(unsigned packet_id);

/*! ��������� ��������� ������ �� ��������� ip_head_tx � IP ���������� ������ c ������������ packet_id
*/
void ip_put_tx_header(unsigned packet_id);

/*! ��������� ��������� ARP ��������� ��������� ������ � ��������� arp_head_rx
*/
void ip_get_arp_rx_header(void);

/*! ��������� ��������� ARP ��������� ������ � ������������ packet_id � ��������� arp_head_tx
*/
void ip_get_arp_tx_header(unsigned packet_id);

/*! ��������� ��������� ������ �� ��������� arp_head_tx � IP ���������� ������ c ������������ packet_id
*/
void ip_put_arp_tx_header(unsigned packet_id);

/*! ��������� ����������� ���� ��������� ������ IP  (������ ����� ��������� IP) �� ��������
* �������� � ip_rx_body_pointer, � ����� buf ������ len, ����� ���� �������������� ip_rx_body_pointer �� ������ len.
*/
void ip_get_rx_body(void *buf, unsigned len);

/*! ��������� ����������� ���� ������ IP � ������������ packet_id (������ ����� ��������� IP) �� ��������
* �������� � ip_tx_body_pointer,� ����� buf ������ len, ����� ���� �������������� ip_tx_body_pointer �� ������ len.
*/
void ip_get_tx_body(unsigned pkt, void *buf, unsigned len);

/*! ��������� ����������� � ���� ������ IP � ������������ packet_id (������ ����� ��������� IP) �� ��������
* �������� � ip_tx_body_pointer, ����� buf ������ len, ����� ���� �������������� ip_tx_body_pointer �� ������ len.
*/
void ip_put_tx_body(unsigned pkt, void *buf, unsigned len);


/*! ������������� �������� ip_rx_body_pointer
*/
void ip_set_rx_body_addr(unsigned addr);

/*! ������������� �������� ip_tx_body_pointer
*/
void ip_set_tx_body_addr(unsigned addr);

/*! ��������� �������������� IP, MAC ������ � �����, ����������� � ������ IP
*/
void ip_reload(void); // LBS 07.2009

/*! � ���������� ���������� ������ ���� ��������� IP ������, � ������ ����� ���������
*/
extern unsigned ip_rx_body_length;

/*! ��������� gratious arp
*/
void garp_exec(void);
void garp_init(void);

// check not zero, not empty flash/broadcast
// moved from LOG.c in DKSF53, 12.2009
int valid_ip(unsigned char *ip);

unsigned copy4(unsigned char *ip4);

extern unsigned char zero_ip[4]; // 0.0.0.0
extern unsigned char broadcast[6]; // 255.255.255.255,0xff,0xff

void ip_init(void);
void ip_parsing(void); // ������� �������� ip � arp �������
unsigned ip_event(enum event_e event);

#endif

