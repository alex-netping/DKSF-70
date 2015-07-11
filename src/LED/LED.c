/*
* LED module
* by P.Lyubasov
* v2.2
* 22.02.2010
* v2.3
* 24.03.2010 blink bug patch by LBS
* v2.4
* 31.05.2010 driver for DKST 51
v3.0
4.03.2013
  some rewrite, CortexM3 style
*/

#include "platform_setup.h"

#ifdef LED_MODULE

struct led_state_s led_state[MAX_LED];

void led_init(void)
{
  led_pin_init();
  unsigned s = proj_disable_interrupt();
  util_fill((unsigned char*)led_state, sizeof led_state, 0);
  proj_restore_interrupt(s);
  led(CPU_LED, 1);
}

void led_timer_1ms(void)
{
  int led_n;
  struct led_state_s *led;
  char led_onoff;

  for(led_n=0, led=led_state; led_n<MAX_LED; ++led_n, ++led)
  {
    led_onoff = led->led_static;
    if(led->led_time < led->led_event_time)
    {
      ++ led->led_time;
      if( led->led_time < led->led_pulse) led_onoff ^= 1;
    }
    else
    {
      /* if(led->led_blinks==255) // LBS 24.03.2010
      {
        led->led_time=0;
        if(led->led_blinks != 255) --led->led_blinks;
      } */
      if(led->led_blinks == 255) led->led_time = 0; // blink forever
      if(led->led_blinks>0) if(--led->led_blinks>0) led->led_time = 0; // start Nth blink
    }
    led_pin((enum leds_e)led_n, led_onoff);
  }
}

void led_blink(unsigned led_n, int ms)
{
  if(led_n >= MAX_LED) return;
  if(ms<50) ms=50;
  if(ms>10000) ms=10000;
  struct led_state_s *led = &led_state[led_n];
  if(led->led_blinks) return;
  if(led->led_time >= led->led_event_time)
  {
    led->led_time = 0;
    led->led_pulse = ms;
    led->led_event_time = ms<<2; // *2
  }
}

void led_generation(unsigned int t_low, unsigned int t_hi, uword num)
{
  struct led_state_s *led = &led_state[CPU_LED];
  if(t_low<50) t_low = 50;
  if(t_hi<50) t_hi = 50;
  if(num>254) num = 254;
  if(num==0) num = 255; // blink forever
  led->led_time=0;
  led->led_pulse = t_hi;
  led->led_event_time = t_low + t_hi;
  led->led_blinks = num;
}


void led_stop_generation(void)
{
  led_state[CPU_LED].led_blinks=0;
}

void led(enum leds_e led, char state)
{
  led_state[(unsigned)led].led_static = state;
}

unsigned led_event(enum event_e event)
{
  switch(event)
  {
  case E_TIMER_1ms:
    led_timer_1ms();
    break;
  case E_INIT:
    led_init();
    break;
  }
  return 0;
}

#endif // LED_MODULE
