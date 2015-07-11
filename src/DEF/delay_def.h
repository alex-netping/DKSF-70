#define DELAY_MODULE		//Флажок включения модуля
//#define DELAY_DEBUG		//Флажок включения отладки в модуле

#ifdef  T1_USED
#error "Timer usage conflict!"
#endif
#define T1_USED

#include "delay\delay.h"
