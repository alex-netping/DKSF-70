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

// Версия модуля

//Структура описания модуля
extern const struct module_rec nic_struct;


//---------------- Раздел, где будут определяться структуры модуля -------------------------
/*!Описание структуры хранения заголовка MAC уровня
*\param dest_mac[6] MAC назначения
*\param src_mac[6]  MAC источника
*\param prot_type[2] Тип вложенного протокола
*/
struct mac_header{
  unsigned char dest_mac[6];
  unsigned char src_mac[6];
  unsigned char prot_type[2];
};


//---------------- Раздел, где будут определяться глобальные переменные модуля -------------
///Смещение в теле принятого пакета
extern upointer nic_rx_body_pointer;
///Смещение в теле передаваемого пакета
extern upointer nic_tx_body_pointer;
///Струкрура хранения заголовка пакета для передачи
extern struct mac_header mac_head_tx;
///Струкрура хранения заголовка принятого пакета
extern struct mac_header mac_head_rx;
/*! Массив динамического рапределения памяти NIC контроллера
*Номер элемента массива обозначает страницу памяти NIC контроллера
*Элемент массива сотоит:
* бит 0-6: Кол-во страниц в блоке начиная с текушего
* бит 7: Флаг занятости блока. Если в данном бите стоит 1 это обозначает что блок занят.
* При инициализации модуля все элементы инициализируется нулями, кроме первого в который записывается кол-во страниц памяти NIC.
* что означает что все страницы свободны.
* При выделении блока начинается проход массива с первого элемента, для каждого элемента выполняются действия:
* 1. Проверяется свободен ли блок (по биту 7), если нет то переходим к п.4.
* 2. Если блок свободен, проверяется его размер. Если размер не достаточен для рамещения блока переходим к п4
* 3. Начинаем размещать блок, для этого модифицируем элемент массива:
*      3.1. Записываем размер требуемого блока
*      3.2. Устанавливаем бит занятости блока в еденицу
*      3.3. Если размер выделенного блока больше размера блока в котором мы его выделили, модифицируем
*           элемент с номером равным текущему плюс размер выделенного блока. Записываем в данный элемент
*           кол-во страниц равное кол-во страниц исходного блока минус кол-во страниц выделенного блока и ставим
*           флаг занятости в 0 обозначая что блок свободен.
*      3.4. Блок успешно размещен, дескриптор блока равен номеру элемента в массиве.
* 4. Переходим к элементу массива с номером равным номеру текущего блока плюс кол-во страниц которые он занимает и возвращаемся к п1 если
*    не достигнут максимальный конец массива.
* 5. Блок заданной длинны веделить не возможно.
* При удалении(освобождении) блока производятся следующие действия:
* 1. В элементе массива с номером дескриптора блока сбрасывается флаг занятости.
* 2. Производится полный проход массива по цепочке рамещения, и подряд идущие свободные блоки объеденяются в один.
*/
extern unsigned char nic_mem_allocation[NIC_RAM_SIZE];

//---------------- Раздел, где будут определяться функции модуля ---------------------------
/*! Процедура начальной инициализации модуля NIC
* Процедура выполняет следующие действия:
* 1. обнуляет указатели nic_rx_body_pointer, nic_tx_body_pointer и внешний флаг приема пакета
* 2. инициализирует массив распределения памяти nic_mem_allocation (см. описание массива)
*/
extern void nic_init(void);

/*! Процедура обработки принятого пакета
* Процедура выполняет след. действия:
* 1. Вызывает внешню функцию драйвера NIC контроллера NIC_GET_PACKET
* 2. Анализирует флаг приема пакета от этой функции NIC_RX_FLAG. Если флаг в нуле завершает работу
* 3. Заполняет поля mac_head_rx информацией из принятого пакета используя функцию NIC_READ_BUF (адрес принятого пакета лежит в NIC_RX_ADDR)
* 4. Вызывает обработчик NIC_PACKET_PARSING
* 5. Освобождает принятый пакет функцией NIC_PACKET_REMOVE
* 6. Завершение работы.
*/
extern void nic_exec(void);

/*! Процедура создания пакета
* Процедура производит следующие операции:
* 1. Выделяет место под хранение пакета в nic_mem_allocation (см. описание массива) размером 6 страниц.
*    Если место выделить не удалось возвращает 0xFF (вставить вызов ERROR обработчика исключений,
*    номер ошибки описать аналогично с модулем rtl8019).
* 2. На странице NIC c номером дескриптора выделенного блока плюс NIC_RAM_START заполняет заголовок MAC уровня след. образом:
     2.1. заполняет поле src_mac собственным МАС (используя функцию NIC_WRITE_BUF)
* 3. Возвращает дескриптор выделенного блока полученный в п.1
\return дескриптор пакета
*/



/*! Процедура создания пакета размера size
* LBS 11.2009
*/

uword nic_create_packet_sized(unsigned size);


extern uword nic_create_packet(void);

/*! Процедура отправки пакета
* Процедура производит следующие операции:
* 1. Отправляет пакет длинной len со страницы NIC равной номеру дескриптора плюс NIC_RAM_START.
* 2. Если packet_free равен 1, освобождаем занимаемый блок nic_mem_allocation (см. описание массива) и завершаем работу.
* 3. Если packet_free равен 0 то:
*    3.1. изменяем длинну занимаемого блока до размера максимально близкого к len и кратного размеру страницы NIC(256)
*    3.2. Освободившийся остакток блока выделяем в отдельный свободный блок.
*    3.3. Выполняем слияние свободных блоков по всему массиву nic_mem_allocation
* 4. Завершение работы.
\param packet_id дескриптор пакета
\param len длинна пакета
\param packet_free флаг который говорит надо ли освободить занимаемый пакетом блок после его отправки (1 -освободить)
*/
extern void nic_send_packet(uword packet_id,unsigned short len,uword packet_free);

/*! Процедура изменяет размер блока с дискриптером packet_id
*/
extern void nic_resize_packet(uword packet_id, unsigned short len);


/*! Процедура освобождает блок по дискриптору packet_id
* При удалении(освобождении) блока производятся следующие действия:
* 1. В элементе массива с номером дескриптора блока сбрасывается флаг занятости.
* 2. Производится полный проход массива по цепочке рамещения, и подряд идущие свободные блоки объеденяются в один.
*/
extern void nic_free_packet(uword packet_id);

/*! Процедура заполняет структуру mac_head_rx, данными принятого пакета
 Принятый пакет находится на странице NIC_RX_ADDR
*/
extern void nic_get_rx_head(void);

/*! Процедура заполняет структуру mac_head_tx, данными пакета с дескриптором packet_id
 пакет находится на странице packet_id+NIC_RAM_START
\param packet_id -дескриптор пакета
*/
extern void nic_get_tx_head(uword packet_id);

/*! Процедура заполяет заголовок пакета с дескриптором packet_id данными структуры mac_head_tx
 пакет находится на странице packet_id+NIC_RAM_START
\param packet_id -дескриптор пакета
*/
extern void nic_put_tx_head(uword packet_id);

/*! Процедура перегружает тело принятого пакета (данные после заголовка) со смещения
* заданого в nic_rx_body_pointer,в буфер buf длиной len, после чего инкрементирует nic_rx_body_pointer на длинну len.
* Прим: стартовый адрес для пересылки в буфер равняется (NIC_RX_ADDR<<8)+nic_rx_body_pointer+длинна заголовка MAC
\param buf  указатель на буфер
\param len  длинна передаваемых данных
*/
extern void nic_get_rx_body(unsigned char* buf, uword len);

/*! Процедура перегружает тело пакета c дескриптором packet_id (данные после заголовка) со смещения
* заданого в nic_tx_body_pointer,в буфер buf длиной len, после чего инкрементирует nic_tx_body_pointer на длинну len.
* Прим: стартовый адрес для пересылки в буфер равняется ((NIC_RAM_START+packet_id)<<8)+nic_tx_body_pointer+длинна заголовка MAC
\param packet_id дескриптор пакета
\param buf  указатель на буфер
\param len  длинна передаваемых данных
*/
extern void nic_get_tx_body(uword packet_id,unsigned char* buf, uword len);

/*! Процедура перегружает буфер buf длиной len в тело пакета с дескриптором packet_id (данные после заголовка) со смещения
* заданого в nic_tx_body_pointer, после чего инкрементирует nic_tx_body_pointer на длинну len.
* Прим: стартовый адрес для пересылки из буфера равняется ((NIC_RAM_START+packet_id)<<8)+nic_tx_body_pointer+длинна заголовка MAC
\param packet_id дескриптор пакета
\param buf  указатель на буфер
\param len  длинна передаваемых данных
*/
extern void nic_put_tx_body(uword packet_id,unsigned char* buf, uword len);

/*! Процедура сливает смежные свободные блоки
*/
void nic_merge_blocks(void);


/// Макрос устанавливает смещение nic_rx_body_pointer
#define nic_set_rx_body_addr(addr) nic_rx_body_pointer=addr;
/// Макрос устанавливает смещение nic_tx_body_pointer
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
