#include "crc\crc.h"

/*#define MODULE2 &crc_struct
#define MODULE2_INITS  CRC_INITS
#define MODULE2_EXECS  CRC_EXECS
#define MODULE2_TIMERS CRC_TIMERS*/
#define CRC_MODUL		//Флажок включения модуля
//#define CRC_DEBUG		//Флажок включения отладки в модуле

//---- Переопределение внешних связей модуля----------
/*! Внешняя функция обращения к памяти NIC
функция перегружает блок из NIC в буфер в RAM
\param addr адрес блока в NIC
\param buf адрес буфера в RAM
\param len длинна байт для пересылки
*/
#define CRC_GET_NIC_DATA(addr,buf,len) NIC_READ_BUF(addr,buf,len)


