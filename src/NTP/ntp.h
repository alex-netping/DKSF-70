/*
 * Модуль NTP.c
v 2.3
22.02.2010
v 2.4
16.03.2010
v 2.5
31.03.2010
 english clock adjust msg
v 2.6-51,52
20.09.2010
  removed legacy PARAMETERS references
v 2.7-51,52
20.09.2010
  ntp_time_is_actual()
  starting time bugfix
v 2.8-200
18.02.2011
  short-time reboot persistence
v2.9-52
31.03.2011
  russian DST is removed
  ntp_status added, ntp_exec() rewrite, 1 min timeout if no responce from servers
v2.10-48
8.04.2013
  ntp_calendar() argument meaning is changed
  DST flag addded
  RTC adopted
v2.11-48
18.06.2013
  bugfix of ntp to rtc saving
  ntp_calendar() renamed to ntp_calendar_tz(), to avoid confusion with old variant
*/

#include "platform_setup.h"
#ifndef  NTP_H
#define  NTP_H

// Версия модуля
#define  NTP_VER	2
// Сборка модуля
#define  NTP_BUILD	11

// timebase difference NTP to Unix, in sec
#define TIMEBASE_DIFF           ((70LL*365LL + 17LL) * 24LL * 3600LL)
// divider to get millisecounds from lower 32 bits of NTP timestamp
#define MS_DIVIDER ((unsigned)((1LL<<32)/1000))

struct tm {
  int tm_sec;     /* seconds after the minute - [0,59] */
  int tm_min;     /* minutes after the hour - [0,59] */
  int tm_hour;    /* hours since midnight - [0,23] */
  int tm_mday;    /* day of the month - [1,31] */
  int tm_mon;     /* months since January - [0,11] */
  int tm_year;    /* years since 1900 */
  int tm_wday;    /* days since Sunday - [0,6] */
  int tm_yday;    /* days since January 1 - [0,365] */
  int tm_isdst;   /* daylight savings time flag */
};

typedef unsigned long time_t;

struct tm *ntp_gmtime_r(const time_t time, struct tm *tmbuf);
struct tm *ntp_calendar_tz(int use_timezone_and_dst);
int ntp_time_is_actual(void);

enum ntp_status_e {
  NTP_STAUS_NO_SETUP = 0,
  NTP_STATUS_TIME_NOT_SET,
  NTP_STATUS_SET_FROM_SERVER_1,
  NTP_STATUS_SET_FROM_SERVER_2,
  NTP_STATUS_NO_REPLY
};


extern unsigned long long local_time; // in NTP format
extern enum ntp_status_e ntp_status;


void ntp_init(void);
void ntp_parsing(void);
void ntp_exec(void);
void ntp_timer(void);
void parse_ntp(void);

unsigned ntp_event(enum event_e event);

#endif

