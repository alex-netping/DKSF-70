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
#ifdef DELAY_MODULE

#warning "FYI, module uses CPU Timer 1"

#if CPU_FAMILY == LPC21xx

// задержка в мкс
void delay_us(unsigned d /*мкс*/)
{
  /*
  PWMTCR = 2; // stop, zero TC&PC
  PWMMCR = 0; // disable any match action
  PWMPR = 44; // PCLK = CCLK = 44 236 800 Hz -> PWM clock ~ 1MHz
  PWMTCR = 1; // enable count in Timer mode
  while(PWMTC < d) {}
  */
  T1MTCR = 2; // stop, zero TC&PC
  T1MCR = 0; // disable any match action
  T1MPR = 44; // PCLK = CCLK = 44 236 800 Hz -> timer clock ~ 1MHz
  T1TCR = 1; // enable count in Timer mode
  while(T1TC < d) {}
}

#elif CPU_FAMILY == LPC23xx

// задержка в мкс
void delay_us(unsigned d /*мкс*/)
{
  /*
  PWM1TCR = 2; // stop, zero TC&PC
  PCLKSEL0_bit.PCLK_PWM1 = 1; // PCLK_PWM1 = CCLK
  PWM1MCR = 0; // disable any match action
  PWM1PR = 44; // PCLK = CCLK = 44 236 800 Hz -> PWM clock ~ 1MHz
  PWM1TCR = 1; // enable count in Timer mode
  while(PWM1TC < d) {}
  */
  T1TCR = 2; // stop, zero TC&PC
  PCLKSEL0_bit.PCLK_TIMER1 = 1; // PCLK_TIMER = CCLK
  T1MCR = 0; // disable any match action
  T1PR = 44; // PCLK = CCLK = 44 236 800 Hz -> PWM clock ~ 1MHz
  T1TCR = 1; // enable count in Timer mode
  while(T1TC < d) {}
}

#elif CPU_FAMILY == LPC17xx

void delay_us(unsigned d /*мкс*/)
{
  T1PC = T1TC = 0; // async (immediate) zeroing prescale counter and timer counter
  do {} while(T1TC < d);
}

#else
#  error "Undefined CPU family!"
#endif

// задержка в мс
void delay(unsigned d /* ms */)
{
  delay_us(d*1000);
}

void delay_init(void)
{
  T1TCR = 0; // stop, zero TC&PC
  T1PR = SYSTEM_PCLK / 1000000 - 1; // 1MHz clock
  T1MCR = 0; // disable any match action
  T1CTCR = 0; // timer mode (counts clock)
  T1TCR = 1; // enable count
}

#warning "this version of delay.c needs delay_init() call!"

#endif
