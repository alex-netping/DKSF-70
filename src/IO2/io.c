/*
*P.V.Lyubasov
*v2.2
*01.2010
*
*v2.3-50-PCA
*2.07.2010
v2.4-50-PCA
*6.07.2010 by LBS
*  added io_state.pulse_counter, added external for io_state in .h
*  removed some legacy
*  table index via snmp_data.index
*20.07.2010
v2.5-50-PCA
*  on_event bits macro
*v2.6-50.77
* changed TRAP enterprise value to lightcom.8900.2 for MIB v2 conformance
*v2.7-53
* additional variables in io Trap
*v2.8-53
*  bugfix in io line driver
*v2.8-51,52
* 23.11.2010
*  external command api io_set_line() added
*  sms notification added
*  removed io pin drivers (moved to project.c)
*v2.9 merged
*1.02.2011
*  bugfix, io_set_level() replaced to io_set_line() in snmp set handler
*  minor bugfix, added .0 at the end of scalar oids in IO trap
*  removed sending trap to second ip for NetPings
*v2.10-253
*7.02.2011
*  4th io line data in trap
*v2.10-200
*24.02.2011
*  io_start_pulse() API except dksf50, webinterface, snmp i-face, updated 'all io lines' in Trap for 53,253 only
*v2.10-50
*5.03.2011
*  English log
*v2.11-50
*11.03.2011
*  Single pulse HTTP interface ported to dksf50
*v2.11-52
*10.09.2011
*  integration with logic.c
*  rewrite of single pulse web interface
*  param reset of extended io lines IO3, IO4 on DKST 51.1.7 hardware
*v2.12-200
*11.10.2011
*  restored second trap (used old trap scheme)
*v2.12-52
*28.05.2012
*  io.cgi url-encoded interface
*v2.12-60
*5.06.2012
*  dksf60 support (dkst 50.25)
*  io_set_pulse.cgi bugfix for dksf50 (double io web page)
*  rewrited http part to make single io.html on 16-line device
*  io_http_set_single_pulse() for XmlHttpRequest-ed call
*  second Trap destination ip
*v2.13-201
*25.10.2011
*  English log message
*v2.14-52
*29.03.2013
*  bugfix url-encoded interface, wrong API use, new code copied from 2.13-50
*v2.15-48
*12.04.2013
* cosmetic edit, quoted_name() used, logic.c reference under #ifdef LOGIC_MODULE
* bugfix io_snmp_get(), memo
*v2.13-60
*20.03.2013
*  using io_reload_pull_register() after io lines setting save
*v2.14-60
*30.04.2013
*  some rewrite for quick restore of IO lines on hot restart
*v2.14-50
*21.06.213
*  Use logic output flag in io_get.cgi output, model-independent web page update with logic output mode for IO line
*v2.15-52
*15.08.2013
*  flip output level command via url (io.cgi&io1=f), snmp (set level=-1)
*  add SSE notification
*  minor bugfix in snmp_io_set (incorrect error messages was fixed)
*v2.16-70
*3.07.2013
*  dkst70 support
*v2.16-48
*17.07.2013
  minor rework, sse and js
v2.17-70
14.05.2014
  notify support
v2.18-70
28.07.2014
  json-p url-encoded control (different implementation from DKSF253)
v2.18-253
28.10.2014
  pulse_counter bugfix if used with notify.c
v2.19-60
9.10.2014
  io line level legends
v2.19-48
5.11.2014
  bugfixed io_exec() logic, order of setting of io_registered_state
v2.20-60
6.11.2014
  bugfix, BAD DATA returned on SNMP SET of npIoPulseCounter, wrong data type check
20.11.2014
v2.21-253
  more bugfixed io_exec() logic, illegal level in io_registered_state after direction switch
v2.22-60
21.11.2014
  bugfix io_snmp_set(), npIoLine
v2.23-70
22.12.2014
  removed io_state_registered interface, actual io state is routed to io_state[ch].level_filtered
v2.23-48
24.12.2014
  alternative rewrite of io_snmp_set() for npIoPulseCounter bugfix
*/


#include "platform_setup.h"
#ifdef   IO_MODULE

#include "eeprom_map.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "plink.h"


#pragma location="DATA_Z"
__no_init struct io_setup_s io_setup[IO_MAX_CHANNEL];
#pragma location="DATA_Z"
__no_init struct io_state_s io_state[IO_MAX_CHANNEL];

const unsigned io_signature = 533466;
#if IO_MAX_CHANNEL == 4
const unsigned io_signature_io34 = 533479;
#endif

void io_extender_init(void);
void io_restart(void);

unsigned char io_switch_direction = 0;
unsigned io_registered_state;
char io_send_sse_flag;

void io_send_trap(int ch);
void io_start_pulse_ms(unsigned ch, unsigned time_ms);
unsigned io_get_state_bitmap(void);

unsigned io_http_get_data(unsigned pkt, unsigned more_data)
{
  char buf[768];
  char *dest = buf;

  if(more_data == 0)
  {
    dest+=sprintf((char*)dest,"var packfmt={");
    PLINK(dest, io_setup[0], name);
    PLINK(dest, io_setup[0], direction);
    PLINK(dest, io_setup[0], delay);
    PLINK(dest, io_setup[0], level_out);
    PLINK(dest, io_setup[0], on_event);     // LBS 18.06.2010
    PLINK(dest, io_setup[0], pulse_dur);    // LBS 24.02.2011
    PSIZE(dest, (char*)&io_setup[1] - (char*)&io_setup[0]); // must be the last // alignment!
    *dest++ = '}';
    *dest++ = ';';
#ifdef LOGIC_MODULE
    dest += sprintf(dest, " var use_logic_output=%u;", LOGIC_IO_LINES_NUMBER); // 21.06.2013
#endif
    dest+=sprintf((char*)dest, "var data_status=%u; var data=[", io_get_state_bitmap());
#if PROJECT_MODEL == 50 || PROJECT_MODEL == 60
    switch(http.page->name[2]) // '/cNio_get.html'
    {
    case '1': more_data = 0; break;
    case '9': more_data = 8; break;
    }
#endif
  }

  unsigned last_data_n;
#if PROJECT_MODEL == 50 || PROJECT_MODEL == 60
  switch(http.page->name[2]) // '/cNio_get.html'
  {
  case '1': last_data_n = 7; break;
  case '9': last_data_n = 15; break;
  default: return 0;
  }
#else
  last_data_n = IO_MAX_CHANNEL-1;
#endif
  int n;
  struct io_setup_s *setup = &io_setup[more_data];
  for(n=more_data; n<=last_data_n; ++n, ++setup)
  {
    *dest++ = '{';
    PDATA_PASC_STR(dest, (*setup), name);
    PDATA(dest, (*setup), direction);
    PDATA(dest, (*setup), delay);
    PDATA(dest, (*setup), level_out);
    PDATA(dest, (*setup), on_event);
    PDATA(dest, (*setup), pulse_dur);
    dest += sprintf(dest, "level:%d,", io_state[n].level_filtered); // no trailing comma // 22.12.2014 changed from level_in:
#ifdef NOTIFY_MODULE
    dest += sprintf(dest, "nf_legend_high:\"%s\",nf_legend_low:\"%s\"},",
          io_notify[n].legend_high + 1, io_notify[n].legend_low + 1);
#endif // NOTIFY_MODULE
    if(n == last_data_n) // last channel
    {
      --dest; // clear last comma
      *dest++ = ']'; *dest++ = ';';
      //*dest=0;
      more_data = 0;
      break;
    }
    if(dest > buf + sizeof buf - 180)
    {
      more_data = n + 1;
      break;
    }
  }
  tcp_put_tx_body(pkt, (unsigned char*)buf, dest - buf);
  return more_data;
}


#if PROJECT_MODEL == 50 || PROJECT_MODEL == 60

unsigned io_http_set_data(void)
{
  unsigned ch;
  switch(http.page->name[2]) // '/cNio_set.cgi'
  {
  case '1': ch = 0; break;
  case '9': ch = 8; break;
  default: return 0;
  }
  const unsigned half_setup_size = (char*)&io_setup[7] - (char*)&io_setup[0] + sizeof io_setup[7]; // alignment-wise
  http_post_data((void*)&io_setup[ch], half_setup_size);
  EEPROM_WRITE(&eeprom_io_setup[ch], &io_setup[ch], half_setup_size);
  io_restart();
  http_redirect(ch==0 ? "/cio.html?ch=1" : "/cio.html?ch=9");
  return 0;
}

#else

unsigned io_http_set_data(void)
{
  http_post_data((void*)&io_setup, sizeof io_setup);
  EEPROM_WRITE(&eeprom_io_setup, io_setup, sizeof io_setup);
  io_restart();
  http_redirect("/io.html");  ////////////// proj-dependant
  return 0;
}

#endif

unsigned io_http_set_single_pulse(void)
{
  struct {
    unsigned char ch;
    unsigned char duration;
  } data;
  data.duration = 0xaa;
  http_post_data((void*)&data, sizeof data);
  if(data.duration != 0xaa && data.ch < IO_MAX_CHANNEL)
  {
    struct io_setup_s *setup = &io_setup[data.ch];
    if(setup->pulse_dur != data.duration) // save duration
    {
      setup->pulse_dur = data.duration;
      EEPROM_WRITE(&eeprom_io_setup[data.ch].pulse_dur, &setup->pulse_dur, sizeof eeprom_io_setup[0].pulse_dur);
    }
    io_start_pulse(data.ch);
  }
  //// http_redirect("/io.html");
  http_reply(200, ""); // in 48/52/201/202 html used XHR, not submit
  return 0;
}

unsigned io_http_get_cgi(unsigned pkt, unsigned more_data)
{
  unsigned n, ch;
  char *p = req_args;
  char result[64] = "io_result('error');";
  if(*p++ != 'i') goto end;
  if(*p++ != 'o') goto end;
  n = atoi(p);
  if(n == 0) goto end;
  ++p;  if(n > 9) ++p; // skip number
  ch = n - 1;
  if(ch >= IO_MAX_CHANNEL) goto end;
  if(*p == 0)
  { // 'read' request
    sprintf(result, "io_result('ok', -1, %u, %u);", io_state[ch].level_filtered, io_state[ch].pulse_counter); // 22.12.2014 changed from io_regidtered_state
  }
  else
  {
    if(*p++ != '=') goto end;
    char m = *p++;
    if     (m == '0') io_set_line(ch, 0);
    else if(m == '1') io_set_line(ch, 1);
    else if(m == 'f')
    {
      if(*p == 0) io_set_line(ch, !io_setup[ch].level_out);
      else if(*p == ',')
      {
        unsigned t;
        t = atoi(p + 1); // skip ,
        if(t == 0 || t > 1800) goto end;
        io_start_pulse_ms(ch, t * 1000);
      }
      else goto end;
    }
    else goto end;
    strcpy(result, "io_result('ok');");
  }
end:
  tcp_put_tx_body(pkt, (void*)result, strlen(result));
  return 0;
}

#if PROJECT_MODEL == 50 || PROJECT_MODEL == 60
HOOK_CGI(c1io_get,     (void*)io_http_get_data,          mime_js,  HTML_FLG_GET | HTML_FLG_NOCACHE );
HOOK_CGI(c1io_set,     (void*)io_http_set_data,          mime_js,  HTML_FLG_POST );
HOOK_CGI(c9io_get,     (void*)io_http_get_data,          mime_js,  HTML_FLG_GET | HTML_FLG_NOCACHE );
HOOK_CGI(c9io_set,     (void*)io_http_set_data,          mime_js,  HTML_FLG_POST );
#else
HOOK_CGI(io_get,       (void*)io_http_get_data,          mime_js,  HTML_FLG_GET | HTML_FLG_NOCACHE );
HOOK_CGI(io_set,       (void*)io_http_set_data,          mime_js,  HTML_FLG_POST );
#endif
HOOK_CGI(io_set_pulse, (void*)io_http_set_single_pulse,  mime_js,  HTML_FLG_POST );
HOOK_CGI(io,           (void*)io_http_get_cgi,           mime_js,  HTML_FLG_GET | HTML_FLG_NOCACHE );

void io_restart(void)
{
  io_switch_direction = 1;
}

void io_reset_params(void)
{
  int n;
  struct io_setup_s *setup;
  util_fill((unsigned char*)io_setup, sizeof io_setup, 0);
  for(n=0, setup=io_setup; n<IO_MAX_CHANNEL; ++n, ++setup)
  {
   setup->delay  = 500;
   setup->pulse_dur = 10;
  }
  EEPROM_WRITE(&eeprom_io_setup, io_setup, sizeof io_setup);
  EEPROM_WRITE(&eeprom_io_signature, &io_signature, sizeof eeprom_io_signature);
}

#if IO_MAX_CHANNEL == 4
// reset of user io lines 3,4 for update from 2 io lines to 4
void io_reset_params_io34_only(void)
{
  struct io_setup_s *setup;
  for(int n=2; n<4; ++n)
  {
    setup = &io_setup[n];
    util_fill((void*)setup, sizeof *setup, 0);
    setup->delay = 500;
    setup->pulse_dur = 10;
  }
  EEPROM_WRITE(&eeprom_io_setup[2], &io_setup[2], (char*)&eeprom_io_setup[4] - (char*)&eeprom_io_setup[2]);
  EEPROM_WRITE(&eeprom_io_signature_io34, &io_signature_io34, sizeof eeprom_io_signature_io34);
}
#endif

void io_init(void)
{
#ifdef IO_USE_PCA9535
  io_extender_init();
#elif PROJECT_MODEL == 60 || PROJECT_MODEL == 70 || PROJECT_MODEL == 71
 // do nothing, io_hardware_init() is called in init_basic_modules() for quick IO line restore on hot (re)start
#else
  io_hardware_init();
#endif
  unsigned sign;
  EEPROM_READ(&eeprom_io_signature, &sign, sizeof sign);
  if(sign != io_signature)
  {
    io_reset_params();
  }
  else
  {
#if IO_MAX_CHANNEL == 4
    // check 'update' sign, reset setup of user io lines 3,4
    EEPROM_READ(&eeprom_io_signature_io34, &sign, sizeof sign);
    if(sign != io_signature_io34) io_reset_params_io34_only();
#endif
  }
  EEPROM_READ(&eeprom_io_setup, io_setup, sizeof io_setup);
  // safe pulse_dur value, it's new field from uninitialized reserved bytes
  for(int n=0; n<IO_MAX_CHANNEL; ++n)
    if(io_setup[n].pulse_dur == 0)
      io_setup[n].pulse_dur = 10; // 1s
  //
  io_restart();
}

void io_log(int ch)
{
#if PROJECT_CHAR == 'E'
  log_printf("Input/output: line %d%s: %s",
#else
  log_printf("Input/output: линия %d%s: %s",
#endif
     ch + 1,
     quoted_name(io_setup[ch].name),
     io_state[ch].level_filtered ? "0->1" : "1->0");
}

void io_send_notification(unsigned ch)
{
#ifdef NOTIFY_MODULE
  if(ch >= IO_MAX_CHANNEL) return;
  unsigned lvl = io_state[ch].level_filtered;
  struct binary_notify_s *ionf = &io_notify[ch];
  unsigned mask = lvl ? ionf->high : ionf->low;
  notify(mask, "IO%u=%u %s %s",
      ch + 1, lvl,
      io_setup[ch].name + 1,
      lvl ? ionf->legend_high + 1 : ionf->legend_low + 1 );
#ifdef SMS_MODULE
  if(mask & NOTIFY_SMS) sms_io_event(ch);
#endif
  if(mask & NOTIFY_TRAP) io_send_trap(ch);
#else // NOTIFY_MODULE
  unsigned mask = io_setup[ch].on_event;
  if(mask & IO_LOG_TRANSITION) io_log(ch); // 0x04 = enable logging
  if(lvl)
  {
    if(mask & IO_TRANSITION_0_TO_1)
      io_send_trap(ch);
  }
  else
  {
    if(mask & IO_TRANSITION_1_TO_0)
      io_send_trap(ch);
  }
#ifdef SMS_MODULE
  sms_io_event(ch);
#endif
#endif // NOTIFY_MODULE
}

void io_register_state(unsigned ch, int state) // from 22.12.2014 must be used for 'reboot persistance' needs
{
  if(state) io_registered_state |=  (1U<<ch);
  else      io_registered_state &=~ (1U<<ch);
}

unsigned io_get_state_bitmap(void) // 22.12.2014
{
  unsigned state = 0;
  for(int i=0; i<IO_MAX_CHANNEL; ++i)
    state |= io_state[i].level_filtered << i;
  return state;
}

void io_exec(void)
{
  struct io_state_s *io;
  struct io_setup_s *setup;
  int io_ch_n, output;
  unsigned char b;
  systime_t time;

  for(io_ch_n=0, io=io_state, setup=io_setup;
      io_ch_n<IO_MAX_CHANNEL;
      ++io_ch_n, ++io, ++setup)
  {
    time = sys_clock(); // inside of cycle for more precision on filtering
    if(setup->direction)
    { // output
      output = setup->level_out;
#ifdef LOGIC_MODULE // 3.10.2014 moved here; now io line reset pulse is effective 'above' logic
      if(setup->direction == 2) // io line function = LOGIC module output
        output = (logic_io_output >> io_ch_n) & 1;
#endif
      if(time < io->pulse_stop_time) output = !output;
      io_set_level(io_ch_n, output);
      if(io_switch_direction)
        io_set_dir(io_ch_n, 1);
      if(io->level_filtered != output)
      {
        io->level_filtered = output; // 22.12.2014, route actual state to level_filtered
        io_send_sse_flag = 1;
        io_send_notification(io_ch_n);
      }
      io_register_state(io_ch_n, output);
    }
    else
    { // input
      if(io_switch_direction)
      {
        io_set_dir(io_ch_n, 0);
        delay_us(300);
      }
      b = io_read_level(io_ch_n);
      if(b != io->level_in)
      {
        io->time_of_change = time + setup->delay; // ms
        io->level_in = b;
      }
      if(time > io->time_of_change)
      {
        if(io->level_filtered != io->level_in)
        {
          io->level_filtered = io->level_in; // drop event!
          io_register_state(io_ch_n, io->level_filtered);
          io_send_sse_flag = 1;
          if(io->level_filtered != 0) ++ io->pulse_counter;
          io_send_notification(io_ch_n);
        }
      }
      io_register_state(io_ch_n, io->level_filtered); // returned as is 14.11.2014
    } // if
  } // for io ch
#if PROJECT_MODEL == 60 || PROJECT_MODEL == 70 || PROJECT_MODEL == 71
  io_pin_mark_saved();
#endif
#if PROJECT_MODEL == 60
  if(io_switch_direction)
    io_reload_pull_register();
#endif
  io_switch_direction = 0;

  // sending SSE updates
  unsigned pkt;
  if(io_send_sse_flag)
  {
    if(http_can_send_sse())
    {
      pkt = tcp_create_packet_sized(sse_sock, 256);
      if(pkt != 0xff)
      {
        char s[64];
        unsigned n = sprintf(s,"event: io_state\n""data: %u\n\n", io_get_state_bitmap());
        tcp_tx_body_pointer = 0;
        tcp_put_tx_body(pkt, (void*)s, n);
        tcp_send_packet(sse_sock, pkt, n);
        io_send_sse_flag = 0;
      } // if pkt ok
    } // if socket can send
  } // sse update

} // io_exec()

void io_start_pulse_ms(unsigned ch, unsigned time_ms)
{
  io_state[ch].pulse_stop_time = sys_clock() + time_ms;
}

void io_start_pulse(unsigned ch)
{
  io_start_pulse_ms(ch, io_setup[ch].pulse_dur * 100);
}

/* returns last number of oid sequence (=0 for scalar data)
   range is 14 bit (2 bytes in oid binary)
*/
////// defined in TERMO.c (temp. hack)
int get_oid_index(void);



int io_snmp_get(unsigned id, unsigned char *data)
{
  /*
  unsigned ch = (id & 0x000f) -  1; // legacy ch index
  */
  unsigned ch = snmp_data.index - 1;
  if(ch >= IO_MAX_CHANNEL) return SNMP_ERR_NO_SUCH_NAME;

  int val = 0;
  switch(id & 0xfffffff0)
  {
  case 0x8910: val = ch + 1; break;
  case 0x8920: val = io_state[ch].level_filtered; break;
  case 0x8930: val = io_setup[ch].level_out; break;
  case 0x8960:    // t_memo, octet string
    snmp_add_asn_obj(SNMP_TYPE_OCTET_STRING, io_setup[ch].name[0],  io_setup[ch].name+1); // 12.04.2013
    return 0;
  /// case 0x8990: val = io_state[ch].pulse_counter & 0x7fffffff; break; // 18.06.2010
  case 0x8990: // npIoPulseCounter
    snmp_add_asn_unsigned(SNMP_TYPE_COUNTER32, io_state[ch].pulse_counter); // LBS 14.08.2010
    return 0;
  case 0x89b0: val = io_setup[ch].pulse_dur * 100; break;
  case 0x89c0: val = io_state[ch].pulse_stop_time > sys_clock(); break;
  default:
    return SNMP_ERR_NO_SUCH_NAME;
  }
  snmp_add_asn_integer(val);
  return 0;
}

int io_snmp_set(unsigned id, unsigned char *data)
{
  int val = 0;
  unsigned uval = 0;
  unsigned char t;

  unsigned ch = snmp_data.index - 1;
  if(ch >= IO_MAX_CHANNEL) return SNMP_ERR_NO_SUCH_NAME;

  switch(id&0xfffffff0)
  {
  case 0x8930:
    if(*data != SNMP_TYPE_INTEGER) return SNMP_ERR_BAD_VALUE;
    asn_get_integer(data, &val);
    if(val == -1) val = !io_setup[ch].level_out; // 15.08.2013
    io_set_line(ch, val);
    snmp_add_asn_integer(val);
    break;
  case 0x8990: // npIoPulseCounter
    if(*data == SNMP_TYPE_INTEGER)
    {
      asn_get_integer(data, &val);
      if(val != 0) return SNMP_ERR_BAD_VALUE;
      io_state[ch].pulse_counter = val;
      snmp_add_asn_integer(val);
    }
    else if(*data == SNMP_TYPE_COUNTER32)
    {
      unsigned uval;
      asn_get_unsigned(data, &uval); // only 0 accepted
      if(uval != 0) return SNMP_ERR_BAD_VALUE;
      io_state[ch].pulse_counter = uval;
      snmp_add_asn_unsigned(SNMP_TYPE_COUNTER32, uval);
    }
    else
      return SNMP_ERR_BAD_VALUE;
    break;
  case 0x89b0: // npIoSinglePulseDuration
    if(*data == SNMP_TYPE_INTEGER)
    {
      asn_get_integer(data, &val);
      t = (val + 50) / 100; // convert from ms to 100ms intervals
      if(t == 0) ++t;
      io_setup[ch].pulse_dur = t;
      EEPROM_WRITE(&eeprom_io_setup[ch].pulse_dur, &t, sizeof eeprom_io_setup[0].pulse_dur);
      snmp_add_asn_integer(val);
    }
    else
      return SNMP_ERR_BAD_VALUE;
    break;
  case 0x89c0: // npIoSinglePulseStart
    if(data[0] == SNMP_TYPE_INTEGER && data[1] == 1 && data[2] == 1) // ASN Integer 1
    {
      io_start_pulse(ch);
      snmp_add_asn_integer(1);
    }
    else
      return SNMP_ERR_BAD_VALUE;
    break;
  default:
    return SNMP_ERR_READ_ONLY;
  }
  return 0;
}


const unsigned char io_enterprise[] =
// .1.3.6.1.4.1.25728.8900.2
{SNMP_OBJ_ID, 11, // ASN.1 type/len, len is for 'enterprise' trap field
0x2b,6,1,4,1,0x81,0xc9,0x00,0xc5,0x44,2}; // OID for "enterprise" in trap msg

unsigned char io_trap_data_oid[] =
{0x2b,6,1,4,1,0x81,0xc9,0x00,0xc5,0x44,2, // table entry
0,0}; // last oid components (pre-last is variable, last is .0 for scalar oid)


void io_add_vbind_integer(unsigned char last_oid_component, int value)
{
  unsigned seq_ptr;
  seq_ptr = snmp_add_seq();
  io_trap_data_oid[sizeof io_trap_data_oid - 2] = last_oid_component;
  snmp_add_asn_obj(SNMP_OBJ_ID, sizeof io_trap_data_oid, io_trap_data_oid);
  snmp_add_asn_integer(value);
  snmp_close_seq(seq_ptr);
}

/*
ch - zero-based channel number (0..7)
*/
void io_make_trap(int ch)
{
  struct io_state_s *io = &io_state[ch];
  struct io_setup_s *setup = &io_setup[ch];
  unsigned seq_ptr;

  snmp_create_trap((void*)io_enterprise);
  if(snmp_ds == 0xff) return; // if can't create packet

  io_add_vbind_integer(1, ch+1);   // npIoTrapLineN
  io_add_vbind_integer(2, io->level_filtered);  // npIoTralLevelIn

  seq_ptr = snmp_add_seq();    // npIoTrapMemo
  io_trap_data_oid[sizeof io_trap_data_oid - 2] = 6;
  snmp_add_asn_obj(SNMP_OBJ_ID, sizeof io_trap_data_oid, io_trap_data_oid);
  snmp_add_asn_obj(SNMP_TYPE_OCTET_STRING,
    setup->name[0], setup->name+1);
  snmp_close_seq(seq_ptr);
  // LBS 7.02.2011 DKSF253
#if PROJECT_MODEL==53 || PROJECT_MODEL==253
  for(int i=0; i<IO_MAX_CHANNEL; ++i)
  {
    io_add_vbind_integer(21+i, io_state[i].level_filtered);  // npIoTralLevelIn i
  }
#endif
}


void io_send_trap(int ch)
{
  if(valid_ip(sys_setup.trap_ip1)) { io_make_trap(ch); snmp_send_trap(sys_setup.trap_ip1); }
  if(valid_ip(sys_setup.trap_ip2)) { io_make_trap(ch); snmp_send_trap(sys_setup.trap_ip2); }
}

#ifdef IO_USE_PCA9535

unsigned short ext_out, ext_out_old;
unsigned short ext_in,  ext_in_old;
unsigned short ext_dir, ext_dir_old;

void io_extender_init(void)
{
  ext_out = ext_dir = 0;
  for(int n=0;n<IO_MAX_CHANNEL;++n)
  {
    if(io_setup[n].level_out) ext_out |= 1<<n;
    if(io_setup[n].direction) ext_dir |= 1<<n;
  }
  ext_out_old = ~ ext_out;
  ext_dir_old = ~ ext_dir;
}

void io_extender_exec(void)
{
  unsigned char buf[3];

  static unsigned char cycle = 0;
  if(cycle == 0)
  {
    // 1й цикл и затем иногда прокачать все регистры
    ext_out_old = ~ ext_out;
    ext_dir_old = ~ ext_dir;
  }
  ++cycle;

  buf[0] = 0; // input 0 register index
  hw_i2c_write(0, PCA0, buf, 1);
  hw_i2c_read(0, PCA0,  (void*)&ext_in, 2);

  if(ext_out != ext_out_old)
  {
    buf[0] = 2; // output 0 register
    buf[1] = ext_out & 0xff;
    buf[2] = ext_out >> 8;
    hw_i2c_write(0, PCA0, buf, 3);
    ext_out_old = ext_out;
  }

  if(ext_dir != ext_dir_old)
  {
    buf[0] = 6;  // config 0 register, dir bits are inverse!
    buf[1] = ~ (ext_dir & 0xff);
    buf[2] = ~ (ext_dir >> 8);
    hw_i2c_write(0, PCA0, buf, 3);
    ext_dir_old = ext_dir;
  }
}

__monitor int  io_read_level(unsigned ch)
{
  return ext_in & (1<<ch) ? 1 : 0;
}

__monitor void io_set_level(unsigned ch, unsigned level)
{
  if(level) ext_out |= 1<<ch;
  else ext_out &=~ (1<<ch);
}

__monitor void io_set_dir(unsigned ch, unsigned dir)
{
  if(dir) ext_dir |= 1<<ch;
  else ext_dir &=~ (1<<ch);
}

#endif // IO_USE_PCA9535

void io_set_line(unsigned ch, unsigned level)
{
  if(ch >= IO_MAX_CHANNEL) return;
  if(level != 0) level = 1;
  io_setup[ch].level_out = level;
  // output pin is set in io_exec() main loop
  EEPROM_WRITE(&eeprom_io_setup[ch],
                 &io_setup[ch],
                 sizeof io_setup[0]);
}

void io_event(enum event_e event)
{
  switch(event)
  {
  case E_EXEC:
    #ifdef IO_USE_PCA9535
      io_extender_exec();
    #endif
    io_exec();
    break;
  case E_INIT:
    io_init();
    break;
  case E_RESET_PARAMS:
    io_reset_params();
    break;
  }
}

#endif // IO_MODULE

