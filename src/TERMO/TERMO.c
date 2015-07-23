/* by P.Lyubasov
*v 1.3
* 8.02.2010
* i2c bus sharing added
*v1.4-50
*5.07.2010 by LBS
* i2c bus sharing restored, cpu unload corrected in termo_exec()
* removed old stuff (PARAMETERS related)
* table index via snmp_data.index
*v1.5-50
*8.07.2010
* removed periodic logging in TERMO to prevent EEPROM wear
*v1.5-162
* 22.09.2010
* termo traps modified
*v1.6-52-6
* sms notification added
*v1.7-50
* 14.02.2011
* send START/STOP on init
*v1.7-200
*11.12.2011
* second trap ip restored
*v1.8-201
*25.10.2011
*  Adjusted English status
*v1.9-213
*5.09.2012
*  threshold >= <= corrections (long-standing errata eliminated!)
*  dksf213 support (sans http)
*v1.10-48
*4.04.2013
*  stdlib used
*  cosmetic rewrite of log, using quoted_name()
*v1.11-70
*17.07.2013
  rewrite for 1w compatibility
v1.12-70
15.05.2014
  notify support
v1.13-70
30.07.2014
  json-p url-encoded api
v1.14-60
28.10.2014
  bugfix - no traps if Notify module is used
*/

#include "platform_setup.h"
#ifdef TERMO_MODULE

#include "termo\termo.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "eeprom_map.h"
#include "plink.h"


#ifdef OW_MODULE
const unsigned termo_signature = 329155; // with ow_addr
#else
const unsigned termo_signature = 329854;
#endif

struct   termo_setup_s   termo_setup[TERMO_N_CH];

struct termo_state_s termo_state[TERMO_N_CH]; // 25.11.2010 struct definition is moved to .h file

unsigned termo_scan_time = 30;

void termo_send_trap(int ch);
void check_termo_status(int ch);

#ifdef HTTP_MODULE

unsigned termo_http_get_data(unsigned pkt, unsigned more_data)
{
  char buf[768];
  char *dest = buf;

  unsigned handler_id = strcmp(http.page->name, "/termo_data.cgi") == 0 ? 0x8801 : 0x8800; // some hack

  if(more_data == 0)
  {
    if(handler_id == 0x8800) // id8800 - js code for http data link
    {
      dest+=sprintf(dest,"var packfmt={");
      PLINK(dest, termo_setup[0], name);
#ifdef OW_MODULE
      PLINK(dest, termo_setup[0], ow_addr);
#endif
      PLINK(dest, termo_setup[0], bottom);
      PLINK(dest, termo_setup[0], top);
#ifndef NOTIFY_MODULE
      PLINK(dest, termo_setup[0], trap_delay);
      PLINK(dest, termo_setup[0], trap_low);
      PLINK(dest, termo_setup[0], trap_norm);
      PLINK(dest, termo_setup[0], trap_high);
#endif // NOTIFY_MODULE
      PSIZE(dest, (char*)&termo_setup[1] - (char*)&termo_setup[0]); // must be the last // alignment!
      *dest++='}'; *dest++=';';
      dest+=sprintf(dest, "var data=[");
    }
    else // id8801 - only JSON data for eval()
    {
      *dest++ = '[';
    }
  }

  int n = more_data;
  *dest++ = '{';
  PDATA_PASC_STR(dest, termo_setup[n], name);
#ifdef OW_MODULE
  unsigned char *owa = termo_setup[n].ow_addr;
  if(owa[0] != 0)
    dest += sprintf(dest, "ow_addr:\"%02x%02x %02x%02x %02x%02x %02x%02x\",",
              owa[0], owa[1], owa[2], owa[3], owa[4], owa[5], owa[6], owa[7]);
  else
    dest += sprintf(dest, "ow_addr:\"\",");
#endif
  PDATA_SIGNED(dest, termo_setup[n], bottom);
  PDATA_SIGNED(dest, termo_setup[n], top);
#ifndef NOTIFY_MODULE
  PDATA(dest, termo_setup[n], trap_delay);
  PDATA(dest, termo_setup[n], trap_low);
  PDATA(dest, termo_setup[n], trap_norm);
  PDATA(dest, termo_setup[n], trap_high);
#endif // NOTIFY_MODULE
  dest+=sprintf(dest, "double_hyst:%d,", TERMO_HYST*2);
  char *stat;
  switch(termo_state[n].status)
  {
#if PROJECT_CHAR=='E'
  case 1: stat = "below safe range"; break;
  case 2: stat = "ok"; break;
  case 3: stat = "above safe range"; break;
  default: stat = "sensor fault"; break;
#else
  case 1: stat = "ниже нормы"; break;
  case 2: stat = "в норме"; break;
  case 3: stat = "выше нормы"; break;
  default: stat = "сбой"; break;
#endif
  }
  dest+=sprintf(dest, "tval:%d,", termo_state[n].value);
  dest+=sprintf(dest, "status:'%s'},", stat); // also print },

  if(++more_data >= TERMO_N_CH)
  { // last data chunk, terminate output
    more_data = 0;
    --dest; // clear last comma
    *dest++ = ']';
    if(handler_id == 0x8800) *dest++ = ';';
  }
  tcp_put_tx_body(pkt, (unsigned char*)buf, dest - buf);
  return more_data;
}

unsigned termo_http_get_cgi(unsigned pkt, unsigned more_data)
{
  char buf[128] = "thermo_result('error');";
  char *p = req_args;
  
  if(*p++ != 't') goto end;
  unsigned ch;
  ch = atoi(p);
  if(ch == 0 || ch > TERMO_N_CH) goto end;
  ch -= 1;
  sprintf(buf, "thermo_result('ok', %d, %u);", termo_state[ch].value, termo_state[ch].status);
end:
  tcp_put_tx_body(pkt, (void*)buf, strlen(buf));
  return 0;
}

int termo_http_set_data(void)
{
  http_post_data((void*)termo_setup, sizeof termo_setup);
  EEPROM_WRITE(&eeprom_termo_setup, termo_setup, sizeof termo_setup);
  // immediately restart periodic traps with new periods
  for(int i=0; i<TERMO_N_CH; ++i)
  {
    termo_state[i].next_time = sys_clock() + termo_setup[i].trap_delay * 1000; // ms
    if(termo_state[i].status != 0) // if failed, don't compare T
      check_termo_status(i); // recalculate T status (low-norm-high)
  }
#ifdef OW_MODULE
  ow_restart();
#endif
  http_redirect("/termo.html");
  return 0;
}


HOOK_CGI(termo_data,  (void*)termo_http_get_data,  mime_js,  HTML_FLG_GET | HTML_FLG_NOCACHE );
HOOK_CGI(termo_get,   (void*)termo_http_get_data,  mime_js,  HTML_FLG_GET | HTML_FLG_NOCACHE );
HOOK_CGI(thermo,      (void*)termo_http_get_cgi,   mime_js,  HTML_FLG_GET | HTML_FLG_NOCACHE );
HOOK_CGI(termo_set,   (void*)termo_http_set_data,  mime_js,  HTML_FLG_POST );

#endif // ifdef HTTP_MODULE

// #warning "minor bug/wrong feature: > < >= <= on threshold, +/- 1 deg. relative to threshold"

void check_termo_status(int ch)
{
  struct termo_state_s *termo = &termo_state[ch];
  struct termo_setup_s *setup = &termo_setup[ch];
  int new_state, bottom, top;
  top = setup->top;
  bottom = setup->bottom;
  // histerezis
  switch(termo->status)
  {
  case 1: // low
    bottom += TERMO_HYST;
    top    += TERMO_HYST;
    break;
  case 2: // norm
    bottom -= TERMO_HYST;
    top    += TERMO_HYST;
    break;
  case 3: // high
    bottom -= TERMO_HYST;
    top    -= TERMO_HYST;
    break;
  }
  if     (termo->value <= bottom)    new_state = 1; // low //// updated >= <= : swithes when hits threshold
  else if(termo->value >= top  )     new_state = 3; // high
  else                               new_state = 2; // norm
  termo->status = new_state;
}

void termo_i2c_scan(void) // TCN75A
{
  struct termo_state_s *termo;
  /* struct termo_setup_s *setup; */
  int n;

  for(n=0, termo = termo_state; n<TERMO_N_CH; ++n, ++termo)
  {
#ifdef OW_MODULE
    if(termo_setup[n].ow_addr[0] != 0) continue;
#endif
#if defined(SW_I2C_MODULE) || defined(HW_I2C_MODULE)
    signed char buf[2];
    unsigned char b;
    b = 1; // point to config reg
    TERMO_IIC_WRITE( TERMO_SLA | (n<<1), &b, 1); // write pointer
    if(!TERMO_IIC_ACK) goto failed;
    TERMO_IIC_READ( TERMO_SLA | (n<<1), &b, 1); // read config reg
    if(!TERMO_IIC_ACK) goto failed;
    if(b & 1)
    { // TCN75 is in standby mode
      // switch sensor on, read temperature on next termo_exec() call
      buf[0] = 1; // point to config reg
      buf[1] = 0; // config reg = 0
      TERMO_IIC_WRITE( TERMO_SLA | (n<<1), (unsigned char*)buf, 2); // write config = 0
      goto failed; // just now, no data available
    }
    b = 0; // point to temperature reg
    TERMO_IIC_WRITE( TERMO_SLA | (n<<1), &b, 1); // write pointer
    if(!TERMO_IIC_ACK) goto failed;
    TERMO_IIC_READ( TERMO_SLA | (n<<1), (unsigned char*)buf, 2); // read temperature
    if(!TERMO_IIC_ACK) goto failed;
    termo->value = buf[0]; // high 8 bits of 9-bit value, signed, 1 deg C
    check_termo_status(n); // set termo->status
    continue;

  failed:
#endif // SW_I2C_MODULE, HW_I2C_MODULE
    termo->status = 0;
    termo->value = 0;
  }
}

void termo_log(int ch)
{
  char *stat, temperature[24];
  sprintf(temperature, " (%d.0C)", termo_state[ch].value);
#if PROJECT_CHAR=='E'
  switch(termo_state[ch].status)
  {
  case 1: stat = "too cold"; break;
  case 2: stat = "ok"; break;
  case 3: stat = "too hot"; break;
  default: stat = "no sensor"; temperature[0] = 0;  break;
  }
  log_printf("Thermo: chan.%d%s: %s%s.",
      ch+1, quoted_name(termo_setup[ch].name),
      stat,
      temperature);
#else
  switch(termo_state[ch].status)
  {
  case 1: stat = "ниже нормы"; break;
  case 2: stat = "в норме"; break;
  case 3: stat = "выше нормы"; break;
  default: stat = "сбой"; temperature[0] = 0;  break;
  }
  log_printf("Термо: кан.%d%s: %s%s",
      ch+1, quoted_name(termo_setup[ch].name),
      stat,
      temperature);
#endif
}


void termo_exec(void)
{
  if(sys_clock_100ms > termo_scan_time)
  {
    if(i2c_accquire('T'))
    {
      termo_i2c_scan();
      i2c_release('T');
      ////// ow_scan(); // now called in ow.c
      termo_scan_time = sys_clock_100ms + TERMO_READ_PERIOD / 100;
    }
  }

  static unsigned char unload_cnt;
  if(++unload_cnt < 29) return; // cpu load limiting
  unload_cnt = 0;

  systime_t time = sys_clock();
  int n;
  struct termo_state_s *termo = termo_state;
  struct termo_setup_s *setup = termo_setup;
  for(n=0; n<TERMO_N_CH;  ++n, ++termo, ++setup)
  {
    if (termo->status != termo->prev_status)
    {
#     ifdef NOTIFY_MODULE
        unsigned mask = 0;
        char *stat = "";
        struct range_notify_s *nf = &thermo_notify[n];
#       if PROJECT_CHAR == 'E'
        switch(termo->status)
        {
        case 0: mask = nf->fail; stat = "failed"; break;
        case 1: mask = nf->low; stat = "below safe range"; break;
        case 2: mask = nf->norm; stat = "in safe range"; break;
        case 3: mask = nf->high; stat = "above safe range"; break;
        }
        if(termo->status == 0)
          notify(mask, "Thermo: ch.%d%s - sensor is absent or failed", n+1, quoted_name(setup->name));
        else
          notify(mask, "Thermo: ch.%d%s %+dC, %s (%d..%dC)", n+1, quoted_name(setup->name),
             termo->value, stat, setup->bottom, setup->top);
#       else // E
        switch(termo->status)
        {
        case 0: mask = nf->fail; stat = "недоступен"; break;
        case 1: mask = nf->low; stat = "ниже нормы"; break;
        case 2: mask = nf->norm; stat = "в норме"; break;
        case 3: mask = nf->high; stat = "выше нормы"; break;
        }
        if(termo->status == 0)
          notify(mask, "Термо: кан.%d%s - датчик отсутствует или неисправен", n+1, quoted_name(setup->name));
        else
          notify(mask, "Термо: кан.%d%s %+dC, %s (%d..%dC)", n+1, quoted_name(setup->name),
             termo->value, stat, setup->bottom, setup->top);
#       endif // E
        if(mask & NOTIFY_TRAP) termo_send_trap(n);
#       ifdef SMS_MODULE
          if(mask & NOTIFY_SMS) sms_thermo_event(n);
#       endif
#     else // NOTIFY_MODULE
        termo_log(n);
        termo_send_trap(n);
#       ifdef SMS_MODULE
          sms_thermo_event(n);
#       endif
#     endif // NOTIFY_MODULE
      termo->prev_status = termo->status;
      termo->next_time = time + setup->trap_delay * 1000; // ms
    }

#   ifndef NOTIFY_MODULE
    if(setup->trap_delay) // периодич. посылка трапа, срабатывает по заданному периоду, вызывать часто
    {
      if(time > termo->next_time)
      {
        // termo_log(n); // be removed to conserve flash resource // LBS 8.07.2010
        termo->next_time = time + setup->trap_delay * 1000; // ms
        termo_send_trap(n);
      }
    }
#   endif
  } // for(;;)

} // termo_exec()

int termo_snmp_get(unsigned id, unsigned char *data)
{
  unsigned ch = snmp_data.index - 1;
  if(ch >= TERMO_N_CH) return SNMP_ERR_NO_SUCH_NAME;

  int val = 0;
  switch(id&0xfffffff0)
  {
  case 0x8810: val = ch + 1; break;
  case 0x8820: val = termo_state[ch].value; break;
  case 0x8830: val = termo_state[ch].status; break;
  case 0x8840: val = termo_setup[ch].bottom; break;
  case 0x8850: val = termo_setup[ch].top; break;
  case 0x8860:
    // t_memo, octet string
    snmp_add_asn_obj(SNMP_TYPE_OCTET_STRING,
                     termo_setup[ch].name[0],
                     &termo_setup[ch].name[1]);
    return 0;
  }
  if(val > 0x7FFF) val = 0x7FFF;
  snmp_add_asn_integer(val);
  return 0;
}


const unsigned char termo_enterprise[] =
// .1.3.6.1.4.1.25728.8800.2
{SNMP_OBJ_ID, 11, // ASN.1 type/len, len is for 'enterprise' trap field
0x2b,6,1,4,1,0x81,0xc9,0x00,0xc4,0x60,2}; // OID for "enterprise" in trap msg

unsigned char termo_trap_data_oid[] =
{0x2b,6,1,4,1,0x81,0xc9,0x00,0xc4,0x60,2, // like table entry
0,0}; // two last oid components (pre-last is variable, last is .0 i.e. scalar data)

void termo_add_vbind_integer(unsigned char last_oid_component, int value)
{
  unsigned seq_ptr = snmp_add_seq();
  termo_trap_data_oid[sizeof termo_trap_data_oid - 2] = last_oid_component;
  snmp_add_asn_obj(SNMP_OBJ_ID, sizeof termo_trap_data_oid, termo_trap_data_oid);
  snmp_add_asn_integer(value);
  snmp_close_seq(seq_ptr);
}

/*
ch - zero-based channel number (0..7)
*/
void termo_make_trap(int ch)
{
  struct termo_state_s *termo = &termo_state[ch];
  struct termo_setup_s *setup = &termo_setup[ch];
  unsigned seq_ptr;

  snmp_create_trap((void*)termo_enterprise);
  if(snmp_ds == 0xff) return; // if can't create packet

  termo_add_vbind_integer(1, ch+1);          // T_CHANNEL
  termo_add_vbind_integer(2, termo->value);  // T_VALUE
  termo_add_vbind_integer(3, termo->status); // T_STATUS
  termo_add_vbind_integer(4, setup->bottom); // T_LOW
  termo_add_vbind_integer(5, setup->top);    // T_HIGH

  seq_ptr = snmp_add_seq();
  termo_trap_data_oid[sizeof termo_trap_data_oid - 2] = 6;  // T_MEMO
  snmp_add_asn_obj(SNMP_OBJ_ID, sizeof termo_trap_data_oid, termo_trap_data_oid);
  snmp_add_asn_obj(SNMP_TYPE_OCTET_STRING,
    setup->name[0], setup->name+1);
  snmp_close_seq(seq_ptr);
}

void termo_send_trap(int ch)
{
#ifndef NOTIFY_MODULE
  // filter unwanted traps
  int send = 0;
  struct termo_state_s *termo = &termo_state[ch];
  struct termo_setup_s *setup = &termo_setup[ch];
  if(termo->prev_status == 0 || termo->status == 0) send = 1;   // if 'failed' status transition, send always
  if(termo->status == 1 && setup->trap_low) send = 1;
  if(termo->status == 2 && setup->trap_norm) send = 1;
  if(termo->status == 3 && setup->trap_high) send = 1;
  if(!send) return;
#endif // NOTIFY_MODULE

  // snmp_send_trap() calls udp_send_packet()
  // udp_send_packet() kills sent packet
  // so, make new trap message every time

  if(valid_ip(sys_setup.trap_ip1)) { termo_make_trap(ch); snmp_send_trap(sys_setup.trap_ip1); }
  if(valid_ip(sys_setup.trap_ip2)) { termo_make_trap(ch); snmp_send_trap(sys_setup.trap_ip2); }
 }

void termo_reset_params(void)
{
  int n;
  struct termo_setup_s *setup;
  util_fill((unsigned char*)termo_setup, sizeof termo_setup, 0);
  for(n=0, setup=termo_setup; n<TERMO_N_CH; ++n, ++setup)
  {
   setup->name[0]      = 0;
   setup->top          = 60;
   setup->bottom       = 10;
   setup->trap_delay   = 0;
   setup->trap_low     = 0;
   setup->trap_norm    = 0;
   setup->trap_high    = 0;
  }
  EEPROM_WRITE(&eeprom_termo_setup, termo_setup, sizeof termo_setup);
  EEPROM_WRITE(&eeprom_termo_signature, &termo_signature, 4);
}

void termo_init(void)
{
  unsigned sign;
  EEPROM_READ(&eeprom_termo_signature, (unsigned char*)&sign, 4);
  if(sign != termo_signature) termo_reset_params();
  EEPROM_READ(&eeprom_termo_setup, termo_setup, sizeof termo_setup);
  memset(termo_state, 0, sizeof termo_state);  // i.e. status=0 for all channels
  termo_scan_time = sys_clock_100ms + 30;
  /*
  // send START/STOP
  swi2c_scl(TERMO_I2C,1);
  delay(1);
  swi2c_sda(TERMO_I2C,0);
  delay(10);
  swi2c_sda(TERMO_I2C,1);
  delay(10);
  */
}

void termo_event(enum event_e event)
{
  switch(event)
  {
  case E_EXEC:
    termo_exec();
    break;
  case E_INIT:
    termo_init();
    break;
  case E_RESET_PARAMS:
    termo_reset_params();
    break;
  }
}

#endif
