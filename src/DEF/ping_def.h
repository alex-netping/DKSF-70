#define PING_MODULE		//Флажок включения модуля

//---- Переопределение внешних связей модуля----------

///Кол-во  каналов
#define PING_MAX_CHANNELS       3+1+2 // 0..5, Wdog 1ch, Sms, Logic

#define SMS_PING_CH             3
#define LOGIC_PING_CH           4 /* 4 и 5 */

#include "ping\ping.h"

