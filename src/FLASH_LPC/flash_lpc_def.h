#include "flash_lpc\flash_lpc.h"

/*
#define MODULE0_INITS  FLASH_LPC_INITS
#define MODULE0_EXECS  FLASH_LPC_EXECS
#define MODULE0_TIMERS FLASH_LPC_TIMERS*/
#define FLASH_LPC_MODUL		//Флажок включения модуля
//#define FLASH_LPC_DEBUG		//Флажок включения отладки в модуле

//---- Переопределение внешних связей модуля----------

/*! Тип процессора 
 Прим. один из перечисленных в flash_lpc.h типов
*/
#define FLASH_LPC_PROC 3

