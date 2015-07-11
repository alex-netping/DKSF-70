#include "crc\crc.h"

#define CRC_MODULE		//Флажок включения модуля
//#define CRC_DEBUG		//Флажок включения отладки в модуле

//---- Переопределение внешних связей модуля----------
/*! Внешняя функция обращения к памяти NIC
функция перегружает блок из NIC в буфер в RAM
\param addr адрес блока в NIC
\param buf адрес буфера в RAM
\param len длинна байт для пересылки
*/
#define CRC_GET_NIC_DATA(addr,buf,len) NIC_READ_BUF(addr,buf,len)


