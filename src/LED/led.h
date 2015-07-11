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

///������ ������
#define  LED_VER	3
///������ ������
#define  LED_BUILD	0

extern struct led_state_s {
  short led_time;
  short led_pulse;
  short led_event_time;
  char  led_static;       // ����������� ��������� �� (1 �����, 0 �� �����)
  char  led_blinks;
} led_state[MAX_LED];



/*! ��������� ���������� ��������� ��������� �� ����������
\param t_low -����� ���� ��������� ��������
\param t_hi - ����� ���� ��������� �������
\param num - ���-�� ��������� ������� ���������� ���������, ���� �������� ����� 0
             �� �������� �������� ����������.
*/
void led_generation(unsigned t_low, unsigned t_hi, unsigned num);

/*! ��������� ������������� ��������� ��������� �� ����������
 ����� ���� ������� ��� ��������
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

