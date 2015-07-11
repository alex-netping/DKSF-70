
#define TCP_MODULE		//������ ��������� ������
//#define TCP_DEBUG		//������ ��������� ������� � ������

//---- ��������������� ������� ������ ������----------
///��������� �� ������ ��� ����� IP ����� �������
//#define TCP_MY_IP arp_table[IP_ARP_SELF].ip
///������ ���� TCP ������
#define TCP_WINDOW 4096
///������������ ���-�� ��������  TCP ������/������
#define TCP_MAX_SOCKETS 3
///������������ ���-�� ������� ��������� �������� ������ TCP
#define TCP_MAX_RETRY 4
///������� �������� ������������� �� ��������� TCP �����

//#define TCP_TIMEOUT 30
#define TCP_TIMEOUT 200 // LBS 06.2009
// initial rexmit timeout
#define TCP_REXMIT_TIMEOUT_us  200 // LBS 25.03.2011
//


/*!������� ������� CRC
 ������� ������� ����������� ����� ��� ������ buf, ������� len
*/

#define TCP_CALC_CRC(buf,len)  crc_calc(buf,len)

/*!������� ������� CRC
 ������� ������� ����������� ����� ��� ������ buf, ������� len
*/

#define TCP_CALC_CRC_NIC(addr,len)  crc_calc_nic(addr,len)

///������� ������������ �������� CRC
#define TCP_GET_CRC crc_get()

#define IP_SET_RX_BODY_ADDR ip_set_rx_body_addr
#define IP_SET_TX_BODY_ADDR ip_set_tx_body_addr

#define IP_GET_RX_BODY ip_get_rx_body
#define IP_GET_TX_BODY ip_get_tx_body
#define IP_PUT_TX_BODY ip_put_tx_body





#include "tcp\tcp.h"
