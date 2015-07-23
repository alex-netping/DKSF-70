/*
v1.1
6.05.2015
 ininal release
*/


#include "platform_setup.h"

#ifdef SMOKE_MODULE

#include <stdio.h>
#include <stdlib.h>
#include "eeprom_map.h"
#include "plink.h"

#define SMOKE_RESET_PERIOD 100 // in 100ms ticks, take into account discrete nature of sensor scans (once per 6s or so)

const unsigned smoke_signature = 635460575;

struct smoke_setup_s smoke_setup[SMOKE_MAX_CH];

unsigned smoke_reset_time[SMOKE_MAX_CH];
enum smoke_satus_e   smoke_status[SMOKE_MAX_CH];

enum smoke_satus_e   old_smoke_status[SMOKE_MAX_CH];

#if PROJECT_CHAR != 'E'
char const * const smoke_status_text[6] = {
  "Норма",
  "ТРЕВОГА",
  "", // cut and short not used in 1w wersion
  "",
  "Выкл",
  "Отказ"
};
#else
char const * const smoke_status_text[6] = {
  "OK",
  "ALARM",
  "", // cut and short not used in 1w wersion
  "",
  "Off",
  "Failed"
};
#endif

char const * const smoke_status_text_sms[6] =
{
  "OK",
  "ALARM!",
  "", // cut and short not used in 1w wersion
  "",
  "SWITCHED OFF",
  "FAILED"
};

static const char * const smoke_status_text_micro[6] =
{
  "OK",
  "AL!",
  "",
  "",
  "OFF",
  "?"
};

char const *smoke_get_status_text(unsigned ch, int message_length_type)
{
  if(ch >= SMOKE_MAX_CH) return "";
  unsigned status = smoke_status[ch];
  if(status >= sizeof smoke_status_text / sizeof smoke_status_text[0]) return "?";
  switch(message_length_type)
  {
  case 3: return smoke_status_text[status];
  case 2: return smoke_status_text_sms[status];
  case 1: return smoke_status_text_micro[status];
  default: return "";
  }
}

void smoke_start_reset(unsigned ch)
{
  if(ch >= SMOKE_MAX_CH) return;
  smoke_reset_time[ch] = sys_clock_100ms + SMOKE_RESET_PERIOD; // in practice, it will be ~12c because of 6s OWT rescan time
}

unsigned smoke_http_get_data(unsigned pkt, unsigned more_data)
{
  char buf[768];
  char *dest = buf;

  if(more_data == 0)
  {
    dest+=sprintf(dest,"var packfmt={");
    PLINK(dest, smoke_setup[0], name);
    PLINK(dest, smoke_setup[0], ow_addr);
    PLINK(dest, smoke_setup[0], flags);
    PSIZE(dest, (char*)&smoke_setup[1] - (char*)&smoke_setup[0]); // must be the last // alignment!
    dest+=sprintf(dest, "}; var data=[");
  }
  for(unsigned n = more_data;;++n)
  {
    if(dest > buf + sizeof buf - 128) break;
    *dest++ = '{';
    PDATA_PASC_STR(dest, smoke_setup[n], name);
    PDATA_OW_ADDR (dest, smoke_setup[n], ow_addr);
    PDATA         (dest, smoke_setup[n], flags);
    dest += sprintf(dest, "status:%u},", smoke_status[n]);
    if(++more_data >= SMOKE_MAX_CH)
    { // last data chunk, terminate output
      more_data = 0;
      --dest; // clear last comma
      *dest++ = ']';
      *dest++ = ';';
      break;
    }
  }
  tcp_put_tx_body(pkt, (unsigned char*)buf, dest - buf);
  return more_data;
}

unsigned smoke_http_get_reset(unsigned pkt, unsigned more_data)
{
  if(memcmp(req_args, "ch=", 3) == 0)
  {
    int n = atoi(req_args+3);
    if(n > 0 && n <= SMOKE_MAX_CH)
      smoke_start_reset(n - 1);
  }
  return 0;
}

int smoke_http_set_data(void)
{
  http_post_data((void*)smoke_setup, sizeof smoke_setup);
  EEPROM_WRITE(&eeprom_smoke_setup, smoke_setup, sizeof smoke_setup);
  ow_restart();
  http_redirect("/smoke.html");
  return 0;
}

unsigned smoke_http_get_cgi(unsigned pkt, unsigned more_data)
{
  char buf[128], *p;
  unsigned ch;

  strcpy(buf, "smoke_result('error');");
  if(req_args[0] != 's') goto end;
  ch = atoi(req_args + 1) - 1;
  if(ch >= SMOKE_MAX_CH) goto end;
  p = req_args + 2;
  if(ch + 1 >= 10) ++p;
  if(*p == 0)
  {
    sprintf(buf, "smoke_result('ok',%u,'%s');",
       (unsigned)smoke_status[ch], smoke_get_status_text(ch, 3));
    goto end;
  }
  else if(strcmp(p, "&on") == 0)
    smoke_setup[ch].flags |= 1;
  else if(strcmp(p, "&off") == 0)
    smoke_setup[ch].flags &=~ 1U;
  else if(strcmp(p, "&reset") == 0)
    smoke_start_reset(ch);
  else goto end;
  strcpy(buf, "smoke_result('ok');");
end:
  tcp_put_tx_body(pkt, (unsigned char*)buf, strlen(buf));
  return 0;
}

HOOK_CGI(smoke_get,   (void*)smoke_http_get_data,  mime_js,  HTML_FLG_GET | HTML_FLG_NOCACHE );
HOOK_CGI(smoke_set,   (void*)smoke_http_set_data,  mime_js,  HTML_FLG_POST );
HOOK_CGI(smoke_reset, (void*)smoke_http_get_reset, mime_js,  HTML_FLG_GET | HTML_FLG_NOCACHE );
HOOK_CGI(smoke,       (void*)smoke_http_get_cgi,   mime_js,  HTML_FLG_GET | HTML_FLG_NOCACHE );

const unsigned char smoke_enterprise[] =
// .1.3.6.1.4.1.25728.8200.2
{SNMP_OBJ_ID, 11, // ASN.1 type/len, len is for 'enterprise' trap field
0x2b,6,1,4,1,0x81,0xc9,0x00,0xC0,0x08,2}; // OID for "enterprise" in trap msg

unsigned char smoke_trap_data_oid[] =
{0x2b,6,1,4,1,0x81,0xc9,0x00,0xC0,0x08,2, // like table entry
0,0}; // two last oid components (pre-last is variable, last is .0 i.e. scalar data)

void smoke_add_vbind_integer(unsigned char last_oid_component, int value)
{
  unsigned seq_ptr = snmp_add_seq();
  smoke_trap_data_oid[sizeof smoke_trap_data_oid - 2] = last_oid_component;
  snmp_add_asn_obj(SNMP_OBJ_ID, sizeof smoke_trap_data_oid, smoke_trap_data_oid);
  snmp_add_asn_integer(value);
  snmp_close_seq(seq_ptr);
}

void smoke_make_trap(unsigned ch)
{
  snmp_create_trap((void*)smoke_enterprise);
  if(snmp_ds == 0xff) return; // if can't create packet
  smoke_add_vbind_integer(1, ch+1);    // npSmokeTrapSensorN
  smoke_add_vbind_integer(2, (int)smoke_status[ch]);      // npSmokeTrapStatus
  // npSmokeTrapMemo
  unsigned seq_ptr = snmp_add_seq();
  smoke_trap_data_oid[sizeof smoke_trap_data_oid - 2] = 6;  // T_MEMO
  snmp_add_asn_obj(SNMP_OBJ_ID, sizeof smoke_trap_data_oid, smoke_trap_data_oid);
  snmp_add_asn_obj(SNMP_TYPE_OCTET_STRING,
    smoke_setup[ch].name[0], smoke_setup[ch].name+1);
  snmp_close_seq(seq_ptr);
}

void smoke_send_notifications(unsigned ch)
{
  if(ch >= SMOKE_MAX_CH) return;
  struct range_notify_s *nf = &smoke_notify[ch];
  unsigned mask;
  switch(smoke_status[ch])
  {
  case SMOKE_STATUS_NORM:   mask = nf->norm; break;
  case SMOKE_STATUS_ALARM:  mask = nf->high; break;
  case SMOKE_STATUS_OFF:    mask = nf->fail; break;
  case SMOKE_STATUS_FAILED: mask = nf->fail; break;
  default: return;
  }
  notify(mask, "Датчик дыма %u%s перешёл в состояние %s",
      ch+1, quoted_name(smoke_setup[ch].name), smoke_get_status_text(ch,3) );
  if(mask & NOTIFY_TRAP)
  {
    if(valid_ip(sys_setup.trap_ip1)) { smoke_make_trap(ch); snmp_send_trap(sys_setup.trap_ip1); }
    if(valid_ip(sys_setup.trap_ip2)) { smoke_make_trap(ch); snmp_send_trap(sys_setup.trap_ip2); }
  }
#ifdef SMS_MODULE
  if(mask & NOTIFY_SMS)
  {
    char tlq[100];
    str_transliterate(tlq, quoted_name(smoke_setup[ch].name), 36);
    sms_msg_printf("SMOKE SENS.%u%s - %s",
      ch+1, tlq, smoke_status_text_sms[smoke_status[ch]] );
  }
#endif
  // sending SSE updates
  unsigned pkt;
  if(http_can_send_sse())
  {
    pkt = tcp_create_packet_sized(sse_sock, 256);
    if(pkt != 0xff)
    {
      unsigned d = ch<<4 | smoke_status[ch];
      unsigned n = sprintf(tcp_ref(pkt, 0), "event: smoke_state\n""data: %u\n\n", d);
      tcp_send_packet(sse_sock, pkt, n);
    } // if pkt ok
  } // if socket can send
}

int smoke_snmp_get(void)
{
  unsigned ch = snmp_data.index - 1;
  if(ch >= SMOKE_MAX_CH) return SNMP_ERR_NO_SUCH_NAME;

  int val = 0;
  switch(snmp_data.id & 0xff)
  {
  case 1: val = ch + 1; break; // npSmokeSensorN
  case 2: val = (int)smoke_status[ch]; break; // npSmokeStatus
  case 3: val = smoke_setup[ch].flags & 1; break; // npSmokePower
  case 4: val = 0; break; // npSmokeReset
  case 6: // npSmokeMemo
    // t_memo, octet string
    snmp_add_asn_obj(SNMP_TYPE_OCTET_STRING,
        smoke_setup[ch].name[0],
        smoke_setup[ch].name+1   );
    return 0;
  default: return SNMP_ERR_NO_SUCH_NAME;
  }
  snmp_add_asn_integer(val);
  return 0;
}


int smoke_snmp_set(void)
{ // use snmp_data.rx_data if necessary
  unsigned ch = snmp_data.index - 1;
  if(ch >= SMOKE_MAX_CH) return SNMP_ERR_NO_SUCH_NAME;

  switch(snmp_data.id & 0xff)
  {
  case 4:
    smoke_start_reset(ch);
    break;
  default:
    return SNMP_ERR_READ_ONLY;
  }
  return 0;
}

unsigned smoke_summary_short(char *dest)
{
  int i;
  char *p = dest;
  *p = 0;
  for(i=0; i<SMOKE_MAX_CH; ++i)
    if(smoke_notify[i].report & NOTIFY_SMS)
      break;
  if(i < SMOKE_MAX_CH)
  {
    p += sprintf(p, "SMOKE ");
    for(int i=0; i<SMOKE_MAX_CH; ++i)
      if(smoke_notify[i].report & NOTIFY_SMS)
      {
        if(smoke_notify[i].report & NOTIFY_SMS)
          p += sprintf(p, "SM%u=%s ", i+1, smoke_get_status_text(i, 1) );
      }
  }
  return p - dest;
}

void smoke_exec(void)
{
  for(int n=0; n<SMOKE_MAX_CH; ++n)
    if(smoke_status[n] != old_smoke_status[n])
    {
      smoke_send_notifications(n);
      old_smoke_status[n] = smoke_status[n];
    }
}

void smoke_reset_params(void)
{
  memset(smoke_setup, 0, sizeof smoke_setup);
  EEPROM_WRITE(eeprom_smoke_setup, smoke_setup, sizeof smoke_setup);
  EEPROM_WRITE(&eeprom_smoke_signature, &smoke_signature, sizeof eeprom_smoke_signature);
}

void smoke_init(void)
{
  unsigned sign;
  EEPROM_READ(&eeprom_smoke_signature, &sign, sizeof sign);
  if(sign != smoke_signature)
    smoke_reset_params();
  EEPROM_READ(eeprom_smoke_setup, smoke_setup, sizeof smoke_setup);
  for(int i=0; i<SMOKE_MAX_CH; ++i)
    smoke_status[i] = old_smoke_status[i] = SMOKE_STATUS_FAILED;
}

void smoke_event(enum event_e event)
{
  switch(event)
  {
  case E_EXEC:
    smoke_exec();
    break;
  case E_RESET_PARAMS:
    smoke_reset_params();
    break;
  }
}


#endif // SMOKE_MODULE
#warning ********** integrate SMOKE! Logic, MIB file, summary reps ****************
