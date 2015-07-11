/*
*P.V.Lyubasov
*v2.2
*01.2010
*
*v2.3-50-PCA
*2.07.2010
v2.4-50-PCA
*6.07.2010 by LBS
*  added io_state.pulse_counter, added external for io_state in .h
*  removed some legacy
*  table index via snmp_data.index
*20.07.2010
v2.5-50-PCA
*  on_event bits macro
*v2.6-50.77
* changed TRAP enterprise value to lightcom.8900.2 for MIB v2 conformance
*v2.7-53
* additional variables in io Trap
*v2.8-53
*  bugfix in io line driver
*v2.8-51,52
* 23.11.2010
*  external command api io_set_line() added
*  sms notification added
*  removed io pin drivers (moved to project.c)
*v2.9 merged
*1.02.2011
*  bugfix, io_set_level() replaced to io_set_line() in snmp set handler
*  minor bugfix, added .0 at the end of scalar oids in IO trap
*  removed sending trap to second ip for NetPings
*v2.10-253
*7.02.2011
*  4th io line data in trap
*v2.10-200
*24.02.2011
*  io_start_pulse() API except dksf50, webinterface, snmp i-face, updated 'all io lines' in Trap for 53,253 only
*v2.10-50
*5.03.2011
*  English log
*v2.11-50
*11.03.2011
*  Single pulse HTTP interface ported to dksf50
*v2.11-52
*10.09.2011
*  integration with logic.c
*  rewrite of single pulse web interface
*  param reset of extended io lines IO3, IO4 on DKST 51.1.7 hardware
*v2.12-200
*11.10.2011
*  restored second trap (used old trap scheme)
*v2.12-52
*28.05.2012
*  io.cgi url-encoded interface
*v2.12-60
*5.06.2012
*  dksf60 support (dkst 50.25)
*  io_set_pulse.cgi bugfix for dksf50 (double io web page)
*  rewrited http part to make single io.html on 16-line device
*  io_http_set_single_pulse() for XmlHttpRequest-ed call
*  second Trap destination ip
*v2.13-201
*25.10.2011
*  English log message
*v2.14-52
*29.03.2013
*  bugfix url-encoded interface, wrong API use, new code copied from 2.13-50
*v2.15-48
*12.04.2013
* cosmetic edit, quoted_name() used, logic.c reference under #ifdef LOGIC_MODULE
* bugfix io_snmp_get(), memo
*v2.13-60
*20.03.2013
*  using io_reload_pull_register() after io lines setting save
*v2.14-60
*30.04.2013
*  some rewrite for quick restore of IO lines on hot restart
*v2.14-50
*21.06.213
*  Use logic output flag in io_get.cgi output, model-independent web page update with logic output mode for IO line
*v2.15-52
*15.08.2013
*  flip output level command via url (io.cgi&io1=f), snmp (set level=-1)
*  add SSE notification
*  minor bugfix in snmp_io_set (incorrect error messages was fixed)
*v2.16-70
*3.07.2013
*  dkst70 support
*v2.16-48
*17.07.2013
  minor rework, sse and js
v2.17-70
14.05.2014
  notify support
v2.18-70
28.07.2014
  json-p url-encoded control (different implementation from DKSF253)
v2.18-253
28.10.2014
  pulse_counter bugfix if used with notify.c
v2.19-60
9.10.2014
  io line level legends
v2.19-48
5.11.2014
  bugfixed io_exec() logic, order of setting of io_registered_state
v2.20-60
6.11.2014
  bugfix, BAD DATA returned on SNMP SET of npIoPulseCounter, wrong data type check
20.11.2014
v2.21-253
  more bugfixed io_exec() logic, illegal level in io_registered_state after direction switch
v2.22-60
21.11.2014
  bugfix io_snmp_set(), npIoLine
v2.23-70
22.12.2014
  removed io_state_registered interface, actual io state is routed to io_state[ch].level_filtered
v2.23-48
24.12.2014
  alternative rewrite of io_snmp_set() for npIoPulseCounter bugfix
*/

#include "platform_setup.h"
#ifndef  IO_H
#define  IO_H
///Версия модуля
#define  IO_VER	 2
///Сборка модуля
#define  IO_BUILD 23


//---------------- Раздел, где будут определяться константы модуля -------------------------


//---------------- Раздел, где будут определяться структуры модуля -------------------------

struct io_setup_s {
   unsigned char   name[32];
   unsigned short  delay;
   unsigned char   direction;
   unsigned char   level_out;
   unsigned char   on_event;
   unsigned char   pulse_dur;
};

// on_event bits
#define IO_TRANSITION_0_TO_1    1
#define IO_TRANSITION_1_TO_0    2
#define IO_LOG_TRANSITION       4

struct io_state_s {
   systime_t     time_of_change;
   systime_t     pulse_stop_time; // LBS 24.02.2011, 'reset' pulse output line
   unsigned      pulse_counter;  // LBS 6.07.2010   counter of pulses on input line
   unsigned char level_in;
   unsigned char level_filtered;
//   unsigned char level_out;
};

//---------------- Раздел, где будут определяться глобальные переменные модуля -------------

extern struct io_state_s io_state[IO_MAX_CHANNEL];
extern struct io_setup_s io_setup[IO_MAX_CHANNEL]; // 23.08.2011
extern unsigned io_registered_state;

//---------------- Раздел, где будут определяться функции модуля ---------------------------


//void io_http_get_data(int handler_id, unsigned char*dest);
//void io_http_set_data(int handler_id, unsigned char*dest);
#if SNMP_VER >= 2
int io_snmp_get(unsigned id, unsigned char *data);
int io_snmp_set(unsigned id, unsigned char *data);
#else
void io_snmp_get(upointer addr, uword struct_type);
void io_snmp_set(upointer addr, uword struct_type);
#endif

void io_set_line(unsigned ch, unsigned level);
void io_start_pulse(unsigned ch);

void io_init(void);
extern void io_exec(void);
//extern void io_timer(void);
void io_reset_params(void);
void io_event(enum event_e event);

#endif
//}@
