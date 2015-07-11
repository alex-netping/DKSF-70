/*
* LED module
* by P.Lyubasov
* v2.2
* 22.02.2010
* v2.3
* 24.03.2010 blink bug patch by LBS
*/

#ifndef  LED_H
#define  LED_H

///Версия модуля
#define  LED_VER	3
///Сборка модуля
#define  LED_BUILD	0

extern struct led_state_s {
  short led_time;
  short led_pulse;
  short led_event_time;
  char  led_static;       // статическое состояние СД (1 горит, 0 не горит)
  char  led_blinks;
} led_state[MAX_LED];



/*! Процедура инициирует генерацию импульсов на светодиоде
\param t_low -время пока светодиод выключен
\param t_hi - время пока светодиод включен
\param num - кол-во импульсов которые необходимо сгенерить, если параметр равен 0
             то генерить импульсы бесконечно.
*/
void led_generation(unsigned t_low, unsigned t_hi, unsigned num);

/*! Процедура останавливает генерацию импульсов на светодиоде
 после чего влючает его постояно
*/
extern void led_stop_generation(void);

// blink led_n, period 2*ms, 50% duty cycle
void led_blink(unsigned led_n, int ms);
// set software led state, don't interfere with blink
void led(enum leds_e led, char state);

void led_timer_1ms(void);
void led_init(void);
unsigned led_event(enum event_e event);

#endif

