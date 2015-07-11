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
варианты поведени€ при неуспехе перезагрузок
защита переполнени€ счЄтчиков ресетов и циклов опроса
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
  bugfix, wrong pause after relay switch by another module, relay clicks on pwr_restart()
*/

#include "platform_setup.h"
#ifdef PWR_MODUL

#include "eeprom_map.h"
#include "plink.h"

#include <stdio.h>
#include <string.h>

#ifndef PWR_DEBUG
	
	#undef DEBUG_SHOW_TIME_LINE
	#undef DEBUG_MSG			
	#undef DEBUG_PROC_START
	#undef DEBUG_PROC_END
	#undef DEBUG_INPUT_PARAM
        #undef DEBUG_OUTPUT_PARAM

	#define DEBUG_SHOW_TIME_LINE
	#define DEBUG_MSG			
	#define DEBUG_PROC_START(msg)
	#define DEBUG_PROC_END(msg)
	#define DEBUG_INPUT_PARAM(msg,val)	
        #define DEBUG_OUTPUT_PARAM(msg,val)	

#endif

#ifdef DNS_MODULE
const unsigned pwr_sign = 0x569a55a2;
#else
const unsigned pwr_sign = 0x569a45a3;
#endif

struct pingchannel_s {
//       unsigned char *ip;
   unsigned      fail_cnt;
   unsigned char fail;
} pingchannels[PWR_MAX_CHANNEL*IP_PER_PWR_CH];

#define PING_CH_NUMBER (PWR_MAX_CHANNEL*IP_PER_PWR_CH)
#if PING_MAX_CHANNELS < PING_CH_NUMBER
#  error "Insufficient PING_MAX_CHANNELS (defined in ping_def.h) for PWR2 module!" // 31.05.2010
#endif

struct pwr_state_s pwr_state[PWR_MAX_CHANNEL];

struct pwr_setup_s pwr_setup[PWR_MAX_CHANNEL];


#define MAX_POLL_PERIOD 1000  // 1000s



////__monitor void pwr_relay(uword ch,uword relay_st); // must be defined in project.c, HAL/drivers section

void pwr_restart(void);
// void begin_reset(struct pwr_state_s *pwr, struct pwr_setup_s *setup, int a_reset_time);
unsigned pwr_http_get_data(unsigned pkt, unsigned more_data);
unsigned pwr_http_get_manual(unsigned pkt, unsigned more_data);
int pwr_http_set_data(void);
int pwr_http_set_manual(void);
int pwr_http_forced_reboot(void);
void pwr_log_manual_reset(int ch, const char *auxlog);
void start_watchdog_reset(unsigned ch, unsigned reset_time);
unsigned pwr_http_get_relay_cgi(unsigned pkt, unsigned more_data);


HOOK_CGI(pwr_get,    (void*)pwr_http_get_data,   mime_js,  HTML_FLG_GET | HTML_FLG_NOCACHE );
HOOK_CGI(pwr_set,    (void*)pwr_http_set_data,   mime_js,  HTML_FLG_POST );
HOOK_CGI(pwr_m_get,  (void*)pwr_http_get_manual, mime_js,  HTML_FLG_GET | HTML_FLG_NOCACHE );
HOOK_CGI(pwr_m_set,  (void*)pwr_http_set_manual, mime_js,  HTML_FLG_POST );
HOOK_CGI(pwr_reset,  (void*)pwr_http_forced_reboot, mime_js,  HTML_FLG_POST );
HOOK_CGI(relay, (void*)pwr_http_get_relay_cgi,   mime_js,  HTML_FLG_GET | HTML_FLG_NOCACHE );

// returns !=0 if mode is inappropriate
int pwr_change_manual_mode(int ch, int mode, const char *auxlog)
{
  struct pwr_setup_s *setup = &pwr_setup[ch];
  struct pwr_state_s *pwr   = &pwr_state[ch];

  int old_mode = setup->manual;
  if(old_mode == mode) return 0;

  unsigned char ch_name[32];
  str_pasc_to_zeroterm(setup->name, ch_name, sizeof ch_name);
  switch(mode)
  {
  case PWR_MODE_OFF:
  case PWR_MODE_ON:
#if PROJECT_CHAR=='E'
    log_printf("PWR: chan. %d \"%s\" is switched %s %s", ch+1, ch_name,
               mode?"on":"off",
               auxlog);
#else
    log_printf("PWR: канал %d \"%s\" %s %s", ch+1, ch_name,
               mode?"включен":"выключен",
               auxlog);
#endif
    break;
  case PWR_MODE_WDOG:
#if PROJECT_CHAR=='E'
    log_printf("PWR: chan. %d \"%s\" watchdog is activated %s",
#else
    log_printf("PWR: канал %d \"%s\" сторож активирован %s",
#endif
               ch+1, ch_name,
               auxlog);
    break;
  case PWR_MODE_SCHEDULE:
#if PROJECT_CHAR=='E'
    log_printf("PWR: chan. %d \"%s\" schedule is activated %s",
#else
    log_printf("PWR: канал %d \"%s\" управление по расписанию активировано %s",
#endif
               ch+1, ch_name,
               auxlog);
    break;
  case PWR_MODE_LOGIC:
#if PROJECT_CHAR=='E'
    log_printf("PWR: chan. %d \"%s\" logic control is activated %s",
#else
    log_printf("PWR: канал %d \"%s\" логическое управление активировано %s",
#endif
               ch+1, ch_name,
               auxlog);
    break;
  default:
      return 0xff; // wrong mode
  }

  setup->manual = mode;
  setup->active &=~ 0x80; // clear "saved watchdog on-off state" bit (for DKSF35 front-panel buttons) --- probably bug? LBS 24.03.2011
  EEPROM_WRITE(&eeprom_pwr_setup[ch], (void*)setup, sizeof *setup);
  /*  24.03.2011 - теперь пауза выставл€етс€ при любом включении канала, в pwr_exec()
  if(old_manual == setup->reset_mode) // if it's powered on just now
  {
    start_watchdog_reset(ch, 0); // set pause after power-on
  }*/
  if(setup->manual == PWR_MODE_WDOG)
    pwr->repeating_resets_cnt = 0;

#ifdef SMS_MODULE
  if(auxlog != via_sms) // молчать если переключили SMS командой
    sms_pwr_event(ch, 'm');
#endif

  return 0;
}


unsigned pwr_http_get_data(unsigned pkt, unsigned more_data)
{
  char buf[768];
  char *dest = buf;
  if(more_data == 0)
  {
    dest+=sprintf((char*)dest,"var packfmt={");
    PLINK(dest, pwr_setup[0], name);
    PLINK(dest, pwr_setup[0], ip0);
    PLINK(dest, pwr_setup[0], ip1);
    PLINK(dest, pwr_setup[0], ip2);
    PLINK(dest, pwr_setup[0], poll_period);
    PLINK(dest, pwr_setup[0], retry_period);
    PLINK(dest, pwr_setup[0], ping_timeout);
    PLINK(dest, pwr_setup[0], reset_time);
    PLINK(dest, pwr_setup[0], reboot_pause);
    PLINK(dest, pwr_setup[0], max_retry);
    PLINK(dest, pwr_setup[0], doubling_pause_resets);
    PLINK(dest, pwr_setup[0], reset_mode);
    PLINK(dest, pwr_setup[0], active);
    PLINK(dest, pwr_setup[0], logic_mode);
    PLINK(dest, pwr_setup[0], manual);
#ifdef DNS_MODULE
    PLINK(dest, pwr_setup[0], hostname0);
    PLINK(dest, pwr_setup[0], hostname1);
    PLINK(dest, pwr_setup[0], hostname2);
#endif
    PSIZE(dest, (char*)&pwr_setup[1] - (char*)&pwr_setup[0]); // must be the last // alignment!
    dest+=sprintf(dest, "};\r\nvar data=[");
    tcp_put_tx_body(pkt, (unsigned char*)buf, dest - buf);
    return 1;
  }
  else
  {
    int n = more_data - 1;
    struct pwr_setup_s *s = &pwr_setup[n];
    *dest++ = '{';
    PDATA_PASC_STR(dest, (*s), name);
    PDATA_IP(dest, (*s), ip0);
    PDATA_IP(dest, (*s), ip1);
    PDATA_IP(dest, (*s), ip2);
    PDATA(dest, (*s), poll_period);
    PDATA(dest, (*s), retry_period);
    PDATA(dest, (*s), ping_timeout);
    PDATA(dest, (*s), reset_time);
    PDATA(dest, (*s), reboot_pause);
    PDATA(dest, (*s), max_retry);
    PDATA(dest, (*s), doubling_pause_resets);
    PDATA(dest, (*s), reset_mode);
    PDATA(dest, (*s), active);
    PDATA(dest, (*s), logic_mode);
    PDATA(dest, (*s), manual);
#ifdef DNS_MODULE
    PDATA_PASC_STR(dest, (*s), hostname0);
    PDATA_PASC_STR(dest, (*s), hostname1);
    PDATA_PASC_STR(dest, (*s), hostname2);
#endif
    dest+=sprintf((char*)dest, "reset_cnt:%d", pwr_state[n].reset_cnt);
    //--dest; // clear last PDATA-created comma
    *dest++ = '}'; *dest++ = ',';
    if(more_data < PWR_MAX_CHANNEL)
    {
      tcp_put_tx_body(pkt, (unsigned char*)buf, dest - buf);
      return more_data + 1;
    }
    else
    {
      --dest; // clear last comma
      *dest++ = ']'; *dest++ = ';';
      tcp_put_tx_body(pkt, (unsigned char*)buf, dest - buf);
      return 0;
    }
  }
}

int pwr_http_set_data(void)
{
  http_post_data((void*)pwr_setup, sizeof pwr_setup);
#ifdef DNS_MODULE
  struct pwr_setup_s *ep = eeprom_pwr_setup;
  struct pwr_setup_s *p = pwr_setup;
  for(int i=0; i<PWR_MAX_CHANNEL; ++i, ++ep, ++p)
  {
    dns_resolve(ep->hostname0, p->hostname0);
    dns_resolve(ep->hostname1, p->hostname1);
    dns_resolve(ep->hostname2, p->hostname2);
  }
#endif
  EEPROM_WRITE(&eeprom_pwr_setup, (unsigned char*)pwr_setup, sizeof eeprom_pwr_setup);
  pwr_restart();
  http_redirect("/pwr.html");
  return 0;
}

unsigned pwr_http_get_manual(unsigned pkt, unsigned more_data)
{
  char buf[512];
  char *dest = buf;
  dest += sprintf((char*)dest,"var data=[");
  for(int n=0;n<PWR_MAX_CHANNEL;++n)
  {
    dest += sprintf(dest, "{mode:%u},", pwr_setup[n].manual);
  }
  --dest; // clear last comma
  *dest++=']';
  *dest++=';';
  tcp_put_tx_body(pkt, (unsigned char*)buf, dest - buf);
  return 0;
}

int pwr_http_set_manual(void)
{
  // command format : hex string, two hex digits (one byte) per pwr channel
  // bits 0..1 of byte: channel mode (0 off, 1 on, 2 wathdog)
  // bit 3 of byte: force reset

  int skip = sizeof("data=")-1;
  if(http.post_content_length - skip != PWR_MAX_CHANNEL*2 ) return 0;
  char *dest = req + skip;

  int ch;
  unsigned char c;
  struct pwr_setup_s *setup;
  struct pwr_state_s *pwr;

  for(ch=0, pwr=pwr_state, setup=pwr_setup; ch<PWR_MAX_CHANNEL; ++ch, ++pwr, ++setup)
  {
    c = hex_to_byte(dest + ch*2);
#if PROJECT_MODEL==35
    if(c & 0x10) continue; // skip bit is set
    if(current_access_rights != 1 &&  // not admin
       (current_access_groups & (1<<ch)) == 0) // not channel owner
      continue; // access denied
#endif
    pwr_change_manual_mode(ch, c & 0x07, via_web);
  } // for ch

  http_redirect("/pwr_man.html");
  return 0;
}


int pwr_http_forced_reboot(void)
{
  unsigned char data = 0xaa;
  http_post_data((void*)&data, sizeof data);
  if(data != 0xaa && data < PWR_MAX_CHANNEL)
    pwr_watchdog_force_reset(data, via_web);
  http_reply(200,"");
  return 0;
}

unsigned pwr_http_get_relay_cgi(unsigned pkt, unsigned more_data) // 5.05.2012
{
  char *p = req_args;
  unsigned ch, mode;
  if(*p++ != 'r') goto error;
  ch = *p++ - '1';
  if(ch > PWR_MAX_CHANNEL) goto error;
  if(*p++ != '=') goto error;
  mode = *p++ - '0';
  if(mode > 2) goto error;
  if(*p != 0) goto error;
  pwr_change_manual_mode(ch, mode, via_url);
  http_reply(200,"ok");
  return 0;
error:
  http_reply(200,"error");
  return 0;
}

const systime_t MAX_TIME_VALUE = 0xFFFFFFFFFFFFFFFF;

void pwr_restart(void)
{
  int n, w, cnt;
  systime_t time;
  struct pwr_state_s *pwr;
  struct pwr_setup_s *setup;

  time = sys_clock() + 1;

  util_fill((unsigned char*)pingchannels, sizeof pingchannels, 0);
  for(n=0; n<PING_CH_NUMBER; ++n)
  {
    w = n / IP_PER_PWR_CH;
    ping_table[n].ping_step = PING_RESET;
    ping_table[n].timeout = pwr_setup[w].retry_period;
    ping_table[n].ttl = 127;
  }
  for(w=0, pwr=pwr_state, setup=pwr_setup;
      w<PWR_MAX_CHANNEL;
      ++w, ++pwr, ++setup)
  {
    n = w * IP_PER_PWR_CH;
    util_cpy(setup->ip0, ping_table[n+0].ip, 4);
    util_cpy(setup->ip1, ping_table[n+1].ip, 4);
    util_cpy(setup->ip2, ping_table[n+2].ip, 4);
    //////// conserve some fields !!!!!
    cnt = pwr->reset_cnt;
    util_fill((unsigned char*)pwr, sizeof(*pwr), 0); // clear state
    pwr->reset_cnt = cnt; // restore
    // delayed ping start
    int p = setup->reboot_pause * 1000; // s->ms
    if(p<8000) p = 8000; // 8s min. pause
    pwr->reboot_end_time = time + p;
    pwr->next_time = pwr->reboot_end_time;
  }
}

void pwr_init(void)
{
  unsigned sign = 0;
  EEPROM_READ(&eeprom_pwr_signature, &sign, sizeof sign);
  if(sign != pwr_sign) pwr_reset_params();
  EEPROM_READ(&eeprom_pwr_setup, pwr_setup, sizeof pwr_setup);
  pwr_restart();
}

void pwr_reset_params(void)
{
  int n;
  struct pwr_setup_s *setup;
  util_fill((unsigned char*)pwr_setup, sizeof pwr_setup, 0);
  for(n=0, setup=pwr_setup; n<PWR_MAX_CHANNEL; ++n, ++setup)
  {
   setup->name[0]      = 0;
   setup->poll_period  = 10;
   setup->retry_period = 1000;
   setup->ping_timeout = 750;
   setup->reset_time   = 4;
   setup->reboot_pause = 15;
   setup->max_retry    = 8;
   setup->doubling_pause_resets = 0;
   setup->logic_mode   = A_or_B_or_C;
   setup->reset_mode   = 0;
   setup->active       = 0;
   setup->manual       = 1;
  }
  EEPROM_WRITE(&eeprom_pwr_setup, pwr_setup, sizeof pwr_setup);
  unsigned sign = pwr_sign;
  EEPROM_WRITE(&eeprom_pwr_signature, &sign, sizeof eeprom_pwr_signature);
}


/*
logic - логика работы канала
pingch - ссылка на (внутримодульный) пинг-канал ј данного watchdog канала
возвращает Ќ≈ 0 если нужен сброс
*/
int check_reset(int logic, unsigned char mask, struct pingchannel_s *pingch)
{
   switch(logic)
   {
   case A_and_B_and_C: // сброс питани€  в случае, если недоступен любой из адресов (A)(B)(C).

     if(mask&1 && pingch[0].fail) return 1;
     if(mask&2 && pingch[1].fail) return 1;
     if(mask&4 && pingch[2].fail) return 1;
     return 0;

   case A_or_B_or_C: //  сброс питани€ в случае одновременной недоступности всех Ip-адресов

     if(mask&1 && !pingch[0].fail) return 0;
     if(mask&2 && !pingch[1].fail) return 0;
     if(mask&4 && !pingch[2].fail) return 0;
     if(mask&7) return 1;
     return 0;


   case A_or_B_and_C: // сброс питани€ в случае, если одновременно недоступен адрес (ј) и недоступен любой из адресов (B) или (C)

     if(! mask&1) return 0; // disabled A is always OK, so no resets // 52.3.1
     if(mask&1 && !pingch[0].fail) return 0;

     if(mask&2 &&  pingch[1].fail) return 1;
     if(mask&4 &&  pingch[2].fail) return 1;
     return 0;

   case A_not_B_or_C: // сброс питани€ в случае, если недоступен адрес (ј) но доступен любой из адресов (B) или (C)

     if(!(mask&1)) return 0; // disabled A is always OK, so no resets
     if(mask&1 && !pingch[0].fail) return 0;

     if(!(mask&6)) return 1; // if both B and C disabled, check only A
     if(mask&2 &&  !pingch[1].fail) return 1;
     if(mask&4 &&  !pingch[2].fail) return 1;
     return 0; // if both B and C not available, no reset (this case is: A unavailable due to network fault)


   default:
      return 0;
   }
}

void clear_reset(struct pingchannel_s *pingch)
{
   int n;
   for(n=0; n<IP_PER_PWR_CH; ++n)
   {
      pingch[n].fail = 0;
      pingch[n].fail_cnt = 0;
   }
}

void start_watchdog_reset(unsigned ch, unsigned reset_time)
{
  if(ch >= PWR_MAX_CHANNEL) return;
  struct pwr_state_s *pwr = &pwr_state[ch];
  struct pwr_setup_s *setup = &pwr_setup[ch];
  systime_t time = sys_clock();
  pwr->reset_end_time = time + reset_time * 1000; // s->ms
  pwr->reboot_end_time = pwr->reset_end_time + setup->reboot_pause * 1000; // s->ms
  pwr->next_time = pwr->reboot_end_time;
  if(reset_time != 0) pwr->reset = 1;
}

static const unsigned char zero_ip[] = {0,0,0,0};

char * ip_status(int active, struct pingchannel_s *pingch)
{
#if PROJECT_CHAR=='E'
  if(!active) return "is ignored";
  if(pingch->fail) return "no reply";
  return "is ok";
#else
  if(!active) return "игнорируетс€";
  if(pingch->fail) return "молчит";
  return "отвечает";
#endif
}

#ifdef LOG_MODUL
void pwr_log_status_msg(int pwr_ch_n, int status) // modified LBS 31.05.2010
{
  struct pwr_setup_s *setup = &pwr_setup[pwr_ch_n];
  char ch_label[32];

  str_pasc_to_zeroterm(setup->name, (unsigned char*)ch_label, sizeof ch_label);
#if PROJECT_CHAR=='E'
  switch(status)
  {
  case 0:
    log_printf("Watchdog: chan.%d \"%s\" - max number of unsuccessful resets in a raw (%d). Resetting is stopped.",
               pwr_ch_n + 1,
               ch_label,
               setup->doubling_pause_resets);
    break;
  case 1:
    log_printf("Watchdog: chan.%d \"%s\" - got reply. Watchdog is re-activated.",
               pwr_ch_n + 1,
               ch_label);
    break;
  }
#else // PROJECT_CHAR
  switch(status)
  {
  case 0:
    log_printf("—торож: кан.%d \"%s\" - достигнут лимит повторных сбросов (%d). —бросы приостановлены.",
               pwr_ch_n + 1,
               ch_label,
               setup->doubling_pause_resets);
    break;
  case 1:
    log_printf("—торож: кан.%d \"%s\" - получен ответ. ѕриостановка сбросов завершена.",
               pwr_ch_n + 1,
               ch_label);
    break;
  }
#endif // PROJECT_CHAR=='E'
}
#endif

void pwr_log_reset(int pwr_ch_n, unsigned char mask, struct pingchannel_s *pingch)
{
#ifdef LOG_MODUL
  //struct pwr_state_s *pwr = &pwr_state[pwr_ch_n];
  struct pwr_setup_s *setup = &pwr_setup[pwr_ch_n];

  char ch_label[32];
  str_pasc_to_zeroterm(setup->name, (unsigned char*)ch_label, sizeof ch_label);

  unsigned char ips[IP_PER_PWR_CH][16];
  str_ip_to_str(setup->ip0, ips[0]);
  str_ip_to_str(setup->ip1, ips[1]);
  str_ip_to_str(setup->ip2, ips[2]);

#if PROJECT_CHAR=='E'
  log_printf("Watchdog: reset of chan.%d \"%s\". A (%s) %s, B (%s) %s, C (%s) %s.",
#else
  log_printf("—торож: сброс кан.%d \"%s\". A (%s) %s, B (%s) %s, C (%s) %s.",
#endif
     pwr_ch_n + 1, ch_label,
     ips[0], ip_status(mask & 1, &pingch[0]),
     ips[1], ip_status(mask & 2, &pingch[1]),
     ips[2], ip_status(mask & 4, &pingch[2])
   );
#endif
}

void pwr_log_manual_reset(int ch, const char *auxlog)
{
  unsigned char ch_name[32];
  if(ch >= PWR_MAX_CHANNEL) return;
  str_pasc_to_zeroterm(pwr_setup[ch].name, ch_name, sizeof ch_name);
#if PROJECT_CHAR=='E'
  log_printf("PWR: chan. %d \"%s\" forced (manual) reset %s", ch+1, ch_name, auxlog);
#else
  log_printf("PWR: канал %d \"%s\" принудительный сброс %s", ch+1, ch_name, auxlog);
#endif
}

void pwr_exec(void)
{
  systime_t pwr_time;
  struct pingchannel_s *pingch;
  struct ping_channel_struct *pingt; // ptr to ping.c interface table
  struct pwr_state_s *pwr;
  struct pwr_setup_s *setup;
  int pwr_ch_n, ping_ip_n;
  unsigned char relay_state;

  pwr_time = sys_clock();

  for(pwr_ch_n=0, pwr=pwr_state, setup=pwr_setup;
      pwr_ch_n<PWR_MAX_CHANNEL;
      ++pwr_ch_n, ++pwr, ++setup)
  {

    if(pwr_time > pwr->reset_end_time) pwr->reset = 0;
    /*
    if(setup->manual!=2 && pwr->reset==0)
    {
      relay_state = setup->manual & 1;
    }
    else
    {
      relay_state = (setup->reset_mode^1)^pwr->reset;
    }
    */
    if(pwr->reset)
    {
      relay_state = setup->reset_mode;
    }
    else
    {
      switch(setup->manual)
      {
      case PWR_MODE_OFF:
        relay_state = 0;
        break;
      case PWR_MODE_ON:
        relay_state = 1;
        break;
      case PWR_MODE_WDOG:
        relay_state = !setup->reset_mode;
        if(pwr->reset) relay_state ^= 1;
        break;
#ifdef WTIMER_MODULE
      case PWR_MODE_SCHEDULE:
        relay_state = (wtimer_schedule_output >> pwr_ch_n) & 1;
        break;
#endif
#ifdef LOGIC_MODULE
      case PWR_MODE_LOGIC:
        relay_state = (logic_pwr_output >> pwr_ch_n) & 1;
        break;
#endif
      }
    }
#ifdef POWERSAVING_ENABLED
    if(powersaving) relay_state = 0;
#endif
    if(pwr->relay_state == setup->reset_mode // old mode is reset
    && relay_state == !setup->reset_mode) // switching On
    { // semi-hack, postpone ping tests after switching on by another modules except watchdog
      pwr->reboot_end_time = /*pwr->reset_end_time*/ pwr_time + setup->reboot_pause * 1000; // s->ms  // 28.05.2013, removed relay clicks on http save
      pwr->next_time = pwr->reboot_end_time;
    }
    pwr->relay_state = relay_state;
    pwr_relay(pwr_ch_n, relay_state);

    if(pwr_time <= pwr->reboot_end_time) continue;
    if(setup->manual != PWR_MODE_WDOG) continue; // skip if in manual control mode

    pingch = &pingchannels[pwr_ch_n * IP_PER_PWR_CH]; // don't move it!!!
    pingt = &ping_table[pwr_ch_n * IP_PER_PWR_CH];

    // schedule next ping burst in this pwr channel
    if(pwr_time > pwr->next_time)
    {
      pwr->start_pings = 1;
      pwr->next_time = pwr_time + setup->poll_period * 1000;
      /// if(pwr->poll_cnt < 1000000) pwr->poll_cnt += 1; // prevent overflow
      if(pwr->poll_cnt < 0xffff) // prevent overflow // bugfix LBS 31.05.2010
         pwr->poll_cnt += 1;
    }

    // scan IPs
    for(ping_ip_n=0; ping_ip_n<IP_PER_PWR_CH; ++ping_ip_n, ++pingt, ++pingch)
    {
      if((setup->active&(1<<ping_ip_n)) == 0 )
      {  // this ip is disabled by checkbox
        pingch->fail = 0;
        continue;
      }
      unsigned char *ip = setup->ip0 + 4 * ping_ip_n; // semi-hack! indirect access to ip1, ip2 ******
      unsigned char *name = setup->hostname0 + sizeof setup->hostname0 * ping_ip_n; // semi-hack! indirect access
      if(memcmp(ip, zero_ip, 4) == 0)
      {
        if(name[0] != 0)
        {  // hostname is unresolved (time to resolve is equivalent to reboot pause) // 28.05.2013
          pingch->fail = 1;
        }
        else
        { // channel is disabled by all-zero ip
          pingch->fail = 0;
        }
        continue;
      }
      // ping burst (flag will be cleared after cycle by IPs ends)
      if(pwr->start_pings)
      {
        pingch->fail = 0;
        pingch->fail_cnt = 0;
        pwr->pings_in_progress |= (1<<ping_ip_n);
        memcpy(pingt->ip, setup->ip0 + 4 * ping_ip_n, 4); // 22.05.2013 for DNS **********************
        pingt->timeout = setup->retry_period;
        pingt->ping_step = PING_START;
        /*
        ///////// DEBUGGG
        // ping LED (ближний к реле)
        debug_led_on();
        //
        */
      }
      // scan results
      switch(pingt->ping_step)
      {
      case PING_TIME_OUT:
        pingt->ping_step = PING_RESET;
        pingch->fail_cnt += 1;
        if(pingch->fail_cnt >= setup->max_retry)
        {
          pingch->fail = 1;                       // "channel failed" flag
          pwr->pings_in_progress &= ~(1<<ping_ip_n);
        }
        else
        {
          pingt->timeout = setup->retry_period;
          pingt->ping_step = PING_START;
          /*
          ///////// DEBUGGG
          // ping LED (ближний к реле)
          debug_led_on();
          //
          */
        }
        break;
      case PING_OK:
        if(util_cmp(pingt->ip, pingt->answer_ip, 4) != 0) // host isn't available ICMP reply???? (Ping module underdevelopment)
        {
          pingt->ping_step = PING_TIME_OUT;  // ping failed, set appropriate state in ping channel (Ping module underdevelopment)
        }
        else
        {
          pingt->ping_step = PING_RESET;
          pwr->pings_in_progress &= ~(1<<ping_ip_n);
          pingch->fail = 0;
          pingch->fail_cnt = 0;
        }
        break;
      } // switch
    } // for ping ip

    pwr->start_pings = 0;

    if(pwr->pings_in_progress)
    {
      // postpone next pings if poll isn't completed
      systime_t t = pwr_time + setup->retry_period; // ms
      if(t > pwr->next_time) pwr->next_time = t;
      // postpone reset logic processing till poll completed
      continue; // 27.08.2009
    }

    // first ping channel of this pwr channel
    pingch = &pingchannels[pwr_ch_n * IP_PER_PWR_CH];
    // process reset logic
    if(check_reset(setup->logic_mode, setup->active, pingch))
    {
      if(setup->doubling_pause_resets &&  // feature enabled
         pwr->poll_cnt < 2 &&             // it's repeating reset on 1st poll cycle after reset
         pwr->repeating_resets_cnt >= setup->doubling_pause_resets ) // counter reached on prev. reset
      {
        /* LBS 31.05.2010
        // switch to manual mode
        setup->manual = setup->reset_mode ^ 1; // switch to manual mode in "not reset" state
                 */
#ifdef LOG_MODUL
        if(pwr->resets_disabled == 0)
          pwr_log_status_msg(pwr_ch_n, 0); // show resets is ceased
#endif
        // do nothing, continue to poll - LBS 31.05.2010
        pwr->resets_disabled = 1;
      }
      else
      {
        // do reset
        /// pwr->reset = 1; // is set in start_watchdog_reset() */
        if(pwr->reset_cnt < 0xffff)  pwr->reset_cnt += 1; // protect overflow // LBS 31.05.2010
        if(pwr->poll_cnt < 2)
          if(pwr->repeating_resets_cnt < 0xffff) // protect overflow // LBS 31.05.2010
            pwr->repeating_resets_cnt += 1; // count repeating resets
        start_watchdog_reset(pwr_ch_n, setup->reset_time);
#ifdef LOG_MODUL
        pwr_log_reset(pwr_ch_n, setup->active, pingch);
#endif
#ifdef SMS_MODULE
        sms_pwr_event(pwr_ch_n, 'w');
#endif
        clear_reset(pingch); // resets fail flag in all three ping channels
      } // reset actions
      pwr->poll_cnt = 0; // next poll cycle will be first after reset
    } // if check_reset
    else
    { // no fail, pings and logic reports ok // LBS 31.05.2010
      pwr->repeating_resets_cnt = 0;
#ifdef LOG_MODUL
      if(pwr->resets_disabled != 0)
        pwr_log_status_msg(pwr_ch_n, 1);
#endif
      pwr->resets_disabled = 0;
    } // end if check_reset
  } // for pwr ch

} // pwr_exec()



int pwr_snmp_get(unsigned id, unsigned char *data)
{
  unsigned ch = (id & 0x000f) -  1;
  if(ch >= PWR_MAX_CHANNEL) return SNMP_ERR_NO_SUCH_NAME;
  struct pwr_setup_s *setup = &pwr_setup[ch];
  struct pwr_state_s *pwr =   &pwr_state[ch];
  int val = 0;
  switch(id&0xfffffff0)
  {
  case 0x5810: // P_CHANNEL_N
    val = ch + 1; // idexes is 1-based!
    break;
  case 0x5820: // P_START_RESET - i.e., reset status
    if(pwr->reset) val = 1; // reset active
    else if(sys_clock() <= pwr->reboot_end_time) val=2; // reset is over, reboot active
    else val = 0;
    break;
  case 0x5830: // P_MANUAL_MODE
    val = setup->manual;
    break;
  case 0x5840: // P_RESET_COUNTER
    val = pwr->reset_cnt;
    break;
  case 0x5850: // P_REPEATING_RESETS_COUNTER
    val = pwr->repeating_resets_cnt;
    break;
  case 0x5860: // P_MEMO
    snmp_add_asn_obj(SNMP_TYPE_OCTET_STRING, setup->name[0], &setup->name[1]);
    return 0; // exit here, don't add val!
  case 0x58f0: // npPwrRelayState
    val = pwr->relay_state;
    break;
  default:
    return SNMP_ERR_NO_SUCH_NAME;
  }
  if(val > 0x7FFF) val = 0x7FFF;
  snmp_add_asn_integer(val);
  return 0;
}

int pwr_snmp_set(unsigned id, unsigned char *data)
{
  unsigned ch = (id & 0xf) - 1; // index in resource id is 1-based
  if(ch >= PWR_MAX_CHANNEL) return SNMP_ERR_NO_SUCH_NAME;

  struct pwr_state_s *pwr =   &pwr_state[ch];

  // read vbind value
  int val = 0;
  if((id & 0xfffffff0) != 0x5860)
  { // except MEMO, all other variables are INTEGER
    if(*data != 0x02) return SNMP_ERR_BAD_VALUE; // not INTEGER
    data += asn_get_integer(data, &val);
  }

  switch(id & 0xfffffff0)
  {
  extern const char via_snmp[];
  case 0x5820:  // P_START_RESET
    // snmp SET any value, i.e. no validation
    /*
    begin_reset(&pwr_state[ch], setup, setup->reset_time);
    pwr_log_manual_reset(ch, "через snmp");
    */
    pwr_watchdog_force_reset(ch, (char *)via_snmp);
    break;
  case 0x5830:  // P_MANUAL_MODE
    if(val<0 || val>2) return SNMP_ERR_BAD_VALUE;
    pwr_change_manual_mode(ch, val, (char *)via_snmp);
    break;
  case 0x5840: // P_RESET_COUNTER
    // write any value to reset counter
    pwr->reset_cnt = 0;
    pwr->repeating_resets_cnt = 0;
    val = 0; // return 0 as value
    break;
  default:
    return SNMP_ERR_READ_ONLY;
  }
  snmp_add_asn_integer(val);
  //snmp_add_asn_obj(SNMP_TYPE_INTEGER, 2, (unsigned char*)&val);  // little endian, 2-byte signed
  return 0;
}


void pwr_watchdog_force_reset(unsigned ch, const char *aux_msg)
{
  start_watchdog_reset(ch, pwr_setup[ch].reset_time);
#ifdef LOG_MODUL
  pwr_log_manual_reset(ch, aux_msg);
#endif
#ifdef SMS_MODULE
  if(aux_msg != via_sms)
    sms_pwr_event(ch, 'r');
#endif
}

int pwr_event(enum event_e event)
{
  switch(event)
  {
  case E_EXEC:
    pwr_exec();
    break;
  case E_INIT:
    pwr_init();
    return 0;
  case E_RESET_PARAMS:
    pwr_reset_params();
    break;
    /*  LBS 24.03.2011 - не используетс€, теперь пауза выставл€етс€ при любом включении
  case E_POWERSAVING:
    if(powersaving == 0) // if powersaving goes to off
    { // pause pings after powering-up channels
      for(int n=0;n<PWR_MAX_CHANNEL;++n) start_watchdog_reset(n, 0);
    }
    break;
    */
  }
  return 0;
}

#warning TODO переделать уведомлени€ под DNS!
#endif // PWR_MODUL

