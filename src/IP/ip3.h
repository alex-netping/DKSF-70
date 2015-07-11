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

///Версия модуля
#define  IP_VER	3
///Сборка модуля
#define  IP_BUILD 1

///Значение поля type MAC заголовка для протокола ARP
#define IP_PROT_TYPE_ARP 0x0806
///Значение поля type MAC заголовка для протокола IP
#define IP_PROT_TYPE_IP  0x0800

/*! Описание структуры хранения заголовка ARP
Поля представлены в соостветсвии с ARP заголовком пакета RFC826
\param hw_type тип аппаратуры
\param prot_type тип протокола
\param hw_size длинна аппаратного адреса
\param prot_size длинна адреса протокола
\param opcode тип ARP запроса
\param sender_mac MAC адрес отправителя
\param sender_ip IP адрес отправителя
\param target_mac MAC адрес получателя
\param target_ip IP адрес получателя
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

/*! Описание структуры хранения заголовка IP
Поля данной структуры представлены в соответсвии с RFC 791
\param ver_ihl версия заголовка
\param tos тип сервиса
\param total_lenght длинна пакета
\param id идентификатор пакета
\param fragment_offset смещение фрагмента
\param ttl время жизни пакета
\param protocol тип вложенного протокола
\param crc контрольная сумма
\param src_ip  ip адрес источника
\param dest_ip ip адрес приемника
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

//---------------- Раздел, где будут определяться глобальные переменные модуля -------------
///Структура для промежуточного хранения для заголовка ARP передаваемого пакета
extern struct arp_header_s arp_head_tx;
///Структура для промежуточного хранения для заголовка ARP принятого пакета
extern struct arp_header_s arp_head_rx;
///Структура для промежуточного хранения для заголовка IP передаваемого пакета
extern struct ip_header_s  ip_head_tx;
///Структура для промежуточного хранения для заголовка IP принятого пакета
extern struct ip_header_s ip_head_rx;
///Указатель позиции в теле принятого IP пакета (см процедуру ip_get_rx_body)
extern unsigned ip_rx_body_pointer;
///Указатель позиции в теле прередаваемого IP пакета (см процедуры ip_get_tx_bod, ip_put_tx_body)
extern unsigned ip_tx_body_pointer;


/*! Процедура создания IP пакета
* Процедура выполняет следующие действия:
*  1. Создает пакет в NIC (NIC_CREATE_PACKET), если пакет не создан возвращает дескриптор 0xFF
*  2. Модифицирует поле заголовка MAC type, записывая в него 0x08,0x00
*  3. Создает в теле пакета заголовок IP уровня заполняя поля след образом:
*   ver_ihl 0x45
*   ttl     0x80
*   src_ip  - IP ситемы (arp_table[IP_ARP_SELF].ip)
*  4. Возвращает дескриптор пакета.
\return дескриптор пакета.
*/
unsigned ip_create_packet(void);
unsigned ip_create_packet_sized(unsigned size, unsigned protocol);

/*! Процедура удаления IP пакета
* Убирает пакет из очереди ожидающих разрешения ARP (если он там стоит),
* затем вызывает nic_free_packet()
*/
void ip_free_packet(uword packet_id); // 5.03.2012

/*! Процедура отправки IP пакета
*/
/////uword ip_send_packet(uword packet_id,unsigned short len,uword flag);
//v3
void ip_send_packet(unsigned pkt, unsigned ip_payload_len, int remove_flag);
void ip_send_packet_to(unsigned pkt, unsigned char *ip, unsigned ip_payload_len, int remove_flag);

/*! Процедура размещает заголовок принятого пакета в структуре ip_head_rx
*/
void ip_get_rx_header(void);

/*! Процедура размещает IP заголовок пакета c дескриптором packet_id в структуре ip_head_tx
*/
void ip_get_tx_header(unsigned packet_id);

/*! Процедура размещает данные из структуры ip_head_tx в IP заголовоке пакета c дескриптором packet_id
*/
void ip_put_tx_header(unsigned packet_id);

/*! Процедура размещает ARP заголовок принятого пакета в структуре arp_head_rx
*/
void ip_get_arp_rx_header(void);

/*! Процедура размещает ARP заголовок пакета с дескриптором packet_id в структуре arp_head_tx
*/
void ip_get_arp_tx_header(unsigned packet_id);

/*! Процедура размещает данные из структуры arp_head_tx в IP заголовоке пакета c дескриптором packet_id
*/
void ip_put_arp_tx_header(unsigned packet_id);

/*! Процедура перегружает тело принятого пакета IP  (данные после заголовка IP) со смещения
* заданого в ip_rx_body_pointer, в буфер buf длиной len, после чего инкрементирует ip_rx_body_pointer на длинну len.
*/
void ip_get_rx_body(void *buf, unsigned len);

/*! Процедура перегружает тело пакета IP с дескриптором packet_id (данные после заголовка IP) со смещения
* заданого в ip_tx_body_pointer,в буфер buf длиной len, после чего инкрементирует ip_tx_body_pointer на длинну len.
*/
void ip_get_tx_body(unsigned pkt, void *buf, unsigned len);

/*! Процедура перегружает в тело пакета IP с дескриптором packet_id (данные после заголовка IP) со смещения
* заданого в ip_tx_body_pointer, буфер buf длиной len, после чего инкрементирует ip_tx_body_pointer на длинну len.
*/
void ip_put_tx_body(unsigned pkt, void *buf, unsigned len);


/*! устанавливает смещение ip_rx_body_pointer
*/
void ip_set_rx_body_addr(unsigned addr);

/*! устанавливает смещение ip_tx_body_pointer
*/
void ip_set_tx_body_addr(unsigned addr);

/*! Процедура инициализирует IP, MAC адреса и маску, применяемые в модуле IP
*/
void ip_reload(void); // LBS 07.2009

/*! в переменную помещается размер тела принятого IP пакета, с учётом опций заголовка
*/
extern unsigned ip_rx_body_length;

/*! процедуры gratious arp
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
void ip_parsing(void); // парсинг входящих ip и arp пакетов
unsigned ip_event(enum event_e event);

#endif

