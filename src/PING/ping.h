/*
* ping.c
*v1.1
*23.01.08 by Kovlyagin V.N.
*25.03.2010 modified by LBS
*v1.3
*2.06.2010 compatible rewrite by LBS
*v2.4-48
*  rewrite, internal retries, removed unused functionality
*v2.5-52
*  non-valid ip => channel fail (use with DNS)
*/

#include "platform_setup.h"
#ifndef  PING_H
#define  PING_H
///Версия модуля
#define  PING_VER	2
///Сборка модуля
#define  PING_BUILD	5


//---------------- Раздел, где будут определяться константы модуля -------------------------

enum ping_state_e {
    PING_RESET = 0,
    PING_START,
    PING_RETRY,
    PING_WAIT_ANSWER,
    PING_COMPLETED
};

//---------------- Раздел, где будут определяться структуры модуля -------------------------

struct ping_state_s {
  unsigned char  ip[4];
  unsigned short timeout;     // ms
  unsigned short max_retry;
  unsigned short count;
  enum ping_state_e state;
  unsigned char  result;
  systime_t      wait_end_time;
  unsigned       icmp_seq;
};


//---------------- Раздел, где будут определяться глобальные переменные модуля -------------

extern struct   ping_state_s ping_state[PING_MAX_CHANNELS];
extern unsigned ping_reset;
extern unsigned ping_start;
extern unsigned ping_completed;

//---------------- Раздел, где будут определяться функции модуля ---------------------------


void ping_init(void);
void ping_parsing(void);
void ping_event(enum event_e event);

#endif

