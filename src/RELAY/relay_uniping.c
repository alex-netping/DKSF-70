/*
v1.1-70
3.06.2013
  completed rewrite based on dksf48
v1.2-52
12.08.2013
  rewrite for dksf 52.7
  added flip command for url (relay.cgi?r2=f), snmp (set mode = -1)
v1.3-52
7.11.2013
  critical bugfix in relay_save_and_log()
v1.4-70
28.07.2014
  extended, json-p compatible url-encoded API
v1.5-70
17.09.2014
 json-p CGI bugfixes
*/


#include "platform_setup.h"

#ifdef RELAY_MODULE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "plink.h"
#include "eeprom_map.h"

const unsigned relay_signature = 0x63fb7710;

char send_sse_flag;

/*
__no_init unsigned relay_hot_restart;
const unsigned     relay_hot_restart_signature = 0x7096d8fb;
*/

struct relay_setup_s relay_setup[RELAY_MAX_CHANNEL];
unsigned relay_forced_reset_time[RELAY_MAX_CHANNEL];
unsigned char relay_forced_reset_polarity[RELAY_MAX_CHANNEL];
/*__no_init*/ unsigned char relay_state;   // actual pin-level state

unsigned relay_http_sse(unsigned pkt, unsigned unused_more_data);

void relay_forced_reset(unsigned ch, unsigned reset_time_10_ticks_per_sec, int polarity, char const *via)
{
  relay_forced_reset_time[ch] = sys_clock_100ms + reset_time_10_ticks_per_sec;
  relay_forced_reset_polarity[ch] = polarity;
#if PROJECT_CHAR != 'E'
  log_printf("PWR: реле %u%s временно %s на %u.%uс %s",
        ch + 1, quoted_name(relay_setup[ch].name),
        polarity ? "включено" : "выключено",
        reset_time_10_ticks_per_sec / 10,
        reset_time_10_ticks_per_sec % 10, via );
#else
  log_printf("PWR: relay %u%s switched %s for short period of %u.%us %s",
        ch + 1, quoted_name(relay_setup[ch].name),
        polarity ? "on" : "off",
        reset_time_10_ticks_per_sec / 10,
        reset_time_10_ticks_per_sec % 10, via );
#endif
}

int get_relay_state(unsigned ch, int apply_wdog_reset, int apply_forced_reset)
{
  if(ch >= RELAY_MAX_CHANNEL) return 0;
  int output = 0;
  int reset_mode;
  switch(relay_setup[ch].mode)
  {
  case RELAY_MODE_MANUAL_OFF:
    output = 0;
    break;
  case RELAY_MODE_MANUAL_ON:
    output = 1;
    break;
#ifdef WATCHDOG_MODULE
  case RELAY_MODE_WDOG:
    output = wdog_setup[ch].reset_mode ^ 1;
    if(apply_wdog_reset)
      output = wdog_state[ch].output;
    break;
#endif
#ifdef WTIMER_MODULE
  case RELAY_MODE_SCHED:
    output = wtimer_schedule_output[ch];
    break;
#endif
#if defined(WATCHDOG_MODULE) && defined(WTIMER_MODULE)
  case RELAY_MODE_SCHED_WDOG:
    reset_mode = wdog_setup[ch].reset_mode;
    output = wtimer_schedule_output[ch];
    if(apply_wdog_reset)
      if(output != reset_mode)
        if(wdog_state[ch].output == reset_mode)
          output = reset_mode;
    break;
#endif
#ifdef LOGIC_MODULE
  case RELAY_MODE_LOGIC:
    output = (logic_relay_output >> ch) & 1;
    break;
#endif
  }
  if(apply_forced_reset)
    if(sys_clock_100ms < relay_forced_reset_time[ch])  // forced reset
      output = relay_forced_reset_polarity[ch];
  return output;
}

#ifdef WATCHDOG_MODULE
int pingable_by_wdog(unsigned ch)
{
  int r_state = get_relay_state(ch, 0, 1);
  return r_state != wdog_setup[ch].reset_mode;
}
#endif

int relay_is_controlled_by_schedule(unsigned ch)
{
  switch(relay_setup[ch].mode)
  {
  case RELAY_MODE_SCHED:
  case RELAY_MODE_SCHED_WDOG:
    return 1;
  }
  return 0;
}

unsigned relay_http_get_data(unsigned pkt, unsigned more_data)
{
  char buf[768];
  char *dest = buf;
  if(more_data == 0)
  {
    dest+=sprintf(dest,"var packfmt={");
    PLINK(dest, relay_setup[0], name);
    PLINK(dest, relay_setup[0], mode);
    PLINK(dest, relay_setup[0], reset_time);
    PSIZE(dest, (char*)&relay_setup[1] - (char*)&relay_setup[0]); // must be the last // alignment!
    dest += sprintf(dest, "}; data=[");
  }
  int n;
  struct relay_setup_s *setup = &relay_setup[more_data];
  for(n=more_data; n<RELAY_MAX_CHANNEL; ++n, ++setup)
  {
    if(dest > buf + sizeof buf - 192) break;
    *dest++ = '{';
    PDATA_PASC_STR(dest, (*setup), name);
    PDATA(dest, (*setup), mode);
    PDATA(dest, (*setup), reset_time);
    dest += sprintf(dest, "relay_state:%u},",
               get_relay_state(n, 1, 1));
  }
  --dest; // remove last comma
  *dest++=']'; *dest++=';';
  tcp_put_tx_body(pkt, (unsigned char*)buf, dest - buf);
  return n == RELAY_MAX_CHANNEL ? 0 : n ;
};


void relay_save_and_log(int only_ch, char const *via) // if -1, all channels, -2 - input relays
{
  struct relay_setup_s old_su;
  struct relay_setup_s *setup;
  unsigned ch, sta_ch, end_ch;
  if(only_ch == -1)
  {
    sta_ch = 0;
    end_ch = RELAY_MAX_CHANNEL - 1;
    setup = relay_setup;
  }
  else
  {
    if(only_ch < 0 || only_ch >= RELAY_MAX_CHANNEL) return; // protection
    sta_ch = end_ch = only_ch;
    setup = &relay_setup[only_ch];
  }
  for(ch = sta_ch; ch <= end_ch; ++ch, ++setup)
  {
    EEPROM_READ(&eeprom_relay_setup[ch], &old_su, sizeof old_su);
    if(memcmp(&old_su, setup, sizeof old_su) == 0) continue;

    setup->name[sizeof setup->name - 1] = 0; // z-term protect-n
    EEPROM_WRITE(&eeprom_relay_setup[ch], setup, sizeof eeprom_relay_setup[0]);

#if WDOG_MAX_CHANNEL == RELAY_MAX_CHANNEL
    // unified name for relay and wdog channels
    if(sizeof wdog_setup[0].name == sizeof relay_setup[0].name
    && memcmp(wdog_setup[ch].name, relay_setup[ch].name, sizeof wdog_setup[0].name) != 0 )
    {
      memcpy(wdog_setup[ch].name, relay_setup[ch].name, sizeof wdog_setup[0].name);
      EEPROM_WRITE(eeprom_wdog_setup[ch].name, wdog_setup[ch].name, sizeof eeprom_wdog_setup[0].name);
    }
#endif

    if(setup->mode != old_su.mode)
    {
      #if PROJECT_CHAR != 'E'
      char *s;
      switch(setup->mode)
      {
      case RELAY_MODE_MANUAL_OFF:  s = "выключено вручную"; break;
      case RELAY_MODE_MANUAL_ON:   s = "включено вручную"; break;
      case RELAY_MODE_WDOG:        s = "управление от сторожа"; break;
      case RELAY_MODE_SCHED:       s = "управление по расписанию"; break;
      case RELAY_MODE_SCHED_WDOG:  s = "управление по расписанию и от сторожа"; break;
      case RELAY_MODE_LOGIC:       s = "управление от логики"; break;
      default: s = "unknown"; break;
      }
      log_printf("PWR: реле %u%s переведено в режим \"%s\" %s",
                 ch + 1, quoted_name(setup->name), s, via);
      #else
      char *s;
      switch(setup->mode)
      {
      case RELAY_MODE_MANUAL_OFF:  s = "switched off manually"; break;
      case RELAY_MODE_MANUAL_ON:   s = "switched on manually"; break;
      case RELAY_MODE_WDOG:        s = "watchdog"; break;
      case RELAY_MODE_SCHED:       s = "schedule"; break;
      case RELAY_MODE_SCHED_WDOG:  s = "schedule and watchdog"; break;
      case RELAY_MODE_LOGIC:       s = "logic"; break;
      default: s = "unknown"; break;
      }
      if(setup->mode <= RELAY_MODE_MANUAL_ON)
        log_printf("PWR: relay %u%s %s %s",
                 ch + 1, quoted_name(setup->name), s, via);
      else
        log_printf("PWR: relay %u%s switched %s to be controled by %s",
                 ch + 1, quoted_name(setup->name), via, s);
      #endif
    } // mode
  } // for relay ch
}

int relay_http_set_data(void)
{
  http_post_data((void*)relay_setup, sizeof relay_setup);
  relay_save_and_log(-1, (char*)via_web);
  http_redirect("/relay.html");
  return 0;
}

int relay_http_set_forced_reset(void)
{
  unsigned char data[2];
  data[0] = 0xaa;
  http_post_data(data, sizeof data);
  unsigned ch = data[0];
  unsigned polarity = data[1] ? 1 : 0 ;
  if(ch != 0xaa && ch < RELAY_MAX_CHANNEL)
    relay_forced_reset(ch, relay_setup[ch].reset_time * 10, polarity, via_web);
  http_reply(200,"");
  return 0;
}

unsigned relay_http_get_cgi(unsigned pkt, unsigned more_data)
{
  char *p = req_args;
  struct relay_setup_s *setup;
  char *result = "relay_result('error');" ;
  char *ok = "relay_result('ok');" ;
  char buf[64];
  char m, a;
  unsigned ch;
  if(*p++ != 'r') goto end;
  ch = *p++ - '1';
  if(ch >= RELAY_MAX_CHANNEL) goto end;
  setup = &relay_setup[ch];
  a = *p++;
  if(a == 0)
  {
    sprintf(buf, "relay_result('ok', %u, %u);", setup->mode, (relay_state >> ch) & 1);
    result = buf;
  }
  else if(a == '=')
  {
    m = *p++;
    if(m >= '0' && m < '0' + (unsigned)RELAY_MAX_USED_MODE )
    {
      if(*p != 0) goto end;
      setup->mode = (enum relay_mode_e)(m - '0');
      relay_save_and_log(ch, via_url);
      result = ok;
    }
    else if(m == 'f')
    {
      if(*p == 0)
      {
        if((unsigned)relay_setup[ch].mode > 1) goto end; // if not manual
        setup->mode = (enum relay_mode_e)(!(unsigned)setup->mode); // flip
        relay_save_and_log(ch, via_url);
        result = ok;
      }
      else if(*p == ',')
      {
        ++p; // skip ,
        unsigned t;
        t = atoi(p);
        if(t == 0 || t > 1800) goto end;
        relay_forced_reset(ch, t * 10, !get_relay_state(ch,0,0), via_url);
        result = ok;
      }
      else goto end;
    }
    else goto end;
  } // after '='
end:
  tcp_put_tx_body(pkt, (void*)result, strlen(result));
  return 0;
}

HOOK_CGI(relay_get,    (void*)relay_http_get_data,  mime_js,  HTML_FLG_GET | HTML_FLG_NOCACHE );
HOOK_CGI(relay_set,    (void*)relay_http_set_data,  mime_js,  HTML_FLG_POST );
HOOK_CGI(relay_reset,  (void*)relay_http_set_forced_reset, mime_js,  HTML_FLG_POST );
HOOK_CGI(relay,        (void*)relay_http_get_cgi,   mime_js,  HTML_FLG_GET | HTML_FLG_NOCACHE );

int relay_snmp_get(unsigned id, unsigned char *data)
{
  int val = 0;

  unsigned ch = snmp_data.index - 1;
  switch(id)
  {
  case 0x5501: // npRelayN
    if(ch >= RELAY_MAX_CHANNEL) return SNMP_ERR_NO_SUCH_NAME;
    val = ch + 1;
    break;
  case 0x5502: // npRelayMode
    if(ch >= RELAY_MAX_CHANNEL) return SNMP_ERR_NO_SUCH_NAME;
    val = (int)relay_setup[ch].mode;
    break;
  case 0x5503: // npRelayStartReset
    if(ch >= RELAY_MAX_CHANNEL) return SNMP_ERR_NO_SUCH_NAME;
    val = 0;
    break;
  case 0x5506: // npRelayMemo, display string
    if(ch >= RELAY_MAX_CHANNEL) return SNMP_ERR_NO_SUCH_NAME;
    snmp_add_asn_obj(SNMP_TYPE_OCTET_STRING, relay_setup[ch].name[0], &relay_setup[ch].name[1] );
    return 0;
  case 0x550e: // npRelayFlip (not in MIB yet)
   if(ch >= RELAY_MAX_CHANNEL) return SNMP_ERR_NO_SUCH_NAME;
   val = 0;
   break;
  case 0x550f: // npRelayState
    if(ch >= RELAY_MAX_CHANNEL) return SNMP_ERR_NO_SUCH_NAME;
    val = (relay_state >> ch) & 1;
    break;
  default:
#ifndef WATCHDOG_MODULE
    return SNMP_ERR_NO_SUCH_NAME;
#else
    switch(id & 0xfffffff0)
    {
    case 0x5810: // P_CHANNEL_N
      val = ch + 1; // idexes is 1-based!
      break;
    case 0x5820: // P_START_RESET - i.e., reset status
      if(ch >= WDOG_MAX_CHANNEL) return SNMP_ERR_NO_SUCH_NAME;
      if(wdog_state[ch].output == wdog_setup[ch].reset_mode) val = 1; // reset active
      else if(sys_clock() <= wdog_state[ch].reboot_end_time) val = 2; // reset is over, reboot active
      else val = 0;
      break;
    case 0x5830: // P_MANUAL_MODE
      if(ch >= RELAY_MAX_CHANNEL) return SNMP_ERR_NO_SUCH_NAME;
      val = (int)relay_setup[ch].mode;
      break;
    case 0x5840: // P_RESET_COUNTER
      if(ch >= WDOG_MAX_CHANNEL) return SNMP_ERR_NO_SUCH_NAME;
      val = wdog_state[ch].reset_count;
      break;
    case 0x5850: // P_REPEATING_RESETS_COUNTER
      if(ch >= WDOG_MAX_CHANNEL) return SNMP_ERR_NO_SUCH_NAME;
      val = wdog_state[ch].repeating_resets_count;
      break;
    case 0x5860: // P_MEMO
      if(ch >= RELAY_MAX_CHANNEL) return SNMP_ERR_NO_SUCH_NAME;
      snmp_add_asn_obj(SNMP_TYPE_OCTET_STRING, relay_setup[ch].name[0], relay_setup[ch].name + 1);
      return 0; // exit here, don't add val!
    case 0x58e0: // npPwrRelayFlip
      val = 0;
      break;
    case 0x58f0: // npPwrRelayState
      if(ch >= RELAY_MAX_CHANNEL) return SNMP_ERR_NO_SUCH_NAME;
      val = (relay_state >> ch) & 1;
      break;
    default:
      return SNMP_ERR_NO_SUCH_NAME;
    } // switch legacy table
#endif // defined WATCHDOG_MODULE
  } // switch modern relay table

  snmp_add_asn_integer(val);
  return 0;
}

int relay_snmp_set(unsigned id, unsigned char *data)
{
  unsigned ch = snmp_data.index - 1; // index in resource id is 1-based
  // read vbind value
  int val = 0;
  if((id & 0xfffffff0) != 0x5860)
  { // except MEMO, all other variables are INTEGER
    if(*data != 0x02) return SNMP_ERR_BAD_VALUE; // not INTEGER
    data += asn_get_integer(data, &val);
  }

  switch(id)
  {
  case 0x5501: // npRelayN
    if(ch >= RELAY_MAX_CHANNEL) return SNMP_ERR_NO_SUCH_NAME;
    return SNMP_ERR_READ_ONLY;
  case 0x5502: // npRelayMode
    if(ch >= RELAY_MAX_CHANNEL) return SNMP_ERR_NO_SUCH_NAME;
    if(val == -1)
    { // flip command
      switch(relay_setup[ch].mode)
      {
      case RELAY_MODE_MANUAL_OFF: val = 1; break;
      case RELAY_MODE_MANUAL_ON:  val = 0; break;
      default: return 0;
      }
    }
    if(val >= 0 && val < (unsigned)RELAY_MAX_USED_MODE)
    {
      relay_setup[ch].mode = (enum relay_mode_e)val;
    }
    else
      return SNMP_ERR_BAD_VALUE;
    relay_save_and_log(ch, via_snmp);
    break;
  case 0x5503: // npRelayStartReset
    if(ch >= RELAY_MAX_CHANNEL) return SNMP_ERR_NO_SUCH_NAME;
    if(val == 1) relay_forced_reset(ch, relay_setup[ch].reset_time * 10, 0, via_snmp);
    break;
  case 0x5506: // npRelayMemo, display string
    return SNMP_ERR_READ_ONLY;
  case 0x550e: // npRelayFlip (not in MIB yet!)
    if(ch >= RELAY_MAX_CHANNEL) return SNMP_ERR_NO_SUCH_NAME;
    if(val != -1) return SNMP_ERR_BAD_VALUE;
    switch(relay_setup[ch].mode)
    {
      case RELAY_MODE_MANUAL_OFF: val = 1; break;
      case RELAY_MODE_MANUAL_ON:  val = 0; break;
      default: return 0; // do nothing
    }
    relay_setup[ch].mode = (enum relay_mode_e)val;
    relay_save_and_log(ch, via_snmp);
    val = -1; // return the same as was passed to set
    break;
  case 0x550f: // npRelayState
    return SNMP_ERR_READ_ONLY;

  default:
#ifndef WATCHDOG_MODULE
    return SNMP_ERR_NO_SUCH_NAME;
#else
    switch(id & 0xfffffff0) // legacy table, ch also encoded id id as bits 0..3
    {
    case 0x5820:  // P_START_RESET
      // snmp SET any value, i.e. no validation
      if(ch >= RELAY_MAX_CHANNEL) return SNMP_ERR_NO_SUCH_NAME;
      unsigned ticks;
      int pol;
      ticks = relay_setup[ch].reset_time * 10; // from s to 100ms ticks
      pol = 0;
      if(ch < WDOG_MAX_CHANNEL)
      {
        ticks = wdog_setup[ch].reset_time / 100; // from ms to 100ms ticks
        pol = wdog_setup[ch].reset_mode;
      }
      if(ticks < 10) ticks = 10;
      relay_forced_reset(ch, ticks, pol, via_snmp);
      break;
    case 0x5830:  // P_MANUAL_MODE
      if(ch >= RELAY_MAX_CHANNEL) return SNMP_ERR_NO_SUCH_NAME;
      if(val >= 0 && val < (unsigned)RELAY_MAX_USED_MODE)
      {
        relay_setup[ch].mode = (enum relay_mode_e)val;
      }
      else
        return SNMP_ERR_BAD_VALUE;
      relay_save_and_log(ch, via_snmp);
      break;
    case 0x5840: // P_RESET_COUNTER
      // write any value to reset counter
      if(ch >= WDOG_MAX_CHANNEL) return SNMP_ERR_NO_SUCH_NAME;
      wdog_state[ch].reset_count = 0;
      wdog_state[ch].repeating_resets_count = 0;
      val = 0; // return 0 as value
      break;
    case 0x58e0: // flip relay command
      if(ch >= RELAY_MAX_CHANNEL) return SNMP_ERR_NO_SUCH_NAME;
      if(val != -1) return SNMP_ERR_BAD_VALUE;
      switch(relay_setup[ch].mode)
      {
      case RELAY_MODE_MANUAL_OFF: val = 1; break;
      case RELAY_MODE_MANUAL_ON:  val = 0; break;
      default: return 0; // do nothing
      }
      relay_setup[ch].mode = (enum relay_mode_e)val;
      relay_save_and_log(ch, via_snmp);
      val = -1; // return the same as was passed to set
      break;
    default:
      return SNMP_ERR_READ_ONLY;
    } // switch legacy table
#endif // defined WATCHOG_MODULE
  } // switch modern relay table

  snmp_add_asn_integer(val);
  return 0;
}

void relay_param_reset(void)
{
  memset(relay_setup, 0, sizeof relay_setup);
  for(int n=0; n<RELAY_MAX_CHANNEL; ++n)
  {
    relay_setup[n].reset_time = 15;
    relay_setup[n].mode = RELAY_MODE_MANUAL_ON;
  }
  EEPROM_WRITE(eeprom_relay_setup, relay_setup, sizeof relay_setup);
  EEPROM_WRITE(&eeprom_relay_signature, &relay_signature, sizeof eeprom_relay_signature);
}

void relay_exec(void)
{
  // control routing
  unsigned n, output = 0;
  for(n=0; n<RELAY_MAX_CHANNEL; ++n)
  {
    output = get_relay_state(n, 1, 1);
    unsigned old = relay_state;
    if(output) relay_state |=  (1<<n);
    else       relay_state &=~ (1<<n);
    if(relay_state != old)
    {
      relay_pin(n, output);
#if PROJECT_LETTER != 'E'
      notify(relay_notify[n].on_off, "PWR: реле %u%s %s",
          n+1, quoted_name(relay_setup[n].name), output ? "включено" : "выключено");
#else
      notify(relay_notify[n].on_off, "PWR: relay %u%s switched %s",
          n+1, quoted_name(relay_setup[n].name), output ? "on" : "off");
#endif
      send_sse_flag = 1;
    }
  } // for

  if(send_sse_flag && http_can_send_sse())
  {
    unsigned pkt = tcp_create_packet_sized(sse_sock, 256);
    if(pkt != 0xff)
    {
      send_sse_flag = 0;
      char *p = tcp_ref(pkt, 0);
      unsigned len = sprintf(p, "retry: 1250\n""event: relay_state\n""data: %u\n\n", relay_state);
      tcp_send_packet(sse_sock, pkt, len);
    }
  }
///  relay_hot_restart = relay_hot_restart_signature;
}

/*
void relay_early_init(void)
{
  // fast restore of relay state on init
  if(relay_hot_restart == relay_hot_restart_signature)
  {
    for(int n=0; n<RELAY_MAX_CHANNEL; ++n)
      relay_pin(n, (relay_state >> n) & 1);
  }
  else
  {
      relay_state = 0;
  }
  relay_pin_init();
}
*/

void relay_init(void)
{
  unsigned sign;
  EEPROM_READ(&eeprom_relay_signature, &sign, sizeof sign);
  if(sign != relay_signature)
    relay_param_reset();
  EEPROM_READ(eeprom_relay_setup, relay_setup, sizeof relay_setup);
  // hw is initalized via relay_early_init() call early in main()
  relay_pin_init();
}

void relay_event(enum event_e event)
{
  switch(event)
  {
  case E_EXEC:
    relay_exec();
    break;
  case E_INIT:
    relay_init();
    break;
  case E_RESET_PARAMS:
    relay_param_reset();
    break;
  }
}

#endif
