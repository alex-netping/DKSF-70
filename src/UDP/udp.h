
#ifndef UDP_H
#define UDP_H

#define UDP_VER     2
#define UDP_BUILD   0

//---------------- ������, ��� ����� ������������ ��������� ������ -------------------------
///�������� ���� protocol, IP ���������, ��� ��������� UDP
#define UDP_PROTO  0x11
//---------------- ������, ��� ����� ������������ ��������� ������ -------------------------
///��������� UDP
struct udp_header {
  unsigned char src_port[2];  //���� ���������
  unsigned char dest_port[2]; //���� ���������
  unsigned char len[2];       //����� ����� ������
  unsigned char CRC[2];       //����������� �����
};

struct udp_hdr_s { // v2
  unsigned short src_port;
  unsigned short dst_port;
  unsigned short total_len;
  unsigned short checksum;
};

//---------------- ������, ��� ����� ������������ ���������� ���������� ������ -------------

extern struct udp_header udp_rx_head;
extern struct udp_header udp_tx_head;
extern unsigned udp_rx_body_pointer;
extern unsigned udp_tx_body_pointer;

//---------------- ������, ��� ����� ������������ ������� ������ ---------------------------

unsigned udp_create_packet(void);
unsigned udp_create_packet_sized(unsigned size);

void udp_send_packet(unsigned packet_id, unsigned payload_len);
void udp_send_packet_to(unsigned pkt, unsigned char *ip, unsigned dst_port, unsigned src_port, unsigned payload_len);

/*! ��������� �������� ��������� ��������� UDP ������ � ��������� udp_rx_head */
void udp_get_rx_header(void);

/*! ��������� �������� ��������� UDP ������ c ������������ id � ��������� udp_tx_head */
void udp_get_tx_header(unsigned packet_id);

/*! ��������� �������� ��������� udp_tx_head � ��������� UDP ������ c ������������ packet_id */
void udp_put_tx_header(unsigned packet_id);


/*! ��������� �������� ������ �� ���� ��������� UDP ������ � ����� buf ������� len
    �������� ������ � ���� UDP ������ ������� ���������� ���������� udp_rx_body_pointer,
    ������� ���������������� �� len */
void udp_get_rx_body(void *buf, unsigned len);


/*! ��������� �������� ������ �� ���� UDP ������ c ������������ packet_id � ����� buf ������� len
    �������� ������ � ���� UDP ������ ������� ���������� ���������� udp_tx_body_pointer,
    ������� ���������������� �� len  */
void udp_get_tx_body(unsigned packet_id, void *buf, unsigned len);

/*! ��������� �������� ����� buf ������� len � ���� UDP ������ c ������������ packet_id
    �������� ������ � ���� UDP ������ ������� ���������� ���������� udp_tx_body_pointer,
    ������� ���������������� �� len */
void udp_put_tx_body(unsigned packet_id, void *buf, unsigned len);

void udp_init(void);
void udp_parsing(void);
void udp_event(enum event_e);

#endif
//}@

