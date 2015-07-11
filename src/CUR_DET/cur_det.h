/* Модуль CUR_DET реализует функции работы с токовым входом устройства DKST-50
* P.Lyubasov
*v2.0
*01.2010
*v2.1-50
*6.07.2010 by LBS
*  loop power on, 12V by default
*v 2.2-50
*5.03.2011
*  Valid Traps. English log.
*v2.3-60
*14.06.2012
* dkst60 support (dkst50.25)
*v2.4-60
*26.10.2012
*  changed 'cut' threshold 4000 to 4500 Ohm, 'short' hyst to 50
    (it was buggy 200 vs threshold 100)
*v2.3-50
*7.11.2012
  curdet_power_switch(), curdet_start_reset() for ext. control
v2.4-50
2.04.2013
  explicit control of loop power by logic.c
v2.5-70
1.07.2013
  cosmetic rewrite, dksf70 support
v2.6-70
21.01.2014
  update of table channel check in curdet_snmp_get(), curdet_snmp_set()
  to be compatible from .table.N designation in ..._MIB.py
  cosmetic interface changes curdet_real_i,v,r instead of real_i,v,r
v2.7-70
11.06.2014
  support of 70.1.2 board
v2.8-70
30.07.2014
  url-encoded api (json-p)
v2.9-70
3.12.2014
  changed Log messages, Smoke sensor
*/


#include "platform_setup.h"
#ifndef  CUR_DET_H
#define  CUR_DET_H
///Версия модуля
#define  CUR_DET_VER	2
///Сборка модуля
#define  CUR_DET_BUILD	9

///Шаг алгоритма
#define CUR_DET_INIT_WAIT         1
#define CUR_DET_MEASURE           2
#define CUR_DET_MEASURE_WAIT      3
#define CUR_DET_RESET             4

///Статус петли
enum curdet_states_e {
CURDET_NORM    = 0,
CURDET_ALARM   = 1,
CURDET_CUT     = 2,
CURDET_SHORT   = 3,
CURDET_NOT_POWERED= 4
};

//---------------- Раздел, где будут определяться структуры модуля -------------------------

struct curdet_setup_s {
unsigned char  al_mode;
unsigned char  cut_mode;
unsigned char  short_mode;
unsigned char  al_cmp;

unsigned short al_threshld;
unsigned short cut_threshld;
unsigned short short_threshld;
unsigned short al_hyst;
unsigned short cut_hyst;
unsigned short short_hyst;

unsigned char  power;
unsigned char  voltage;
unsigned short rst_period;

unsigned char  trap;
unsigned char  rst_flag;
unsigned char  dummy[2];
};

//---------------- Раздел, где будут определяться глобальные переменные модуля -------------


extern unsigned curdet_real_v;
extern unsigned curdet_real_i;
extern unsigned curdet_real_r;
extern unsigned char curdet_status;
extern volatile unsigned char curdet_power_logic_input;
extern const char * const curdet_status_text[5];

//---------------- Раздел, где будут определяться функции модуля ---------------------------

void curdet_init(void);
void curdet_event(enum event_e event);
int curdet_snmp_get(unsigned id, unsigned char *data);
int curdet_snmp_set(unsigned id, unsigned char *data);

#endif
