/* by P.Lyubasov
*v 1.3
* 8.02.2010
* i2c bus sharing added
*v1.4-50
*5.07.2010 by LBS
* i2c bus sharing restored, cpu unload corrected in termo_exec()
* removed old stuff (PARAMETERS related)
* table index via snmp_data.index
*v1.5-50
*8.07.2010
* removed periodic logging in TERMO to prevent EEPROM wear
*v1.5-162
* 22.09.2010
* termo traps modified
*v1.6-52-6
* sms notification added
*v1.7-50
* 14.02.2011
* send START/STOP on init
*v1.7-200
*11.12.2011
* second trap ip restored
*v1.8-201
*25.10.2011
*  Adjusted English status
*v1.9-213
*5.09.2012
*  threshold >= <= corrections (long-standing errata eliminated!)
*  dksf213 support (sans http)
*v1.10-48
*4.04.2013
*  stdlib used
*  cosmetic rewrite of log, using quoted_name()
*v1.11-70
*17.07.2013
  rewrite for 1w compatibility
v1.13-70
30.07.2014
  json-p url-encoded api
v1.14-60
28.10.2014
  bugfix - no traps if Notify module is used  
*/

#include "platform_setup.h"
#ifndef  TERMO_H
#define  TERMO_H

#define  TERMO_VER	     1
#define  TERMO_BUILD	  14


struct termo_setup_s { //  26 bytes, aligned to 2 byte boundary, total size 208 bytes (check it!)
  unsigned char name[18];    // памятка (pascal string)
  signed char   bottom;      // ниж граница нормы
  signed char   top;         // верх граница нормы
#ifdef OW_MODULE
  unsigned char ow_addr[8];  // 1wire sensor addr
#endif
  unsigned short trap_delay; // мин период посылки trap
  unsigned char trap_low;    // флаги посылки trap'ов
  unsigned char trap_norm;
  unsigned char trap_high;
/*unsigned char trap_periodic;  */
};

extern struct termo_setup_s termo_setup[TERMO_N_CH];

struct termo_state_s {
  signed char   value;
  unsigned char status;
  unsigned char prev_status;
  systime_t        next_time;
};

extern struct termo_state_s termo_state[TERMO_N_CH];

unsigned termo_http_get_data(unsigned pkt, unsigned more_data);
int      termo_snmp_get(unsigned id, unsigned char *data);

void check_termo_status(int ch);

void termo_init(void);
void termo_event(enum event_e event);

#endif

