/*
v1.1-70
14.08.2014
  Rus/Eng Summary
v1.2-70
5.12.2014
  relay_notify, sys_setup.nf_disable
v1.3-48
6.05.2015
  1w smoke support, some reformatting of summary
v1.4-48
18.06.2015
  summary on IO with mode and legends
v1.5-70
7.07.2015
  1w multi humidity support; pwrmon support
*/

#include "eeprom_map.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

const unsigned notify_signature              = 35143520;
const unsigned notify_relay_signature        = 35143530;
const unsigned notify_smoke_signature        = 35143573;
const unsigned notify_relhum_signature       = 35143559;

struct range_notify_s    thermo_notify[TERMO_N_CH];
struct binary_notify_s   io_notify[IO_MAX_CHANNEL];
struct relay_notify_s    relay_notify[RELAY_MAX_CHANNEL];
#ifdef RELHUM_MAX_CH
struct relhum_notify_s   relhum_notify[RELHUM_MAX_CH];
#else
struct range_notify_s    relhum_notify;
#endif
struct range_notify_s    curdet_notify;
struct range_notify_s    smoke_notify[SMOKE_MAX_CH];
struct range_notify_s    pwrmon_notify[PWRMON_MAX_CH];

unsigned notify_http_get_data(unsigned pkt, unsigned more_data)
{
  char buf[384];
  buf[0] = 0;
  struct range_notify_s  *rn = 0;
  struct binary_notify_s *bn = 0;
  struct relay_notify_s  *pn = 0;
  struct relhum_notify_s *rhn = 0;
  unsigned id, ch;

  if(memcmp(req_args, "nfid=", 5) != 0) goto err;
  id = hex_to_byte(&req_args[5]); // 6.10.2014 hex_to_byte() from http2.c
  ch = hex_to_byte(&req_args[7]);
  switch(id)
  {
  case 1:
    if(ch >= TERMO_N_CH) goto err;
    rn = &thermo_notify[ch];
    break;
  case 2:
    if(ch >= IO_MAX_CHANNEL) goto err;
    bn = &io_notify[ch];
    break;
  case 3:
#ifdef RELHUM_MAX_CH
    if(ch >= RELHUM_MAX_CH) goto err;
    rhn = &relhum_notify[ch];
#else
    rn = &relhum_notify;
#endif
    break;
#ifdef CUR_DET_MODULE
  case 4:
    rn = &curdet_notify;
    break;
#endif // CUR_DET_MODULE
  case 5:
    if(ch >= RELAY_MAX_CHANNEL) goto err;
    pn = &relay_notify[ch];
    break;
#ifdef SMOKE_MODULE
  case 6:
    if(ch >= SMOKE_MAX_CH) goto err;
    rn = &smoke_notify[ch];
    break;
#endif
#ifdef PWRMON_MODULE
  case 7:
    if(ch >= PWRMON_MAX_CH) goto err;
    rn = &pwrmon_notify[ch];
    break;
#endif
  default:
    goto err;
  }
  if(rn)
    sprintf(buf, "({high:%u,norm:%u,low:%u,fail:%u,report:%u,flags:%u})",
      rn->high, rn->norm, rn->low, rn->fail, rn->report, rn->flags);
  else if(rhn)
    sprintf(buf, "({h_high:%u,h_norm:%u,h_low:%u,t_high:%u,t_norm:%u,t_low:%u,fail:%u,report:%u,flags:%u})",
      rhn->h_high, rhn->h_norm, rhn->h_low, rhn->t_high, rhn->t_norm, rhn->t_low, rhn->fail, rhn->report, rhn->flags);
  else if(bn)
    sprintf(buf, "({high:%u,low:%u,report:%u,legend_high:\"%s\",legend_low:\"%s\"})",
       bn->high, bn->low, bn->report,
       bn->legend_high+1, bn->legend_low+1 ); // pasc+zterm strings
  else if(pn)
    sprintf(buf, "({on_off:%u,report:%u})",
       pn->on_off, pn->report);
  tcp_put_tx_body(pkt, (void*)buf, strlen(buf));
  return 0;
err:
  tcp_put_tx_body(pkt, "error", 5);
  return 0;
}

unsigned notify_http_set_data(void)
{
  char hdr[2];
  if(http.post_content_length - HTTP_POST_HDR_SIZE < (2<<1) ) return 0; // at least 3 hex byte must be posted
  http_post_data_part(req + HTTP_POST_HDR_SIZE, hdr, sizeof hdr);
  char *src = req + HTTP_POST_HDR_SIZE + 4;
  unsigned ch = hdr[1];
  switch(hdr[0])
  {
  case 1:
    if(ch >= TERMO_N_CH) break;
    http_post_data_part(src, (void*)&thermo_notify[ch], sizeof thermo_notify[0]);
    EEPROM_WRITE(&eeprom_thermo_notify[ch], &thermo_notify[ch], sizeof eeprom_thermo_notify[0]);
    break;
  case 2:
    if(ch >= IO_MAX_CHANNEL) break;
    http_post_data_part(src, (void*)&io_notify[ch], sizeof io_notify[0]);
    EEPROM_WRITE(&eeprom_io_notify[ch], &io_notify[ch], sizeof eeprom_io_notify[0]);
    break;
  case 3:
#ifdef RELHUM_MAX_CH
    if(ch >= RELHUM_MAX_CH) break;
    http_post_data_part(src, (void*)&relhum_notify[ch], sizeof relhum_notify[0]);
    EEPROM_WRITE(&eeprom_relhum_notify[ch], &relhum_notify[ch], sizeof eeprom_relhum_notify[0]);
#else
    http_post_data_part(src, (void*)&relhum_notify, sizeof relhum_notify);
    EEPROM_WRITE(&eeprom_relhum_notify, &relhum_notify, sizeof eeprom_relhum_notify);
#endif
    break;
#ifdef CUR_DET_MODULE
  case 4:
    http_post_data_part(src, (void*)&curdet_notify, sizeof curdet_notify);
    EEPROM_WRITE(&eeprom_curdet_notify, &curdet_notify, sizeof eeprom_curdet_notify);
    break;
#endif // CUR_DET_MODULE
  case 5:
    if(ch >= RELAY_MAX_CHANNEL) break;
    http_post_data_part(src, (void*)&relay_notify[ch], sizeof relay_notify[0]);
    EEPROM_WRITE(&eeprom_relay_notify[ch], &relay_notify[ch], sizeof eeprom_relay_notify[0]);
    break;
#ifdef SMOKE_MODULE
  case 6:
    if(ch >= SMOKE_MAX_CH) break;
    http_post_data_part(src, (void*)&smoke_notify[ch], sizeof smoke_notify[0]);
    EEPROM_WRITE(&eeprom_smoke_notify[ch], &smoke_notify[ch], sizeof eeprom_smoke_notify[0]);
    break;
#endif
#ifdef PWRMON_MODULE
  case 7:
    if(ch >= PWRMON_MAX_CH) break;
    http_post_data_part(src, (void*)&pwrmon_notify[ch], sizeof pwrmon_notify[0]);
    EEPROM_WRITE(&eeprom_pwrmon_notify[ch], &pwrmon_notify[ch], sizeof eeprom_pwrmon_notify[0]);
    break;
#endif
  }
  http_reply(200,"");
  return 0;
}

HOOK_CGI(notify_get,  (void*)notify_http_get_data,     mime_js,   HTML_FLG_GET | HTML_FLG_NOCACHE );
HOOK_CGI(notify_set,  (void*)notify_http_set_data,     mime_js,   HTML_FLG_POST );

#pragma __printf_args
void notify(unsigned mask, char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);

  if(mask == 0) return;

  char buf[400];
  char *p = buf;

  struct tm *date = ntp_calendar_tz(1); // 15.04.2013, different args, show local time
  if(mask & NOTIFY_LOG)
  {
    p += print_date(buf, date);
    *p++ = ' ';
  }
  char *msg = p;
  p += vsprintf(msg, fmt, args);
  if(mask & NOTIFY_SYSLOG)
    send_syslog(date, msg);
  if(sys_setup.nf_disable == 0)
    if(mask & NOTIFY_EMAIL)
      sendmail(msg, msg, date);
  if(mask & NOTIFY_LOG)
  {
    *p++ = '\r'; // mark end of message for weblog ring memory
    *p++ = '\n';
    memcpy(p, log_marker, sizeof log_marker);
    unsigned len = p - buf; // len w/o marker
    log_wrapped_write((unsigned char*)buf, len + sizeof log_marker);
    log_pointer = log_wrap_addr(log_pointer + len);
  }
  va_end(args);
}

void make_short_report(unsigned media_mask, char *buf)
{
  int i;
  char *dest = buf;
  dest += sprintf(dest, "Sensor summary: ");
  char *empty = dest;
  struct termo_state_s *t   = termo_state;
  for(i=0; i<TERMO_N_CH; ++i, ++t)
    if(thermo_notify[i].report & media_mask)
    {
      dest += sprintf(dest, "T%u=", i+1);
      if(t->status == 0) *dest++ = '?';
      else dest += sprintf(dest, "%dC", t->value);
      *dest++ = ' ';
      *dest = 0;
    }
#ifdef RELHUM_MAX_CH
  struct relhum_state_s *rhst = relhum_state;
  for(i=0; i<RELHUM_MAX_CH; ++i, ++rhst)
  {
    if(relhum_notify[i].report & media_mask)
    {
      *dest++ = 'R'; *dest++ = 'H';
      *dest++ = '1' + i;
      *dest++ = '=';
      if(rhst->rh_status == 0) *dest++ = '?';
      else dest += sprintf(dest, "%u%% %dC", rhst->rh, rhst->t);
      *dest++ = ' ';
      *dest = 0;
    }
  }
#elif defined(RELHUM_MODULE)
  if(relhum_notify.report & media_mask)
  {
    *dest++ = 'R'; *dest++ = 'H';
    *dest++ = '=';
    if(rh_status_h == 0) *dest++ = '?';
    else dest += sprintf(dest, "%u%% RHT=%dC", rh_real_h, rh_real_t);
    *dest++ = ' ';
    *dest = 0;
  }
#endif
#ifdef CUR_DET_MODULE
  static const char * const state[5] = {"OK", "ALARM!", "FAILED (OPEN)", "FAILED (SHORT)", "NOT POWERED"};
  if(curdet_notify.report & media_mask)
    dest += sprintf(dest, "CS(SMOKE)=%s ", curdet_status < 5 ? state[curdet_status] : "?" );
#endif
#ifdef SMOKE_MODULE
  dest += smoke_summary_short(dest);
#endif
  for(i=0; i<IO_MAX_CHANNEL; ++i)
    if(io_notify[i].report & media_mask)
      dest += sprintf(dest, "%c%u=%u ", io_setup[i].direction ? 'O' : 'I', i+1,
                  // (io_registered_state >> i) & 1 ); // 22.12.2014 changed to level_filtered, which is now actual state
                  io_state[i].level_filtered);
  if(dest == empty) { *dest++ = '-'; *dest = 0; } // nothing added
}

char *name_in_brackets(unsigned char *pzt_name)
{
  static char buf[36];
  buf[0] = 0;
  if(pzt_name[0])
  {
    strcat(buf, " (");
    strlcpy(buf + 2, (char*)pzt_name+1, 30);
    strcat(buf, ")");
  }
  return buf;
}

void notify_exec(void)
{
#warning TODO move to ntp time from timer time and make saparate startup pause from timer
  static unsigned next_time = 200; // 20c
  if(sys_clock_100ms < next_time) return;
  next_time = sys_clock_100ms + 600; // once per min
  if(!ntp_time_is_actual()) return;

  struct tm *date = ntp_calendar_tz(1);
  char time_txt[32];
  sprintf(time_txt, "%02u:%02u", date->tm_hour, date->tm_min);

#if PROJECT_CHAR  != 'E'
  const char * const txt_status[] = {"ниже нормы", "в норме", "выше нормы"};
#else
  const char * const txt_status[] = {"below safe range", "in safe range", "above safe range"};
#endif

  /// e-mail report
  if(strstr((char*)sendmail_setup.reports + 1, time_txt))
  {
    char buf[1000]; // ATTN! check tcp_cli.tx_data size to fit letter!
    char *dest = buf;
    int i;
    for(i=0; i<TERMO_N_CH; ++i)
      if(thermo_notify[i].report & NOTIFY_EMAIL)
        break;
    if(i < TERMO_N_CH)
    {
#if PROJECT_CHAR  != 'E'
      dest += sprintf(dest, "Термодатчики\r\n\r\n");
#else
      dest += sprintf(dest, "Thermo sensors\r\n\r\n");
#endif
      struct termo_setup_s *setup = termo_setup;
      struct termo_state_s *state = termo_state;
      for(i=0; i<TERMO_N_CH; ++i, ++state, ++setup)
      {
        if((thermo_notify[i].report & NOTIFY_EMAIL) == 0) continue;
        /*if(termo_state[i].status == 0 && termo_setup[i].name[0] == 0)
        continue;*/
        if(termo_state[i].status == 0)
        {
#if PROJECT_CHAR  != 'E'
          dest += sprintf(dest, "  T%u отказ или не подключен%s\r\n", i+1, name_in_brackets(termo_setup[i].name));
#else
          dest += sprintf(dest, "  T%u failed or absent%s\r\n", i+1, name_in_brackets(termo_setup[i].name));
#endif
        }
        else
        {
          dest += sprintf(dest, "  T%u = %dС %s (%d..%dC) %s\r\n",
                          i+1, state->value, txt_status[state->status-1],
                          setup->bottom, setup->top, name_in_brackets(setup->name) );
        }
      } // for
    } // if found t sensor(s) to report
#ifdef RELHUM_MAX_CH
    for(i=0; i<RELHUM_MAX_CH; ++i)
      if(relhum_setup[i].ow_addr && (relhum_notify[i].report & NOTIFY_EMAIL))
        break;
    if(i < RELHUM_MAX_CH)
    {
#if PROJECT_CHAR  != 'E'
      dest += sprintf(dest, "\r\nДатчики влажности\r\n\r\n");
#else
      dest += sprintf(dest, "\r\nRelative Humidity sensors\r\n\r\n");
#endif
      for(i=0; i<RELHUM_MAX_CH; ++i)
      {
#warning ****** make RHv3 e-mail report  *************
      }
    }
#elif defined(RELHUM_MODULE)
    if(relhum_notify.report & NOTIFY_EMAIL)
    {
#if PROJECT_CHAR  != 'E'
      dest += sprintf(dest, "\r\nДатчик влажности\r\n\r\n");
#else
      dest += sprintf(dest, "\r\nRelative Humidity sensor\r\n\r\n");
#endif
      if(rh_status_h == 0)
      {
#if PROJECT_CHAR  != 'E'
        dest += sprintf(dest, "  отказ или не подключен\r\n");
#else
        dest += sprintf(dest, "  Failed or absent\r\n");
#endif
      }
      else
      {
        dest += sprintf(dest, "  RH = %d%% %s (%d..%d%%)\r\nT = %dC\r\n",
                        rh_real_h, txt_status[rh_status_h-1],
                        relhum_setup.rh_low, relhum_setup.rh_high,
                        rh_real_t);
      }
    } // if RelHum notification
#endif // RELHUM_MODULE

#ifdef CUR_DET_MODULE
    if(curdet_notify.report & NOTIFY_EMAIL)
    {
#if PROJECT_CHAR  != 'E'
      dest += sprintf(dest, "\r\nДатчик дыма (токовая петля)\r\n\r\n"
                       "  Статус датчика: %s\r\n  Ток в петле %d mA; падение напряжения %d mV; сопротивление петли %d Ohm\r\n",
                       curdet_status < 5 ? curdet_status_text[curdet_status] : "-",
                       curdet_real_i, curdet_real_v, curdet_real_r);
#else
      dest += sprintf(dest, "\r\nSmoke sensor (current loop)\r\n\r\n"
                       "  Sensor status: %s\r\n  Current %d mA; voltage drop %d mV; loop resistance %d Ohm\r\n",
                       curdet_status < 5 ? curdet_status_text[curdet_status] : "-",
                       curdet_real_i, curdet_real_v, curdet_real_r);
#endif
    }
#endif // CUR_DET_MODULE

#ifdef SMOKE_MODULE
    for(i=0; i<SMOKE_MAX_CH; i++)
      if(smoke_notify[i].report & NOTIFY_EMAIL)
        break;
    if(i != SMOKE_MAX_CH)
    {
#if PROJECT_CHAR  != 'E'
      dest += sprintf(dest, "\r\nДатчики дыма 1W\r\n\r\n");
#else
      dest += sprintf(dest, "\r\nSmoke sensors 1W\r\n\r\n");
#endif
      for(i=0; i<SMOKE_MAX_CH; i++)
      {
        if(smoke_notify[i].report & NOTIFY_EMAIL)
        {
          dest += sprintf(dest, "SM%u %s%s\r\n",
                      i+1, smoke_get_status_text(i, 3),
                      name_in_brackets(smoke_setup[i].name) );
        }
      }
    }
#endif // SMOKE_MODULE

    for(i=0; i<IO_MAX_CHANNEL; ++i)
      if(io_notify[i].report & NOTIFY_EMAIL)
        break;
    if(i < IO_MAX_CHANNEL)
    {
#if PROJECT_CHAR  != 'E'
      dest += sprintf(dest, "\r\nIO линии\r\n\r\n");
#else
      dest += sprintf(dest, "\r\nDiscrete IO\r\n\r\n");
#endif
      struct io_setup_s *ios = io_setup;
      struct binary_notify_s *ion = io_notify;
      for(i=0; i<IO_MAX_CHANNEL; ++i, ++ios, ++ion)
      {
        if(io_notify[i].report & NOTIFY_EMAIL)
        {
          char *sm = "???";
#if PROJECT_CHAR  != 'E'
          switch(ios->direction)
          {
          case 0: sm = "вход"; break;
          case 1: sm = "выход"; break;
          case 2: sm = "выход логики"; break;
          }
#else
          switch(ios->direction)
          {
          case 0: sm = "input"; break;
          case 1: sm = "output"; break;
          case 2: sm = "output of Logic"; break;
          }
#endif
          int lvl = io_state[i].level_filtered;
          dest += sprintf(dest, "  IO%d=%u (%s) %s %s\r\n",
                          i + 1, lvl, sm,
                          ios->name + 1,
                          lvl ? ion->legend_high + 1 : ion->legend_low + 1 );
        }
        if(dest - buf > sizeof buf - 80)
        {
          dest += sprintf(dest, "... (too long message)\r\n");
          break; // buf protection
        }
      }
    }

#ifdef RELAY_MODULE
    for(i=0; i<RELAY_MAX_CHANNEL; ++i)
      if(relay_notify[i].report & NOTIFY_EMAIL)
        break;
    if(i < RELAY_MAX_CHANNEL)
    {
      struct relay_setup_s *rs = relay_setup;
      for(i=0; i<RELAY_MAX_CHANNEL; ++i, ++rs)
      {
        if((relay_notify[i].report & NOTIFY_EMAIL) == 0) continue;
        char *s = "unknown";
#if PROJECT_CHAR  != 'E'
        switch(rs->mode)
        {
        case RELAY_MODE_MANUAL_OFF:
        case RELAY_MODE_MANUAL_ON:   s = "Ручной"; break;
        case RELAY_MODE_WDOG:        s = "Сторож"; break;
        case RELAY_MODE_SCHED:       s = "Распис"; break;
        case RELAY_MODE_SCHED_WDOG:  s = "Расп+С"; break;
        case RELAY_MODE_LOGIC:       s = "Логика"; break;
        }
        dest += sprintf(dest, "  Реле %u: режим %6s, состояние %4s %s\r\n",
                        i + 1, s, get_relay_state(i, 0, 0) ? "Вкл" : "Выкл", quoted_name(rs->name) );
#else // E
        switch(rs->mode)
        {
        case RELAY_MODE_MANUAL_OFF:
        case RELAY_MODE_MANUAL_ON:   s = "Manual"; break;
        case RELAY_MODE_WDOG:        s = "Watchdog"; break;
        case RELAY_MODE_SCHED:       s = "Schedule"; break;
        case RELAY_MODE_SCHED_WDOG:  s = "Sched+Wd"; break;
        case RELAY_MODE_LOGIC:       s = "Logic"; break;
        }
        dest += sprintf(dest, "  Relay %u: mode %8s, switched %3s %s\r\n",
                        i + 1, s, get_relay_state(i, 0, 0) ? "On" : "Off", quoted_name(rs->name) );
#endif // E
        if(dest - buf > sizeof buf - 80)
        {
          dest += sprintf(dest, "... (too long message)\r\n");
          break; // buf protection
        }
      }
    }
#endif // RELAY_MODULE

#if PROJECT_CHAR  != 'E'
    sendmail("Отчёт о состоянии датчиков", buf, date);
#else
    sendmail("Summary on status of sensors and IO", buf, date);
#endif
  } // if report time
  // SMS report
#ifdef SMS_MODULE
  if(strstr((char*)sms_setup.periodic_time + 1, time_txt))
  {
    char buf[256];
    make_short_report(NOTIFY_SMS, buf);
    sms_msg_printf("%s", buf);
  }
#endif // SMS_MODULE
}

void notify_reset_params(void)
{
  memset(thermo_notify, 0, sizeof thermo_notify);
  EEPROM_WRITE(eeprom_thermo_notify, thermo_notify, sizeof eeprom_thermo_notify);
  memset(io_notify, 0, sizeof io_notify);
  EEPROM_WRITE(eeprom_io_notify, io_notify, sizeof eeprom_io_notify);
  memset(&relhum_notify, 0, sizeof relhum_notify);
  EEPROM_WRITE(&eeprom_relhum_notify, &relhum_notify, sizeof eeprom_relhum_notify);
#ifdef CUR_DET_MODULE
  memset(&curdet_notify, 0, sizeof curdet_notify);
  EEPROM_WRITE(&eeprom_curdet_notify, &curdet_notify, sizeof eeprom_curdet_notify);
#endif // CUR_DET_MODULE
  memset(&pwrmon_notify, 0, sizeof eeprom_pwrmon_notify);
  EEPROM_WRITE(&eeprom_pwrmon_notify, &pwrmon_notify, sizeof eeprom_pwrmon_notify);
  EEPROM_WRITE(&eeprom_notify_signature, &notify_signature, sizeof eeprom_notify_signature);
}

void notify_relay_reset_params(void)
{
  memset(relay_notify, 0, sizeof relay_notify);
  EEPROM_WRITE(eeprom_relay_notify, relay_notify, sizeof eeprom_relay_notify);
  EEPROM_WRITE(&eeprom_notify_relay_signature, &notify_relay_signature, sizeof eeprom_notify_relay_signature);
}

void notify_relhum_reset_params(void)
{
  memset(relhum_notify, 0, sizeof relhum_notify);
  EEPROM_WRITE(&eeprom_relhum_notify, &relhum_notify, sizeof eeprom_relhum_notify);
  EEPROM_WRITE(&eeprom_notify_relhum_signature, &notify_relhum_signature, sizeof eeprom_notify_relhum_signature);
}

void notify_smoke_reset_params(void)
{
  memset(smoke_notify, 0, sizeof smoke_notify);
  EEPROM_WRITE(eeprom_smoke_notify, smoke_notify, sizeof eeprom_smoke_notify);
  EEPROM_WRITE(&eeprom_notify_smoke_signature, &notify_smoke_signature, sizeof eeprom_notify_smoke_signature);
}

void notify_init(void)
{
  unsigned sign;
  EEPROM_READ(&eeprom_notify_signature, &sign, sizeof sign);
  if(sign != notify_signature)
    notify_reset_params();
  EEPROM_READ(&eeprom_notify_relay_signature, &sign, sizeof sign);
  if(sign != notify_relay_signature)
    notify_relay_reset_params();
#ifdef RELHUM_MAX_CH
  EEPROM_READ(&eeprom_notify_relhum_signature, &sign, sizeof sign); // notify init for v3 relhum
  if(sign != notify_relhum_signature)
    notify_relhum_reset_params();
#endif
  EEPROM_READ(eeprom_thermo_notify, thermo_notify, sizeof thermo_notify);
  EEPROM_READ(eeprom_io_notify, io_notify, sizeof io_notify);
  EEPROM_READ(&eeprom_relhum_notify, &relhum_notify, sizeof relhum_notify); // good for v2 and v3 relhum
#ifdef CUR_DET_MODULE
  EEPROM_READ(&eeprom_curdet_notify, &curdet_notify, sizeof curdet_notify);
#endif // CUR_DET_MODULE
  EEPROM_READ(&eeprom_notify_relay_signature, &sign, sizeof sign);
  if(sign != notify_relay_signature)
    notify_relay_reset_params();
  EEPROM_READ(&eeprom_relay_notify,  &relay_notify,  sizeof relay_notify);
#ifdef SMOKE_MODULE
  EEPROM_READ(&eeprom_notify_smoke_signature, &sign, sizeof sign);
  if(sign != notify_smoke_signature)
    notify_smoke_reset_params();
  EEPROM_READ(&eeprom_smoke_notify, &smoke_notify, sizeof smoke_notify);
#endif

  EEPROM_READ(&eeprom_pwrmon_notify, &pwrmon_notify, sizeof pwrmon_notify);
}

void notify_event(enum event_e event)
{
  switch(event)
  {
  case E_EXEC:
    notify_exec();
    break;
  case E_RESET_PARAMS:
    notify_reset_params();
    notify_relay_reset_params();
    notify_smoke_reset_params();
    break;
  }
}

