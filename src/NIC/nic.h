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

#ifndef NIC_H
#define NIC_H

// ������ ������

//��������� �������� ������
extern const struct module_rec nic_struct;


//---------------- ������, ��� ����� ������������ ��������� ������ -------------------------
/*!�������� ��������� �������� ��������� MAC ������
*\param dest_mac[6] MAC ����������
*\param src_mac[6]  MAC ���������
*\param prot_type[2] ��� ���������� ���������
*/
struct mac_header{
  unsigned char dest_mac[6];
  unsigned char src_mac[6];
  unsigned char prot_type[2];
};


//---------------- ������, ��� ����� ������������ ���������� ���������� ������ -------------
///�������� � ���� ��������� ������
extern upointer nic_rx_body_pointer;
///�������� � ���� ������������� ������
extern upointer nic_tx_body_pointer;
///��������� �������� ��������� ������ ��� ��������
extern struct mac_header mac_head_tx;
///��������� �������� ��������� ��������� ������
extern struct mac_header mac_head_rx;
/*! ������ ������������� ������������ ������ NIC �����������
*����� �������� ������� ���������� �������� ������ NIC �����������
*������� ������� ������:
* ��� 0-6: ���-�� ������� � ����� ������� � ��������
* ��� 7: ���� ��������� �����. ���� � ������ ���� ����� 1 ��� ���������� ��� ���� �����.
* ��� ������������� ������ ��� �������� ���������������� ������, ����� ������� � ������� ������������ ���-�� ������� ������ NIC.
* ��� �������� ��� ��� �������� ��������.
* ��� ��������� ����� ���������� ������ ������� � ������� ��������, ��� ������� �������� ����������� ��������:
* 1. ����������� �������� �� ���� (�� ���� 7), ���� ��� �� ��������� � �.4.
* 2. ���� ���� ��������, ����������� ��� ������. ���� ������ �� ���������� ��� ��������� ����� ��������� � �4
* 3. �������� ��������� ����, ��� ����� ������������ ������� �������:
*      3.1. ���������� ������ ���������� �����
*      3.2. ������������� ��� ��������� ����� � �������
*      3.3. ���� ������ ����������� ����� ������ ������� ����� � ������� �� ��� ��������, ������������
*           ������� � ������� ������ �������� ���� ������ ����������� �����. ���������� � ������ �������
*           ���-�� ������� ������ ���-�� ������� ��������� ����� ����� ���-�� ������� ����������� ����� � ������
*           ���� ��������� � 0 ��������� ��� ���� ��������.
*      3.4. ���� ������� ��������, ���������� ����� ����� ������ �������� � �������.
* 4. ��������� � �������� ������� � ������� ������ ������ �������� ����� ���� ���-�� ������� ������� �� �������� � ������������ � �1 ����
*    �� ��������� ������������ ����� �������.
* 5. ���� �������� ������ �������� �� ��������.
* ��� ��������(������������) ����� ������������ ��������� ��������:
* 1. � �������� ������� � ������� ����������� ����� ������������ ���� ���������.
* 2. ������������ ������ ������ ������� �� ������� ���������, � ������ ������ ��������� ����� ������������ � ����.
*/
extern unsigned char nic_mem_allocation[NIC_RAM_SIZE];

//---------------- ������, ��� ����� ������������ ������� ������ ---------------------------
/*! ��������� ��������� ������������� ������ NIC
* ��������� ��������� ��������� ��������:
* 1. �������� ��������� nic_rx_body_pointer, nic_tx_body_pointer � ������� ���� ������ ������
* 2. �������������� ������ ������������� ������ nic_mem_allocation (��. �������� �������)
*/
extern void nic_init(void);

/*! ��������� ��������� ��������� ������
* ��������� ��������� ����. ��������:
* 1. �������� ������ ������� �������� NIC ����������� NIC_GET_PACKET
* 2. ����������� ���� ������ ������ �� ���� ������� NIC_RX_FLAG. ���� ���� � ���� ��������� ������
* 3. ��������� ���� mac_head_rx ����������� �� ��������� ������ ��������� ������� NIC_READ_BUF (����� ��������� ������ ����� � NIC_RX_ADDR)
* 4. �������� ���������� NIC_PACKET_PARSING
* 5. ����������� �������� ����� �������� NIC_PACKET_REMOVE
* 6. ���������� ������.
*/
extern void nic_exec(void);

/*! ��������� �������� ������
* ��������� ���������� ��������� ��������:
* 1. �������� ����� ��� �������� ������ � nic_mem_allocation (��. �������� �������) �������� 6 �������.
*    ���� ����� �������� �� ������� ���������� 0xFF (�������� ����� ERROR ����������� ����������,
*    ����� ������ ������� ���������� � ������� rtl8019).
* 2. �� �������� NIC c ������� ����������� ����������� ����� ���� NIC_RAM_START ��������� ��������� MAC ������ ����. �������:
     2.1. ��������� ���� src_mac ����������� ��� (��������� ������� NIC_WRITE_BUF)
* 3. ���������� ���������� ����������� ����� ���������� � �.1
\return ���������� ������
*/



/*! ��������� �������� ������ ������� size
* LBS 11.2009
*/

uword nic_create_packet_sized(unsigned size);


extern uword nic_create_packet(void);

/*! ��������� �������� ������
* ��������� ���������� ��������� ��������:
* 1. ���������� ����� ������� len �� �������� NIC ������ ������ ����������� ���� NIC_RAM_START.
* 2. ���� packet_free ����� 1, ����������� ���������� ���� nic_mem_allocation (��. �������� �������) � ��������� ������.
* 3. ���� packet_free ����� 0 ��:
*    3.1. �������� ������ ����������� ����� �� ������� ����������� �������� � len � �������� ������� �������� NIC(256)
*    3.2. �������������� �������� ����� �������� � ��������� ��������� ����.
*    3.3. ��������� ������� ��������� ������ �� ����� ������� nic_mem_allocation
* 4. ���������� ������.
\param packet_id ���������� ������
\param len ������ ������
\param packet_free ���� ������� ������� ���� �� ���������� ���������� ������� ���� ����� ��� �������� (1 -����������)
*/
extern void nic_send_packet(uword packet_id,unsigned short len,uword packet_free);

/*! ��������� �������� ������ ����� � ������������ packet_id
*/
extern void nic_resize_packet(uword packet_id, unsigned short len);


/*! ��������� ����������� ���� �� ����������� packet_id
* ��� ��������(������������) ����� ������������ ��������� ��������:
* 1. � �������� ������� � ������� ����������� ����� ������������ ���� ���������.
* 2. ������������ ������ ������ ������� �� ������� ���������, � ������ ������ ��������� ����� ������������ � ����.
*/
extern void nic_free_packet(uword packet_id);

/*! ��������� ��������� ��������� mac_head_rx, ������� ��������� ������
 �������� ����� ��������� �� �������� NIC_RX_ADDR
*/
extern void nic_get_rx_head(void);

/*! ��������� ��������� ��������� mac_head_tx, ������� ������ � ������������ packet_id
 ����� ��������� �� �������� packet_id+NIC_RAM_START
\param packet_id -���������� ������
*/
extern void nic_get_tx_head(uword packet_id);

/*! ��������� �������� ��������� ������ � ������������ packet_id ������� ��������� mac_head_tx
 ����� ��������� �� �������� packet_id+NIC_RAM_START
\param packet_id -���������� ������
*/
extern void nic_put_tx_head(uword packet_id);

/*! ��������� ����������� ���� ��������� ������ (������ ����� ���������) �� ��������
* �������� � nic_rx_body_pointer,� ����� buf ������ len, ����� ���� �������������� nic_rx_body_pointer �� ������ len.
* ����: ��������� ����� ��� ��������� � ����� ��������� (NIC_RX_ADDR<<8)+nic_rx_body_pointer+������ ��������� MAC
\param buf  ��������� �� �����
\param len  ������ ������������ ������
*/
extern void nic_get_rx_body(unsigned char* buf, uword len);

/*! ��������� ����������� ���� ������ c ������������ packet_id (������ ����� ���������) �� ��������
* �������� � nic_tx_body_pointer,� ����� buf ������ len, ����� ���� �������������� nic_tx_body_pointer �� ������ len.
* ����: ��������� ����� ��� ��������� � ����� ��������� ((NIC_RAM_START+packet_id)<<8)+nic_tx_body_pointer+������ ��������� MAC
\param packet_id ���������� ������
\param buf  ��������� �� �����
\param len  ������ ������������ ������
*/
extern void nic_get_tx_body(uword packet_id,unsigned char* buf, uword len);

/*! ��������� ����������� ����� buf ������ len � ���� ������ � ������������ packet_id (������ ����� ���������) �� ��������
* �������� � nic_tx_body_pointer, ����� ���� �������������� nic_tx_body_pointer �� ������ len.
* ����: ��������� ����� ��� ��������� �� ������ ��������� ((NIC_RAM_START+packet_id)<<8)+nic_tx_body_pointer+������ ��������� MAC
\param packet_id ���������� ������
\param buf  ��������� �� �����
\param len  ������ ������������ ������
*/
extern void nic_put_tx_body(uword packet_id,unsigned char* buf, uword len);

/*! ��������� ������� ������� ��������� �����
*/
void nic_merge_blocks(void);


/// ������ ������������� �������� nic_rx_body_pointer
#define nic_set_rx_body_addr(addr) nic_rx_body_pointer=addr;
/// ������ ������������� �������� nic_tx_body_pointer
#define nic_set_tx_body_addr(addr) nic_tx_body_pointer=addr;

#define _NIC_MEM_ALLOCATION nic_mem_allocation

#define NIC_RX_PACKET 0x88

void *nic_ref(unsigned pkt, unsigned raw_offset);
void *mac_ref(unsigned pkt, int body_offset); // -1 for header, 0 for start of body, >0 for body offset
void *ip_ref(unsigned pkt, int body_offset); // -1 for header, 0 for start of body, >0 for body offset
void *udp_ref(unsigned pkt, int body_offset); // -1 for header, 0 for start of body, >0 for body offset
void *tcp_ref(unsigned pkt, int body_offset); // -1 for header, 0 for start of body, >0 for body offset

int htonps(void *p, unsigned short d);
int htonpl(void *p, unsigned d);

unsigned short pntohs(void *p);
unsigned long  pntohl(void *p);

unsigned short htons(unsigned short d);
unsigned long  htonl(unsigned d);

unsigned nic_event(enum event_e event);

#endif
