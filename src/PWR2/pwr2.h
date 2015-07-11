/*
* PWR - watchdog and relay control
* by P.V.Lyubasov
*version 2.0
*date 1.06.2009
*version 2.5
* ENG messages
* date - ?
v 2.10
31.05.2010
чистка
варианты поведения при неуспехе перезагрузок
защита переполнения счётчиков ресетов и циклов опроса
PING_MAX_CHANNELS check
*v 2.11 - 52 - 6
* 23.11.2010
* struct pwr_state_s задвинута в .h файл
* optimized reset routines (only api, not reset logic)
* sms notification
* power saving on relays and channel LEDs
v2.11-50
5.03.2011
  some English log phrases
v2.12-52
28.03.2011
  adjustments for wtimer module
v2.13-52
30.08.2011
  adjustments for LOGIC module
  pwr_http_get_manual(), pwr_http_set_manual() rewrite, pwr_http_forced_reboot() is added
v2.14-52
31.05.2012
  url-encoded cgi commands added (5.05.2012)
  npPwrRelayState snmp variable
v2.15-60
20.05.2013
  dns integration
*/

#include "platform_setup.h"
#ifndef  PWR_H
#define  PWR_H
///Версия модуля
#define  PWR_VER	   2
///Сборка модуля
#define  PWR_BUILD	  15


struct pwr_setup_s {
   unsigned char   name[32];
   unsigned char   ip0[4];
   unsigned char   ip1[4];
   unsigned char   ip2[4];
   unsigned short  poll_period;
   unsigned short  retry_period;
   unsigned short  ping_timeout;
   unsigned short  reset_time;
   unsigned short  reboot_pause;
   unsigned char   max_retry;
   unsigned char   doubling_pause_resets;
   unsigned char   reset_mode;                // ==0 power down, ==1 power up, exactly 0 or 1 (bitwise xor used)
   unsigned char   active;                    // bits 0 1 2 = ip A,B,C
                                                // bit 7 = saved WDT state LBS 11.2009
                                                // bit 6 = Front panel button disable LBS 07.04.2010
   unsigned char   logic_mode;
   unsigned char   manual;                    // ==0 off, ==1 on, ==2 WDT

#ifdef DNS_MODULE
   unsigned char   reserved[2]; // NOT aligned to 64 byte boundary, EEPROM map problem, don't fit before THERMO
   unsigned char   hostname0[64];
   unsigned char   hostname1[64];
   unsigned char   hostname2[64];
#endif
};

struct pwr_state_s {
   systime_t next_time;
   systime_t reset_end_time;
   systime_t reboot_end_time;
   unsigned short reset_cnt;                 // statistics
   unsigned short repeating_resets_cnt;      // repeating resets
   unsigned short poll_cnt;                  // poll cycle after reset
   unsigned char pings_in_progress;          // bits -> active pings for this pwr channel
   unsigned char start_pings;                // signal to start ping burst
   unsigned char reset;                      // reset is active flag
   unsigned char resets_disabled;            // if fail detected, don't reset - LBS 31.05.2010
   unsigned char relay_state;                // current relay state - LBS 24.03.2011
};

//---------------- Раздел, где будут определяться константы модуля -------------------------

// v2
///Режим работы реле

#define PWR_MODE_OFF            0
#define PWR_MODE_ON             1
#define PWR_MODE_WDOG           2
#define PWR_MODE_LOGIC          3
#define PWR_MODE_SCHEDULE       4


#define PWR_RESET_MODE_POWER_DOWN 0
#define PWR_RESET_MODE_POWER_UP   1


enum pwr_reset_logic_mode {
    A_and_B_and_C = 0, // сброс питания  в случае, если недоступен любой из адресов (A)(B)(C).
    A_or_B_or_C,       // сброс питания в случае одновременной не доступности всех Ip-адресов
    A_or_B_and_C,      // сброс питания в случае, если одновременно недоступен адрес (А) и недоступен любой из адресов (B) или (C)
    A_not_B_or_C       // сброс питания в случае, если недоступен адрес (А) но доступен любой из адресов (B) или (C)
};


extern struct pwr_setup_s pwr_setup[PWR_MAX_CHANNEL];

//void pwr_reset(unsigned int ch); // legacy

int  pwr_change_manual_mode(int ch, int val, const char *auxlog);
void pwr_watchdog_force_reset(unsigned ch, const char *aux_msg);

int pwr_snmp_get(unsigned id, unsigned char *data);
int pwr_snmp_set(unsigned id, unsigned char *data);

void pwr_reset_params(void);
void pwr_init(void);
int  pwr_event(enum event_e event);

#endif

