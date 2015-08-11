#ifndef PLATFORM_SETUP
#define PLATFORM_SETUP

// Типы используемых процессоров

#define LPC21xx             2100
#define LPC23xx             2300
#define LPC17xx             1700

#define _CPU_NXP2131        2131
#define _CPU_NXP2134        2134
#define _CPU_NXP2136        2136
#define _CPU_NXP2366        2366
#define _CPU_NXP1778        1778

// Выбор типа процесора

#define CPU_FAMILY       LPC17xx
#define CPU_TYPE        _CPU_NXP1778
// Версия проекта

#define PROJECT_MODEL  70
//#define PROJECT_MODEL  71 // w/o modem and serial ports
#define PROJECT_VER    5
//#define PROJECT_BUILD  1.2 // initial release, dkst70.1.1 hardware, numerous hw bugs, curloop and 232 are disabled
//#define PROJECT_BUILD 1.3.C-1 // board comissioning tests
//#define PROJECT_BUILD  1.4 // added web SN setup (only webpage); modified sw_i2c; modified delay.c
//#define PROJECT_BUILD  2.1 // CurDet snmp bugfix, 1W humidity support, relay_uniping.c parameters ch bugfix, sms' pinger bugfix
//#define PROJECT_BUILD  2.2 // ip3.c, nic.c, mac236x.c updates (tx packet loss etc); relay mode set via snmp bugfix; removed some FP calc from ow.c; sms_cmd ported from dksf48
//#define PROJECT_BUILD  2.3   // bugfix, bad ICF region
//#define PROJECT_BUILD  2.4  // trap bugfix on settings.html, D202 LED bugfix; ow rh setup cgi
//#define PROJECT_BUILD  2.5 // ow rh setup bugfix, rh bugfix (frac. part of T value was wrong)
//#define PROJECT_BUILD  2.6 // notif.email restored on setup page; gsm reboot, log chkbox, last error; ifDescr.1
//#define PROJECT_BUILD  2.7 // warning dialog before reboot on index.html
//#define PROJECT_BUILD  3.1
//#define PROJECT_BUILD  3.2 // RH -> TSTAT, flip in logic.c; runtime reroute of ADC pins in cur_det.c
//#define PROJECT_BUILD  3.3 // IE10 fw update, MTU shrink, New json-p web API, English ver, Uptime, CS NORM in logic etc
//#define PROJECT_BUILD  3.4 // ir json-p; Rus html backport; Eng Summary in notify.c; CS NORM js bugfix
//#define PROJECT_BUILD  3.5 // relay.cgi bugfix; TCP bugfix and window tuning; 1W timing updated; new OW page; notify/traps/sms conflicts removed; sendmail enchancements;
//                          io.c bugfixes; relhum JSON-P cosmetics; DNS renew; SIM900 memry purge; sendmail/tcpc enchanecements; EMAC alignment bugfix
//#define PROJECT_BUILD  4.1 // see _HS.txt
//#define PROJECT_BUILD  4.2 // notifications from io output; dns slots added for w-dog, sendmail is ok now; sendmail cc: saving fixed; snmp test buttons fixed; minor html fix; downgrade warning fixed
//#define PROJECT_BUILD  4.3 // released limited? no src published; pinger resources bugfix
//#define PROJECT_BUILD  4.4 // sendmail ignore errors on session close; tcp issue with dups longer than original; session timeout; clear ip of empty fqdn posted; resr TCP Client errors
//#define PROJECT_BUILD  4.5 // curdet and wtimer setup interference removed; at.html fix; updated assembly process
//#define PROJECT_BUILD  4.6 // snmp community length bugfix; level legend to trap and sms; credentials lengh limited; RTC set on Enter; JSON string escaping; io.cgi?io4&mode=0/1/2; " : in text fields, http credentials
//#define PROJECT_BUILD  4.7 // minor bugfix on QA; sendmail update (Date and #)
#define PROJECT_BUILD  2 // not released yet; 220v sensors; 1w smoke; multi 1w RH
#define PROJECT_CHAR   'R'
#define PROJECT_ASSM   1

#include <string.h>
#define util_fill(buf,size,val) memset((void*)(buf), val, (unsigned)(size))
#define util_cpy(from,to,size)  memcpy((void*)(to),(void*)(from),(unsigned)(size))
/*
unsigned int util_cmp(unsigned char *pbf1, unsigned char *pbf2, unsigned int num);
void util_fill(unsigned char *pbf, unsigned num, unsigned val);
void util_cpy(unsigned char *pbf1, unsigned char *pbf2, unsigned num);
*/

#define WDT_ON	  wdt_on()
#define WDT_RESET wdt_reset()

// header for IAR intrisics
#include <intrinsics.h>
//#include <inarm.h>


#if CPU_TYPE == _CPU_NXP2136
#include "iolpc2136.h"
#elif CPU_TYPE == _CPU_NXP2131
#include "iolpc2131.h"
#elif CPU_TYPE == _CPU_NXP2134
#include "iolpc2134.h"
#elif CPU_TYPE == _CPU_NXP2366
#include "iolpc2366.h"
#elif CPU_TYPE == _CPU_NXP1778
#include <NXP\iolpc1778.h>
#else
#error "CPU not defined!!!"
#endif

#define uword 		unsigned int
#define upointer	unsigned int

// CCLK in Hz, set in pll_ini() in main_arm.c
#if  CPU_FAMILY == LPC23xx   // bugfix LBS 31.05.2010
#define SYSTEM_CCLK     44544000
#elif CPU_FAMILY == LPC21xx
#define SYSTEM_CCLK     44236800
#elif CPU_FAMILY == LPC17xx
#define SYSTEM_CCLK     48000000
#define SYSTEM_PCLK     24000000
#endif

#define WORD_ALIGN(a) ((a)&3?((a)+4)&0xFFFFFFF8:(a)) // LBS 26.03.2010


//---------------------- BOOT_MODULE ---------------------------------------

// Адрес начала размещения кода ПО

#define APPL_START          0x1000

// Расположение буфера обновления ПО

#define FIRMWARE_START      0x40000 // new firmware storage area
#define FIRMWARE_SIZE       0x3F000 // = 258048 decimal

#define FW_UPDATE_BLOCK     512

#define FW_SECTOR_START             1   // 0x1000 .. working firmware area
#define FW_SECTOR_END              21   // .. 0x3 ffff
#define FW_UPDATE_SECTOR_START     22   // 0x4 0000 ..  update (new) fw area
#define FW_UPDATE_SECTOR_END       29   // .. 0x7 ffff

//---------------------- SYSTEM CLOCK ------------------------------

// Тип для системного времени, тик = 1мс
typedef unsigned long long systime_t;

// Возвращает значение сиcтемных часов
systime_t sys_clock(void);

// Счётчик времени - тик 100мс
extern volatile unsigned sys_clock_100ms;

// Счётчик времени 1ms, напрямую не читать!
extern volatile systime_t sys_clock_counter;


//----------------------------- Раздел подключения модулей -----------------

#include "system_event.h"
#include "def\led_def.h"
#include "def\project_def.h"
#include "def\sys_def.h"
#include "def\delay_def.h"
#include "def\flash_lpc_def.h"
#include "def\spi_eeprom_def.h"
#include "def\crc_def.h"
#include "def\mac236x_def.h"
#include "def\nic_def.h"
#include "def\phy_def.h"
#include "def\ip_def.h"
#include "def\icmp_def.h"
#include "def\ping_def.h"
#include "def\udp_def.h"
#include "def\snmp_def.h"
#include "def\tcp_def.h"
#include "def\dns_def.h"
#include "def\http2_def.h"
#include "def\ntp_def.h"
#include "def\rtc_def.h"
#include "def\log_def.h"

#include "str\str2.h"

#include "def\relay_uniping_def.h"
#include "def\watchdog_def.h"
#include "def\wtimer_def.h"
#include "def\io_def.h"
#include "def\hw_i2c_def.h"
#include "def\sw_i2c_def.h"
#include "def\ow_def.h"
#include "def\relhum_def.h"
#include "def\termo_def.h"
#include "def\ir2_def.h"
#include "def\logic_def.h"
#include "def\snmp_setter_def.h"
#if PROJECT_MODEL == 70
#include "def\uart_def.h"
#include "def\tcpcom_def.h"
#include "def\sms_def.h"
#endif
#include "def\cur_det_def.h"
#include "def\smoke_def.h"
#include "def\pwrmon_def.h"
#include "def\notify_def.h" // after io,termo,curloop,rh
#include "def\sendmail_def.h"


#endif
