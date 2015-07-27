/*
v1.0
7.7.2015
*/

#include "platform_setup.h"
#include "eeprom_map.h"

#ifdef PWRMON_MODULE

#include "plink.h"
#include <stdio.h>
#include <stdlib.h>

#define PWRMON_SENSOR_SETUP_DATALEN	26			// byte length of sensor setup registers

void pwrmon_set_comm_status(unsigned ch, int ok_flag);

const unsigned pwrmon_signature = 0x7077726d; // 'pwrm'

struct pwrmon_setup_s pwrmon_setup[PWRMON_MAX_CH];
struct pwrmon_state_s pwrmon_state[PWRMON_MAX_CH];

char           pwrmon_sensor_setup_save_it; // flag to write setup data into sensor for ow.c
unsigned short pwrmon_sensor_addr_and_setup[4+13]; // ow address (8 byte = 4 elements), then sensor profiles (13 elements)

enum pwrmon_event_e {
  PWRMON_FAIL = 0,
  PWRMON_WORKING,
  PWRMON_PROF1UV,
  PWRMON_PROF1OV,
  PWRMON_PROF2UV,
  PWRMON_PROF2OV,
  PWRMON_PROF3UV
};

#if PROJECT_LETTER != 'E'
const char *pwrmon_msg[7] = {
  "отказ датчика",
  "датчик работает",
  "короткий провал",
  "короткое превышение",
  "длит.провал",
  "длит.превышение",
  "блэкаут"
};
#else
const char *pwrmon_msg[7] = {
  "sensor failed",
  "sensor is ok",
  "short undervoltage",
  "short overvoltage",
  "long undervoltage",
  "long overvoltage",
  "blackout"
};
#endif

void pwrmon_param_reset(void)
{
  memset(pwrmon_setup, 0, sizeof pwrmon_setup);

  EEPROM_WRITE(&eeprom_pwrmon_setup, &pwrmon_setup, sizeof eeprom_pwrmon_setup);
  EEPROM_WRITE(&eeprom_pwrmon_signature, &pwrmon_signature, sizeof eeprom_pwrmon_signature);
}

void pwrmon_init(void)
{
  unsigned sign;
  EEPROM_READ(&eeprom_pwrmon_signature, &sign, sizeof sign);
  if(sign != pwrmon_signature) pwrmon_param_reset();
  EEPROM_READ(&eeprom_pwrmon_setup, &pwrmon_setup, sizeof pwrmon_setup);
}

unsigned pwrmon_http_get(unsigned pkt, unsigned more_data)
{
  char buf[768];
  char *dest = buf;
  
  dest += sprintf(dest,"var packfmt={");
  PLINK(dest, pwrmon_setup[0], name);
  PLINK(dest, pwrmon_setup[0], ow_addr);
  PSIZE(dest, sizeof pwrmon_setup[0]); // must be the last // alignment!
  dest += sprintf(dest, "}; var data=[");
  struct pwrmon_setup_s *su = &pwrmon_setup[more_data];
  struct pwrmon_state_s *st = &pwrmon_state[more_data];
  int n;
  for(n = more_data;; ++su, ++st)
  {
    *dest++ = '{';
    PDATA_PASC_STR(dest, *su, name);
    PDATA_OW_ADDR(dest, *su, ow_addr);
    dest += sprintf(dest, "comm_status:%u,v:%u,f:%u},",
        st->comm_status, st->v, st->f );
    if(++n == PWRMON_MAX_CH)
    {
      --dest; // remove last comma
      *dest++ = ']'; *dest++ = ';';
      n = 0;
      break;
    }
    if(dest > buf + sizeof buf - 128)
      break;
  }
  tcp_put_tx_body(pkt, (unsigned char*)buf, dest - buf);
  return n;
}

unsigned pwrmon_http_get_cgi(unsigned pkt, unsigned more_data)
{
#warning TODO this
#warning add HOOK for this
  char buf[128];
  struct relhum_state_s *st;

  strcpy(buf, "relhum_result('error');");
  unsigned ch = atoi(req_args + 1);
  if(ch >= RELHUM_MAX_CH) goto end;
  st = &relhum_state[ch];
  if(req_args[0] == 'h')
  {
    sprintf(buf, "pwrmon_result('ok', %u, %u);", st->rh, st->rh_status);
  }
  else if(req_args[0] == 't')
  {
    sprintf(buf, "pwrmon_result('ok', %d, %u);", st->t, st->t_status);
  }
end:
  tcp_put_tx_body(pkt, (unsigned char*)buf, strlen(buf));
  return 0;
}

int pwrmon_http_set(void)
{	
  http_post_data((void*)pwrmon_setup, sizeof pwrmon_setup);

  EEPROM_WRITE(eeprom_pwrmon_setup, pwrmon_setup, sizeof eeprom_pwrmon_setup);
  for(int n=0; n<PWRMON_MAX_CH; ++n)
    pwrmon_state[n].refresh = 1;
#ifdef OW_MODULE
  ow_restart();
#endif

  http_redirect("/pwrmon.html");
  return 0;
}

unsigned pwrmon_http_sensor_get(unsigned pkt, unsigned more_data)
{
  if(memcmp(req_args, "ch=", 3) != 0) return 0;
  unsigned ch1 = atoi(req_args + 3);
  if(ch1 == 0 || ch1 > PWRMON_MAX_CH) return 0;
  //struct pwrmon_state_s *st = &pwrmon_state[ch1 - 1];
  struct pwrmon_setup_s *su = &pwrmon_setup[ch1 - 1];
  
  char buf[768];
  char *dest = buf;
  *dest++ = '('; *dest++ = '{';
  PDATA_PASC_STR(dest, pwrmon_setup[ch1 - 1], name);
  PDATA(dest, *su, t1);
  PDATA(dest, *su, uv1);
  PDATA(dest, *su, ov1);
  PDATA(dest, *su, t12);
  PDATA(dest, *su, ov2);
  PDATA(dest, *su, uv2);
  PDATA(dest, *su, t2);
  --dest; // remove last comma
  *dest++ = '}'; *dest++=')'; *dest++=';';
  tcp_put_tx_body(pkt, (unsigned char*)buf, dest - buf);
  return 0;
}

int pwrmon_http_sensor_set(void)
{
  const unsigned data_len_bytes = PWRMON_SENSOR_SETUP_DATALEN;
  unsigned ch;

  if (memcmp(req_args, "ch=", 3) != 0) 
  	goto error;
 
  ch = atoi(req_args + 3);
  if(ch == 0 || ch > PWRMON_MAX_CH) 
  	goto error;
  
  ch -= 1; // 1-based to 0-based
  if (http.post_content_length != (HTTP_POST_HDR_SIZE + (data_len_bytes * 2)))
  	goto error;
  
  http_post_data_part(req + HTTP_POST_HDR_SIZE, (void*)&pwrmon_state[ch].uv1, data_len_bytes);

  pwrmon_state[ch].write_sensor_setup = 1;
  http_reply(200, "");
  return 0;
  
error:
  http_reply(200, "internal error");
  return 0;
}

HOOK_CGI(pwrmon_get,  pwrmon_http_get,               mime_js, HTML_FLG_GET | HTML_FLG_NOCACHE );
HOOK_CGI(pwrmon_set,  pwrmon_http_set,               mime_js, HTML_FLG_POST );
HOOK_CGI(pwrmon_sensor_get,  pwrmon_http_sensor_get, mime_js, HTML_FLG_GET | HTML_FLG_NOCACHE );
HOOK_CGI(pwrmon_sensor_set,  pwrmon_http_sensor_set, mime_js, HTML_FLG_POST );

void pwrmon_parse_setup_data_from_sensor(unsigned ch, unsigned char *buf)
{
  // setup from sensor stored in pwrmon_state[], not pwrmon_setup[]
  
  if(ch >= PWRMON_MAX_CH) return;
  struct pwrmon_setup_s *su = &pwrmon_setup[ch];
	
  pwrmon_set_comm_status(ch, 1);
  // copy useful data (truncated) to properly aligned word array
  unsigned short sensor_data[20];
  memcpy(sensor_data, buf, sizeof sensor_data);
  su->uv1 = sensor_data[0];
  su->ov1 = sensor_data[1];
  su->t1  = sensor_data[2];
  su->t12 = sensor_data[3];
  su->uv2 = sensor_data[6];
  su->ov2 = sensor_data[7];
  su->t2  = sensor_data[9];
}

void pwrmon_parse_full_stats_from_sensor(unsigned ch, unsigned char *buf)
{
  if(ch >= PWRMON_MAX_CH) return;

  const unsigned len = 26;
  if(buf[0] == len) // check data len read from sensor
  {
    memcpy(&pwrmon_state[ch].cnt1uv, buf + 1 + 2, (len-3)); // skip length and version, copy data	
    pwrmon_set_comm_status(ch, 1);
  }
  else
    pwrmon_set_comm_status(ch, 0);
}

unsigned char pwrmon_trap_oid[]=
// .1.3.6.1.4.1.25728.5100.2.0.0
{0x2b, 6, 1, 4, 1, 0x81, 0xc9, 0x00, 0xa7, 0x6c,
2, // trap branch prefix
0, 0}; // trap/var identity

void pwrmon_make_trap(unsigned ch, enum pwrmon_event_e event)
{
  unsigned seq_ptr;

  pwrmon_trap_oid[sizeof pwrmon_trap_oid - 2] = (pwrmon_notify[ch].flags & NOTIFY_COMMON_ALL_EVENTS) ? 99 : ch + 1;
  pwrmon_trap_oid[sizeof pwrmon_trap_oid - 1] = (pwrmon_notify[ch].flags & NOTIFY_COMMON_ALL_CHANNELS) ? 127 : 100 + (unsigned)event;

  snmp_create_trap_v2(sizeof pwrmon_trap_oid, pwrmon_trap_oid);
  if(snmp_ds == 0xff) return; // if can't create packet

  seq_ptr = snmp_add_seq(); // npPwrmonTrapEvent
  pwrmon_trap_oid[sizeof pwrmon_trap_oid - 2] = 1;
  pwrmon_trap_oid[sizeof pwrmon_trap_oid - 1] = 0;
  snmp_add_asn_obj(SNMP_OBJ_ID, sizeof pwrmon_trap_oid, pwrmon_trap_oid);
  snmp_add_asn_integer((int)event);
  snmp_close_seq(seq_ptr);

  seq_ptr = snmp_add_seq();    // npPwrmonTrapMemo
  pwrmon_trap_oid[sizeof pwrmon_trap_oid - 2] = 6;
  pwrmon_trap_oid[sizeof pwrmon_trap_oid - 1] = 0;
  snmp_add_asn_obj(SNMP_OBJ_ID, sizeof pwrmon_trap_oid, pwrmon_trap_oid);
  snmp_add_asn_obj(SNMP_TYPE_OCTET_STRING,
    pwrmon_setup[ch].name[0], pwrmon_setup[ch].name+1);
  snmp_close_seq(seq_ptr);

}

void pwrmon_send_trap(unsigned ch,  enum pwrmon_event_e event)
{
  pwrmon_make_trap(ch, event);
  snmp_send_trap(sys_setup.trap_ip1);
  pwrmon_make_trap(ch, event); // snmp_ds udp packet was thrown, make new
  snmp_send_trap(sys_setup.trap_ip2);
}

void pwrmon_send_notifications(unsigned ch, unsigned counter_delta, unsigned short enabled_events, enum pwrmon_event_e event)
{
  if(event >= PWRMON_PROF1UV && counter_delta == 0) return;
  char repeat_txt[16] = "";
  if(counter_delta)
    sprintf(repeat_txt, " (%ux)", counter_delta);
  notify(enabled_events, "Датчик напряжения %u%s - %s%s",
     ch + 1, quoted_name(pwrmon_setup[ch].name),
     pwrmon_msg[event], repeat_txt );
#ifdef SMS_MODULE
  if(enabled_events & NOTIFY_SMS)
    sms_msg_printf("V.MON%u%s %s%s",
       ch + 1, quoted_name(pwrmon_setup[ch].name),
       pwrmon_msg[event], repeat_txt );
#endif
  if(enabled_events & NOTIFY_TRAP)
    pwrmon_send_trap(ch, event);
}

unsigned pwrmon_chk_counter(unsigned *counter, unsigned char new_low_byte)
{
  unsigned old_counter = *counter;
  if((*counter & 0xff) > new_low_byte) ++*counter; // low byte overflowed and rolled over; carry owerflow
  *counter = (*counter & 0xffffff00) | new_low_byte; // replace low byte
  return *counter - old_counter;
}

void pwrmon_parse_short_stats_from_sensor(unsigned ch, unsigned char *buf)
{
  if(ch >= PWRMON_MAX_CH) return;  
  if(buf[0] != 10) return; // check data len read from sensor
  struct pwrmon_state_s *st = &pwrmon_state[ch];
  struct range_notify_s *nf = &pwrmon_notify[ch];
  pwrmon_send_notifications(ch, pwrmon_chk_counter(&st->cnt1uv, buf[2]), nf->low,  PWRMON_PROF1UV);
  pwrmon_send_notifications(ch, pwrmon_chk_counter(&st->cnt1ov, buf[3]), nf->low,  PWRMON_PROF1OV);
  pwrmon_send_notifications(ch, pwrmon_chk_counter(&st->cnt2uv, buf[4]), nf->norm, PWRMON_PROF2UV);
  pwrmon_send_notifications(ch, pwrmon_chk_counter(&st->cnt2ov, buf[5]), nf->norm, PWRMON_PROF2OV);
  pwrmon_send_notifications(ch, pwrmon_chk_counter(&st->cnt3uv, buf[6]), nf->high, PWRMON_PROF3UV);
  st->v = buf[7]<<0 | buf[8]<<8;
  st->f = buf[9]<<0 | buf[10]<<8;
}

void pwrmon_set_comm_status(unsigned ch, int ok_flag)
{
  if(ch >= PWRMON_MAX_CH) return;
  if(pwrmon_state[ch].comm_status != ok_flag)
    pwrmon_send_notifications(ch, 0, pwrmon_notify[ch].fail, ok_flag ? PWRMON_WORKING : PWRMON_FAIL);
  pwrmon_state[ch].comm_status = ok_flag;
  if(!ok_flag)
  {
    pwrmon_state[ch].write_sensor_setup = 0; // cancel writing
    pwrmon_state[ch].refresh = 1; // order re-reading of sensor setup and counters
    memset(&pwrmon_state[ch].uv1, 0, 13 * 2); // clear sensor setup data // unsafe!
  }
}

#endif // PWRMON_MODULE
