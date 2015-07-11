
#define TCP_MODULE		//Флажок включения модуля
//#define TCP_DEBUG		//Флажок включения отладки в модуле

//---- Переопределение внешних связей модуля----------
///Указатель на массив где лежит IP адрес системы
//#define TCP_MY_IP arp_table[IP_ARP_SELF].ip
///Размер окна TCP сессии
#define TCP_WINDOW 4096
///Максимальное кол-во открытых  TCP сессий/портов
#define TCP_MAX_SOCKETS 3
///Максимальное кол-во попыток повторной отправки пакета TCP
#define TCP_MAX_RETRY 4
///Таймаут ожидания подтверждения на посланный TCP пакет

//#define TCP_TIMEOUT 30
#define TCP_TIMEOUT 200 // LBS 06.2009
// initial rexmit timeout
#define TCP_REXMIT_TIMEOUT_us  200 // LBS 25.03.2011
//


/*!Функция расчета CRC
 Функция расчета конторльной суммы для буфера buf, длинной len
*/

#define TCP_CALC_CRC(buf,len)  crc_calc(buf,len)

/*!Функция расчета CRC
 Функция расчета конторльной суммы для буфера buf, длинной len
*/

#define TCP_CALC_CRC_NIC(addr,len)  crc_calc_nic(addr,len)

///Функция возвращающая значение CRC
#define TCP_GET_CRC crc_get()

#define IP_SET_RX_BODY_ADDR ip_set_rx_body_addr
#define IP_SET_TX_BODY_ADDR ip_set_tx_body_addr

#define IP_GET_RX_BODY ip_get_rx_body
#define IP_GET_TX_BODY ip_get_tx_body
#define IP_PUT_TX_BODY ip_put_tx_body





#include "tcp\tcp.h"
