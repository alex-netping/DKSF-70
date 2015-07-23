/*
Relative humidity sensing
SHT1x (FOST02) sensor, 1W sensor since v2.0

P.Lyubasov
v1.1
20.03.2010
v1.2
5.07.2010
 bug if below -40C corrected
v1.3
14.02.2011
  minor timing
v1.4
17.04.2012
  cosmetic rewrite and relhum_cancel()
v1.5
11.11.2012
  RH safe range notification, parameters
v1.5-60
26.04.2013
  cpu unload
v2.0-70
24.01.2014
  1w version
  driver moved to ow.c
v2.1-70
  hr_status_check() call moved from ow_scan() in ow.c to rh_exec()
v2.2-70
15.05.2014
  notify support
v2.3-70
30.07.2014
  url-encoded api (json-p)
v2.4-70
24.11.2014
  JSON-P returns relhum_result(...) instead of rh_result(...)
v3.0
22.06.2015
  multiple 1w sensors
*/

#include "platform_setup.h"
#include "eeprom_map.h"
#include "plink.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define RH_IDLE_PERIOD 6000 // ms

static const unsigned char rh_hyst = 1;
static const unsigned char t_hyst = 1;

//const unsigned relhum_signature = 561132741;
const unsigned relhum_signature = 561132753; // v3

struct relhum_setup_s relhum_setup[RELHUM_MAX_CH];
struct relhum_state_s relhum_state[RELHUM_MAX_CH];

void relhum_send_trap_h(unsigned ch);
void relhum_send_trap_t(unsigned ch);

void rh_check_status(unsigned ch)
{
  if(ch >= RELHUM_MAX_CH) return;
  struct relhum_setup_s *su = &relhum_setup[ch];
  struct relhum_state_s *st = &relhum_state[ch];
  struct relhum_notify_s *nf = &relhum_notify[ch];
  int a, b;
#if PROJECT_CHAR != 'E'
  static const char * const status_txt[4] = {"отказ","ниже нормы","в норме","выше нормы"};
#else
  static const char * const status_txt[4] = {"failed","below safe","safe","above safe"};
#endif

  int hs = st->rh_status;
  if(st->error == 0)
  {
    switch(st->rh_status)
    {
    case 0: a =  0; b =  0; break;
    case 1: a =  1; b =  1; break;
    case 2: a = -1; b =  1; break;
    case 3: a = -1; b = -1; break;
    }
    a = su->rh_low + a * rh_hyst;
    b = su->rh_high + b * rh_hyst;
    if(st->rh < a) hs = 1;
    else if(st->rh <= b) hs = 2;
    else hs = 3;
  }

  int ts = st->t_status;
  if(st->error == 0)
  {
    switch(st->t_status)
    {
    case 0: a =  0; b =  0; break;
    case 1: a =  1; b =  1; break;
    case 2: a = -1; b =  1; break;
    case 3: a = -1; b = -1; break;
    }
    a = su->t_low + a * t_hyst;
    b = su->t_high + b * t_hyst;
    if(st->t < a) ts = 1;
    else if(st->t <= b) ts = 2;
    else ts = 3;
  }

  if(st->error && st->rh_status != 0)
  { // sensor just failed
    st->rh_status = 0;
    st->t_status = 0;
    st->rh = 0;
    st->t = 0;
    notify(nf->fail,
#if PROJECT_CHAR != 'E'
      "Датчик влажности %u%s - отказ",
#else
      "Humidity %u%s - failed",
#endif
       ch + 1, quoted_name(su->name)
    );
#ifdef SMS_MODULE
    if(nf->fail & NOTIFY_SMS)
      if(sys_setup.nf_disable == 0)
        sms_msg_printf("RH%u%s FAILED", ch+1, quoted_name(su->name));
#endif
    if(nf->fail & NOTIFY_TRAP)
    {
      relhum_send_trap_h(ch);
      relhum_send_trap_t(ch);
    }
    return;
  }

  if(st->rh_status == 0 && st->error == 0)
  { // sensor just restored
    st->rh_status = hs;
    st->t_status = ts;
    notify(nf->fail,
#if PROJECT_CHAR != 'E'
      "Датчик влажности %u%s работает, %u%% (%s %u..%u), %d град.C (%s %d..%d)",
#else
      "Humidity %u%s is working, %u%% (%s %u..%u), %d deg.C (%s %d..%d)",
#endif
       ch + 1, quoted_name(su->name),
       st->rh,  status_txt[hs], su->rh_low, su->rh_high,
       st->t, status_txt[ts], su->t_low, su->t_high
    );
#ifdef SMS_MODULE
    if(nf->fail & NOTIFY_SMS)
      if(sys_setup.nf_disable == 0)
        sms_msg_printf("RH%u%s is OK, %u%% %s, %dC %s",
          ch+1, quoted_name(su->name),
          st->rh, status_txt[hs],
          st->t, status_txt[ts]
        );
#endif
    if(nf->fail & NOTIFY_TRAP)
    {
      relhum_send_trap_h(ch);
      relhum_send_trap_t(ch);
    }
    return;
  }

  if(hs != st->rh_status)
  {
    unsigned mask;
    if(hs == 1) mask = nf->h_low;
    else if(hs == 2) mask = nf->h_norm;
    else mask = nf->h_high;
    notify(mask,
#if PROJECT_CHAR != 'E'
       "Датчик влажности %u%s - %u%% (%s %u..%u%%)",
#else
       "Humidity %u%s - %u%% (%s %u..%u%%)",
#endif
        ch + 1, quoted_name(su->name),
        st->rh, status_txt[hs], su->rh_low, su->rh_high
     );
#ifdef SMS_MODULE
    if(mask & NOTIFY_SMS)
      if(sys_setup.nf_disable == 0)
        sms_msg_printf("RH%u%s %u%% (%s %u..%u%%)",
          ch+1, quoted_name(su->name),
          st->rh, status_txt[hs], su->rh_low, su->rh_high
        );
#endif
    st->rh_status = hs; // save new status before trap
    if(mask & NOTIFY_TRAP)
      relhum_send_trap_h(ch);
  }

  if(ts != st->t_status)
  {
    unsigned mask;
    if(ts == 1) mask = nf->t_low;
    else if(ts == 2) mask = nf->t_norm;
    else mask = nf->t_high;
    notify(mask,
#if PROJECT_CHAR != 'E'
       "Датчик влажности %u%s - температура %dC (%s %u..%uC)",
#else
       "Humidity %u%s - temperature %dC (%s %d..%dC)",
#endif
        ch + 1, quoted_name(su->name),
        st->t, status_txt[ts], su->t_low, su->t_high
     );
#ifdef SMS_MODULE
    if(mask & NOTIFY_SMS)
      if(sys_setup.nf_disable == 0)
        sms_msg_printf("RH%u%s %dC (%s %d..%dC)",
          ch+1, quoted_name(su->name),
          st->t, status_txt[ts], su->t_low, su->t_high
        );
#endif
    st->t_status = ts;  // save new status before trap
    if(mask & NOTIFY_TRAP)
      relhum_send_trap_t(ch);
  }
}

void relhum_param_reset(void)
{
  memset(relhum_setup, 0, sizeof relhum_setup);
  struct relhum_setup_s *rh = relhum_setup;
  for(int n=0; n<RELHUM_MAX_CH; ++n, ++rh)
  {
    rh->rh_high = 85;
    rh->rh_low  = 5;
    rh->t_low   = 10;
    rh->t_high  = 60;
  }
  EEPROM_WRITE(&eeprom_relhum_setup, &relhum_setup, sizeof eeprom_relhum_setup);
  EEPROM_WRITE(&eeprom_relhum_signature, &relhum_signature, sizeof eeprom_relhum_signature);
}

void relhum_init(void)
{
  unsigned sign;
  EEPROM_READ(&eeprom_relhum_signature, &sign, sizeof sign);
  if(sign != relhum_signature) relhum_param_reset();
  EEPROM_READ(&eeprom_relhum_setup, &relhum_setup, sizeof relhum_setup);
}

int relhum_snmp_get(unsigned id, unsigned char *data)
{
  unsigned ch = snmp_data.index - 1;
  if(ch >= RELHUM_MAX_CH) return SNMP_ERR_NO_SUCH_NAME;
  struct relhum_setup_s *su = &relhum_setup[ch];
  struct relhum_state_s *st = &relhum_state[ch];
  int val = 0;
  switch(snmp_data.id & 0xfffffff0)
  {
  case 0x8410: val = ch + 1; break;
  case 0x8420: val = st->rh; break;
  case 0x8430: val = st->rh_status; break;
  case 0x8440: val = st->t; break;
  case 0x8450: val = st->t_status; break;
  case 0x8460: snmp_add_asn_obj(SNMP_TYPE_OCTET_STRING, su->name[0], su->name + 1); return 0;
  case 0x8470: val = su->rh_high; break;
  case 0x8480: val = su->rh_low; break;
  case 0x8490: val = su->t_high; break;
  case 0x84a0: val = su->t_low; break;
  default: return SNMP_ERR_NO_SUCH_NAME;
  }
  snmp_add_asn_integer(val);
  return 0;
}

unsigned char relhum_oid[]=
// .1.3.6.1.4.1.25728.8400.0.0.0
{0x2b, 6, 1, 4, 1, 0x81, 0xc9, 0x00, 0xc1, 0x50,
0, 0, 0};


void relhum_add_vbind_integer(int suffix1, int suffix2, int suffix3, int value)
{
  relhum_oid[sizeof relhum_oid - 3] = suffix1;
  relhum_oid[sizeof relhum_oid - 2] = suffix2;
  relhum_oid[sizeof relhum_oid - 1] = suffix3;
  unsigned seq_ptr = snmp_add_seq();
  snmp_add_asn_obj(SNMP_OBJ_ID, sizeof relhum_oid, relhum_oid);
  snmp_add_asn_integer(value);
  snmp_close_seq(seq_ptr);
}

void relhum_make_trap_h(unsigned ch)
{
  relhum_oid[sizeof relhum_oid - 3] = 6; // h trap
  relhum_oid[sizeof relhum_oid - 2] = (relhum_notify[ch].flags & NOTIFY_COMMON_ALL_CHANNELS) ? 127 : 100 + relhum_state[ch].rh_status;
  relhum_oid[sizeof relhum_oid - 1] = (relhum_notify[ch].flags & NOTIFY_COMMON_ALL_EVENTS) ? 99 : ch + 1;

  snmp_create_trap_v2(sizeof relhum_oid, relhum_oid);
  if(snmp_ds == 0xff) return; // if can't create packet

  relhum_add_vbind_integer(3, 1, 0, ch + 1); // npRelHumTrapDataN
  relhum_add_vbind_integer(3, 2, 0, relhum_state[ch].rh); // npRelHumTrapDataValue
  relhum_add_vbind_integer(3, 4, 0, relhum_state[ch].rh_status); // npRelHumTrapDataStatus

  unsigned seq_ptr = snmp_add_seq();
  relhum_oid[sizeof relhum_oid - 2] = 6; // npRelHumTrapDataName
  snmp_add_asn_obj(SNMP_OBJ_ID, sizeof relhum_oid, relhum_oid);
  snmp_add_asn_obj(SNMP_TYPE_OCTET_STRING, relhum_setup[ch].name[0], relhum_setup[ch].name+1);
  snmp_close_seq(seq_ptr);

  relhum_add_vbind_integer(3, 7, 0, relhum_setup[ch].rh_high); // npRelHumTrapDataSafeRangeHigh
  relhum_add_vbind_integer(3, 8, 0, relhum_setup[ch].rh_low); // npRelHumTrapDataSafeRangeLow
}

void relhum_make_trap_t(unsigned ch)
{
  relhum_oid[sizeof relhum_oid - 3] = 7; // t trap
  relhum_oid[sizeof relhum_oid - 2] = (relhum_notify[ch].flags & NOTIFY_COMMON_ALL_CHANNELS) ? 127 : 100 + relhum_state[ch].t_status;
  relhum_oid[sizeof relhum_oid - 1] = (relhum_notify[ch].flags & NOTIFY_COMMON_ALL_EVENTS) ? 99 : ch + 1;

  snmp_create_trap_v2(sizeof relhum_oid, relhum_oid);
  if(snmp_ds == 0xff) return; // if can't create packet

  relhum_add_vbind_integer(3, 1, 0, ch + 1); // npRelHumTrapDataN
  relhum_add_vbind_integer(3, 2, 0, relhum_state[ch].t); // npRelHumTrapDataValue
  relhum_add_vbind_integer(3, 4, 0, relhum_state[ch].t_status); // npRelHumTrapDataStatus

  unsigned seq_ptr = snmp_add_seq();
  relhum_oid[sizeof relhum_oid - 2] = 6; // npRelHumTrapDataName
  snmp_add_asn_obj(SNMP_OBJ_ID, sizeof relhum_oid, relhum_oid);
  snmp_add_asn_obj(SNMP_TYPE_OCTET_STRING, relhum_setup[ch].name[0], relhum_setup[ch].name+1);
  snmp_close_seq(seq_ptr);

  relhum_add_vbind_integer(3, 7, 0, relhum_setup[ch].t_high); // npRelHumTrapDataSafeRangeHigh
  relhum_add_vbind_integer(3, 8, 0, relhum_setup[ch].t_low); // npRelHumTrapDataSafeRangeLow
}

void relhum_send_trap_h(unsigned ch)
{
  relhum_make_trap_h(ch); snmp_send_trap(sys_setup.trap_ip1);
  relhum_make_trap_h(ch); snmp_send_trap(sys_setup.trap_ip2);
}

void relhum_send_trap_t(unsigned ch)
{
  relhum_make_trap_t(ch); snmp_send_trap(sys_setup.trap_ip1);
  relhum_make_trap_t(ch); snmp_send_trap(sys_setup.trap_ip2);
}

unsigned relhum_http_get_status(unsigned pkt, unsigned more_data)
{
  char buf[768];
  char *dest = buf;
  int n;
  struct relhum_state_s *st = relhum_state;
  *dest++ = '('; *dest++ = '[';
  for(n=0; n<RELHUM_MAX_CH && dest < buf + 600; ++n, ++st)
  {
    dest += sprintf(dest, "{rh:%u,rh_status:%u,t:%d,t_status:%u},",
               st->rh, st->rh_status, st->t, st->t_status );
  }
  --dest; *dest++ = ']'; *dest++= ')';
  tcp_put_tx_body(pkt, (unsigned char*)buf, dest - buf);
  return 0; // no more data
}

unsigned relhum_http_get(unsigned pkt, unsigned more_data)
{
  char buf[768];
  char *dest = buf;
  dest += sprintf(dest,"var packfmt={");
  PLINK(dest, relhum_setup[0], name);
  PLINK(dest, relhum_setup[0], ow_addr);
  PLINK(dest, relhum_setup[0], rh_high);
  PLINK(dest, relhum_setup[0], rh_low);
  PLINK(dest, relhum_setup[0], t_high);
  PLINK(dest, relhum_setup[0], t_low);
  PSIZE(dest, sizeof relhum_setup[0]); // must be the last // alignment!
  dest += sprintf(dest, "}; var data=[");
  struct relhum_setup_s *su = &relhum_setup[more_data];
  int n;
  for(n = more_data;; ++su)
  {
    *dest++ = '{';
    PDATA_PASC_STR(dest, *su, name);
    PDATA_OW_ADDR(dest, *su, ow_addr);
    PDATA(dest, *su, rh_high);
    PDATA(dest, *su, rh_low);
    PDATA(dest, *su, t_high);
    PDATA(dest, *su, t_low);
    --dest; // remove last comma
    *dest++='}'; *dest++=',';
    if(++n == RELHUM_MAX_CH)
    {
      --dest; // remove last comma
      *dest++ = ']'; *dest++ = ';';
      n = 0;
      break;
    }
    if(dest > buf + sizeof buf - 200)
      break;
  }
  tcp_put_tx_body(pkt, (unsigned char*)buf, dest - buf);
  return n;
}

unsigned relhum_http_get_cgi(unsigned pkt, unsigned more_data)
{
  char buf[128];
  struct relhum_state_s *st;

  strcpy(buf, "relhum_result('error');");
  unsigned ch = atoi(req_args + 1);
  if(ch >= RELHUM_MAX_CH) goto end;
  st = &relhum_state[ch];
  if(req_args[0] == 'h')
  {
    sprintf(buf, "relhum_result('ok', %u, %u);", st->rh, st->rh_status);
  }
  else if(req_args[0] == 't')
  {
    sprintf(buf, "relhum_result('ok', %d, %u);", st->t, st->t_status);
  }
end:
  tcp_put_tx_body(pkt, (unsigned char*)buf, strlen(buf));
  return 0;
}

int relhum_http_set(void)
{
  http_post_data((void*)&relhum_setup, sizeof relhum_setup);
  EEPROM_WRITE(&eeprom_relhum_setup, &relhum_setup, sizeof eeprom_relhum_setup);
#ifdef OW_MODULE
  ow_restart();
#endif
  http_redirect("/rh.html");
  return 0;
}

HOOK_CGI(rh_stat_get, relhum_http_get_status, mime_js, HTML_FLG_GET | HTML_FLG_NOCACHE );
HOOK_CGI(relhum_get,  relhum_http_get,        mime_js, HTML_FLG_GET | HTML_FLG_NOCACHE );
HOOK_CGI(relhum,      relhum_http_get_cgi,    mime_js, HTML_FLG_GET | HTML_FLG_NOCACHE );
HOOK_CGI(relhum_set,  relhum_http_set,        mime_js, HTML_FLG_POST );

void relhum_event(enum event_e event)
{
  switch(event)
  {
  case E_RESET_PARAMS:
    relhum_param_reset();
    break;
  }
}

#warning *** remove debugggg *** this is legacy shim! *****

unsigned rh_real_h, rh_status_h;
int rh_real_t;

#warning ************ make En page
#warning *********  rewrite relhum HTTP API doc
#warning *********** check relhum.html save_notify() data forming, reserved and flags

