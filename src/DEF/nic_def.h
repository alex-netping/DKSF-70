
#define NIC_MODULE		//Флажок включения модуля
//#define NIC_DEBUG		//Флажок включения отладки в модуле

//---- Переопределение внешних связей модуля----------

///Стартовая страница для динамического создания пакетов
#define NIC_RAM_START 0
///Размер динамической области в страницах
#define NIC_RAM_SIZE MAC236X_MAC_TX_PAGE_NUM

///Функция чтения буфера из памяти NIC контроллера
#define NIC_READ_BUF(addr,buf,len) mac236x_read_buf(addr,buf,len)
///Функция записи буфера в память NIC контроллера
#define NIC_WRITE_BUF(addr,buf,len) mac236x_write_buf(addr,buf,len)
///функция получения информации о принятом пакете
#define NIC_GET_PACKET mac236x_get_packet()
///Флаг "пакет принят"
#define NIC_RX_FLAG mac236x_parse_struct.rcv_flag
///Переменная которая показывает на какой странице памяти NIC контроллера лежит принятый пакет
#define NIC_RX_ADDR  mac236x_parse_struct.packet_addr
///Процедура обработки принятого пакета (прим. может быть не определена)
#define NIC_PACKET_PARSING do { ip_parsing(); } while(0)

///Процедура освобождения кольцевого буфера NIC от принятого пакета
#define NIC_PACKET_REMOVE  mac236x_remove_packet()
///Процедура отправки пакета
#define NIC_SEND_PACKET(addr,len) do { mac236x_send_packet(addr,len); led_blink(CPU_LED, TRAFFIC_LED_TIME); } while(0)
///Переменная, где хранится МАС-адрес системы
#define NIC_MAC sys_setup.mac // 11.11.2012

//------------- Определение вызовов из зависимых модулей ---------------------

#define _NIC_CREATE_PACKET nic_create_packet()
#define _NIC_SEND_PACKET(packet_id,len,packet_free) nic_send_packet(packet_id,len,packet_free)
#define _NIC_SEND_LOCAL(packet_id,len) nic_send_local(packet_id,len)
#define _NIC_GET_RX_HEAD nic_get_rx_head()
#define _NIC_GET_TX_HEAD(packet_id) nic_get_tx_head(packet_id)
#define _NIC_PUT_TX_HEAD(packet_id) nic_put_tx_head(packet_id)
#define _NIC_GET_RX_BODY(buf,len) nic_get_rx_body(buf,len)
#define _NIC_GET_TX_BODY(packet_id,buf,len) nic_get_tx_body(packet_id,buf,len)
#define _NIC_PUT_TX_BODY(packet_id,buf,len) nic_put_tx_body(packet_id,buf,len)
#define _NIC_SET_RX_BODY_ADDR(addr) nic_set_rx_body_addr(addr)
#define _NIC_SET_TX_BODY_ADDR(addr) nic_set_tx_body_addr(addr)
#define _NIC_RESIZE_PACKET(packet_id,len) nic_resize_packet(packet_id,len)
#define _NIC_FREE_PACKET(packet_id) nic_free_packet(packet_id)


#include "nic\nic.h"
