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
*v1.4-48
*25.03.2012
* incremental checksum rewrite
*/

#include "platform_setup.h"

#ifdef CRC_MODULE

#include <string.h>

//Описание переменных модуля
unsigned long CRC16;
unsigned short crc16;


//Описание констант модуля


//Описание процедур модуля
void crc_calc(unsigned char *buf, uword len)
{
  while(len>1)
  {
    CRC16 += *((unsigned short*)buf);
    buf += 2;
    len -= 2;
  }
  if(len) CRC16 += *buf;
}

void crc_calc_nic(upointer addr, unsigned short len)
{
  unsigned char buf[2];
  if (len)
  {
    while(len>1)
    { CRC_GET_NIC_DATA(addr,buf,2);
      CRC16 += *((unsigned short*)buf);
      len -= 2;
      addr+=2;
    }
  CRC_GET_NIC_DATA(addr,buf,1);
  if(len) CRC16 += *buf;
 }
}

unsigned int crc_get(void)
{
  while (CRC16>>16) CRC16 = (CRC16 & 0xffff) + (CRC16 >> 16);
  return ((~CRC16)&0xffff);
}

//v3 rock solid
// buf must be aligned at 16 bit in the ip package - LBS 25.03.2013
void calc_and_place_checksum(void *buf, unsigned len, unsigned char *checksum) // LBS 10.11.2012
{
  unsigned char *p = buf;
  unsigned n = len;
  unsigned sum = 0;

  checksum[0] = 0;
  checksum[1] = 0;
  while(n > 1)
  {
    sum +=  *p++ << 8;
    sum +=  *p++ << 0;
    n -= 2;
  }
  if(n) sum += *p++ << 8;
  while(sum >> 16) sum = (sum & 0xffff) + (sum >> 16);
  sum = ~sum;
  checksum[0] = sum >> 8;
  checksum[1] = sum & 0xff;
}

unsigned checksum_acc;

void checksum_reset(void *cs_ptr)
{
  ((char*)cs_ptr)[0] = ((char*)cs_ptr)[1] = 0;
  checksum_acc = 0;
}

// buf может быть выровнен произвольно
// средние части при инкрементальном подсчёте должны иметь _чётную_ длину в байтах
// последняя часть может иметь нечётную длину
// алгоритм использует свойство контрольной суммы - независимость от порядка байт в процессоре
void checksum_incremental_calc(void *buf, unsigned len)
{
  unsigned acc = checksum_acc;
  char *p = buf;
  unsigned n = len;
  while(n > 1)
  {
    acc += *p++ << 0; // 9.05.2013, using native byte order, but alignment independance
    acc += *p++ << 8;
    n -= 2;
  }
  if(n) acc += *p; // 9.05.2013, native byte order
  checksum_acc = acc;
}

void checksum_place(void *cs_ptr)
{
  unsigned cs = checksum_acc;
  while(cs >> 16) cs = (cs & 0xffff) + (cs >> 16);
  cs = ~cs;
  ((char*)cs_ptr)[0] = (cs >> 0) & 0xff; // 9.05.2013, using native byte order
  ((char*)cs_ptr)[1] = (cs >> 8) & 0xff;
}

// 9.05.2013, requires 'by ref' network stack
void checksum_incremental_pseudo(unsigned char *dst_ip, unsigned char *src_ip, char protocol, unsigned ip_payload_length)
{
  char pseudo[12];
  memcpy(pseudo + 0, src_ip, 4);
  memcpy(pseudo + 4, dst_ip, 4);
  pseudo[8]  = 0;
  pseudo[9]  = protocol;
  pseudo[10] = (ip_payload_length >> 8) & 0xff; // netwk byte order
  pseudo[11] = (ip_payload_length >> 0) & 0xff;
  checksum_incremental_calc(pseudo, 12);
}


// LBS 21.05.2010
void crc16_reset(unsigned short *crc16p)
{
  *crc16p = 0x4C7F;
}

// LBS 21.05.2010
void crc16_incremental_calc(unsigned short *crc16p, unsigned char *addr, unsigned size)
{
  unsigned i, j, data, tmp;
  unsigned short crc = *crc16p;
  for (j=0; j<size; j++)
  {
    if((j & 0x3ff) == 0) { WDT_RESET; }
    data = addr[j];
    i=0;
    do {		
      tmp = crc ^ data;
      crc >>= 1;
      if (tmp & 1) crc ^= 0xA001;
      data>>=1;
    } while(++i < 8);
  }
  *crc16p = crc;
}



#endif

