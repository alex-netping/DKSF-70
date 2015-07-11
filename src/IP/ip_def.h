
#define IP_MODULE		//Флажок включения модуля

/// Константа определяющая сколько сотен мСек живет запись ARP таблицы (максимум 255 т.е 25.5 секунд)
#define IP_ARP_LIVE_TIME 20 // LBS 05.2009  0.1s units
#define IP_ARP_TIMEOUT   5

///Функция для разборки принятого пакета IP уровня
#define IP_PARSING  do{ icmp_parsing(); udp_parsing(); tcp_handler(); }while(0)

#define _IP_HEAD_RX  ip_head_rx
#define _IP_HEAD_TX  ip_head_tx
#define _IP_CREATE_PACKET ip_create_packet()
#define _IP_SEND_PACKET(packet_id,len,flag) ip_send_packet(packet_id,len,flag)
#define _IP_GET_RX_HEADER() ip_get_rx_header()
#define _IP_GET_TX_HEADER(packet_id) ip_get_tx_header(packet_id)
#define _IP_PUT_TX_HEADER(packet_id) ip_put_tx_header(packet_id)
#define _IP_GET_ARP_RX_HEADER  ip_get_arp_rx_header()
#define _IP_GET_ARP_TX_HEADER(packet_id) ip_get_arp_tx_header(packet_id)
#define _IP_PUT_ARP_TX_HEADER(packet_id) ip_put_arp_tx_header(packet_id)
#define _IP_GET_RX_BODY(buf,len) ip_get_rx_body(buf,len)
#define _IP_GET_TX_BODY(packet_id,buf,len) ip_get_tx_body(packet_id,buf,len)
#define _IP_PUT_TX_BODY(packet_id,buf,len) ip_put_tx_body(packet_id,buf,len)
#define _IP_SET_RX_BODY_ADDR(addr) ip_set_rx_body_addr(addr)
#define _IP_SET_TX_BODY_ADDR(addr) ip_set_tx_body_addr(addr)


#include "ip\ip3.h"
