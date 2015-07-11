/*
logic v2
v2.0.0
15.08.2011
v2.1.0
23.08.2011
v2.2-52
28.05.2012
  IR command
v2.3-60
15.06.2012
  cur_loop sensor
v2.4-50
21.11.2012
  reset signal actions
  snmp setter integration
v2.5-50
2.04.2013
  modified control of curdet power
v2.4-60
21.05.2013
  dns integration
v2.6-50
21.06.2013
  LOGIC_IO_LINES_NUMBER def in logic_def.h and it's check in logic.c, used in io web page
v2.7-70
4.07.2013
  ping v2 adopted
v2.7-60
12.08.2013
  bugfix in logic_pinger_http_set_data(), wrong macro
  own dns init in logic_pinger_init()
v2.8-52
26.09.2013
  bugfix: ping_state[n].max_retry was not set
v2.9-48
30.10.2013
  merge with dksf50, incl. usable http stuff
  rewrite of strtup and RESET signal support
  DNS compatibility
v2.10-70
11.06.2014
  RH in termostat
  flip support
*/

#include "platform_setup.h"

#ifdef LOGIC_MODULE

#include "eeprom_map.h"
#include "plink.h"
#include <stdio.h>
#include <string.h>

const unsigned logic_signature = 3757700883;
const unsigned tstat_signature = 1002380117;
#ifdef DNS_MODULE
const unsigned logic_pinger_signature = 1608705239;
#else
const unsigned logic_pinger_signature = 1608704239;
#endif

// interface with PWR and IO modules
unsigned short logic_relay_output;
unsigned short logic_io_output;

unsigned char logic_flags;

enum logic_run_state_e logic_run_state;
unsigned logic_delay_time;
signed char logic_reset_front = 0;

struct logic_state_s {
  unsigned char old_input;
  unsigned char output_idx;
} logic_state[LOGIC_MAX_RULES];
struct logic_setup_s logic_setup[LOGIC_MAX_RULES];

struct tstat_setup_s tstat_setup[TSTAT_MAX_CHANNEL];
unsigned char tstat_state[TSTAT_MAX_CHANNEL];

struct logic_pinger_setup_s logic_pinger_setup[LOGIC_MAX_PINGER];
struct logic_pinger_state_s logic_pinger_state[LOGIC_MAX_PINGER];


#if    !defined(LOGIC_IO_LINES_NUMBER) || LOGIC_IO_LINES_NUMBER!=4
#error "Wrong LOGIC_IO_LINES_NUMBER def in logic_def.h!"
#endif

const unsigned char input_id_table[] = { // board-dependant
  0x01,                // Reset signal
  0x10,0x11,0x12,0x13, // IOs // ATTN! check LOGIC_IO_LINES_NUMBER!
  0x20,0x21,           // TStats
  0x30,0x31,           // Pingers
  0x40, // Current Loop sensor Alarm
  0x50, // Current Loop sensor Fail
  0x60  // Current Loop sensor Norm
};

const unsigned char output_id_table[] = { // board-dependant
  0xa0,0xa1,0xa2,0xa3, // IOs // ATTN! check LOGIC_IO_LINES_NUMBER!
  0xb0,0xb1,           // Relays
  0xc0,0xc1,           // snmp setters
  0xd0,0xd1,0xd2,0xd3, // IR Commands 1-4
  0xe0                 // CurLoop power
};

#define LOGIC_MAX_OUTPUTS (sizeof output_id_table)

unsigned char new_logic_output[LOGIC_MAX_OUTPUTS + 1]; // last output is safe dummy for unknown output id

void logic_restart(void);
void tstat_restart(void);
void logic_pinger_restart(void);
void logic_run_once(void);

unsigned logic_http_get_data(unsigned pkt, unsigned more_data)
{
  char buf[1024];
  char *dest = buf;
  if(more_data == 0)
  {
    dest+=sprintf((char*)dest,"var packfmt={");
    PLINK(dest, logic_setup[0], flags);
    PLINK(dest, logic_setup[0], input);
    PLINK(dest, logic_setup[0], condition);
    PLINK(dest, logic_setup[0], action);
    PLINK(dest, logic_setup[0], output);
    PSIZE(dest, sizeof logic_setup[0]); // must be the last // size must be word-aligned!
    dest += sprintf(dest, "}; var data_logic_flags=%u; var data=[", logic_flags);
  }
  struct logic_setup_s *setup;
  for(;more_data<LOGIC_MAX_RULES;)
  {
    setup = &logic_setup[more_data];
    *dest++ = '{';
    PDATA(dest, (*setup), flags);
    PDATA(dest, (*setup), input);
    PDATA(dest, (*setup), condition);
    PDATA(dest, (*setup), action);
    PDATA(dest, (*setup), output);
    --dest; // clear last PDATA-created comma
    *dest++ = '}';
    *dest++ = ',';
    ++more_data;
    if(dest - buf > sizeof buf - 128) break; // data buffer capacity used up to 80%
  }
  if(more_data == LOGIC_MAX_RULES)
  {
    more_data = 0; // reset pumping
    --dest; // remove last comma
    *dest++ = ']';
    *dest++ = ';';
  }
  tcp_put_tx_body(pkt, (unsigned char*)buf, dest - buf);
  return more_data;
}

unsigned logic_http_get_run(unsigned pkt, unsigned more_data) // control of logic running and reset
{
  extern char* util_scan_caseless(char* s, char *sub, char* end, int skip_flag);
  char *data = util_scan_caseless(req, "/logic_run.cgi?", req + 64, 1); // parsing routine from http2.c module
  unsigned char c;
  if(data)
  {
    c = *data;
    if(c=='0') logic_flags &=~ LOGIC_RUNNING;
    if(c=='1') logic_flags |=  LOGIC_RUNNING;
    if(c=='2') logic_restart();
    if(c=='0' || c=='1') EEPROM_WRITE(&eeprom_logic_flags, &logic_flags, sizeof eeprom_logic_flags);
  }
  c = logic_flags & LOGIC_RUNNING ? '1' : '0' ;
  tcp_put_tx_body(pkt, &c, 1);
  return 0;
}

int logic_http_set_data(void)
{
  http_post_data((void*)&logic_setup, sizeof logic_setup);
  EEPROM_WRITE(eeprom_logic_setup, logic_setup, sizeof eeprom_logic_setup);
  logic_restart();
  http_redirect("/logic.html");
  return 0;
}

HOOK_CGI(logic_get,    (void*)logic_http_get_data,  mime_js,  HTML_FLG_GET | HTML_FLG_NOCACHE );
HOOK_CGI(logic_run,    (void*)logic_http_get_run,   mime_js,  HTML_FLG_GET | HTML_FLG_NOCACHE );
HOOK_CGI(logic_set,    (void*)logic_http_set_data,  mime_js,  HTML_FLG_POST );

unsigned tstat_http_get_data(unsigned pkt, unsigned more_data)
{
  char buf[1024];
  char *dest = buf;
  if(more_data == 0)
  {
    dest+=sprintf((char*)dest,"var tstat_packfmt={");
    PLINK(dest, tstat_setup[0], setpoint);
    PLINK(dest, tstat_setup[0], hyst);
    PLINK(dest, tstat_setup[0], sensor_no);
    PSIZE(dest, sizeof tstat_setup[0]); // must be the last // size must be word-aligned!
    dest += sprintf(dest, "}; var tstat_data=[");
  }
  struct tstat_setup_s *setup;
  for(;more_data<TSTAT_MAX_CHANNEL;)
  {
    setup = &tstat_setup[more_data];
    *dest++ = '{';
    PDATA_SIGNED(dest, (*setup), setpoint);
    PDATA(dest, (*setup), hyst);
    PDATA(dest, (*setup), sensor_no);
    --dest; // clear last PDATA-created comma
    *dest++ = '}';
    *dest++ = ',';
    ++more_data;
    if(dest - buf > sizeof buf - 128) break; // data buffer capacity used up to 80%
  }
  if(more_data == TSTAT_MAX_CHANNEL)
  {
    more_data = 0; // reset pumping
    --dest; // remove last comma
    dest += sprintf(dest, "]; var termo_n_ch=%u;", TERMO_N_CH);
  }
  tcp_put_tx_body(pkt, (unsigned char*)buf, dest - buf);
  return more_data;
}

int tstat_http_set_data(void)
{
  http_post_data((void*)&tstat_setup, sizeof tstat_setup);
  EEPROM_WRITE(eeprom_tstat_setup, tstat_setup, sizeof eeprom_tstat_setup);
  tstat_restart();
  http_redirect("/logic.html");
  return 0;
}

HOOK_CGI(tstat_get,    (void*)tstat_http_get_data,  mime_js,  HTML_FLG_GET | HTML_FLG_NOCACHE );
HOOK_CGI(tstat_set,    (void*)tstat_http_set_data,  mime_js,  HTML_FLG_POST );


unsigned logic_http_get_status(unsigned pkt, unsigned more_data)
{
  char buf[1024];
  char *dest = buf;
  /// ATTENTION! single chunk!!!
  dest += sprintf(dest, "tstat_status=[" );
  for(int n=0; n<TSTAT_MAX_CHANNEL; ++n)
  {
    int t_val = 0xff; // error indicator
    unsigned ts_no = tstat_setup[n].sensor_no;
    if(ts_no == TERMO_N_CH)
    { // RH sensor
      if(rh_status_h != 0) t_val = rh_real_h;
    }
    else if(ts_no < TERMO_N_CH)
    { // Tn sensor
      if(termo_state[ts_no].status != 0) t_val = termo_state[ts_no].value;
    }
    dest += sprintf(dest, "{t_status:%u,t_val:%d},", tstat_state[n], t_val);
    if(dest - buf > sizeof buf - 128) return 0; // emergency buf protection!
  }
  --dest; // remove last comma
  dest += sprintf(dest, "]; pinger_status=[" );
  for(int n=0; n<LOGIC_MAX_PINGER; ++n)
  {
    if(dest - buf > sizeof buf - 128) return 0; // emergency buf protection!
    int pstatus = logic_pinger_state[n].result;
    if(pstatus == 1 && logic_pinger_state[n].attempt_counter > 1 /* more than 1 retry*/) pstatus = 2; // ok with some retry
    dest += sprintf(dest, "%u,", pstatus);
  }
  --dest; // remove last comma
#ifdef SETTER_MAX_CH // snmp setter status
  dest += sprintf(dest, "]; setter_status=[" );
  for(int n=0; n<SETTER_MAX_CH; ++n)
  {
    if(dest - buf > sizeof buf - 128) break; // emergency buf protection!
    dest += sprintf(dest, "%u,", setter_state[n].err_status);
  }
  --dest; // remove last comma
#endif
  *dest++ = ']';
  *dest++ = ';';
  tcp_put_tx_body(pkt, (unsigned char*)buf, dest - buf);
  return 0; // beware single chunk of data
}

HOOK_CGI(logic_status,    logic_http_get_status,  mime_js,  HTML_FLG_GET | HTML_FLG_NOCACHE );

unsigned logic_pinger_http_get_data(unsigned pkt, unsigned more_data)
{
  char buf[1024];
  char *dest = buf;
  if(more_data == 0)
  {
    dest+=sprintf((char*)dest,"var pinger_packfmt={");
    PLINK(dest, logic_pinger_setup[0], ip);
    PLINK(dest, logic_pinger_setup[0], period);
    PLINK(dest, logic_pinger_setup[0], timeout);
#ifdef DNS_MODULE
    PLINK(dest, logic_pinger_setup[0], hostname);
#endif
    PSIZE(dest, sizeof logic_pinger_setup[0]); // must be the last // size must be word-aligned!
    dest += sprintf(dest, "}; var pinger_data=[");
  }
  struct logic_pinger_setup_s *setup;
  for(;more_data<LOGIC_MAX_PINGER;)
  {
    setup = &logic_pinger_setup[more_data];
    *dest++ = '{';
    PDATA_IP(dest, (*setup), ip);
    PDATA(dest, (*setup), period);
    PDATA(dest, (*setup), timeout);
#ifdef DNS_MODULE
    PDATA_PASC_STR(dest, (*setup), hostname);
#endif
    --dest; // clear last PDATA-created comma
    *dest++ = '}';
    *dest++ = ',';
    ++more_data;
    if(dest - buf > sizeof buf - 128) break; // data buffer capacity used up to 80%
  }
  if(more_data == LOGIC_MAX_PINGER)
  {
    more_data = 0; // reset pumping
    --dest; // remove last comma
    *dest++ = ']';
    *dest++ = ';';
  }
  tcp_put_tx_body(pkt, (unsigned char*)buf, dest - buf);
  return more_data;
}

int logic_pinger_http_set_data(void)
{
  http_post_data((void*)&logic_pinger_setup, sizeof logic_pinger_setup);
#ifdef DNS_MODULE
  struct logic_pinger_setup_s *ep = eeprom_logic_pinger_setup;
  struct logic_pinger_setup_s *p =  logic_pinger_setup;
  for(int i=0; i<LOGIC_MAX_PINGER; ++i, ++ep, ++p)
    dns_resolve(ep->hostname, p->hostname);
#endif
  EEPROM_WRITE(eeprom_logic_pinger_setup, logic_pinger_setup, sizeof eeprom_logic_pinger_setup);
  logic_pinger_restart();
  http_redirect("/logic.html");
  return 0;
}

HOOK_CGI(pinger_get,    (void*)logic_pinger_http_get_data,  mime_js,  HTML_FLG_GET | HTML_FLG_NOCACHE );
HOOK_CGI(pinger_set,    (void*)logic_pinger_http_set_data,  mime_js,  HTML_FLG_POST );


void logic_check_rule(int rule_n)
{
  struct logic_setup_s *setup = &logic_setup[rule_n];
  if((setup->flags & RULE_ACTIVE) == 0) return;

  struct logic_state_s *state = &logic_state[rule_n];

  unsigned char new_input = 0xfe; // default non-assigned input value, any condition will be false
  unsigned input_ch = setup->input & 0x0f;
  unsigned input_type = setup->input & 0xf0;
  switch(input_type)
  {
  case 0x00: // Reset Signal
    if(logic_reset_front ==  1) new_input = 1;
    if(logic_reset_front == -1) new_input = 0;
    // if logic_reset_signal == 0 do nothing
    break;
  case 0x10: // IO line input
    new_input = 1;
    //if(io_setup[input_ch].direction == 0) // it's input // removed 22.12.2014
    new_input = io_state[input_ch].level_filtered;
    break;
  case 0x20: // Thermostat
    new_input = tstat_state[input_ch];
    break;
  case 0x30: // Pinger
    new_input = logic_pinger_state[input_ch].result;
    break;
#ifdef CUR_DET_MODULE
  case 0x40: // C.S.Alarm
    new_input = curdet_status == CURDET_ALARM ? 1 : 0 ;
    break;
  case 0x50: // C.S.Fail
    new_input = ( curdet_status == CURDET_CUT || curdet_status == CURDET_SHORT ) ? 1 : 0 ;
    break;
  case 0x60: // C.S.Norm
    new_input = curdet_status == CURDET_NORM ? 1 : 0 ;
    break;
#endif
  }
  // if logic is paused, don't apply rules except ones with RESET input
  if((logic_flags & LOGIC_RUNNING) == 0 && input_type != 0x00) return;
  // if condition is not satisfied, leave output as is, else switch output
  if(new_input == setup->condition)
  {
    if( (setup->flags & RULE_TRIGGER)==0 || new_input != state->old_input ) // logic 'short curquiting'
    { // rule is of 'while' type || input has changed
      if(state->output_idx < LOGIC_MAX_OUTPUTS) // protection
        new_logic_output[state->output_idx] = setup->action;
    }
  }
  state->old_input = new_input;
}

void logic_apply_output(void)
{
  int n;
  unsigned char output;
  for(n=0; n<LOGIC_MAX_OUTPUTS; ++n)
  {
    output = new_logic_output[n];
    if(output == 0xff) continue; // leave old value

#warning "TODO safe error processing (at present, if error, rule does nothing"
    if(output == 0xfe) continue;

    unsigned ch = output_id_table[n] & 0x0f;
    unsigned short mask = 1<<ch;
    switch(output_id_table[n] & 0xf0) // set output value
    {
    case 0xa0: // IO line output
      if(output == 0) logic_io_output &=~ mask;
      if(output == 1) logic_io_output |=  mask;
      if(output == 2) logic_io_output ^=  mask; // flip
      break;
    case 0xb0: // PWR relay output
      if(output == 0) logic_relay_output &=~ mask;
      if(output == 1) logic_relay_output |=  mask;
      if(output == 2) logic_relay_output ^=  mask; // flip
      break;
#ifdef SETTER_MODULE
    case 0xc0: // snmp setter
      setter_send(ch, output);
      break;
#endif
#ifdef IR_MODULE
    case 0xd0: // IR command // 28.05.2012
      if(output) ir_play_record(ch);
      break;
#endif
#ifdef CUR_DET_MODULE
    case 0xe0: // Current loop sensor
      if(output == 2)
        curdet_power_logic_input = !curdet_power_logic_input; // 11.06.2014
      else
        curdet_power_logic_input = output;
      break;
#endif
    }
  }
}

void logic_run_once(void)
{
  int n;
  // neutral values - "don't change"
  util_fill(new_logic_output, sizeof new_logic_output, 0xff);
  // scan rules in reverse priority order
  for(n=LOGIC_MAX_RULES-1; n>=0; --n) // scan 'triggered' rules (lower priority)
    if((logic_setup[n].flags & RULE_TRIGGER) != 0)
      logic_check_rule(n);
  for(n=LOGIC_MAX_RULES-1; n>=0; --n) // scan 'static' rules (hier priority)
    if((logic_setup[n].flags & RULE_TRIGGER) == 0)
      logic_check_rule(n);
  // set default values of 0 to untouched outputs on 'reset on' run
  if(logic_reset_front == 1)
    for(int i=0; i<LOGIC_MAX_OUTPUTS; ++i)
      if(new_logic_output[i] == 0xff)
        new_logic_output[i] = 0;
  // route results to io, pwr etc.
  logic_apply_output();
}

void logic_exec(void)
{
  static unsigned char skip;
  if(++skip < 17) return; // CPU unload
  skip = 0;
  if(sys_clock_100ms < logic_delay_time) return;
  switch(logic_run_state)
  {
  case LOGIC_BOOT_DELAY:
    logic_delay_time = sys_clock_100ms + 50; // 5s
    logic_run_state = LOGIC_RESET_ON;
    break;
  case LOGIC_RESET_ON:
    // scan rules with reset on
    logic_reset_front = 1;
    logic_run_once();
    logic_delay_time = sys_clock_100ms + LOGIC_RESET_TIME / 100;
    logic_run_state = LOGIC_RESET_OFF;
    break;
  case LOGIC_RESET_OFF:
    // scan rules with reset off
    logic_reset_front = -1;
    logic_run_once();
    logic_reset_front = 0;
    logic_run_state = LOGIC_PAUSE;
    break;
  case LOGIC_PAUSE:
    if(logic_flags & LOGIC_RUNNING) logic_run_state = LOGIC_RUN;
    break;
  case LOGIC_RUN:
    if((logic_flags & LOGIC_RUNNING) == 0) logic_run_state = LOGIC_PAUSE;
    logic_run_once();
    break;
  }
}

void logic_restart(void)
{
  int i, n, output_id;
  // initialize state
  struct logic_state_s *state = logic_state;
  for(n=0; n<LOGIC_MAX_RULES; ++n, ++state)
  {
   // convert output id to output index, save in rule state for future use
    output_id = logic_setup[n].output;
    for(i=0; i<LOGIC_MAX_OUTPUTS; ++i)
      if(output_id_table[i] == output_id) break;
    state->output_idx = i; // if unknown output id, it will be last (dummy output) index
    // undefined previous input
    state->old_input = 0xff;
  }
  // can't set default state of outputs before exec() cycles because of SNMP requires DNS to be completed
  // so, init is performed in logic_exec()
  logic_delay_time = 0; // cancel current delay
  logic_run_state = LOGIC_BOOT_DELAY;
}

void logic_reset_params(void)
{
  util_fill((void*)logic_setup, sizeof logic_setup, 0xff);
  struct logic_setup_s *setup = logic_setup;
  for(int n=0; n<LOGIC_MAX_RULES; ++n, ++setup)
  {
    setup->flags = 0;
    setup->input = 0x10;
    setup->condition = 1;
    setup->action = 0;
    setup->output = 0xa0;
  }
  logic_flags = 0;
  EEPROM_WRITE(eeprom_logic_setup, logic_setup, sizeof eeprom_logic_setup);
  EEPROM_WRITE(&eeprom_logic_flags, &logic_flags, sizeof eeprom_logic_flags);
  EEPROM_WRITE(&eeprom_logic_signature, &logic_signature, sizeof eeprom_logic_signature);
}

void tstat_exec(void)
{
  static systime_t next_time = 1000;
  systime_t time = sys_clock();
  if(time < next_time) return;
  next_time = time + 1000;

  struct tstat_setup_s *setup = tstat_setup;
  unsigned char *state = tstat_state;
  int threshold, value, status;
  for(int i=0; i<TSTAT_MAX_CHANNEL; ++i, ++setup, ++state)
  {
#ifdef RELHUM_MODULE
    if(setup->sensor_no > TERMO_N_CH) { *state = 0xfe; continue; } // wrong sensor (termo + RH)
    if(setup->sensor_no == TERMO_N_CH)
    {
      status = rh_status_h;
      value = rh_real_h;
    }
    else
    {
      status = termo_state[setup->sensor_no].status;
      value = termo_state[setup->sensor_no].value;
    }
#else
    if(setup->sensor_no >= TERMO_N_CH) { *state = 0xfe; continue; } // wrong sensor
    status = termo_state[setup->sensor_no].status;
    value = termo_state[setup->sensor_no].value;
#endif // RELHUM_MODULE
    if(status == 0) { *state = 0xfe; continue; } // sensor fault
    // +1 -1 to use exact > and < on comparison, i.e. exact switch threshold (setpoint +/- hyst)
    // min hyst is 1 deg.C => hyst zone is 2 deg.c (exact > and < on discrete integer T measurements)
    threshold = setup->setpoint;
    if(*state == 1) threshold = threshold - setup->hyst + 1;
    if(*state == 0) threshold = threshold + setup->hyst - 1;
    if(value > threshold) *state = 1;
    if(value < threshold) *state = 0;
  }
}

void tstat_restart(void)
{
  for(int n=0; n<TSTAT_MAX_CHANNEL; ++n)
      tstat_state[n] = 0xff;
}

void tstat_reset_params(void)
{
  util_fill((void*)tstat_setup, sizeof tstat_setup, 0xff);
  struct tstat_setup_s *setup = tstat_setup;
  for(int n=0; n<TSTAT_MAX_CHANNEL; ++n, ++setup)
  {
    setup->setpoint = 20;
    setup->hyst = 2;
    setup->sensor_no = 0;
  }
  EEPROM_WRITE(eeprom_tstat_setup, tstat_setup, sizeof eeprom_tstat_setup);
  EEPROM_WRITE(&eeprom_tstat_signature, &tstat_signature, sizeof eeprom_tstat_signature);
}

void tstat_init(void)
{
  unsigned sign;
  EEPROM_READ(&eeprom_tstat_signature, &sign, sizeof sign);
  if(sign != tstat_signature) tstat_reset_params();
  EEPROM_READ(eeprom_tstat_setup, tstat_setup, sizeof tstat_setup);
  tstat_restart();
}

#if PING_VER < 2
#error "Needs ping.c v2!"
#endif

void logic_pinger_exec(void)
{
  int i;
  struct logic_pinger_setup_s *setup = logic_pinger_setup;
  struct logic_pinger_state_s *state = logic_pinger_state;
  struct ping_state_s  *ping  = &ping_state[LOGIC_PING_CH];
  systime_t time = sys_clock();
  for(i=0; i<LOGIC_MAX_PINGER; ++i, ++setup, ++state, ++ping)
  {
    if(ping->state == PING_RESET && time >= state->next_time)
    {
      state->next_time = time + setup->period * 1000;
      if(valid_ip(setup->ip) == 0)
      {
        if(setup->hostname[0] == 0)
        { // no ip or hostname, skip
          state->result = 0xfe;
          continue;
        }
        else
        { // dns unresolved, treat as ping fail
          state->result = 0;
          continue;
        }
      }
      util_cpy(setup->ip, ping->ip, 4);
      ping->timeout = setup->timeout;
      ping->max_retry = 4; // 26.09.2013 fixed value
      state->attempt_counter = 0;
      ping->state = PING_START;
    }
    else
    if(ping->state == PING_COMPLETED)
    {
      state->result = ping->result;
      ping->state = PING_RESET;
    }
  } // for channels
}

void logic_pinger_restart(void)
{
  struct logic_pinger_state_s *state = logic_pinger_state;
  struct ping_state_s  *ping  = &ping_state[LOGIC_PING_CH];
  for(int i=0; i<LOGIC_MAX_PINGER; ++i, ++state, ++ping)
  {
    ping->state = PING_RESET;
    state->result = 0xff;
    state->next_time = sys_clock() + 6000; // changed to 6s to resolve hostname
  }
}

void logic_pinger_reset_params(void)
{
  util_fill((void*)&logic_pinger_setup, sizeof logic_pinger_setup, 0); // it was 0xff, now 0 to clear hostname 21.05.2013
  struct logic_pinger_setup_s *setup = logic_pinger_setup;
  for(int i=0; i<LOGIC_MAX_PINGER; ++i, ++setup)
  {
    setup->ip32 = 0;
    setup->period = 15;
    setup->timeout = 1000;
  }
  EEPROM_WRITE(eeprom_logic_pinger_setup, logic_pinger_setup, sizeof eeprom_logic_pinger_setup);
  EEPROM_WRITE(&eeprom_logic_pinger_signature, &logic_pinger_signature, sizeof eeprom_logic_pinger_signature);
}

void logic_pinger_init(void)
{
  unsigned sign;
  EEPROM_READ(&eeprom_logic_pinger_signature, &sign, sizeof sign);
  if(sign != logic_pinger_signature) logic_pinger_reset_params();
  EEPROM_READ(eeprom_logic_pinger_setup, logic_pinger_setup, sizeof logic_pinger_setup);
#ifdef DNS_MODULE
  struct logic_pinger_setup_s *q = logic_pinger_setup;
  for(int i=0; i<LOGIC_MAX_PINGER; ++i, ++q)
    dns_add(q->hostname, q->ip);
#endif
  logic_pinger_restart();
}

void logic_init(void)
{
  tstat_init();
  logic_pinger_init();

  unsigned sign;
  EEPROM_READ(&eeprom_logic_signature, &sign, sizeof sign);
  if(sign != logic_signature) logic_reset_params();
  EEPROM_READ(eeprom_logic_setup, logic_setup, sizeof logic_setup);
  EEPROM_READ(&eeprom_logic_flags, &logic_flags, sizeof logic_flags);
  logic_restart();
}

/*
#warning "remove debug - fake t sensor"
int debug_t = 20;
*/

void logic_event(enum event_e event)
{
  switch(event)
  {
  case E_EXEC:
/*
///////////
#warning "remove debug - fake termo sensor"
termo_state[1].value = debug_t;
termo_state[1].status = 7;
//////////
*/
    tstat_exec();
    logic_pinger_exec();
    logic_exec();
    break;
  case E_RESET_PARAMS:
    tstat_reset_params();
    logic_pinger_reset_params();
    logic_reset_params();
    break;
  }
}

#endif // LOGIC_MODULE
