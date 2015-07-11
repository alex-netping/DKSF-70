/* Модуль CRC предназначен для расчета контрольных сумм для TCP/IP стека и для модуля PARAMETERS
*\autor
*version 1.0
*\date 19.07.2007
*
* v 1.1
* 08.05.2010 by LBS
*   place_checksum()
*
* v 1.2
* 21.05.2010 by LBS
*   crc16_reset()
*   crc16_incremental_calc()
*v1.3
*10.11.2012
*  place_checksum() rewrite, now calc_and_place_checksum()
*/

#include "platform_setup.h"
#ifndef  CRC_H
#define  CRC_H

#define CRC_VER     1
#define CRC_BUILD   3


#define CRC16_POLINOM		0xA001			

///Глобальная переменная для хранения промежуточной контрольной суммы TCP/IP
extern unsigned long CRC16;

///Глобальная переменная для хранения промежуточной/конечной контрольной суммы для модуля parameters
extern unsigned short crc16;


/*! Функция вычисляет CRC для TCP/IP стека (см RFC 1071)
Прим. Пример кода функции можно взять из проекта DKSF 50 (модуль crc.c функция crc16_sum)
Прим. функция вычисляет CRC в переменной CRC16
\param buf указатель на буфер в памяти
\param len размер буфера в байтах
*/
void crc_calc(unsigned char *buf,uword len); // На самом деле это chechsum, а не CRC - LBS

/*! Функция вычисляет CRC для TCP/IP стека (см RFC 1071)
функция работает аналогично crc_calc но расчитывает контрольную сумму по буферу nic
Для получения масива из NIC функция использует внешнюю функцию CRC_GET_NIC_DATA
Прим.функция вычисляет CRC в переменной CRC16
\param addr адрес начала буфера в NIC
\param len  размер буфера в байтах
*/
void crc_calc_nic(upointer addr,unsigned short len); // На самом деле это chechsum, а не CRC - LBS

/*! Функция возвращает CRC для TCP/IP стека (см RFC 1071)
функция выполняет финальную операцию расчета CRC.
Прим. Пример кода функции можно взять из проекта DKSF 50 (модуль crc.c функция crc16_get)
Прим. функция вычисляет CRC из переменной CRC16
\return вычисленное значение CRC16
*/

unsigned int crc_get(void); // На самом деле это chechsum, а не CRC - LBS

/*! Функция вычисляет CRC16 для модуля Parameters
Прим. Пример кода функции можно взять из проекта DKSF 50 (модуль crc.c функция update_crc16)
функция вычисляет CRC в переменной crc16
\param buf указатель на буфер
\param len длинна буфера
*/
void crc_param(unsigned char *buf,uword len); // А вот это действительно CRC - LBS

// LBS 08.05.2010
void place_checksum(void *vbuf, unsigned len, unsigned char *checksum);
// LBS 21.05.2010
void crc16_reset(unsigned short *crc16p);
void crc16_incremental_calc(unsigned short *crc16p, unsigned char *addr, unsigned size);
// LBS 11.11.2012
void calc_and_place_checksum(void *buf, unsigned len, unsigned char *checksum);


void checksum_reset(void *cs_ptr);
void checksum_incremental_pseudo(unsigned char *dst_ip, unsigned char *src_ip, char protocol, unsigned ip_payload_length);
void checksum_incremental_calc(void *buf, unsigned len);
void checksum_place(void *cs_ptr);

#endif

