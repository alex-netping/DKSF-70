/*
v2.0
25.03.2013
  rewrite, external relay logic, smart ping module
v2.1-48
15.04.2013
  rewrite for dksf48
v2.2-52
12.08.2013
  dns integration
*/

#include "platform_setup.h"
#include "eeprom_map.h"
#include "plink.h"
#include <string.h>
#include <stdio.h>

#if PING_VER < 2
#error "Update ping.c!"
#endif

#if PING_MAX_CHANNELS < WDOG_MAX_CHANNEL * 3
#error "insufficient ping channels!"
#endif

void wdog_restart(void);

const unsigned wdog_signature = 0xac7430e1;

struct wdog_state_s wdog_state[WDOG_MAX_CHANNEL];
struct wdog_setup_s wdog_setup[WDOG_MAX_CHANNEL];
unsigned wdog_ch;

unsigned wdog_http_get_data(unsigned pkt, unsigned more_data)
{
  char buf[768];
  char *dest = buf;
  if(more_data == 0)
  {
    dest+=sprintf((char*)dest,"var packfmt={");
    PLINK(dest, wdog_setup[0], signature);
    PLINK(dest, wdog_setup[0], name);
    PLINK(dest, wdog_setup[0], ip0);
    PLINK(dest, wdog_setup[0], ip1);
    PLINK(dest, wdog_setup[0], ip2);
    PLINK(dest, wdog_setup[0], poll_period);
    PLINK(dest, wdog_setup[0], ping_timeout);
    PLINK(dest, wdog_setup[0], reset_time);
    PLINK(dest, wdog_setup[0], reboot_pause);
    PLINK(dest, wdog_setup[0], max_retry);
    PLINK(dest, wdog_setup[0], doubling_pause_resets);
    PLINK(dest, wdog_setup[0], reset_mode);
    PLINK(dest, wdog_setup[0], active);
    PLINK(dest, wdog_setup[0], logic_mode);
#ifdef DNS_MODULE
    PLINK(dest, wdog_setup[0], hostname0);
    PLINK(dest, wdog_setup[0], hostname1);
    PLINK(dest, wdog_setup[0], hostname2);
#endif
    PSIZE(dest, (char*)&wdog_setup[1] - (char*)&wdog_setup[0]); // must be the last // alignment!
    dest+=sprintf(dest, "}; var data=[");
    tcp_put_tx_body(pkt, (unsigned char*)buf, dest - buf);
    return 1;
  }
  else
  {
    int n = more_data - 1;
    struct wdog_setup_s *su = &wdog_setup[n];
    *dest++ = '{';
    PDATA(dest, (*su), signature);
    PDATA_PASC_STR(dest, (*su), name);
    PDATA_IP(dest, (*su), ip0);
    PDATA_IP(dest, (*su), ip1);
    PDATA_IP(dest, (*su), ip2);
    PDATA(dest, (*su), poll_period);
    PDATA(dest, (*su), ping_timeout);
    PDATA(dest, (*su), reset_time);
    PDATA(dest, (*su), reboot_pause);
    PDATA(dest, (*su), max_retry);
    PDATA(dest, (*su), doubling_pause_resets);
    PDATA(dest, (*su), reset_mode);
    PDATA(dest, (*su), active);
    PDATA(dest, (*su), logic_mode);
#ifdef DNS_MODULE
    PDATA_PASC_STR(dest, (*su), hostname0);
    PDATA_PASC_STR(dest, (*su), hostname1);
    PDATA_PASC_STR(dest, (*su), hostname2);
#endif
    dest+=sprintf((char*)dest, "reset_count:%d,", wdog_state[n].reset_count);
    dest+=sprintf((char*)dest, "manual:%d,", // legacy 'not connected to relay' indication
      relay_setup[n].mode == RELAY_MODE_WDOG || relay_setup[n].mode == RELAY_MODE_SCHED_WDOG ? 2 : 0xff);
    --dest; // clear last PDATA-created comma
    *dest++ = '}'; *dest++ = ',';
    if(more_data < WDOG_MAX_CHANNEL)
    {
      tcp_put_tx_body(pkt, (void*)buf, dest - buf);
      return more_data + 1;
    }
    else
    {
      --dest; // clear last comma
      *dest++ = ']'; *dest++ = ';';
      tcp_put_tx_body(pkt, (void*)buf, dest - buf);
      return 0;
    }
  }
}

int wdog_http_set_data(void)
{
  http_post_data((void*)wdog_setup, sizeof wdog_setup);
#ifdef DNS_MODULE
  struct wdog_setup_s *ep = eeprom_wdog_setup;
  struct wdog_setup_s *p  = wdog_setup;
  for(int i=0; i<WDOG_MAX_CHANNEL; ++i, ++ep, ++p)
  {
    dns_resolve(ep->hostname0, p->hostname0);
    dns_resolve(ep->hostname1, p->hostname1);
    dns_resolve(ep->hostname2, p->hostname2);
  }
#endif
  EEPROM_WRITE(&eeprom_wdog_setup, wdog_setup, sizeof eeprom_wdog_setup);
  wdog_restart();
  http_redirect("/wdog.html");
  return 0;
}

HOOK_CGI(wdog_get,    (void*)wdog_http_get_data,    mime_js,  HTML_FLG_GET | HTML_FLG_NOCACHE );
HOOK_CGI(wdog_set,    (void*)wdog_http_set_data,    mime_js,  HTML_FLG_POST );

int wdog_snmp_get(unsigned id, unsigned char *data)
{
  unsigned ch = (id & 0x000f) -  1;
  if(ch >= WDOG_MAX_CHANNEL) return SNMP_ERR_NO_SUCH_NAME;
  struct wdog_setup_s *setup = &wdog_setup[ch];
  struct wdog_state_s *state = &wdog_state[ch];
  int val = 0;
  switch(id&0xfffffff0)
  {
  case 0x5810: // P_CHANNEL_N
    val = ch + 1; // idexes is 1-based!
    break;
  case 0x5820: // P_START_RESET - i.e., reset status
    if(state->output == setup->reset_mode) val = 1; // reset active
    else if(sys_clock() <= state->reboot_end_time) val=2; // reset is over, reboot active
    else val = 0;
    break;
  case 0x5830: // P_MANUAL_MODE
    return SNMP_ERR_NO_SUCH_NAME;
    /*
    // mooved to relay.c
    val = setup->manual;
    break;
    */
    //return 0;
  case 0x5840: // P_RESET_COUNTER
    val = state->reset_count;
    break;
  case 0x5850: // P_REPEATING_RESETS_COUNTER
    val = state->repeating_resets_count;
    break;
  case 0x5860: // P_MEMO
    snmp_add_asn_obj(SNMP_TYPE_OCTET_STRING, setup->name[0], &setup->name[1]);
    return 0; // exit here, don't add val!
  case 0x58f0: // npPwrRelayState
    return SNMP_ERR_NO_SUCH_NAME;
    /*
    // mooved to relay.c
    val = pwr->relay_state;
    break;
    */
    //return 0;
  default:
    return SNMP_ERR_NO_SUCH_NAME;
  }
  if(val > 0x7FFF) val = 0x7FFF;
  snmp_add_asn_integer(val);
  return 0;
}

int wdog_snmp_set(unsigned id, unsigned char *data)
{
  unsigned ch = (id & 0xf) - 1; // index in resource id is 1-based
  if(ch >= WDOG_MAX_CHANNEL) return SNMP_ERR_NO_SUCH_NAME;

  struct wdog_state_s *state = &wdog_state[ch];

  // read vbind value
  int val = 0;
  if((id & 0xfffffff0) != 0x5860)
  { // except MEMO, all other variables are INTEGER
    if(*data != 0x02) return SNMP_ERR_BAD_VALUE; // not INTEGER
    data += asn_get_integer(data, &val);
  }

  switch(id & 0xfffffff0)
  {
  case 0x5820:  // P_START_RESET
    return SNMP_ERR_NO_SUCH_NAME;
    /*
    // mooved to relay.c
    // snmp SET any value, i.e. no validation
    pwr_watchdog_force_reset(ch, (char *)via_snmp);
    break;
    */
    //return 0;
  case 0x5830:  // P_MANUAL_MODE
    return SNMP_ERR_NO_SUCH_NAME;
    /*
    // mooved to relay.c
    if(val<0 || val>2) return SNMP_ERR_BAD_VALUE;
    pwr_change_manual_mode(ch, val, (char *)via_snmp);
    break;
    */
    //return 0;
  case 0x5840: // P_RESET_COUNTER
    // write any value to reset counter
    state->reset_count = 0;
    state->repeating_resets_count = 0;
    val = 0; // return 0 as value
    break;
  default:
    return SNMP_ERR_READ_ONLY;
  }
  snmp_add_asn_integer(val);
  return 0;
}


static char *ip_status(struct ping_state_s *pingch)
{
#if PROJECT_CHAR=='E'
  if(pingch->state == PING_RESET) return "is ignored";
  if(pingch->state != PING_COMPLETED) return "unknown error";
  if(pingch->result == 0)
  {
    if(pingch->count == 0) return "can't resolve name";
    else return "no reply";
  }
  return "is ok";
#else
  if(pingch->state == PING_RESET) return "игнорируется";
  if(pingch->state != PING_COMPLETED) return "сбой пинга";
  if(pingch->result == 0)
  {
    if(pingch->count == 0) return "не срабатывает DNS";
    else return "молчит";
  }
  return "отвечает";
#endif
}


void wdog_log_resetting_state(unsigned wd_ch)
{
  struct wdog_setup_s *setup = &wdog_setup[wd_ch];
#if PROJECT_CHAR=='E'
  switch(wdog_state[wd_ch].resetting_disabled)
  {
  case 1:
    log_printf("Watchdog: chan.%d%s - max number of unsuccessful resets in a raw (%d). Resetting is stopped.",
               wd_ch + 1,
               quoted_name(setup->name),
               setup->doubling_pause_resets);
    break;
  case 0:
    log_printf("Watchdog: chan.%d%s - got reply. Watchdog is re-activated.",
               wd_ch + 1,
               quoted_name(setup->name));
    break;
  }
#else // PROJECT_CHAR
  switch(wdog_state[wd_ch].resetting_disabled)
  {
  case 1:
    log_printf("Сторож: кан.%d%s - достигнут лимит повторных сбросов (%d). Сбросы приостановлены.",
               wd_ch + 1,
               quoted_name(setup->name),
               setup->doubling_pause_resets);
    break;
  case 0:
    log_printf("Сторож: кан.%d%s - получен ответ. Приостановка сбросов отменена.",
               wd_ch + 1,
               quoted_name(setup->name));
    break;
  }
#endif // PROJECT_CHAR=='E'
#ifdef SMS_MODULE
  if(sms_setup.event_mask & SMS_EVENT_PWR)
  {
    switch(wdog_state[wd_ch].resetting_disabled)
    {
    case 1:
      sms_msg_printf("WDOG %u%s CEASED AFTER %u FAILED RESETS",
         wd_ch + 1,
         quoted_name(setup->name),
         setup->doubling_pause_resets
      );
      break;
    case 0:
      sms_msg_printf("WDOG %u%s GOT REPLY, RESTORED",
         wd_ch + 1,
         quoted_name(setup->name)
      );
      break;
    }
  }
#endif  // SMS
}


void wdog_log_reset(unsigned wd_ch)
{
  struct wdog_setup_s *setup = &wdog_setup[wd_ch];
  struct ping_state_s *pingch = &ping_state[wd_ch * 3];
  // prepare ip texts
  char ips[3][16];
  str_ip_to_str(setup->ip0, ips[0]);
  str_ip_to_str(setup->ip1, ips[1]);
  str_ip_to_str(setup->ip2, ips[2]);
  // z-term protection before sprintf(), it's pasc+zterm strings
  setup->hostname0[sizeof setup->hostname0 - 1] = 0;
  setup->hostname1[sizeof setup->hostname1 - 1] = 0;
  setup->hostname2[sizeof setup->hostname2 - 1] = 0;
#if PROJECT_CHAR=='E'
  log_printf("Watchdog: reset of chan.%d%s. A (%s) %s, B (%s) %s, C (%s) %s.",
#else
  log_printf("Сторож: сброс кан.%d%s. A (%s) %s, B (%s) %s, C (%s) %s.",
#endif
     wd_ch + 1, quoted_name(setup->name),
     setup->hostname0[0] ? (char*)setup->hostname0 + 1 : ips[0], ip_status(&pingch[0]),
     setup->hostname1[0] ? (char*)setup->hostname1 + 1 : ips[1], ip_status(&pingch[1]),
     setup->hostname2[0] ? (char*)setup->hostname2 + 1 : ips[2], ip_status(&pingch[2])
  );
#ifdef SMS_MODULE
  if(sms_setup.event_mask & SMS_EVENT_PWR)
    sms_msg_printf("WDOG %u%s RESET", wd_ch+1, quoted_name(setup->name));
#endif
}

int check_reset_logic(unsigned wd_ch)
{
   unsigned active_mask = wdog_setup[wd_ch].active;
   struct ping_state_s *pingch = &ping_state[wd_ch * 3];

   switch(wdog_setup[wd_ch].logic_mode)
   {
   case A_and_B_and_C: // сброс питания  в случае, если недоступен любой из адресов (A)(B)(C).

     if(active_mask&1 && pingch[0].result==0) return 1;
     if(active_mask&2 && pingch[1].result==0) return 1;
     if(active_mask&4 && pingch[2].result==0) return 1;
     return 0;

   case A_or_B_or_C: //  сброс питания в случае одновременной недоступности всех Ip-адресов

     if(active_mask&1 && pingch[0].result==1) return 0;
     if(active_mask&2 && pingch[1].result==1) return 0;
     if(active_mask&4 && pingch[2].result==1) return 0;
     if(active_mask&7) return 1;
     return 0;

   case A_or_B_and_C: // сброс питания в случае, если одновременно недоступен адрес (А) и недоступен любой из адресов (B) или (C)

     if(! active_mask&1) return 0; // disabled A is always OK, so no resets // 52.3.1
     if(active_mask&1 && pingch[0].result==1) return 0;

     if(active_mask&2 &&  pingch[1].result==0) return 1;
     if(active_mask&4 &&  pingch[2].result==0) return 1;
     return 0;

   case A_not_B_or_C: // сброс питания в случае, если недоступен адрес (А) но доступен любой из адресов (B) или (C)

     if(!(active_mask&1)) return 0; // disabled A is always OK, so no resets
     if(active_mask&1 && pingch[0].result==1) return 0;

     if(!(active_mask&6)) return 1; // if both B and C disabled, check only A
     if(active_mask&2 &&  pingch[1].result==1) return 1;
     if(active_mask&4 &&  pingch[2].result==1) return 1;
     return 0; // if both B and C not available, no reset (this case is: A unavailable due to network fault)

   default:
      return 0;
   }
}

void wdog_exec(void)
{
  struct wdog_state_s *state = &wdog_state[wdog_ch]; // scan one ch per main loop cycle
  struct wdog_setup_s *setup = &wdog_setup[wdog_ch];
  systime_t time = sys_clock();

  unsigned ping_mask = setup->active << (wdog_ch * 3); // active resets if 0.0.0.0 or wrong ip is saved, in wdog_restart()

  // postpone next ping to relay switching on moment + reboot time
  if(pingable_by_wdog(wdog_ch) == 0)
  {
    ping_reset |= 7 << (wdog_ch * 3); // keep all 3 ping channels in reset
    state->pings_in_progress = 0;
    state->next_ping_time = state->reboot_end_time = time + setup->reboot_pause * 1000;
  }

  // do poll // DNS issues resolved in pong.c v2.5-52 (non-valid ip -> fail)
  if(ping_mask != 0 && state->pings_in_progress == 0 && time > state->next_ping_time && time > state->reboot_end_time)
  {
    state->next_ping_time = time + setup->poll_period * 1000;
    ++ state->poll_count_after_reset;
    ping_completed &=~ ping_mask;
    struct ping_state_s *ping  = &ping_state[wdog_ch * 3];
    int i;
    for(i=0; i<3; ++i, ++ping)
      memcpy(ping->ip, setup->ip0 + i * 4, 4);
    ping_start |= ping_mask;
    state->pings_in_progress = 1;
    state->ping_mask = ping_mask;
  }

  // check poll results
  if(state->pings_in_progress && (ping_completed & state->ping_mask) == state->ping_mask)
  { // ping poll result handling
    state->pings_in_progress = 0;
    int fail = check_reset_logic(wdog_ch);
    if(fail == 0)
    {
      state->repeating_resets_count = 0;
      if(state->resetting_disabled)
      {
        state->resetting_disabled = 0;
        wdog_log_resetting_state(wdog_ch);
      }
    }
    else
    { // if fail
      if( setup->doubling_pause_resets != 0 && state->repeating_resets_count >= setup->doubling_pause_resets )
      {
        if(state->resetting_disabled == 0)
        {
          state->resetting_disabled = 1;
          wdog_log_resetting_state(wdog_ch);
        }
      }
      if(state->resetting_disabled == 0)
      {
        ++ state->reset_count;
        if(state->poll_count_after_reset == 1)
          ++ state->repeating_resets_count;
        state->poll_count_after_reset = 0;
        // schedule reset
        state->reset_end_time = time + setup->reset_time * 1000;
        state->next_ping_time = state->reboot_end_time = state->reset_end_time + setup->reboot_pause * 1000;
      }
    }// fail
  }// if ping completed

  // generate output from reset/reboot times
  if(time < state->reset_end_time)
  {
    if(state->output != setup->reset_mode)
      wdog_log_reset(wdog_ch); // 4.09.2013, it was (setup->reset_mode)
    state->output = setup->reset_mode;
  }
  else
  {
    state->output = setup->reset_mode ^ 1;
  }

  if(++wdog_ch == WDOG_MAX_CHANNEL) wdog_ch = 0; // scan channels
}

void wdog_restart(void)
{
  struct wdog_setup_s *setup = wdog_setup;
  struct wdog_state_s *state = wdog_state;
  struct ping_state_s *ping  = ping_state;
  int n, i;
  for(n=0; n<WDOG_MAX_CHANNEL; ++n, ++setup, ++state)
  {
    for(i=0; i<3; ++i, ++ping)
    {
      // ip is copied to ping_state[] in exec() before poll - DNS
      ping->max_retry = setup->max_retry;
      ping->timeout = setup->ping_timeout;
    }
    state->pings_in_progress = 0;
    state->output = setup->reset_mode ^ 1;
    state->poll_count_after_reset = 0;
    state->repeating_resets_count = 0;
    state->resetting_disabled = 0;
    state->reset_end_time = 0; // stop reset on wdog restart
    unsigned pause = setup->reboot_pause; // in sec
    if(pause < 8) pause = 8; // min pre-start pause 8s (network up, DNS ops etc.)
    state->next_ping_time = state->reboot_end_time = sys_clock() + pause * 1000;
  }
#if PROJECT_CHAR != 'E'
  log_printf("Cторож: старт/рестарт наблюдения");
#else
  log_printf("Watchdog: (re)start of monitoring");
#endif
}

void wdog_reset_params_ch(unsigned ch)
{
  if(ch >= WDOG_MAX_CHANNEL) return;
  struct wdog_setup_s *su = &wdog_setup[ch];
  memset(su, 0, sizeof *su);
  su->signature    = wdog_signature;
  su->poll_period  = 15;
  su->ping_timeout = 1000;
  su->max_retry    = 8;
  su->reset_time   = 12;
  su->reboot_pause = 15;
  su->logic_mode   = A_or_B_or_C;
  EEPROM_WRITE(&eeprom_wdog_setup[ch], su, sizeof eeprom_wdog_setup[0]);
}

void watchdog_init(void)
{
  struct wdog_setup_s *s;
  int n;
  EEPROM_READ(&eeprom_wdog_setup, wdog_setup, sizeof wdog_setup);
  for(n=0, s=wdog_setup; n<WDOG_MAX_CHANNEL; ++n)
    if(wdog_setup[n].signature != wdog_signature)
      wdog_reset_params_ch(n);
#ifdef DNS_MODULE
  s= wdog_setup;
  for(n=0, s=wdog_setup; n<WDOG_MAX_CHANNEL; ++n, ++s)
  {
    dns_add(s->hostname0, s->ip0);
    dns_add(s->hostname1, s->ip1);
    dns_add(s->hostname2, s->ip2);
  }
#endif
  wdog_restart();
}


int watchdog_event(enum event_e event)
{
  switch(event)
  {
  case E_EXEC:
    wdog_exec();
    break;
  case E_RESET_PARAMS:
    for(int n=0; n<WDOG_MAX_CHANNEL; ++n)
      wdog_reset_params_ch(n);
    break;
  }
  return 0;
}


