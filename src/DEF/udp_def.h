#define UDP_MODULE		//������ ��������� ������
//#define UDP_DEBUG		//������ ��������� ������� � ������

///������� ������� ���������� UDP ������
#define UDP_PARSING()      do{ dns_parsing(); parse_snmp(); parse_ntp(); /* tftp_parse();  dhcp_parsing(); */}while(0)

///������� ������� CRC
#define UDP_CALC_CRC(buf, len) crc_calc(buf, len)
///������� ������������ �������� CRC
#define UDP_GET_CRC crc_get()

#define _UDP_RX_HEAD udp_rx_head
#define _UDP_TX_HEAD udp_tx_head
#define _UDP_RX_BODY_POINTER  udp_rx_body_pointer
#define _UDP_TX_BODY_POINTER  udp_tx_body_pointer
#define _UDP_CREATE_PACKET udp_create_packet()
#define _UDP_SEND_PACKET(packet_id,len) udp_send_packet(packet_id,len)
#define _UDP_GET_RX_HEADER udp_get_rx_header()
#define _UDP_GET_TX_HEADER(packet_id) udp_get_tx_header(packet_id)
#define _UDP_PUT_TX_HEADER(packet_id) udp_put_tx_header(packet_id)
#define _UDP_GET_RX_BODY(buf,len) udp_get_rx_body(buf,len)
#define _UDP_GET_TX_BODY(packet_id,buf,len) udp_get_tx_body(packet_id,buf,len)
#define _UDP_PUT_TX_BODY(packet_id,buf,len) udp_put_tx_body(packet_id,buf,len)


#include "udp\udp.h"
