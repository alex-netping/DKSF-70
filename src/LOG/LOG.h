/*
* LOG
* by P.Lyubasov
*v1.5
*22.02.2010
*v1.6
*31.05.2010
* minor #if #else adjustment for old/new platform
* english startup msg
*v1.6.1-200
*21.12.2010
* cosmetic edit
*v1.7-50
*12.08.2010 by LBS
* void log_clear(void)  // merged from  DKSF_53.1.7 LBS 8.07.2010
*v1.7-52
*30.08.2011
* char via_web[], via_snmp[] moved here from project.c
* cosmetic edit
*v1.8-50
*28.09.2010 by LBS
*  removed prj_const[]
*1.8-52
*5.05.2012
*  via_url[] added
*v1.9-50
*20.08.2012
* added SN and notification email into SysLog message
*v1.10-60
*11.10.2012
* removed legacy, notif.email ported
*v1.10-213
*15.10.2012
* dksf213 support (sans html)
* print_date()
* English days of week
*v1.11-48
*15.04.2013
* argument of ntp_calendar() has changed
*v1.11-50
*17.06.2013
* bugfix in log_printf()
*/

#include "platform_setup.h"
#ifndef  LOG_H
#define  LOG_H

///Версия модуля
#define  LOG_VER	1
///Сборка модуля
#define  LOG_BUILD	11


extern const char via_web[];
extern const char via_url[];
extern const char via_snmp[];

extern unsigned char log_enable_syslog;

// internal API opened for notify.c
extern unsigned char log_marker[4];
extern int log_pointer;
unsigned print_date(char *buf, struct tm *date);
void send_syslog(struct tm *date, char *message);
void log_wrapped_write(unsigned char *txt, int len);
unsigned log_wrap_addr(unsigned addr);
//

void log_clear(void);
void log_init_enable_syslog(void);
#pragma __printf_args
int  log_printf(char *fmt, ...);

void log_init(void);
unsigned log_event(enum event_e event);
#endif

