
#define ICMP_MODULE		//Флажок включения модуля
//#define ICMP_DEBUG		//Флажок включения отладки в модуле

//---- Переопределение внешних связей модуля----------
///Функция расчета CRC
#define ICMP_CALC_CRC(buf, len)   crc_calc(buf, len)
///Функция возвращающая значение CRC
#define ICMP_GET_CRC crc_get()

///Функция разбора пришедшего ICMP_Echo_reply пакета
// replying to incoming Echo Request is in icmp.c!
#define ICMP_PARSING  do{/*ping_parsing();*/}while(0)


#include "icmp\icmp.h"
