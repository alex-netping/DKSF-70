/*
Relative humidity sensing
SHT1x (FOST02) sensor

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

enum relhum_phase_e {
  RH_IDLE       = 0,
  RH_READ_H  = 1,
  RH_READ_T  = 2
} relhum_phase;

enum relhum_status_e rh_status = RH_STATUS_FAILED;

systime_t      relhum_timer;
unsigned short rh_raw_h;
unsigned short rh_raw_t;
int            rh_real_h;
int            rh_real_t;
unsigned char  rh_fault_count;

void rh_write_reg(unsigned char pointer, unsigned char data)
{
  swi2c_pointer_op(RH_I2C_IF, RH_I2C_ADDR_BYTE | I2C_RWBIT_WRITE, pointer, &data, 1);
}

unsigned rh_read_data(void)
{
  unsigned char data[2];
  data[0] = data[1] = 0x00;
  swi2c_pointer_op(RH_I2C_IF, RH_I2C_ADDR_BYTE | I2C_RWBIT_READ, 1, &data, 2);
  return data[0] << 8 | data[1];
}

int rh_ack(void)
{
  return swi2c_ack;
}

/*
void relhum_sensor_cmd(char cmd, unsigned conversion_time_ms, enum relhum_phase_e next_state)
{
////////////////  relhum_sensor_start();
  unsigned ack = relhum_sensor_write(cmd);
  if(!ack)
  { // no sensor on bus
    ++ rh_fault_count;
    relhum_timer = sys_clock() + 500; // repeat after short pause
    relhum_phase = RH_IDLE;
  }
  else
  { // wait for measuring time
    relhum_timer = sys_clock() + conversion_time_ms;
    relhum_phase = next_state;
  }
////////////////  crc8 = 0;
/////////////////////  crc_byte(cmd);
}
*/

// resets serial interface of the humidity sensor chip, to release shared SDA line
void relhum_cancel(void)
{
  if(relhum_phase != RH_IDLE)
  {
    relhum_phase = RH_IDLE;
    relhum_timer = sys_clock() + 600; // postpone measurement
    i2c_release('H');
  }
}

void relhum_param_reset(void)
{
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

  relhum_timer = sys_clock() + 3000;
  ////////rh_sck(0);
  rh_real_h = 0;
  rh_real_t = 0;
  rh_status = RH_STATUS_FAILED;

#warning remove debug
static char rha[] = {0x26,0xda,0xf4,0xa1,0x01,0x00,0x00,0x5e};
memcpy(relhum_setup.ow_addr, rha, 8);
}

void relhum_exec(void)
{
  static unsigned char cnt = 0;
  if(++cnt<139) return;
  cnt = 0;

  if(sys_clock() < relhum_timer) return;
  if(!i2c_accquire('H')) return; // LBS 5.07.2010


#warning TODO реализовать сбои датчика, проверку результата на валидность
  switch(relhum_phase)
  {
  case RH_IDLE:
    // start convertion of Rel.Humidity
#warning remove debuggg
//////////////////////////    rh_write_reg(3, 1); // set START in CONFIG
    relhum_timer = sys_clock() + 45; // 40ms max conversion
    relhum_phase = RH_READ_H;
    break;
  case RH_READ_H:
    // read Rel.Humidity data
#warning remove debuggg
////////    rh_raw_h = rh_read_data();

rh_raw_h = 0;
swi2c_pointer_op(0, 0x80 | I2C_RWBIT_READ, 17, &rh_raw_h, 1);

    rh_write_reg(3, 0x11); // set TEMP and START in CONFIG
    relhum_timer = sys_clock() + 45; // 40ms max conversion
    relhum_phase = RH_READ_T;
    break;
  case RH_READ_T:
    // read Temperature
    rh_raw_t = rh_read_data();

    rh_real_h = (rh_raw_h >> 4) - 24;
    rh_real_t = (rh_raw_t >> 5) - 50;

    double h, dt;
    h = rh_real_h;
    dt = rh_real_t - 30;

    h = h - (h * h * -0.00393 + h * 0.4008 - 4.7844); // linearization
    h = h + dt * (h * 0.00237 + 0.1973); // temperature compensation
    rh_real_h = (int)ceil(h);

    relhum_timer = sys_clock() + RH_IDLE_PERIOD;
    relhum_phase = RH_IDLE;
    break;

/***
    // compute temperature
    rh_real_t = ((int)rh_raw * 655) >> 16; // 655/65536 = 0,009994507
    rh_real_t -= 40;
    if(rh_real_t > -40)
    {
      // T is Ok
      rh_fault_count = 0;
      rh_status = RH_STATUS_OK;
    }
    else
    { // зашкалило в минус
      rh_real_t = -99;
      rh_status = RH_STATUS_FAILED;
    }
    // wait for next cycle
    ///////////////rh_check_status();
    relhum_timer = sys_clock() + RH_IDLE_PERIOD;
    relhum_phase = RH_IDLE;
    break;
    ******/

  } // switch(relhum_phase)


  if(relhum_phase != RH_IDLE) return;

  i2c_release('H');

  /*
  // check repeating faults
  if(rh_fault_count>=5)
  {
    rh_fault_count = 5; // overflow protection
    rh_status = RH_STATUS_FAILED;
    rh_real_h = 0;
    rh_real_t = 0;
  }
  */

  rh_check_status();
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
    if(rh_status_h == 0) log_printf(fmt);
    else log_printf(fmt, rh_real_h, relhum_setup.rh_low, relhum_setup.rh_high);

    if(valid_ip(sys_setup.trap_ip1)) { relhum_rh_make_trap(); snmp_send_trap(sys_setup.trap_ip1); }
    if(valid_ip(sys_setup.trap_ip2)) { relhum_rh_make_trap(); snmp_send_trap(sys_setup.trap_ip2); }
  }
}

unsigned relhum_http_get_status(unsigned pkt, unsigned more_data)
{
  char buf[256];
  char *dest = buf;
  dest += sprintf(dest, "status_data={rh_value:%d, t_value:%d, rh_status_h:%u}",
                          rh_real_h, rh_real_t, rh_status_h);
  tcp_put_tx_body(pkt, (unsigned char*)buf, dest - buf);
  return 0; // no more data
}

unsigned relhum_http_get(unsigned pkt, unsigned more_data)
{
  char buf[768];
  char *dest = buf;
  dest+=sprintf(dest,"var packfmt={");
  PLINK(dest, relhum_setup, rh_high);
  PLINK(dest, relhum_setup, rh_low);
  PLINK(dest, relhum_setup, flags);
  PSIZE(dest, sizeof relhum_setup); // must be the last // alignment!
  dest+=sprintf(dest, "};\nvar data={");
  PDATA(dest, relhum_setup, rh_high);
  PDATA(dest, relhum_setup, rh_low);
  PDATA(dest, relhum_setup, flags);
  --dest; // remove last comma
  *dest++='}'; *dest++=';';
  tcp_put_tx_body(pkt, (unsigned char*)buf, dest - buf);
  return 0;
}

int relhum_http_set(void)
{
  http_post_data((void*)&relhum_setup, sizeof relhum_setup);
  EEPROM_WRITE(&eeprom_relhum_setup, &relhum_setup, sizeof eeprom_relhum_setup);
  http_redirect("/rh.html");
  return 0;
}


HOOK_CGI(rh_stat_get, relhum_http_get_status, mime_js, HTML_FLG_GET | HTML_FLG_NOCACHE );
HOOK_CGI(relhum_get,  relhum_http_get,        mime_js, HTML_FLG_GET | HTML_FLG_NOCACHE );
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
