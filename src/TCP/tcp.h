/*
*rewrite by LBS
*v1.5
*17.02.2010
*v1.9-52
*24.03.2011
* cosmetic rewrite, some bugfix
*v2.0-52
*25.05.2011
*  major rewrite, retransmission, send queue (max 2 in flight)
v2.1-52
5.03.2012
  ip_free_packet() is called instead of nic_free_packet()
v2.2-50
14.11.2012
  adapted for ip3.c
v2.2-213
5.9.2012
  dksf213 support (sans http)
v2.2-60
24.02.2013
  tcp window for tcpcom flow ctrl
  handling of rx data by upper netwk layer is modified (tcp_active_session argument)
v2.3-60
9.05.2013
  removed FIN_WAIT_2 state, for browser tcp hang mitigation
v2.4-60
28.05.2013
  removed PSH flag in SYN packet
v2.5-52
14.08.2013
  tcp_create_packet_sized(), SSEvents support (TCP_RESERVED state)
v2.6-60
7.11.2014
  bugfixed wrong segment processing on rx, just ACK last received peer seq number (it will be dup ack or fast rexmit for peer)
*/


#include "platform_setup.h"
#ifndef  TCP_H
#define  TCP_H
///������ ������
#define  TCP_VER	2
///������ ������
#define  TCP_BUILD	6


//---------------- ������, ��� ����� ������������ ��������� ������ -------------------------

///��� ��������� ��� TCP � ��������� ��������� IP ������
#define TCP_PROT  0x06

///����� ��������� TCP
#define TCP_MSK_URG 0x20
#define TCP_MSK_ACK 0x10
#define TCP_MSK_PSH 0x08
#define TCP_MSK_RST 0x04
#define TCP_MSK_SYN 0x02
#define TCP_MSK_FIN 0x01

// ��������� TCP ������
enum tcp_state_e {
  TCP_LISTEN,
  TCP_SYN_RCVD,
  TCP_ESTABLISHED,
  TCP_FIN_WAIT_1,
  TCP_FIN_WAIT_2,
  TCP_TIME_WAIT,
  TCP_CLOSE_WAIT,
  TCP_LAST_ACK,
  TCP_WAIT_ACK,
  TCP_RESERVED
};


//---------------- ������, ��� ����� ������������ ��������� ������ -------------------------
/*! ��������� TCP ������
* port_src - ���� ����������� (������� ���� ������)
* port_dest - ���� ���������� (���������� port_src)
* sequence_num - ����� ������������������ (-\\-)
* ack_num -����� ������������� (-\\-)
* data_offs- �������� ������
* flags- �����
* window -������ ���� (������� ���� ������)
* CRC -����������� �����
* p_urg - �������������� ������
*/
struct tcp_header{
  unsigned char port_src[2];
  unsigned char port_dest[2];
  unsigned char sequence_num[4];
  unsigned char ack_num[4];
  unsigned char data_offs;
  unsigned char flags;
  unsigned char window[2];
  unsigned char CRC[2];
  unsigned char p_urg[2];
  unsigned char option[4];
};

/*! ��������� �������� TCP socket
* used -���� ������������� ������, ���� ����� ������ �� ���� ����� �������� 1
* listen_port -����� ����� ������� ������� ������ ����� (������� ���� ������)
* socket_port -���� � ������� ����������� ���������� (������� ���� ������)
* socket_ip - IP � ������� ����������� ���������� (������� ���� ������)
* ack -����� ������������
* seq- ����� ������������������
* tcp_state - ��������� ����������
* last_ack - ��������� ���������� ����� �������������
* resend_packet- ����� ���������� ���������� ������
* resend_retry - ���-�� �����������
* tcp_timeout- ������� ��������
* tcp_rcv_len -���-�� �������� ����
*/
struct tcp_socket_s {
  uword used;
  unsigned char listen_port[2];
  unsigned char socket_port[2];
  unsigned char socket_ip[4];
  unsigned long ack;
  unsigned long seq;
  enum tcp_state_e tcp_state;
  unsigned long last_ack;
  uword resend_packet;
  uword resend_retry;
  uword tcp_timeout; // unused now
  unsigned short tcp_rcv_len;
  unsigned short tcp_tx_len;
  systime_t closing_time; // 25.03.2011
  systime_t timeout;      // 26.04.2011
  unsigned  char out_in_flight;
  unsigned char  send_q[16];
  unsigned window; // 24.04.2013
};

//---------------- ������, ��� ����� ������������ ���������� ���������� ������ -------------
///���������� ���������� ������������ ����� �������� tcp ����� ��� �������� �����������
extern uword tcp_active_session;
///������� �������
extern struct tcp_socket_s tcp_socket[TCP_MAX_SOCKETS];
///��������� ��� �������� ���������� TCP
extern struct tcp_header tcp_head_rx,tcp_head_tx;
extern upointer tcp_tx_body_pointer,tcp_rx_body_pointer;
//---------------- ������, ��� ����� ������������ ������� ������ ---------------------------
/*!��������� ��������� ������������� ������ TCP
* ����: ��������� �������� ��������� �������� ������.
* ��������� �������������� ������� ������� ����. �������:
* ��� ������� ������ ���� used=0
*/
extern void tcp_init(void);

/*! ��������� ������� TCP �����
* ��������� ��������� ��������� ��������
 - ������� IP �����
 - ��������� ��������� IP ������
     - protocol=TCP_PROT
     - dest_ip= tcp_socket[tcp_socket_num].socket_ip
     - src_ip = TCP_MY_IP
 - ��������� ��������� TCP ������ �� ����� ������
      port_dest=tcp_socket[tcp_socket_num].socket_port
      port_src=tcp_socket[tcp_socket_num].listen_port
      sequence_num =tcp_socket[tcp_socket_num].seq
      ack_num= tcp_socket[tcp_socket_num].ack
      data_offs=0x50;
      window=TCP_WINDOW
      flags=TCP_ACK|TCP_PUSH
\param tcp_socket_num -����� tcp ������
\return -������������� ���������� ������
*/
unsigned tcp_create_packet(unsigned tcp_socket_num);
unsigned tcp_create_packet_sized(unsigned tcp_socket_num, unsigned size);


void tcp_send_packet(unsigned sock_n, unsigned pkt, unsigned len); // 25.03.2011


/*! ��������� �������� ���������� ����� � ������������ packet_id
\param packet_id - ���������� ������
*/
extern void tcp_resend_packet(uword packet_id);

/*! ��������� ����������� ������� ����� � ������������ packet_id
\param packet_id - ���������� ������
*/
// DEBUGGG LBS rexmit
/* extern void tcp_remove_packet(uword packet_id); */
extern void tcp_remove_packet(unsigned socket, uword packet_id); // added socket arg
//

/*! ��������� ��������� ��������� ��������� TCP ������ � ��������� tcp_head_rx
*/
extern void tcp_get_rx_head(void);
/*! ��������� ��������� ��������� TCP ������ � ������������ packet_id � ��������� tcp_head_tx
\param packet_id - ���������� ������
*/
extern void tcp_get_tx_head(uword packet_id);

/*! ��������� ��������� ��������� TCP ������ � ������������ packet_id �� ��������� tcp_head_tx
\param packet_id - ���������� ������
*/
extern void tcp_put_tx_head(uword packet_id);

/*! ��������� ��������� ������ �� ���� TCP ������ � ������������ packet_id � ����� buf, ������� len
\param packet_id - ���������� ������
\param buf- ��������� �� �����
\param len- ������ ������
*/
extern void tcp_get_tx_body(uword packet_id,unsigned char *buf,uword len);

/*! ��������� ��������� ������ �� ���� ��������� TCP ������ � ����� buf, ������� len
\param buf- ��������� �� �����
\param len- ������ ������
*/
extern void tcp_get_rx_body(unsigned char *buf,uword len);

/*! ��������� ��������� ������ � ���� TCP ������ � ������������ packet_id �� ������ buf, ������� len
\param packet_id - ���������� ������
\param buf- ��������� �� �����
\param len- ������ ������
*/
extern void tcp_put_tx_body(uword packet_id,unsigned char *buf,uword len);


/*! ������ ��� ������� ������� � ���� ��������� ������
\pointer- ����� �������
*/
#define tcp_set_rx_body_pos(pointer) tcp_rx_body_pointer=pointer


/*! ������ ��� ������� ������� � ���� ������������� ������
\pointer- ����� �������
*/
#define tcp_set_tx_body_pos(pointer) tcp_tx_body_pointer=pointer

/*!��������� �������� ����� TCP
��������� ���� � ������� ������� ���������, � ����������� ��� ��� ��������� TCP �����
��� ���� ��������� TCP ������ ��� ����� ������ ��������������� � TCP_LISTEN. ��������� ���������� �����
�������� ������,���� ��������� ����� ����� �� ������� �� ����������� �������� 0xFF
\param listen_port - ����� TCP ����� ������� ����� ������� �����
*/
extern uword tcp_open(unsigned short listen_port);

/*!��������� ��������� ��������� TCP �����
* ��������� ��������� �������� TCP ����� ����. �������:
*  1. ��������� ��������� TCP � ��������� tcp_head_rx
*  2. ���� � ������� �������, ����� � ����� used==1 � ����� listen_port==tcp_head_rx.port_dest
*   2.1. ���� ����� ����� � ���� tcp_state �������� �������� ��  TCP_LISTEN,
*        �������� ������������ ����� ������ socket_port � socket_ip, ����� tcp_head_rx.port_src � _IP_HEAD_RX.src_ip.
*        ���� ���� ������������� ����� �������� ����� ������ � ������� � ���������� tcp_active_session, ��������� � �5.
*   2.2. ���� ����� ��������� � �������, ��������� � �3., ����� ��������� � �.2
*  3. ���� � ������� �������, ����� � ����� used==1,
*     ����� listen_port==tcp_head_rx.port_dest, � ����� tcp_state==TCP_LISTEN,
*     ���� ����� �� ������ ������� �� ���������.
*  4. ���������� ��������:
*      - tcp_active_session = ����� ���������� ������ � ��������� tcp_state==TCP_LISTEN.
*      - tcp_socket[tcp_active_session].socket_port = tcp_head_rx.port_src.
*      - tcp_socket[tcp_active_session].socket_ip = _IP_HEAD_RX.src_ip.
*      - tcp_socket[tcp_active_session].ack =  tcp_head_rx.ack ( ���������� ������������� tcp_head_rx.ack )
*
*  5. ����������� ��������� tcp ������ (���� tcp_state ��� ��������� ������).
*     5.1. tcp_state== TCP_LISTEN
*       5.1.1. ����������� ���� ��������� tcp_head_rx.flags. ��������� ���������� �� ���  TCP_MSK_SYN,
*              ���� ��� �� ���������� ������� �� ���������
*       5.1.2. ���������� ��������:
*               - tcp_socket[tcp_active_session].seq = 0
*               - tcp_socket[tcp_active_session].ack ++
*               - tcp_socket[tcp_active_session].tcp_state =TCP_SYN_RCVD;
*               - tcp_socket[tcp_active_session].tcp_timeout =TCP_TIMEOUT;
*       5.1.3. �������� tcp ����� � �������������� ������� TCP_MSK_SYN � TCP_MSK_ACK (���� sequence_num, ack_num �����������
*               �� ����� ������ seq � ack �������������)
*       5.1.4. ��������� ������.
*     5.2. tcp_state== TCP_SYN_RCVD
*       5.2.1. ����������� ���� ��������� tcp_head_rx.flags. ��������� ���������� �� ���  TCP_MSK_ACK,
*              ���� ��� �� ���������� ��������� � 5.2.5.
*       5.2.2. ���������� ��������:
*                - tcp_socket[tcp_active_session].tcp_state=TCP_ESTABLISHED
*                - tcp_socket[tcp_active_session].seq++
*       5.2.3. �������� ������� ���������� TCP_OPEN_HANDLER
*       5.2.4. ��������� ������
*       5.2.5. ����������� ���� ��������� tcp_head_rx.flags. ��������� ���������� �� ���  TCP_MSK_RST.
*       5.2.6. ���������� ��������:
*                - tcp_socket[tcp_active_session].tcp_state=TCP_LISTEN
*       5.2.7. ��������� ������.
*     5.3. tcp_state== TCP_ESTABLISHED ���   tcp_state==TCP_WAIT_ACK
*       5.3.1. ���� ����� ���� TCP_MSK_ACK � tcp_head_rx.flags
*         5.3.1.1. ���������� tcp_head_rx.ack_num � ���� ������ tcp_socket[tcp_active_session].last_ack
*         5.3.1.2. ��������� ���� tcp_socket[tcp_active_session].resend_packet!=0xFF �
*                   tcp_socket[tcp_active_session].ack<=tcp_socket[tcp_active_session].last_ack, ��:
*                   - ������� ��������� ����� � ������������ tcp_socket[tcp_active_session].resend_packet
*                   - tcp_socket[tcp_active_session].resend_packet=0xFF;
*                   - tcp_socket[tcp_active_session].tcp_state=TCP_ESTABLISHED
*       5.3.2. ����������� tcp_socket[tcp_active_session].ack �� ������ ������ TCP ��������� � ������.
*       5.3.3. ���� ����� ���� TCP_MSK_FIN � tcp_head_rx.flags
*         5.3.3.1. ���������� ����. ��������:
*                  - tcp_socket[tcp_active_session].ack++;
*                  - tcp_socket[tcp_active_session].tcp_timeout=TCP_TIME_OUT
*                  - tcp_socket[tcp_active_session].tcp_state=TCP_LAST_ACK
*                  - �������� tcp ����� � �������������� ������� TCP_MSK_FIN � TCP_MSK_ACK (���� sequence_num, ack_num �����������
*                    �� ����� ������ seq � ack �������������)
*                  - ��������� ������.
*       5.3.4. ���� ������ ������ � TCP ������ ������ 0
*         5.3.4.1. ���������� ����.��������:
*                  - � tcp_socket[tcp_active_session].tcp_rcv_len �������� ������ �������� ������.
*                  - �������� tcp ����� � ������������� ������ TCP_MSK_ACK (���� sequence_num, ack_num �����������
*                    �� ����� ������ seq � ack �������������)
*                  - �������� ���������� TCP_HANDLER_DATA
*                  - ��������� ������
*     5.4. tcp_state== TCP_FIN_WAIT_1
*         5.4.1. ��������� tcp_head_rx.flags ���� ������������ ����������� ����� TCP_MSK_FIN � TCP_MSK_ACK, ��:
*                   - tcp_socket[tcp_active_session].tcp_state=TCP_FIN_WAIT_2
*                   - ��������� ������
*                 ����� ���� ���������� ���� TCP_MSK_FIN:
*                   - �������� tcp ����� � ������������� ������ TCP_MSK_ACK (���� sequence_num, ack_num �����������
*                     �� ����� ������ seq � ack �������������)
*                   - tcp_socket[tcp_active_session].tcp_state=TCP_LISTEN
*                   - �������� ���������� TCP_HANDLER_CLOSE
*                   - ��������� ������.
*     5.5. tcp_state== TCP_FIN_WAIT_2
*        5.5.1. ��������� tcp_head_rx.flags ���� ���������� ���� TCP_MSK_FIN , ��:
*                   - tcp_socket[tcp_active_session].ack++;
*                   - tcp_socket[tcp_active_session].seq++;
*                   - tcp_socket[tcp_active_session].tcp_state=TCP_LISTEN;
*                   - �������� tcp ����� � �������������� ������� TCP_MSK_ACK � TCP_MSK_FIN
*                     (���� sequence_num, ack_num ����������� tcp_send_flags(tcp,FIN|ACK); //�������� ACK
*     5.6. tcp_state== TCP_LAST_ACK
*       5.6.1. ��������� tcp_head_rx.flags ���� ���������� ���� TCP_MSK_ACK , ��:
*                   - ����  tcp_socket[tcp_active_session].resend_packet!=0xFF  ������� �� ����� �� NIC
*                    - tcp_socket[tcp_active_session].tcp_state=TCP_LISTEN
*                    - �������� ���������� TCP_HANDLER_CLOSE
*                    - ��������� ������
*/
extern void tcp_handler(void);
/*! ��������� ��������� TCP ������ socket_num
\param socket_num- ����� ������ TCP
*/
extern void tcp_close_session(uword session_num);

/* ���������� ���������� TCP ������
* ����: ��������� �������� ��������� �������� ������.
* ��������� ���������� ��������� �������� ��� ������ ��������� ������� TCP �������
* 1. ����������� ���� used, ���� ��� ����� 0 ��������� � ����. ��������, ������������ �� �1.
* 2. ��������� ���� tcp_timeout, ���� ��� �� ����� 0 ��������� � ����. ��������, ������������ �� �1.
* 3. ����������� ��������� ������ (���� tcp_state)
*   3.1. ���� ���� ����� TCP_WAIT_ACK, ��:
*        3.1.1. ���� ���� resend_retry �� ����� ���� �������� �������� ����� � ������������
*                resend_packet � �������������� ���� resend_retry, ����� ����������� ������ NIC ����������
*                ��� ����� � ��������� ������ � TCP_LISTEN.
*   3.2. ���� ���� ����� TCP_FIN_WAIT_1 ��� TCP_FIN_WAIT_2 ��� TCP_LAST_ACK ��� TCP_SYN_RCVD, ��
*         ���� resend_packet!=0xff, ����������� ����� � ��������� ������ � TCP_LISTEN.
*  4. ��������� � ����.�������� � ������������ �� �1.
*
*/
extern void tcp_exec(void);

/*! ��������� �������������� �������� �������� ��� TCP �������
*  ��������� ������ ������ �� ������� ������� � �������������� ���� tcp_timeout
*/
extern void tcp_timer(void);


#define _TCP_ACTIVE_SESSION tcp_active_session
#define _TCP_SOCKET tcp_socket
#define _TCP_CREATE_PACKET(tcp_socket_num) tcp_create_packet(tcp_socket_num)
#define _TCP_SEND_PACKET(tcp_socket_num,packet_id,len) tcp_send_packet(tcp_socket_num,packet_id,len)
#define _TCP_RESEND_PACKET(packet_id) tcp_resend_packet(packet_id)
#define _TCP_REVOME_PACKET(packet_id) tcp_remove_packet(packet_id)
#define _TCP_GET_RX_HEAD tcp_get_rx_head()
#define _TCP_PUT_RX_HEAD(packet_id) tcp_put_tx_head(packet_id)
#define _TCP_GET_TX_BODY(packet_id,buf,len) tcp_get_tx_body(packet_id,buf,len)
#define _TCP_GET_RX_BODY(buf,len) tcp_get_rx_body(buf,len)
#define _TCP_PUT_TX_BODY(packet_id,buf,len) tcp_put_tx_body(packet_id,buf,len)
#define _TCP_SET_RX_BODY_POS(pointer) tcp_rx_body_pointer=pointer
#define _TCP_SET_TX_BODY_POS(pointer) tcp_tx_body_pointer=pointer
#define _TCP_OPEN(listen_port) tcp_open(listen_port)
#define _TCP_CLOSE_SESSION(session_num) tcp_close_session(session_num)

extern unsigned tcp_rx_data_length;
void tcp_send_flags(uword flags, uword socket);
void tcp_clear_connection(int sock_n);

unsigned tcp_event(enum event_e event);

#endif
//}@
