/*
*\autor - modified by LBS
*version 1.5
*\date 22.02.2010
v1.6
by LBS
26.03.2010
v1.7
by LBS
5.04.2010  phy_reset() code
v1.8
by LBS
5.05.2010  mac236x_send_packet()
v 1.9
by LBS
1.06.2010
 phy_reset() moved to separate file
3.06.2010
 lpc23xx_smi_init() bugfix
v1.10-51 by LBS
18.09.2010
 void mac236x_init_rmii_pins(void)
v1.11-52
30.05.2012 by LBS
  additional protection from wrong arguments in module api calls
v1.12-48
4.03.2013
  LPC17xx support
v1.12-60
8.05.2013
  some rewrite, ethermem alignment, pointer access
v1.13-201
  some rewrite of EtherMem read-write
v1.14-60
20.11.2014
  __root-ed (against optimization) buffer align variable (ip etc header from 32 bit boundary)
*/

#include "platform_setup.h"
#ifndef MAC236X_H
#define MAC236X_H

#define MAC236X_VER     1
#define MAC236X_BUILD   14


//---------------- Раздел, где будут определяться константы модуля -------------------------

/*
///Начало области памяти МАС контроллера
#define MAC236X_MAC_MEM_START (0x7FE00000)
///Начало таблицы дескрипторов принятых пакетов
#define MAC236X_MAC_RX_TBL_START (MAC236X_MAC_MEM_START)
///Начало таблицы статусов принятых пакетов
#define MAC236X_MAC_RX_ST_TBL_START (MAC236X_MAC_RX_TBL_START+8*(MAC236X_MAC_RX_PAGE_NUM))
///Начало таблицы дескрипторов передаваемых пакетов
#define MAC236X_MAC_TX_TBL_START (MAC236X_MAC_RX_ST_TBL_START+8*(MAC236X_MAC_RX_PAGE_NUM))
///Начало таблицы статусов передаваемых пакетов
#define MAC236X_MAC_TX_ST_TBL_START (MAC236X_MAC_TX_TBL_START+8*(MAC236X_MAC_TX_DESCR_NUM))
///Начало области памяти под хранение созданных для передачи пакетов
#define MAC236X_PACKETS_START  (0x68+MAC236X_MAC_TX_ST_TBL_START+4*(MAC236X_MAC_TX_DESCR_NUM))
///Начало области памяти под принятые пакеты
#define MAC236X_MAC_RX_MEM_START (MAC236X_PACKETS_START+MAC236X_MAC_TX_PAGE_NUM*MAC236X_TX_PACKET_SIZE)
///Конец занятой памяти MAC контроллера
#define MAC236X_MAC_MEM_END (MAC236X_MAC_RX_MEM_START+MAC236X_RX_PACKET_SIZE*(MAC236X_MAC_RX_PAGE_NUM))
#if MAC236X_MAC_MEM_END > 0x7FE03FFF
 #error "Ethernet MAC memory full!"
#endif
*/


///Начало таблицы дескрипторов принятых пакетов
#define MAC236X_MAC_RX_TBL_START ((unsigned)mac_rx_table)
///Начало таблицы статусов принятых пакетов
#define MAC236X_MAC_RX_ST_TBL_START ((unsigned)mac_rx_status)
///Начало таблицы дескрипторов передаваемых пакетов
#define MAC236X_MAC_TX_TBL_START ((unsigned)mac_tx_table)
///Начало таблицы статусов передаваемых пакетов
#define MAC236X_MAC_TX_ST_TBL_START ((unsigned)mac_tx_status)
///Начало области памяти под хранение созданных для передачи пакетов
#define MAC236X_PACKETS_START  ((unsigned)mac_tx_buf) // placed im ethermem: descriptors, ip hdr align, tx buf, rx buf, unwrap area
///Начало области памяти под принятые пакеты
#define MAC236X_MAC_RX_MEM_START ((unsigned)mac_rx_buf)
///Конец занятой памяти MAC контроллера --- по факту, конец пакетной памяти!!! LBS 11.2009
#define MAC236X_MAC_MEM_END ((unsigned)&mac_rx_buf[MAC236X_MAC_RX_PAGE_NUM])



///Маски поля control для дескриптора принятого пакета
#define MAC236X_RX_LEN_MSK  2047
#define MAC236X_RX_INT_MSK  0x80000000

///Маски поля StatusInfo для статуса принятого пакета
#define MAC236X_RX_SIZE_MSK         2047
#define MAC236X_RX_CTRL_FARME_MSK   0x00040000
#define MAC236X_RX_VLAN_MSK         0x00080000
#define MAC236X_RX_FAIL_FILTER_MSK  0x00100000
#define MAC236X_RX_MULTICAST_MSK    0x00200000
#define MAC236X_RX_BROADCAST_MSK    0x00400000
#define MAC236X_RX_CRC_ERR_MSK      0x00800000
#define MAC236X_RX_SYM_ERR_MSK      0x01000000
#define MAC236X_RX_LEN_ERR_MSK      0x02000000
#define MAC236X_RX_RANGE_ERR_MSK    0x04000000
#define MAC236X_RX_ALIGN_ERR_MSK    0x08000000
#define MAC236X_RX_OVERRUN_MSK      0x10000000
#define MAC236X_RX_NODESCR_MSK      0x20000000
#define MAC236X_RX_LAST_MSK         0x40000000
#define MAC236X_RX_ERR_MSK          0x80000000

///Маски поля control для дескриптора переданного пакета
#define MAC236X_TX_LEN_MSK      2047
#define MAC236X_TX_OVERRIDE_MSK 0x04000000
#define MAC236X_TX_HUGE_MSK     0x08000000
#define MAC236X_TX_PAD_MSK      0x10000000
#define MAC236X_TX_CRC_MSK      0x20000000
#define MAC236X_TX_LAST_MSK     0x40000000
#define MAC236X_TX_INT_MSK      0x80000000

///Маски поля StatusInfo для статуса переданного пакета
#define MAC236X_TX_COLLISION_MSK      0x01E00000
#define MAC236X_TX_DEFER_MSK          0x02000000
#define MAC236X_TX_EX_DEFER_MSK       0x04000000
#define MAC236X_TX_EX_COLLISION_MSK   0x08000000
#define MAC236X_TX_LATECOLLISION_MSK  0x10000000
#define MAC236X_TX_UNDERRUN_MSK       0x20000000
#define MAC236X_TX_NODESCR_MSK        0x40000000
#define MAC236X_TX_ERR_MSK            0x80000000

//---------------- Раздел, где будут определяться структуры модуля -------------------------

struct mac236x_packet_descr {
  unsigned int   packet;
  unsigned int   control;
};


extern struct mac236x_packet_descr mac_rx_table[MAC236X_MAC_RX_PAGE_NUM]; // must be 8 byte aligned!
extern unsigned mac_rx_status[MAC236X_MAC_RX_PAGE_NUM][2];                // must be 8 byte aligned!

extern struct mac236x_packet_descr mac_tx_table[MAC236X_MAC_TX_DESCR_NUM]; // must be 8 byte aligned!
extern unsigned mac_tx_status[MAC236X_MAC_TX_DESCR_NUM];                   // must be 4 byte aligned!

extern unsigned char mac_tx_buf[MAC236X_MAC_TX_PAGE_NUM][256];  // tx buf first, rx second (nic.c/mac236x.c algorithm)
extern unsigned char mac_rx_buf[MAC236X_MAC_RX_PAGE_NUM + 5][256];

//Описание структуры принятого пакета
/*!
\param rcv_flag флаг говорящий что есть принятый пакет
\param packet_addr относительный адрес буфера начала пакета (относительно MAC236X_PACKETS_START)
\param packet_desc номер первого дескриптора принятого пакета
\param packet_len размер принятого пакета
\param next_descr номер следующего RX дескриптора следующего пакета
*/
struct mac236x_parse_rec{
  uword    rcv_flag;
  uword    packet_addr;
  uword    packet_len;
  uword    packet_desc;
  uword    next_descr;
};

//---------------- Раздел, где будут определяться глобальные переменные модуля -------------
//структура принятого пакета
extern struct mac236x_parse_rec mac236x_parse_struct;
//---------------- Раздел, где будут определяться функции модуля ---------------------------
///Процедура инициализации модуля
/*!Процедура производит начальную инициализацию MAC mac236x следующим образом:
* 1. Создает в выделенной памяти MAC (по адресу MAC236X_MAC_RX_TBL_START) таблицу RX дескриторов размером MAC236X_MAC_RX_PAGE_NUM
*    при этом каждый дескриптор указывает на буфер пакета размером 256 байт расположенный по адресу  MAC236X_MAC_RX_MEM_START+256*N
*    где N номер дескриптора пакета (начиная с нуля).
* 2  В регистре MAC1 сбрасвается бит Soft Reset
* 3. Вызывается внешняя процедура инициализации MAC236X_PHY_INIT
* 4. Инициализируются регистры:
*     - RxDescriptor = MAC236X_MAC_RX_TBL_START
*     - RxStatus = MAC236X_MAC_RX_ST_TBL_START
*     - RxDescriptorNumber = MAC236X_MAC_RX_PAGE_NUM
*     - RxConsumeIndex = RxProduceIndex
*     - TxDescriptor = MAC236X_MAC_TX_TBL_START
*     - TxStatus = MAC236X_MAC_TX_ST_TBL_START
*     - TxDescriptorNumber = MAC236X_MAC_RX_PAGE_NUM
*     - TxProduceIndex = TxConsumeIndex
*     - IPGR = 0xC12
*     - IPGT = 0x12
*     - CLRT = 0x370F
*     - MAXF = 0x0600
*     - SUPP = 0x0100
*     - RxFilterCtrl =0x0003
*     - IntEnable =0x0000
*     - Прописать в регистры SA0,SA1,SA2 MAC адрес устройства из внешнего массива MAC236X_MAC_ADDR
*     - MAC1 = 0x0001
*     - MAC2=  0x000D
*     - Command = 0x0603
* 5. Инициализируем структуру mac236x_parse_struct (все поля инициализируются нулем)
*/
extern void  mac236x_init(void);



/*! Процедура анализирует наличие пакетов в очереди принятых пакетов MAC путем сравнения регистров RxConsumeIndex и RxProduceIndex
*   для первого пакета в очереди процедура заполняет структуру mac236x_parse_struct
*   Если в процессе приема пакета возникли ошибки то они обрабатываются процедурой( см User Manual, Reception error handling)
*   и флаг rcv_flag в mac236x_parse_struct сбрасывается
*/
extern void  mac236x_get_packet(void);


/*! Процедура премещает указатель очереди принятых пакетов RxConsumeIndex на следующий пакет за пакетом описанным в структуре mac236x_parse_struct
*/
extern void  mac236x_remove_packet(void);


/*! Процедура отправляет пакет в Ethernet
* Процедура производит следующие действия:
* 1. Создает дескриптор пакета с номером TxProduceIndex и заполяет его поля след. образом:
*  - в поле packet записывает адрес packet_addr*256+MAC236X_PACKETS_START
*  - в поле control записывает длинну len и устанавливает биты MAC236X_TX_PAD_MSK,MAC236X_TX_CRC_MSK и МAC236X_TX_LAST_MSK
* 2. Если TxProduceIndex ==1 то TxProduceIndex =0 иначе TxProduceIndex =1
* 3. Ожидает пока TxProduceIndex==TxConsumeIndex
\param len длинна передаваемых данных
\param packet_addr номер страницы с которой начинается буфер пакета
*/
void  mac236x_send_packet(unsigned int packet_id, unsigned short len);


/*!Процедура читает данные из памяти MAC в буфер
* Прим: Если в процессе чтения данных адрес данных MAC превысил MAC236X_MAC_MEM_END то необходимо установить адреc MAC236X_MAC_RX_MEM_START
* т.е. осуществить переход по кольцу принятых пакетов.
\param addr относительный адрес данных в MAC (относительно MAC236X_PACKETS_START)
\param *buf указатель на буфер
\param len длинна передаваемых данных
*/
extern void  mac236x_read_buf(upointer addr,unsigned char *buf,uword len);


/*! Процедура записывает данные из буфера в память MAC
* Прим: Если в процессе записи данных адрес данных MAC превысил MAC236X_MAC_MEM_END то необходимо установить адреc MAC236X_MAC_RX_MEM_START
* т.е. осуществить переход по кольцу принятых пакетов.
\param addr относительный адрес данных в MAC (относительно MAC236X_PACKETS_START)
\param *buf указатель на буфер
\param len длинна передаваемых данных
*/
extern void  mac236x_write_buf(upointer addr,unsigned char *buf,uword len);


void set_hardware_mac_address(unsigned char *mac);
void mac236x_event(enum event_e event);
void mac236x_init_rmii_pins(void);
void lpc23xx_smi_init(void);
void lpc23xx_smi_write(unsigned fulladdr, unsigned short data);
unsigned short lpc23xx_smi_read(unsigned fulladdr);

#endif
//@}
