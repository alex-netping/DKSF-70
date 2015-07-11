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
*/

#include "platform_setup.h"
#include "eeprom_map.h"
#include "plink.h"
#include <stdio.h>
#include <math.h>

#define RH_IDLE_PERIOD 6000 // ms

const unsigned char rh_hyst_h = 2 - 1;

void rh_check_status(void);

const unsigned relhum_signature = 561132741;
struct relhum_setup_s relhum_setup;
unsigned char rh_status_h;

enum relhum_status_e rh_status = RH_STATUS_FAILED;

unsigned short rh_raw_h;
unsigned short rh_raw_t;
int            rh_real_h;
int            rh_real_t;
int            rh_real_t_100; // *100 temperature

void relhum_param_reset(void)
{
  memset(&relhum_setup, 0, sizeof relhum_setup);
  relhum_setup.rh_high = 85;
  relhum_setup.rh_low = 5;
  relhum_setup.flags = 0;
  EEPROM_WRITE(&eeprom_relhum_setup, &relhum_setup, sizeof eeprom_relhum_setup);
  EEPROM_WRITE(&eeprom_relhum_signature, &relhum_signature, sizeof eeprom_relhum_signature);
}

void relhum_init(void)
{
  unsigned sign;
  EEPROM_READ(&eeprom_relhum_signature, &sign, sizeof sign);
  if(sign != relhum_signature) relhum_param_reset();
  EEPROM_READ(&eeprom_relhum_setup, &relhum_setup, sizeof relhum_setup);

  ///relhum_timer = sys_clock() + 3000;
  ////////rh_sck(0);
  rh_real_h = 0;
  rh_real_t = 0;
  rh_status_h = 0;
  rh_status = RH_STATUS_FAILED;
}

//#warning remove debug
//int goodcnt;

void relhum_exec(void)
{
  static unsigned char cnt = 0;
  if(++cnt<139) return;
  cnt = 0;
  // 1W only, scan in ow.c
  rh_check_status();

/*
  ////// tests ////////////////////////////////////////
#warning remove debug, also restore old swi2c.c
  int qqq;

// read Si7005 id

aswi2c_start(0);
aswi2c_write_byte(0,0x80);
aswi2c_write_byte(0,0x11);
aswi2c_start(0);
aswi2c_write_byte(0,0x81);
qqq=aswi2c_read_byte(0,0);
qqq=qqq*1;
aswi2c_stop(0);
if(qqq != 0x50)
{
  delay(1);
  goodcnt = 0;
}
else ++goodcnt;


  // read T
aswi2c_start(0);
aswi2c_write_byte(0,0x80);
aswi2c_write_byte(0,0x03);
aswi2c_write_byte(0,0x11);
aswi2c_stop(0);
delay(75);
aswi2c_start(0);
aswi2c_write_byte(0,0x80);
aswi2c_write_byte(0,0x01);
aswi2c_start(0);
aswi2c_write_byte(0,0x81);
qqq=aswi2c_read_byte(0,1) << 8;
qqq+=aswi2c_read_byte(0,0);
qqq=(qqq>>2)/32-50;
aswi2c_stop(0);

if(qqq < 18 || qqq > 29)
{
  goodcnt = 0;
}
else ++goodcnt;
  */

/*
aswi2c_start(0);
aswi2c_write_byte(0,0x90|7<<1);
aswi2c_write_byte(0,0x00);
aswi2c_start(0);
aswi2c_write_byte(0,0x91|7<<1);
qqq=aswi2c_read_byte(0, 1);
qqq=aswi2c_read_byte(0, 0);
aswi2c_stop(0);
*/

///////////////// end of tests /////////////////////////////

}

int relhum_snmp_get(unsigned id, unsigned char *data)
{
  int val = 0;
  switch(id&0xfffffff0)
  {
  case 0x8420: val = rh_real_h; break;
  case 0x8430: val = rh_status; break;
  case 0x8440: val = rh_real_t; break;
  case 0x8450: val = rh_status_h; break;
  case 0x8470: val = relhum_setup.rh_high; break;
  case 0x8480: val = relhum_setup.rh_low; break;
  case 0x8490: val = rh_real_t_100; break;
  default: return SNMP_ERR_NO_SUCH_NAME;
  }
  snmp_add_asn_integer(val);
  return 0;
}


const unsigned char relhum_enterprise[] =
// .1.3.6.1.4.1.25728.8400.9
{SNMP_OBJ_ID, 11, // ASN.1 type/len, len is for 'enterprise' trap field
0x2b,6,1,4,1,0x81,0xc9,0x00,0xc1,0x50,9}; // OID for "enterprise" in trap msg
// .1.3.6.1.4.1.25728.8400.2
unsigned char relhum_rh_trap_data_oid[] =
{0x2b,6,1,4,1,0x81,0xc9,0x00,0xc1,0x50,2, // variable prefix
0,0}; // last oid components (pre-last is variable, last is .0 for scalar oid)


void relhum_rh_add_vbind_integer(unsigned char last_oid_component, int value)
{
  unsigned seq_ptr;
  seq_ptr = snmp_add_seq();
  relhum_rh_trap_data_oid[sizeof relhum_rh_trap_data_oid - 2] = last_oid_component; // before 'scalar' zero postfix
  snmp_add_asn_obj(SNMP_OBJ_ID, sizeof relhum_rh_trap_data_oid, relhum_rh_trap_data_oid);
  snmp_add_asn_integer(value);
  snmp_close_seq(seq_ptr);
}

void relhum_rh_make_trap(void)
{
  snmp_create_trap((void*)relhum_enterprise);
  if(snmp_ds == 0xff) return; // if can't create packet
  relhum_rh_add_vbind_integer(5, rh_status_h);    // npRelHumSensorStatusH
  relhum_rh_add_vbind_integer(2, rh_real_h);      // npRelHumSensorValueH
  relhum_rh_add_vbind_integer(7, relhum_setup.rh_high); // npRelHumSafeRangeHigh
  relhum_rh_add_vbind_integer(8, relhum_setup.rh_low);  // npRelHumSafeRangeLow
}

void rh_check_status(void)
{
  unsigned char old_status_h = rh_status_h;
  unsigned low = relhum_setup.rh_low;
  unsigned high = relhum_setup.rh_high;
  if(rh_status == RH_STATUS_FAILED)
    rh_status_h = 0; // if sensor offline or failed
  else
  {
    switch(rh_status_h)
    {
    case 3: high += rh_hyst_h; break;
    case 2: high += rh_hyst_h; low -= rh_hyst_h; break;
    case 1: low -= rh_hyst_h; break;
    }
    if(rh_real_h > high) rh_status_h = 3;
    else if(rh_real_h < high && rh_real_h > low) rh_status_h = 2;
    else if(rh_real_h < low) rh_status_h = 1;
  }
  if(rh_status_h != old_status_h)
  {
    char *fmt = "r.h. error";
    switch(rh_status_h)
    {
#if PROJECT_CHAR != 'E'
    case 3: fmt = "Влажность %d%%, выше нормы (%d..%d%%)"; break;
    case 2: fmt = "Влажность %d%%, в пределах нормы (%d..%d%%)"; break;
    case 1: fmt = "Влажность %d%%, ниже нормы (%d..%d%%)"; break;
    case 0: fmt = "Влажность - датчик отсутствует или неисправен"; break;
#else
    case 3: fmt = "Humidity %d%%, above safe (%d..%d%%)"; break;
    case 2: fmt = "Humidity %d%%, in safe range (%d..%d%%)"; break;
    case 1: fmt = "Humidity %d%%, below safe (%d..%d%%)"; break;
    case 0: fmt = "Humidity sensor is absent or failed"; break;
#endif
    }
#ifdef NOTIFY_MODULE
    unsigned mask = 0;
    switch(rh_status_h)
    {
    case 0: mask = relhum_notify.fail; break;
    case 1: mask = relhum_notify.low; break;
    case 2: mask = relhum_notify.norm; break;
    case 3: mask = relhum_notify.high; break;
    }
    notify(mask, fmt, rh_real_h, relhum_setup.rh_low, relhum_setup.rh_high);
    if(mask & NOTIFY_TRAP)
    {
      if(valid_ip(sys_setup.trap_ip1)) { relhum_rh_make_trap(); snmp_send_trap(sys_setup.trap_ip1); }
      if(valid_ip(sys_setup.trap_ip2)) { relhum_rh_make_trap(); snmp_send_trap(sys_setup.trap_ip2); }
    }
#ifdef SMS_MODULE
    if(mask & NOTIFY_SMS)
    {
      sms_relhum_event();
    }
#endif
#else
    if(rh_status_h == 0) log_printf(fmt);
    else log_printf(fmt, rh_real_h, relhum_setup.rh_low, relhum_setup.rh_high);

    if(valid_ip(sys_setup.trap_ip1)) { relhum_rh_make_trap(); snmp_send_trap(sys_setup.trap_ip1); }
    if(valid_ip(sys_setup.trap_ip2)) { relhum_rh_make_trap(); snmp_send_trap(sys_setup.trap_ip2); }
#endif
  } // if status changed
}

unsigned relhum_http_get_status(unsigned pkt, unsigned more_data)
{
  char buf[256];
  char *dest = buf;
  dest += sprintf(dest, "status_data={rh_value:%d, t_value:%d, t_value_100:%d, rh_status_h:%u}",
                          rh_real_h, rh_real_t, rh_real_t_100, rh_status_h);
  tcp_put_tx_body(pkt, (unsigned char*)buf, dest - buf);
  return 0; // no more data
}

unsigned relhum_http_get(unsigned pkt, unsigned more_data)
{
  char buf[768];
  char *dest = buf;
  dest+=sprintf(dest,"var packfmt={");
#ifdef OW_MODULE
  PLINK(dest, relhum_setup, ow_addr);
#endif
  PLINK(dest, relhum_setup, rh_high);
  PLINK(dest, relhum_setup, rh_low);
  PLINK(dest, relhum_setup, flags);
  PSIZE(dest, sizeof relhum_setup); // must be the last // alignment!
  dest+=sprintf(dest, "};\nvar data={");
#ifdef OW_MODULE
  unsigned char *owa = relhum_setup.ow_addr;
  if(owa[0] != 0)
    dest += sprintf(dest, "ow_addr:\"%02x%02x %02x%02x %02x%02x %02x%02x\",",
              owa[0], owa[1], owa[2], owa[3], owa[4], owa[5], owa[6], owa[7]);
  else
    dest += sprintf(dest, "ow_addr:\"\",");
#endif
  PDATA(dest, relhum_setup, rh_high);
  PDATA(dest, relhum_setup, rh_low);
  PDATA(dest, relhum_setup, flags);
  --dest; // remove last comma
  *dest++='}'; *dest++=';';
  tcp_put_tx_body(pkt, (unsigned char*)buf, dest - buf);
  return 0;
}

unsigned relhum_http_get_cgi(unsigned pkt, unsigned more_data)
{
  char buf[128];
  sprintf(buf, "relhum_result('ok', %u, %d, %u);", rh_real_h, rh_real_t, rh_status_h);
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
  case E_EXEC:
    relhum_exec();
    break;
  case E_RESET_PARAMS:
    relhum_param_reset();
    break;
  }
}
#warning ******* check MIB file, change inNormRange(2) -> inSafeRange(2) // done in 52/201/202,70
