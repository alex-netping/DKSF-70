/*
* v1.2
* 17.02.2010
*v1.4-52
*1.10.2010
* освободил PWM/PWM1, задействовал TIMER1
*1.5-48
*5.03.2013
* LPC17xx support
*1.6-70
*30.09.2013 (~)
* modified delay_us(); needs delay_init()
*/

#include "platform_setup.h"

#ifndef  DELAY_H
#define  DELAY_H

///Версия модуля
#define  DELAY_VER	1
///Сборка модуля
#define  DELAY_BUILD	6

void delay(unsigned d /* ms */);
void delay_us(unsigned d /* мкс */);

void delay_init(void);

#endif


