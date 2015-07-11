
#define MODULE14  &led_struct
#define MODULE14_INITS  LED_INITS
#define MODULE14_EXECS  LED_EXECS
#define MODULE14_TIMERS LED_TIMERS
#define LED_MODUL		//Флажок включения модуля
//#define LED_DEBUG		//Флажок включения отладки в модуле
//#define LED_NO_INTR
#define LED_INIT1_PRI 1
#define LED_EXEC1_PRI 1
//---- Переопределение внешних связей модуля----------
/// Функция включение светодиода
#define LED_ON() {TRISC1=0; RC1=1;}
/// Функция выключение светодиода
#define LED_OFF() {TRISC1=0; RC1=0;} 
// Запретить прерывания
#define LED_DIS_INTR GIE=0;
// Разрешить прерывания
#define LED_EN_INTR	 GIE=1;

#include "LED\led.h"
