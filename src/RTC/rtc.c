#include "platform_setup.h"
#include "eeprom_map.h"

struct rtc_tm_s {
  unsigned sec; // 0..59
  unsigned min; // 0..59
  unsigned hrs; // 0..23
  unsigned dom; // 1..31
  unsigned dow; // 0..6   // saved as in struct tm, 0..6 since Sunday
  unsigned doy; // 1..366
  unsigned mon; // 1..12
  unsigned year; // 0..4095 // saved as in struct tm, i.e. years since 1900, 113=2013
};

# if CPU_FAMILY != LPC17xx
#   error "not implemented!"
# endif

__no_init struct rtc_tm_s rtc @ 0x40024020;

// rtc_mktime() returns UTC unix time made from RTC registers
// ATTN uses DOY reg !!! must be set correctly!

// saves local_time (UTC) to split time RTC regs
void save_to_rtc(unsigned unix_time)
{
  struct tm d;
  ntp_gmtime_r(unix_time, &d); // get split time w/o timezone, arg is implied in UTC
  RTCCCR = 2; // stop, reset dividers
  rtc.sec = d.tm_sec;
  rtc.min = d.tm_min;
  rtc.hrs = d.tm_hour;
  rtc.dom = d.tm_mday;
  rtc.dow = d.tm_wday;
  rtc.doy = d.tm_yday + 1;
  rtc.mon = d.tm_mon + 1;
  rtc.year = d.tm_year;
  RTCAUX = 1<<4; // clear rtc osc fault flag
  RTCCCR = 1; // run, remove reset, disable calibration
}

// returns UTC seconds from 00:00 1.01.1970 (unix time)
// it's not NTP time! for NTP time add TIMEBASE_DIFF!

unsigned get_from_rtc(void) // board dependant !!!
{
  unsigned y, year, days, seconds;
  struct rtc_tm_s t;

  for(;;)
  { // copy if no changes during copy
    memcpy(&t, &rtc, sizeof t);
    if(memcmp(&t, &rtc, sizeof t) == 0)
      break;
  }

  days = t.doy - 1;
  year = t.year + 1900;  // saved in rtc as tm_year is years since 1900, i.e. 2013 = 113

  for(y=1970; y<year; ++y)
  {
    days += 365;
    if((y & 3) == 0) ++days; // every 4th is leap yr, every 100th is not leap yr, 2000 IS leap yr
  }

  seconds = days * (24 * 3600);
  seconds += t.hrs * 3600 + t.min * 60 + t.sec;

  return seconds;
}

unsigned rtc_http_set_data(void)
{
  unsigned char d[6];
  unsigned new_utc;
  http_post_data(d, sizeof d);
  sys_setup.timezone = d[0];
  sys_setup.dst = d[1];
  EEPROM_WRITE(&eeprom_sys_setup.timezone, &sys_setup.timezone, sizeof eeprom_sys_setup.timezone);
  EEPROM_WRITE(&eeprom_sys_setup.dst, &sys_setup.dst, sizeof eeprom_sys_setup.dst);
  memcpy(&new_utc, d+2, 4);
  local_time = (new_utc + TIMEBASE_DIFF) << 32; // local_time is in NTP format
  save_to_rtc(new_utc);
  http_redirect("/settings.html");
  return 0;
}

HOOK_CGI(rtcset,    (void*)rtc_http_set_data,  mime_js,  HTML_FLG_POST );
