/*
v1.5-200
15.08.2011
  "sim busy" error resolved
  two dest phone numbers for sms sending
v1.6-200
13.10.2011
  removed battery stuff (separate module)
  escape quotes in ussd responce
v1.7-200
3.02.2012
  unsigned format %u in termo SMS message now changed to signed %d
v1.8-201
10.04.2012
  decode_ussd() bugfix for ANSI (not UCS) replies
v1.9-201
28.08.2012
  sms_thermo_event() is modified, added return to safe range
  pinger is disabed while powersaving is on
v1.10-213
5.09.2012
 dksf213 support (sans http)
 get_pinger_status()
v1.11-50
22.01.2013
  sms_io_event(), for IO lines 1..4 only
v1.11-200
17.05.2013
  io_cmd() bugfix for LnP command
v1.12-50
7.06.2013
  cmd_ir() added
v1.11-70
  eth link status for dkst70 (two ports)
v1.12-70
29.01.2014
  requesting RH via SMS (command H?)
  requesting T via SMS (command Tn?)
  IR command changed from Tn to Kn
v1.13-70
3.02.2014
  relay mode rewrite, partial port from dksf48
v1.20-48
29.10.2013
  some rewrite of cmd_pwr()
  cmd_pwr_backup() rewrite
v2.0-707
 additions from 707 (reply to calling phone)
v2.1-201
19.03.2014
26.03.2014
  runtime-detected 1 or 2 phy enet link reports
  last_gsm_error output
  periodic sms
v1.14-70
15.05.2014
  sms_relhum_event() added
v2.1-707
22.08.2014
  use of str_escape_for_js_string() in USSD processing
v2.2-48
5.11.2014
  bugfixed double " escaping in ussd
v2.3-70
17.12.2014
  sys_setup.nf_disable adoption
  relay notifications
  sendsms.cgi
  npGsmSendSms snmp api
v2.4-70
10.02.2015
  bugfix in sms_snmp_set(), no returned data
v2.3-707
13.01.2015
 win1251_to_ucs2()
v2.3-70 ????
17.12.2014
  sendsms.cgi
  npGsmSendSms snmp api
v2.4-70
2.06.2015
  nf_disable in the all sms_xxx_event() routines
v2.5-52/201/202
4/06/2015
  hostname added to pinger sms notification
v2.5-70
16.06.2015
  cmd_ir() bugfix
  pinger_exec() fix, SMS reqest possible if SMS notif. switched off; powersaving ignored
  curdet SMS shortened to 70 chars
  changed relay state reply: P1[mode][+/-] to show actual relay state
v2.6-70
14.07.2015
  added line mode for state request in cmd_io()
  /sendsms.cgi?utf8
  npGsmSendSms in utf8
*/

#include "platform_setup.h"

#ifdef SMS_MODULE

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "plink.h"
#include "eeprom_map.h"




unsigned char powersaving;

char  sms_responce[128];       // sms command reply buffer

#define PING_ATTEMPTS   8     // retries
#define PINGER_TIMEOUT  2000  // ms between unsuccessful pings

char  words[MAXWORDS][WORDLEN];

#if PROJECT_CHAR=='E'
const char via_sms[] = "by SMS command";
#else
const char via_sms[] = "командой через SMS";
#endif

#ifndef PWR_MAX_CHANNEL
#define PWR_MAX_CHANNEL RELAY_MAX_CHANNEL
#endif


void cmd_pwr(char *cmd);
void cmd_io(char *cmd);
void cmd_battery(char *cmd);
void cmd_pinger(char *cmd);
void cmd_pwr_backup(char *cmd);
void cmd_ir(char *cmd);
void cmd_relhum(char *cmd);
void cmd_termo(char *cmd);

void place_error(void);
void place_np_reply(void);
void place_np_done(void);
void place_id(void);

// mooved up like forward ref
systime_t next_ping_time = 5000;
static struct ping_state_s *ping = &ping_state[SMS_PING_CH];
static char pinger_status = 1;

static const char * const safe[4] = {"SENSOR FAILED", "BELOW", "IN", "ABOVE"};

#ifdef HTTP_MODULE

unsigned sms_http_get_data(unsigned pkt, unsigned more_data)
{
  char buf[768];
  char *dest = buf;
  if(more_data == 0)
  {
    dest+=sprintf((char*)dest,"var packfmt={");
    //PLINK(dest, sms_setup, hostname);
    //PLINK(dest, sms_setup, dest_phone);
    for(int n=0; n<MAX_DEST_PHONE; ++n)
      dest += sprintf(dest, "dest_phone%d:{offs:%d,len:16},",
                 n, (char*)&sms_setup.dest_phone[n] - (char*)&sms_setup);
    PLINK(dest, sms_setup, ussd_string);
    PLINK(dest, sms_setup, event_mask);
    PLINK(dest, sms_setup, pinger_ip);
    PLINK(dest, sms_setup, pinger_period);
    PLINK(dest, sms_setup, flags);
    PLINK(dest, sms_setup, pinger_hostname);
    PLINK(dest, sms_setup, periodic_time);
    PSIZE(dest, sizeof sms_setup); // must be the last // alignment!
    dest+=sprintf(dest, "};\r\n");
    tcp_put_tx_body(pkt, (unsigned char*)buf, dest - buf);
    return 1;
  }
  else
  {
    dest+=sprintf(dest, "var data=[{");
    //PDATA_PASC_STR(dest, sms_setup, hostname);
    //PDATA_PASC_STR(dest, sms_setup, dest_phone[0]);
    char lbl[] = "dest_phone0";
    for(int n=0; n<MAX_DEST_PHONE; ++n)
    {
      lbl[10] = '0' + n;
      dest += pdata_pstring(dest, lbl, sms_setup.dest_phone[n]);
    }
    PDATA_PASC_STR(dest, sms_setup, ussd_string);
    PDATA(dest, sms_setup, event_mask);
    PDATA_IP(dest, sms_setup, pinger_ip);
    PDATA(dest, sms_setup, pinger_period);
    PDATA(dest, sms_setup, flags);
#ifdef DNS_MODULE
    PDATA_PASC_STR(dest, sms_setup, pinger_hostname);
#endif
    PDATA_PASC_STR(dest, sms_setup, periodic_time);
    dest+=sprintf(dest, "last_gsm_error:'%s' }];", sms_last_error);
    tcp_put_tx_body(pkt, (unsigned char*)buf, dest - buf);
    return 0;
  }
}

int sms_http_set_data(void)
{
  http_post_data((void*)&sms_setup, sizeof sms_setup);
  sms_collate_dest_ph_numbers();
#ifdef DNS_MODULE
  ping->state = PING_RESET;
  next_ping_time = sys_clock() + 5000;
  dns_resolve(eeprom_sms_setup.pinger_hostname, sms_setup.pinger_hostname);
#endif
  EEPROM_WRITE(&eeprom_sms_setup, (unsigned char*)&sms_setup, sizeof eeprom_sms_setup);
  http_redirect("/sms.html");
  return 0;
}

HOOK_CGI(sms_get,    (void*)sms_http_get_data,     mime_js,  HTML_FLG_GET | HTML_FLG_NOCACHE );
HOOK_CGI(sms_set,    (void*)sms_http_set_data,     mime_js,  HTML_FLG_POST );


int sms_send_manual(char *phone_and_msg)
{
  char c, *p, *m;
  // prepare ref to sms body
  m = phone_and_msg;
  while(*m != ']') { if(*m == 0) goto error; ++m; }
  ++m;
  while(*m == ' ') { if(*m == 0) goto error; ++m; }
  if(*m == 0) goto error;
  p = phone_and_msg;
  if(*p++ != '[')
  {
    sms_q_text(m, 0); // no phone(s), send to phones from sms setup
    return 0;
  }
  char phone[16];
  int n;
  for(;;)
  {
    n = 0;
    for(;;)
    {
      c = *p++;
      if(c == ' ' || c == '-') continue; // skip - and white space
      if(c == ',' || c == ']') break; // next phone or end of list
      if(c == '+' || (c >= '0' && c <= '9') )
         phone[n++] = c;
      else // unexpected end of string or wrong char
        goto error;
    }
    if(n == 0) goto error; // empty phone
    phone[n] = 0;
    sms_q_text(m, phone);
    if(c == ']') break;
  }
  return 0;
error:
  log_printf("Bad manual SMS send command, message has dropped");
  return 0xff;
}

// ATTN! this reads long sms text data directly from req, not from req_args[64]!
int sms_http_get_send_cgi(unsigned pkt, unsigned more_data)
{
  char *result = "sms_send_result('error');";
  char buf[256];
  char c, *p, *q, err;
  p = strstr(req, "?data="); // find in http request headers
  if(p == 0) goto error;
  p += 6; // skip ?data=
  strlccpy(buf, p, ' ', sizeof buf); // read to the end of URL in first http string (GET ... HTTP/1.1)
  p = q = buf;
  for(;;)
  {
    c = *p++;
    if(c == '%')
    {
      if(p[0] == 0 || p[1] == 0) { c = 0; break; }
      c = hex_to_byte(p);
      p += 2;
    }
    *q++ = c;
    if(c == 0) break;
  }
  err = sms_send_manual(buf);
  if(err == 0) result = "sms_send_result('ok');";
error:
  tcp_put_tx_body(pkt, (unsigned char*)result, strlen(result));
  return 0;
}

// http://i.voenmeh.ru/kafi5/Kam.loc/inform/UTF-8.htm
void utf8_to_1251(char *src, char *dest, unsigned dest_size)
{
  unsigned c;
  char *p = src, *q = dest;
  char *end = dest + dest_size - 1;
  for(;*p && q < end;)
  {
    if((*p & 0xe0) == 0xc0)
    {
      // utf8 2-byte char to unicode
      c = (((p[0] & 0x1f) << 6) | (p[1] & 0x3f));
      p += 2;
      // unicode to 1251
      if(c == 1025) c = 168; // Ё
      else if(c == 1105) c = 184; // ё
      else c = c - 1040 + 192;
      *q++ = c;
    }
    else
      *q++ = *p++;
  }
  *q = 0;
}

int sms_http_set_send_cgi(void)
{
  char buf[256];
  if(strcmp(req_args, "utf8") == 0)
    utf8_to_1251(req, buf, sizeof buf);
  else
    strlccpy(buf, req, 0, sizeof buf);
  int err = sms_send_manual(buf); // put POSTed data as is
  http_reply(200, err ? "sendsms_result('error');" : "sendsms_result('ok');");
  return 0;
}

// HOOK_CGI(sms_send,   (void*)sms_http_get_send_cgi, mime_js,  HTML_FLG_GET | HTML_FLG_NOCACHE);
HOOK_CGI(sendsms,    (void*)sms_http_set_send_cgi,     mime_js,  HTML_FLG_POST );


unsigned sms_http_get_stat_i(unsigned pkt, unsigned more_data)
{
  sms_gsm_test_time = 0; // initiate at+creg? request next time when sms state will be IDLE
  return 0;
}

unsigned sms_http_get_stat(unsigned pkt, unsigned more_data)
{
  unsigned char buf[512];
  unsigned len, reg;
  reg = sms_emergency_halt ? 254 : sms_gsm_registration;
  len = sprintf((char*)buf, "({creg:%u,sig_level:%u,creg_refresh_time:%i,sms_reboot_counter:%u,sms_gsm_failed:%u})",
           reg,
           sms_sig_level,
           (int)(sms_gsm_test_time - sys_clock()),
           sms_reboot_counter,
           sms_gsm_failed
           );
  tcp_put_tx_body(pkt, buf, len);
  return 0;
}

HOOK_CGI(sms_stat_i, (void*)sms_http_get_stat_i, mime_js,  HTML_FLG_GET | HTML_FLG_NOCACHE );
HOOK_CGI(sms_stat,   (void*)sms_http_get_stat,   mime_js,  HTML_FLG_GET | HTML_FLG_NOCACHE );


unsigned sms_http_get_ussd_i(unsigned pkt, unsigned more_data)
{
  start_ussd();
  return 0;
}

unsigned sms_http_get_ussd(unsigned pkt, unsigned more_data)
{
  unsigned char buf[768];
  unsigned len;
  sms_ussd_responce[sizeof sms_ussd_responce - 1] = 0;
  len = sprintf((char*)buf, "({ussd_responce:\"%s\"})", sms_ussd_responce);
  tcp_put_tx_body(pkt, buf, len);
  return 0;
}

unsigned sms_http_cgi_get_start_ussd(unsigned pkt, unsigned more_data)
{
  char result[64];
  if(strlen(req_args) > 0)
  {
    char *p = req_args;
    while(*p)
    {
      if(*p == 'a') *p = '*';
      if(*p == 'x') *p = '#';
      ++p;
    }
    if(start_ussd_with_req(req_args) != 0xff)
      strcpy(result, "ussdstart_result('ok');");
    else
      strcpy(result, "ussdstart_result('busy');");
  }
  else
    strcpy(result, "ussdstart_result('error');");
  tcp_put_tx_body(pkt, (void*)result, strlen(result));
  return 0;
}

unsigned sms_http_cgi_get_read_ussd(unsigned pkt, unsigned more_data)
{
  char buf[300];
  sprintf(buf, "ussdread_result(\"%s\");", sms_ussd_responce);
  tcp_put_tx_body(pkt, (void*)buf, strlen(buf));
  return 0;
}

HOOK_CGI(sms_ussd_i, (void*)sms_http_get_ussd_i, mime_js,  HTML_FLG_GET | HTML_FLG_NOCACHE );
HOOK_CGI(sms_ussd,   (void*)sms_http_get_ussd,   mime_js,  HTML_FLG_GET | HTML_FLG_NOCACHE );
HOOK_CGI(ussdstart,  (void*)sms_http_cgi_get_start_ussd,   mime_js,  HTML_FLG_GET | HTML_FLG_NOCACHE );
HOOK_CGI(ussdread,   (void*)sms_http_cgi_get_read_ussd,   mime_js,  HTML_FLG_GET | HTML_FLG_NOCACHE );

#endif // ifdef HTTP_MODULE

void upcase(char *s)
{
  char c;
  for(;*s;++s)
  {
    c = *s;
    if(c>='a' && c <= 'z') { c -= 'a'; c+='A'; }
    *s = c;
  }
}

int is_space(char c)
{
  return c==' ' || c=='\r' || c=='\n';
}

void split(char *s)
{
  int w, n;
  util_fill((void*)words, sizeof words, 0);
  for(w=0;w<MAXWORDS;++w)
  {
    for(;;) // skip space
    {
      if(*s==0) return;
      if(is_space(*s)) ++s;
      else break;
    }
    for(n=0;n<WORDLEN-1;)
    {
      if(*s==0) return;
      if(is_space(*s)) break;
      words[w][n++] = *s++;
    }
  }
}

int sms_check_passwd(char *upcased_passwd_from_sms)
{
  char passwd[sizeof sys_setup.community_w];
  str_pasc_to_zeroterm(sys_setup.community_w, (void*)passwd, sizeof passwd);
  upcase(passwd);
  return strcmp(upcased_passwd_from_sms, passwd) == 0;
}

int sms_parse_command(char *cmd_str, char *calling_phone)
{
  upcase(cmd_str);
  split(cmd_str);

  // check NETPING keyword, drop if sms isn't command
  if(strcmp(words[0], "NETPING") != 0) return 0;
  sms_responce[0] = 0; // clear responce 17.05.2013

  if(sms_check_passwd(words[2]))
  { // passwd is ok
    switch(words[1][0]) // parse application, pass to application parser
    {
    case 'L': cmd_io(words[1]); break;
    case 'P': cmd_pwr(words[1]); break;
    case 'T': cmd_termo(words[1]); break;
#if PROJECT_MODEL == 48
    case 'S': cmd_pwr_backup(words[1]); break; // 19.04.2013
#endif
#ifdef BATTERY_MODULE
    case 'A': cmd_battery(words[1]); break;
#endif
    case 'N': cmd_pinger(words[1]); break;
#ifdef IR_MODULE
    case 'K': cmd_ir(words[1]); break;
#endif
#ifdef RELHUM_MODULE
    case 'H': cmd_relhum(words[1]); break;
#endif
    default: place_error(); break;
    }
  }
  else
  { // wrong passwd
    strcpy(sms_responce, "NP WRONG PASSWD");
    if(words[3][0])
    {
      strcat(sms_responce, " IN ");
      strcat(sms_responce, words[3]);
    }
  }
  if(sms_emergency_halt == 0)
    sms_q_text(sms_responce, calling_phone); // enqueue results for sending by sms
  else
    log_printf("GSM is down, reply msg dropped: %s", sms_responce);
  return 1;
}

void place_io_state(unsigned ch)
{
  if(ch >= IO_MAX_CHANNEL) return;
  char *mode = "?";
  switch(io_setup[ch].direction)
  {
  case 0: mode = "IN"; break;
  case 1: mode = "OUT"; break;
  case 2: mode = "OUT.L"; break;
  }
  char *s = sms_responce + strlen(sms_responce);
  sprintf(s, "L%u=%u(%s) ", ch + 1, io_state[ch].level_filtered, mode);
}

void cmd_io(char *s)
{
  char c;
  int ch = 0;
  if(s[1] == '?' && s[2] == 0)
  {
    place_np_reply();
    for(int n=0; n<IO_MAX_CHANNEL; ++n)
      place_io_state(n);
  }
  else
  {
    if(s[3] != 0) goto error;

    c = s[1];
    if(c < '1' || c > '0'+IO_MAX_CHANNEL) goto error;
    ch = c - '1';

    c = s[2];
    if(c == '?')
    {
      place_np_reply();
      place_io_state(ch);
    }
    else if(c == '+' || c == '-')
    {
      if(c == '+') io_set_line(ch, 1);
      if(c == '-') io_set_line(ch, 0);
      place_np_done();
      strcat(sms_responce, words[1]);
    }
    else if(c == 'P')
    {
      io_start_pulse(ch);
      place_np_done(); // 17.05.2013
      strcat(sms_responce, words[1]);
    }
    else goto error;
  }
  place_id();
  return;
error:
  place_error();
}

void place_pwr_state(unsigned ch, unsigned add_wdog_info)
{
  char c;
  if(ch >= PWR_MAX_CHANNEL) return;
  char *s = sms_responce + strlen(sms_responce);
  *s++ = 'P';
  *s++ = '1' + ch;
  //switch(pwr_setup[ch].manual&3)
  enum relay_mode_e m = relay_setup[ch].mode;
  switch(m)
  {
  case RELAY_MODE_MANUAL_OFF: c = 'M'; break;
  case RELAY_MODE_MANUAL_ON:  c = 'M'; break;
  case RELAY_MODE_WDOG:       c = 'W'; break;
  case RELAY_MODE_SCHED:      c = 'S'; break;
  case RELAY_MODE_SCHED_WDOG: c = 'X'; break;
  case RELAY_MODE_LOGIC:      c = 'L'; break;
  default: c = '?'; break;
  }
  *s++ = c;
  *s++ = get_relay_state(ch, 1, 1) ? '+' : '-' ;
  *s++ = 0;
#ifdef WDOG_MODULE
  if(add_wdog_info)
  {
#if PROJECT_MODEL == 48
    unsigned wch = relay_setup[ch].watchdog; // DKSF48 only
#else
    unsigned wch = ch;
#endif
    if(m == RELAY_MODE_WDOG || m == RELAY_MODE_SCHED_WDOG)
      if(wch == 0xff)
        sprintf(s, " (- RESETS, - REP.RESETS)");
      else
        sprintf(s, " (%u RESETS, %u REP.RESETS) ",
           wdog_state[wch].reset_count, wdog_state[wch].repeating_resets_count); // wdog.c
  }
#endif // WDOG_MODULE
}

void cmd_pwr(char *s)
{
  int ch = 0;
  if(s[1] == '?' && s[2] == 0)
  {
    place_np_reply();
    for(int n=0; n<PWR_MAX_CHANNEL; ++n)
      place_pwr_state(n, 0);
  }
  else
  {
    if(s[3] != 0) goto error;
    char c = s[1];
    if(c < '1' || c > '0'+PWR_MAX_CHANNEL) goto error;
    ch = c - '1';
    if(s[2] == '?')
    {
      place_np_reply();
      place_pwr_state(ch, 1);
    }
    else
    {
      place_np_done();
      char c = s[2];
      if(c == 'M') c = s[3];
      switch(c)
      {
      case '-': relay_setup[ch].mode = RELAY_MODE_MANUAL_OFF; break;
      case '+': relay_setup[ch].mode = RELAY_MODE_MANUAL_ON; break;
      case 'W': relay_setup[ch].mode = RELAY_MODE_WDOG; break;
#ifdef WTIMER_MODULE
      case 'S': relay_setup[ch].mode = RELAY_MODE_SCHED; break;
#endif
#if defined(WDOG_MODULE) && defined(WTIMER_MODULE)
      case 'X': relay_setup[ch].mode = RELAY_MODE_SCHED_WDOG; break;
#endif
#ifdef LOGIC_MODULE
      case 'L': relay_setup[ch].mode = RELAY_MODE_LOGIC; break;
#endif
      case 'R':
        relay_forced_reset(ch, relay_setup[ch].reset_time * 10, wdog_setup[ch].reset_mode, via_sms); // *10 sec to 100ms ticks 30.05.14
      break;
      default : goto error;
      }
    relay_save_and_log(ch, via_sms);
    strcat(sms_responce, words[1]);
    }
  }
  place_id();
  return;
error:
  place_error();
}

#if PROJECT_MODEL == 48

void cmd_pwr_backup(char *s)
{
  if(s[1] == '?')
  {
    place_np_reply();
    char *p = sms_responce + strlen(sms_responce);
    sprintf(p, "S1(1..4) =%c M%c B%c R%c \r\n"
               "S2(5..8) =%c M%c B%c R%c \r\n"
               "AC1%c AC2%c ",

       relay_in_state[0] + '1',
       relay_in_setup[0].prime_in + '1',
       relay_in_setup[0].use_backup  ? '+' : '-',
       relay_in_setup[0].auto_revert ? '+' : '-',

       relay_in_state[1] + '1',
       relay_in_setup[1].prime_in + '1',
       relay_in_setup[1].use_backup  ? '+' : '-',
       relay_in_setup[1].auto_revert ? '+' : '-',

       relay_in_no_ac[0] ? '-' : '+',
       relay_in_no_ac[1] ? '-' : '+'
    );
    place_id();
    return;
  }
  else if((s[1] == '1' || s[1] == '2'))
  {
    unsigned ch = s[1] - '1';
    if(s[2] == '=')
    {
      if(s[3] == '1' || s[3] == '2')
        relay_in_setup[ch].prime_in = s[3] - '1';
      else
        goto error;
    }
    else if(s[2] == 'B')
    {
      if(s[3] == '+')
        relay_in_setup[ch].use_backup = 1;
      else if(s[3] == '-')
        relay_in_setup[ch].use_backup = 0;
      else
        goto error;
    }
    else if(s[2] == 'R')
    {
      if(s[3] == 0)
        relay_in_request_revert(ch, via_sms);
      else if(s[3] == '+')
        relay_in_setup[ch].auto_revert = 1;
      else if(s[3] == '-')
        relay_in_setup[ch].auto_revert = 0;
      else
        goto error;
    }
  }
  else
    goto error;
  relay_save_and_log(0, via_sms);
  place_np_done();
  strcat(sms_responce, words[1]);
  place_id();
  return;
error:
  place_error();
}

#endif // PROJECT_MODEL == 48

void cmd_termo(char *cmd)
{
  char *s;
  unsigned ch;
  struct termo_state_s *state;
  struct termo_setup_s *setup;

  if(cmd[0] != 'T' || cmd[2] != '?' || cmd[3] != 0) goto error;
  ch = cmd[1] - '1';
  if(ch >= TERMO_N_CH) goto error;
  state = &termo_state[ch];
  setup = &termo_setup[ch];
  place_np_reply();
  s = sms_responce + strlen(sms_responce);
  if(state->status == 0 || state->status > 3)
    sprintf(s, "T%u=? SENSOR FAILED", ch + 1);
  else
    sprintf(s, "T%u=%dC %s SAFE RANGE (%d..%dC)",
      ch + 1, state->value, safe[state->status], setup->bottom, setup->top);
  place_id();
  return;
error:
  place_error();
}

#ifdef BATTERY_MODULE

void cmd_battery(char *cmd)
{
  char *s;
  if(cmd[1] != '?' || cmd[2] != 0) goto error;
  place_np_reply();
  s = sms_responce + strlen(sms_responce);
  s += sprintf(s, "A? POWER SRC: %s, ", battery_pok ? "220V" : "BATTERY");
  if(battery_pok) sprintf(s, "CHARGING: %s ", battery_chg ? "YES" : "NO");
  else sprintf(s, "CHARGE LEVEL: %s ", battery_low ? "LOW" : "OK");
  place_id();
  return;
error:
  place_error();
}

#endif // BATTERY_MODULE

#ifdef IR_MODULE

void cmd_ir(char *cmd)
{
  unsigned n;
  if(cmd[1] < '1' || cmd[1] > '9') goto error;
  n = atoi(cmd + 1);
  if(n < 1 || n > IR_COMMANDS_N) goto error;
  ir_play_record(n - 1);
  place_np_done();
  strcat(sms_responce, cmd);
  place_id();
  return;
error:
  place_error();
}
#endif // IR_MODULE

#ifdef RELHUM_MODULE
void cmd_relhum(char *cmd)
{
  char *s;
  unsigned ch;
  struct relhum_setup_s *su;
  struct relhum_state_s *st;
#ifdef RELHUM_MAX_CH
  if(cmd[0] != 'H' || cmd[2]!='?' || cmd[3] != 0) goto error;
  ch = cmd[1] - '1';
  if(ch >= RELHUM_MAX_CH) goto error;
  su = &relhum_setup[ch];
  st = &relhum_state[ch];
  place_np_reply();
  s = sms_responce + strlen(sms_responce);
  if(st->rh_status == 0 || st->rh_status > 3)
    sprintf(s, "H%u=? SENSOR FAILED", ch + 1);
  else
    sprintf(s, "H%u=%u%% %s SAFE RANGE (%u..%u%%) T=%dC",
       ch + 1, st->rh, safe[st->rh_status],
       su->rh_low, su->rh_high,
       st->t );
#else
  if(cmd[0] != 'H' || cmd[1]!='?' || cmd[2] != 0) goto error;
  place_np_reply();
  s = sms_responce + strlen(sms_responce);
  if(rh_status_h == 0 || rh_status_h > 3)
    sprintf(s, "H=? SENSOR FAILED");
  else
    sprintf(s, "H=%u%% %s SAFE RANGE (%u..%u%%) T=%dC",
       rh_real_h, safe[rh_status_h],
       relhum_setup.rh_low, relhum_setup.rh_high,
       rh_real_t );
#endif
  place_id();
  return;
error:
  place_error();
}
#endif // RELHUM_MODULE

void place_error(void)
{
  sprintf(sms_responce, "NP WRONG CMD: \"%s %s %s %s\"",
    words[0], words[1], words[2], words[3]);
}

void place_np_reply(void)
{
  strcpy(sms_responce, "NP REPLY ");
}

void place_np_done(void)
{
  strcpy(sms_responce, "NP DONE ");
}

void place_id(void)
{
  if(words[3][0] == 0) return;
  if(sms_responce[strlen(sms_responce) - 1] != ' ')
    strcat(sms_responce, " ");
  strcat(sms_responce, words[3]);
}

/*
int match_till_space(char *a, char *b)
{
  char ca, cb;
  for(int n;;++n)
  {
    ca = *a;
    if(ca>='a' && ca <= 'z') { ca -= 'a'; ca+='A'; }
    cb = *b;
    if(cb>='a' && cb <= 'z') { cb -= 'a'; cb+='A'; }
    if(ca != cb)
    {
      if(ca == 0 || ca == ' ' || ca == '\r') return n;
      if(cb == 0 || cb == ' ' || cb == '\r') return n;
      return 0;
    }
  }
}
*/

int is_latin(char c)
{
  return (c>='0' && c <= '9') || (c>='a' && c<='z') || (c>='A' && c<='Z') || c==' ';
}

void gsmize(unsigned char *s)
{
  for(;*s;++s)
  {
    if(*s == '@') *s='*';
    else
    if(*s == '_') *s=' ';
    else
    if(!is_latin(*s)) *s = '?';
  }
}

/*
// label -> pasc string; returns ref to static char[]
static char *quoted_name(unsigned char *label)
{
  static char name[36];
  unsigned char z[32];
  if(label[0])
  {
    str_pasc_to_zeroterm(label, z, sizeof z);
    sprintf(name, "\"%s\" ", z);
  }
  else
    name[0] = 0;
  return name;
}
*/

#ifdef IO_MODULE
void sms_io_event(int ch)
{
  if(sys_setup.nf_disable) return;
  if(ch >= IO_MAX_CHANNEL) return;
#ifndef NOTIFY_MODULE
  if(ch >= 4) return; // implemented for IO 1..4 (ch 0..3) only // LBS 22.01.2013
  unsigned mask = SMS_EVENT_IO_BASE << ch;
  if((sms_setup.event_mask & mask) == 0) return;
#endif
  struct binary_notify_s *nf = &io_notify[ch];
  unsigned level = io_state[ch].level_filtered;
  unsigned char *legend =  level ? nf->legend_high : nf->legend_low; // pzt strings
  sms_msg_printf("IO%u=%u %s %s", ch + 1, level, io_setup[ch].name + 1, legend + 1);
}
#endif // IO_MODULE

#ifdef TERMO_MODULE
void sms_thermo_event(int ch)
{
  if(sys_setup.nf_disable) return;
#ifndef NOTIFY_MODULE
  if((sms_setup.event_mask & SMS_EVENT_THERMO) == 0) return;
#endif
  struct termo_setup_s *setup = &termo_setup[ch];
  struct termo_state_s *termo = &termo_state[ch];
  char *range_txt = termo->status == 2 ? "IN SAFE" : "OUT OF" ; // 28.08.2012
  if(termo->status == 0)
  {
    sms_msg_printf("TEMP.SENSOR %u%s IS FAILED", ch+1, quoted_name(setup->name)); // new quoted_name() gives sp before "name"
  }
  else
  {
    sms_msg_printf("TEMP.SENSOR %u%s %s RANGE (%d TO %d), NOW %dC", // new quoted_name() gives sp before "name"
      ch+1, quoted_name(setup->name),
      range_txt,
      setup->bottom, setup->top, termo->value);
  }
}
#endif // TERMO_MODULE

#ifdef RELHUM_MAX_CH
 // not needed, placed in relhum.c
#elif RELHUM_MODULE
void sms_relhum_event(void)
{
  if(sys_setup.nf_disable) return;
  const char stat[3][6] = {"BELOW", "IN", "ABOVE"};
  if(rh_status_h > 3) return;
  if(rh_status_h == 0)
    sms_msg_printf("REL.HUMIDITY SENSOR FAILED");
  else
    sms_msg_printf("REL.HUMIDITY %u%% %s SAFE RANGE (%u..%u)",
       rh_real_h, stat[rh_status_h - 1],
       relhum_setup.rh_low, relhum_setup.rh_high);
}
#endif // RELHUM_MODULE

#ifdef CUR_DET_MODULE
const char * const curloop_sms_state_text[5] = {"OK", "ALARM!", "FAILED (OPEN LOOP)", "FAILED (SHORT CIRQUIT)", "NOT POWERED"};
void sms_curdet_event(void)
{
  if(sys_setup.nf_disable) return;
  if(curdet_status > 4) return;
  sms_msg_printf("ANALOG SMOKE SENSOR: %s", curloop_sms_state_text[curdet_status]);
}
#endif // CUR_DET_MODULE

#ifdef PWR_MODULE
#warning make it consistent with rest of FW
void sms_pwr_event(int ch, char event_code)
{
  if(sys_setup.nf_disable) return; // 11.12.14
#ifndef NOTIFY_MODULE
  if((sms_setup.event_mask & SMS_EVENT_PWR) == 0) return;
#endif
  char txt[48];
  struct pwr_setup_s *setup = &pwr_setup[ch];
  unsigned manual = setup->manual & 3;
  switch(event_code)
  {
  case 'm' :
    if(manual < 2) // OFF or ON
      sprintf(txt, "SWITCHED %s", manual ? "ON" : "OFF");
    else if(manual == 2) // WDOG // 24.03.2011
      strcpy(txt, "WATCHDOG ACTIVATED");
    else
      ;
    break;
  case 'r' :
    strcpy(txt, "MANUAL RESET");
    break;
  case 'w' :
    strcpy(txt, "WATCHDOG RESET");
    break;
  default:
    return;
  }
  sms_msg_printf("PWR %u%s %s", ch+1, quoted_name(setup->name), txt); // new quoted_name() gives sp before name
}
#endif


void sms_pinger_event(int status)
{
  if(sys_setup.nf_disable) return; // 11.12.14
  if((sms_setup.event_mask & SMS_EVENT_PINGER) == 0) return;
  sms_msg_printf("PINGER STATUS: %s (%s)", status ? "OK" : "FAILED",
     sms_setup.pinger_hostname + 1);
}

#ifdef BATTERY_MODULE
void sms_battery_event(unsigned event)
{
  if(sys_setup.nf_disable) return; // 11.12.2014
  if((sms_setup.event_mask & SMS_EVENT_BATTERY) == 0) return;
  switch(event)
  {
  case BATTERY_NOTIF_EXT_POWER:
    sms_msg_printf("POWERED FROM 220V");
    break;
  case BATTERY_NOTIF_BATT_POWER:
    sms_msg_printf("POWERED FROM BATTERY");
    break;
  case BATTERY_NOTIF_BATT_LOW:
    sms_msg_printf("BATTERY CHARGE <10%%");
    break;
  }
}
#endif

void sms_ethernet_event(unsigned phy_link_status)
{
  if(sys_setup.nf_disable) return; // 11.12.2014
  if((sms_setup.event_mask & SMS_EVENT_ETHERNET) == 0) return;
  if(sys_phy_number == 2)
  {
    sms_msg_printf("ETHERNET LINK STATUS: 1 %s, 2 %s",
      (phy_link_status & 1) ? "UP" : "DOWN",
      (phy_link_status & 2) ? "UP" : "DOWN"  );
  }
  else
  {
    sms_msg_printf("ETHERNET LINK STATUS: %s",
      phy_link_status ? "UP" : "DOWN" );
  }
}

void set_pinger_status(int status)
{
  if(status) status = 1;
  if(pinger_status != status)
      sms_pinger_event(status);
  pinger_status = status;
}

int get_pinger_status(void)
{
  return pinger_status;
}

#if PING_VER < 2
#error "old ping.c version!"
#endif

void pinger_init(void)
{
  // 5.06.2014, lack of signature for pinger_hostname
  if(sms_setup.pinger_hostname[0] > 62)
  {
    memset(sms_setup.pinger_hostname, 0, sizeof sms_setup.pinger_hostname);
    EEPROM_WRITE(eeprom_sms_setup.pinger_hostname, sms_setup.pinger_hostname, sizeof eeprom_sms_setup.pinger_hostname);
  }//
  dns_add(sms_setup.pinger_hostname, sms_setup.pinger_ip);
  ping->state = PING_RESET;
  next_ping_time = sys_clock() + 5000;
}

void pinger_exec(void)
{
  if(sms_setup.pinger_hostname[0] == 0)
  { // nothing to poll
    set_pinger_status(0);
    ping->state = PING_RESET;
    return;
  }
  if(sms_emergency_halt)
  { // cease useless work
    ping->state = PING_RESET;
    return;
  }
  // powersaving ignored; on dksf 54 Enet may be up while on battery power

  systime_t time = sys_clock();

  if(ping->state == PING_RESET && time >= next_ping_time)
  {
    next_ping_time = time + sms_setup.pinger_period * 1000;
    if(valid_ip(sms_setup.pinger_ip))
    {
      util_cpy(sms_setup.pinger_ip, ping->ip, 4);
      ping->timeout = PINGER_TIMEOUT;
      ping->max_retry = PING_ATTEMPTS; // 24.10.2013, bug!
      ping->state = PING_START;
    }
    else
    {
      set_pinger_status(0);
    }
  }
  else
  if(ping->state == PING_COMPLETED)
  {
    ping->state = PING_RESET;
    set_pinger_status(ping->result);
  }
}

void cmd_pinger(char *cmd)
{
  char *s;
  if(cmd[1] != '?' || cmd[2] != 0) goto error; // 'N?'
  place_np_reply();
  s = sms_responce + strlen(sms_responce);
  s += sprintf(s, "N=%u", pinger_status ? 1 : 0 );
  place_id();
  return;
error:
  place_error();
}

static short ether_status = -1;

void etherstatus_exec(void)
{
  if(phy_link_status != ether_status && ether_status != -1)
    sms_ethernet_event( phy_link_status );
  ether_status = phy_link_status;
}

int is_ucs2(char *txt)
{
  if(strlen(txt) & 1) return 0;
  char *s = txt;
  char c;
  for(;;)
  {
    c = *s++;
    if(c == 0) return 1;
    if(c>='0' && c<='9') continue;
    if(c>='A' && c<='F') continue;
    return 0;
  }
}

#ifndef HTTP_MODULE

unsigned char hex_to_byte(char *s)
{
  unsigned char b,c;
  c=s[0];
  if(c>='0' && c<='9') b = c - '0';
  else if(c>='a' && c<='f') b = c - 'a' + 10;
  else if(c>='A' && c<='F') b = c - 'A' + 10;
  else b = 0;
  b<<=4; c=s[1];
  if(c>='0' && c<='9') b |= c - '0';
  else if(c>='a' && c<='f') b |= c - 'a' + 10;
  else if(c>='A' && c<='F') b |= c - 'A' + 10;
  else ;
  return b;
}

#endif

void ucs2_to_win1251(char *src, char *dest, unsigned dest_len)
{
  unsigned dest_len_in_uc = (dest_len - 1) << 2;
  unsigned len = strlen(src) & (~3); // должно быть кратно 4 (4 hex символа на 1 уникод)
  if(len > dest_len_in_uc) len = dest_len_in_uc;
  char *s;
  unsigned c;
  for(s=src; len; s+=4, len-=4)
  {
    c = hex_to_byte(s)<<8 | hex_to_byte(s+2);
    if(c>126)
    {
      if(c==0x451) c = 'ё'; // Win1251 0xB8 little yo
      else if(c==0x401) c = 'Ё'; // Win1251 0xA8 big YO
      else if(c>=0x410 && c<=0x44f) c = c - 0x410 + 0xC0;
      else c = '?';
    }
    else if(c==0) c = ' '; // errata? UCS-2 space is 0020
///    if(c=='"') *dest++ = '\\'; // escape quotes for JSON code /// removed 5.10.2014, separate escaape in str_escape_for_js_string()
    *dest++ = (char)c;
  }
  *dest = 0;
}

void win1251_to_ucs2(char *src, char *dest, unsigned dest_len)
{
  unsigned max_len = (dest_len - 1) >> 2; // uc 4-digit 'characters' + zeroterm
  unsigned len = strlen(src);
  if(len > max_len) len = max_len;

  char *s = src;
  unsigned c;
  for(; len > 0; --len)
  {
    c = *s++;
    if(c >= 0xC0) c = c - 0xC0 + 0x410; // Russian part of 1251 to UCS2 0x0410 to 0x044F
    else if(c == 'ё') c = 0x451; // 1251 0xB8 little yo
    else if(c == 'Ё') c = 0x401; // 1251 0xA8 big YO
    // else UCS2 and W1251 code is the same
    dest += sprintf(dest, "%04X", c);
  }
  *dest = 0;
}

void decode_ussd(char *s)
{
  char buf[128];
  if(is_ucs2(s))
  {
    ucs2_to_win1251(s, buf, sizeof buf);
    str_escape_for_js_string(sms_ussd_responce, buf, sizeof sms_ussd_responce); // 22.08.2014
  }
  else
  {
    str_escape_for_js_string(sms_ussd_responce, s, sizeof sms_ussd_responce);
  }
  sms_ussd_responce_ready = 1;
}

const unsigned char sms_enterprise[] =
// .1.3.6.1.4.1.25728.3800.2
{SNMP_OBJ_ID, 11, // ASN.1 type/len, len is for 'enterprise' trap field
0x2b,6,1,4,1,0x81,0xc9,0x00,0x9d,0x58,2}; // OID for "enterprise" in trap msg
const unsigned char sms_oid[] =
// .1.3.6.1.4.1.25728.3800.1
{SNMP_OBJ_ID, 11, // ASN.1 type/len
0x2b,6,1,4,1,0x81,0xc9,0x00,0x9d,0x58,1}; // OID for varbinds in trap msg

void sms_make_trap(void)
{
  snmp_create_trap((void*)sms_enterprise);
  if(snmp_ds == 0xff) return; // if can't create packet
  snmp_add_vbind_integer32(sms_oid, 1, sms_gsm_failed); // npGsmFailed
  snmp_add_vbind_integer32(sms_oid, 2, sms_gsm_registration); // npGsmRegistration
  snmp_add_vbind_integer32(sms_oid, 3, sms_sig_level); // npGsmSignal
}

void sms_trap_gsm_alive(void)
{
  if(valid_ip(sys_setup.trap_ip1)) { sms_make_trap(); snmp_send_trap(sys_setup.trap_ip1); }
  if(valid_ip(sys_setup.trap_ip2)) { sms_make_trap(); snmp_send_trap(sys_setup.trap_ip2); }
}

unsigned sms_snmp_get(void)
{
  int value;
  switch(snmp_data.id & 0xff)
  {
  case 1: value = sms_gsm_failed; break;
  case 2: value = sms_gsm_registration; break;
  case 3: value = sms_sig_level; break;
  case 9: // npGsmSendSms
    snmp_add_asn_obj(SNMP_TYPE_OCTET_STRING, 0, "");
    return 0;
  default:
    return SNMP_ERR_NO_SUCH_NAME;
  }
  snmp_add_asn_integer(value);
  return 0;
}

unsigned sms_snmp_set(unsigned id, unsigned char *data)
{
  char buf[384], buf1251[256];
  unsigned len;
  switch(id & 0xff)
  {
  case 9: // npGsmSendSms
    if(*data++ != SNMP_TYPE_OCTET_STRING)
      return SNMP_ERR_BAD_VALUE;
    data += asn_get_length(data, &len);
    if(len >= sizeof buf) len = sizeof buf - 1;
    memcpy(buf, data, len);
    buf[len] = 0;
    utf8_to_1251(buf,buf1251, sizeof buf1251);
    sms_send_manual(buf1251);
    snmp_add_asn_obj(SNMP_TYPE_OCTET_STRING, len, (void*)buf); // 10.02.2015
    return 0;
  default:
    return SNMP_ERR_READ_ONLY;
  }
}

void sms_cmd_periodic_exec(void)
{
  static unsigned next_time = 600; // 1 min
  if(sys_clock_100ms < next_time) return;
  next_time = sys_clock_100ms + 600; // once per min

  struct tm *date = ntp_calendar_tz(1);
  char time_txt[32];
  sprintf(time_txt, "%02u:%02u", date->tm_hour, date->tm_min);
  if(strcmp(time_txt, (char*)sms_setup.periodic_time + 1) != 0)
    return;

  char buf[160];
  char *dest = buf;
  dest += sprintf(dest, "PERIODIC REPORT ");
  for(int i=0; i<TERMO_N_CH; ++i)
  {
    if(thermo_notify[i].report & NOTIFY_SMS)
    {
      if(termo_state[i].status != 0)
        dest += sprintf(dest, "T%u=%dC", i+1, termo_state[i].value);
      else
        dest += sprintf(dest, "T%u=?", i+1);
    }
  }
#ifdef IO_MODULE
  for(int i=0; i<IO_MAX_CHANNEL; ++i)
  {
    if(io_notify[i].report & NOTIFY_SMS)
      dest += sprintf(dest, "%c%u=%u ", io_setup[i].direction ? 'O' : 'I', i+1,
          io_state[i].level_filtered ? 1 : 0);
  }
#endif

  for(int i=0; i<RELAY_MAX_CHANNEL; ++i) // 12.11.2014
  {
    if(relay_notify[i].report & NOTIFY_SMS)
      dest += sprintf(dest, "P%u=%s ", i+1, get_relay_state(i, 0, 0) ? "ON" : "OFF" );
  }
#ifdef RELHUM_MAX_CH
  // not used, mooved to notify.c
#elif defined(RELHUM_MODULE)
  if(relhum_notify.report & NOTIFY_SMS)
    if(rh_status_h != 0)
      dest += sprintf(dest, "RH=%u%%", rh_real_h);
    else
      dest += sprintf(dest, "RH=?");
#endif
#ifdef CUR_DET_MODULE
  if(curdet_notify.report & NOTIFY_SMS)
    if(curdet_status < 5)
      dest += sprintf(dest, "SMOKE(C.LOOP)=%s", curloop_sms_state_text[curdet_status]);
#endif
  *dest++ = 0;
  sms_msg_printf("%s", buf, 0);
}

#endif // SMS_CMD_MODULE

