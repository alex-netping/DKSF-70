/*
v1.10
1.04.2011
  first release
v1.12
16.03.2013
  some rewrite for dksf48
v1.13-48
  added 'relay-agnostic' logging
  removed unused code
  cosmetics, memset() use
v1.14-201
  default relay state, 'ignore wrong time' in flags
*/

#include "platform_setup.h"

#ifdef WTIMER_MODULE

#include "eeprom_map.h"
#include <stdio.h>
#include "plink.h"

const unsigned wtimer_setup_sign = 0x2b9c8877;
const unsigned wtimer_setup_sign_ch = 0x2b9b8879;

#pragma location="DATA_Z"
__no_init struct wtimer_setup_s wtimer_setup[WTIMER_MAX_CHANNEL];
#pragma location="DATA_Z"
__no_init struct wtimer_status_s wtimer_status[WTIMER_MAX_CHANNEL];
#pragma location="DATA_Z"
__no_init struct wtimer_holidays_s wtimer_holidays;

// продукт работы модуля
unsigned char wtimer_schedule_output[WTIMER_MAX_CHANNEL]; // 16.03.2013
//unsigned short wtimer_periodic_output = 0;

unsigned wtimer_http_get_data(unsigned pkt, unsigned more_data)
{
  char buf[768];
  char *dest = buf;
  if (more_data==0)
  {
    dest+=sprintf((char*)dest,"var packfmt={");
    PLINK(dest, wtimer_setup[0], flags);
    PLINK(dest, wtimer_setup[0], same_as_prev_day);
    PLINK(dest, wtimer_setup[0], schedule);
    PLINK(dest, wtimer_setup[0], cycle_time);
    PLINK(dest, wtimer_setup[0], active_time);
    PSIZE(dest, (char*)&wtimer_setup[1] - (char*)&wtimer_setup[0]); // must be the last // alignment!
    dest+=sprintf(dest, "};\nvar hol_day=[");
    int hn = sizeof wtimer_holidays.hol_day;
    for(int i=0; i<hn; ++i) dest += sprintf(dest, "%u,", wtimer_holidays.hol_day[i]);
    --dest; // remove last comma
    dest+=sprintf(dest, "];\nvar hol_month=[");
    for(int i=0; i<hn; ++i) dest += sprintf(dest, "%u,", wtimer_holidays.hol_month[i]);
    --dest;
    dest+=sprintf(dest, "];\nvar hol_replacement=[");
    for(int i=0; i<hn; ++i) dest += sprintf(dest, "%u,", wtimer_holidays.hol_replacement[i]);
    --dest;
    *dest++ = ']'; *dest++=';';
    tcp_put_tx_body(pkt, (unsigned char*)buf, dest - buf);
    return 1;
  }
  if (more_data == 1)
    dest+=sprintf(dest, "\nvar data=[");
  struct wtimer_setup_s *setup;
  if (more_data%2)
  {
    setup = &wtimer_setup[more_data >> 1]; // shift used instead divison
    dest+=sprintf(dest, "{");
    PDATA(dest, (*setup), flags);
    PDATA(dest, (*setup), same_as_prev_day);
    PDATA_ARRAY_2D(dest, (*setup), schedule, 10, 8);
    tcp_put_tx_body(pkt, (unsigned char*)buf, dest - buf);
    return ++more_data;
  }
  else
  {
    setup = &wtimer_setup[(more_data >> 1) - 1]; // shift used instead divison
    PDATA(dest, (*setup), cycle_time);
    PDATA(dest, (*setup), active_time);
    dest += sprintf(dest, "ntp_is_ok:%u,period_point:%u", ntp_time_is_actual(), (unsigned)(local_time>>32) % setup->cycle_time);
    if (setup == &wtimer_setup[WTIMER_MAX_CHANNEL-1] )
    {
      dest+=sprintf(dest, "}];\r\n");
      // extended info
      dest+=sprintf(dest, "var schedule_day_count=%d;\r\nvar schedule_points_count=%d;\r\n", WTIM_AUX_SCHEDULE_N+7, WTIM_ONOFF_POINTS_N);
    }
    else
      dest+=sprintf(dest, "}, ");
    tcp_put_tx_body(pkt, (unsigned char*)buf, dest - buf);
  }
  return setup == &wtimer_setup[WTIMER_MAX_CHANNEL-1] ? 0:++more_data;
};

HOOK_CGI(wtimer_get,    (void*)wtimer_http_get_data,  mime_js,  HTML_FLG_GET | HTML_FLG_NOCACHE );

void wtimer_http_set_data(int ch)
{
  const unsigned short postlen = (sizeof wtimer_setup[0] + sizeof wtimer_holidays) * 2;
  if(http.post_content_length - HTTP_POST_HDR_SIZE == postlen )
  {
    char *src = req + HTTP_POST_HDR_SIZE;
    src += http_post_data_part(src, &wtimer_setup[ch], sizeof wtimer_setup[0]);
    EEPROM_WRITE(&eeprom_wtimer_setup[ch], &wtimer_setup[ch], sizeof wtimer_setup[0]);
    http_post_data_part(src, &wtimer_holidays, sizeof wtimer_holidays);
    EEPROM_WRITE(&eeprom_wtimer_holidays, &wtimer_holidays, sizeof wtimer_holidays);
  }
}

int wtimer_0_http_set_data(void)
{
  wtimer_http_set_data(0);
  http_redirect("/wtimer.html?ch=1");
  return 0;
}

int wtimer_1_http_set_data(void)
{
  wtimer_http_set_data(1);
  http_redirect("/wtimer.html?ch=2");
  return 0;
}

HOOK_CGI(wtm0_set,   (void*)wtimer_0_http_set_data,  mime_js,  HTML_FLG_POST );
HOOK_CGI(wtm1_set,   (void*)wtimer_1_http_set_data,  mime_js,  HTML_FLG_POST );


unsigned wtimer_http_get_status(unsigned pkt, unsigned more_data)
{
  char buf[512];
  char *dest = buf;
  dest += sprintf(dest, "{ltime:%u,tz:%d,dst:%u,time_ok:%u,ntp_stat:%u,active_point:[",
                  (unsigned)(local_time>>32) - (unsigned)TIMEBASE_DIFF,
                  sys_setup.timezone, // 4.06.2012
                  sys_setup.dst, // 19.04.2013
                  ntp_time_is_actual(),
                  ntp_status);
  struct wtimer_status_s *stat = wtimer_status;
  for(int i=0;i<WTIMER_MAX_CHANNEL;++i,++stat)
  {
    dest += sprintf(dest, "{row:%d,col:%d,hol_idx:%d,onoff:%u},",
      stat->row,
      stat->col,
      stat->hol_idx,
      wtimer_schedule_output[i] );
  }
  --dest; // remove last comma
  *dest++ = ']';
  *dest++ = '}';
  tcp_put_tx_body(pkt, (unsigned char*)buf, dest - buf);
  return 0;
}

HOOK_CGI(wtimer_stat,    (void*)wtimer_http_get_status,  mime_js,  HTML_FLG_GET | HTML_FLG_NOCACHE );


int wtimer_day_schedule(unsigned short *day_schedule, unsigned time_value, unsigned ch)
{
  int i;
  for(i=WTIM_ONOFF_POINTS_N-1; i>=0; --i) // scan backward in time
  {
    if(day_schedule[i] != WTIM_UNUSED_TIME && day_schedule[i] <= time_value)
    { // first not empty event before 'now'
      char new_st = (i & 1) ^ 1; // чётные точки - включение, нечётные точки - выключение
      if(new_st != wtimer_schedule_output[ch])
      {
#       if PROJECT_CHAR != 'E'
          log_printf("расписание %u перешло в состояние %s", ch+1, new_st ? "вкл" : "выкл");
#       else
          log_printf("Schedule %u changed state to %s", ch+1, new_st ? "On" : "Off");
#       endif
      }
      wtimer_schedule_output[ch] = new_st;
      return i; // last point with definite satate in schedule found and handled
    }
  }
  return -1;
}


const static char lastday[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

void wtimer_decrement_day(struct tm *tms)  // LBS 24.03.2011
{
  if((tms->tm_wday -= 1) < 0) tms->tm_wday = 6;
  tms->tm_yday -= 1;
  if((tms->tm_mday -= 1) == 0) // tm_mday is [1..31]
  {
    if((tms->tm_mon -= 1) == -1) // tm_mon is [0..11]
    {
      tms->tm_mon = 11;
      tms->tm_year -= 1;
      tms->tm_yday = (tms->tm_year & 3) ? 365 : 366;
    }
    tms->tm_mday = lastday[tms->tm_mon];
    if(tms->tm_mday == 28 && (tms->tm_year & 3) == 0) tms->tm_mday = 29;
  }
}

void wtimer_schedule(struct tm *dtime)
{
  struct tm tms;
  unsigned i, j;
  unsigned wday, time_value;
  struct wtimer_setup_s *setup = wtimer_setup;
  struct wtimer_status_s *status = wtimer_status;
  unsigned ch;
  int idx;

  for(ch=0; ch<WTIMER_MAX_CHANNEL; ++ch, ++setup, ++status)
  {
    if(ntp_time_is_actual() == 0 && (setup->flags & WTIM_IGNORE_WRONG_TIME) == 0) // 18.08.2014
    {
      wtimer_schedule_output[ch] = (setup->flags & WTIM_ACTIVE_STATE_ON) ? 1 : 0 ;
      continue;
    }
    tms = *dtime;
    for (i=0; i<7; i++) // limit search of last point with defined satate in schedule by 7 days in past
    {
      // struct tm has tm_wday days since Sunday
      wday = tms.tm_wday  > 0 ? tms.tm_wday-1 : 6;
      time_value = tms.tm_hour*60 + tms.tm_min;
      // handling holidays replacement
      for (j=0; j<8; j++)
      {
        if (tms.tm_mday  == wtimer_holidays.hol_day[j]
        &&  tms.tm_mon+1 == wtimer_holidays.hol_month[j]
        &&  wtimer_holidays.hol_replacement[j] < 7 + WTIM_AUX_SCHEDULE_N)
        {
          wday = wtimer_holidays.hol_replacement[j];
          status->hol_idx = j;
          break;
        }
      } // for holidays
      if(j==8) status->hol_idx = -1;
      // 'same as previous week's day'
      while (wday > 0 && setup->same_as_prev_day & (1 << wday)) --wday;
      idx = wtimer_day_schedule(setup->schedule[wday], time_value, ch);
      if (idx >= 0)
      {
        status->row = wday;
        status->col = idx;
        break;
      }
      // will look for satate in previos day
      wtimer_decrement_day(&tms);
      // set time to last minute of the day
      tms.tm_hour  = 23;
      tms.tm_min   = 59;
    } // 7 days backward
    // if sheduled state isn't found, switch off
    if(i==7)
    {
      wtimer_schedule_output[ch] = 0;
      status->row = -1; // state not found
      status->col = -1;
    }
  } // wtimer channels
}

void wtimer_exec()
{
  static time_t next_time = 0;
  if(local_time < next_time) return;
  next_time = local_time + (1ULL << 32);
  wtimer_schedule(ntp_calendar_tz(1));
}

void wtimer_param_reset(void)
{
  memset(wtimer_setup, 0xff, sizeof wtimer_setup);
  struct wtimer_setup_s *setup;
  for (setup= &wtimer_setup[0]; setup != &wtimer_setup[WTIMER_MAX_CHANNEL]; setup++)
  {
    setup->flags = 0;
    setup->same_as_prev_day = 0;
    setup->cycle_time = 600; // unused
    setup->active_time = 0;  // unused
    setup->signature = wtimer_setup_sign_ch;
  }
  memset(wtimer_status, -1, sizeof wtimer_status);
  memset(&wtimer_holidays, -1, sizeof wtimer_holidays);
  memset(wtimer_holidays.hol_replacement, 0, sizeof wtimer_holidays.hol_replacement);
  EEPROM_WRITE(&eeprom_wtimer_holidays, &wtimer_holidays, sizeof eeprom_wtimer_holidays);
  EEPROM_WRITE(&eeprom_wtimer_setup, wtimer_setup, sizeof wtimer_setup);
  EEPROM_WRITE(&eeprom_wtimer_signature, &wtimer_setup_sign, sizeof wtimer_setup_sign);
}

void wtimer_init()
{
  unsigned sign;
  EEPROM_READ(&eeprom_wtimer_signature, &sign, sizeof sign);
  if(sign != wtimer_setup_sign) wtimer_param_reset();
  EEPROM_READ(&eeprom_wtimer_setup, &wtimer_setup, sizeof wtimer_setup);
  EEPROM_READ(&eeprom_wtimer_holidays, &wtimer_holidays, sizeof eeprom_wtimer_holidays);
}

void wtimer_event(enum event_e event)
{
  switch(event)
  {
  case E_EXEC:
    wtimer_exec();
    break;
  case E_INIT:
    wtimer_init();
    break;
  case E_RESET_PARAMS:
    wtimer_param_reset();
    break;
  }
}

#endif
