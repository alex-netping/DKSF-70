/* Модуль CUR_DET реализует функции работы с токовым входом устройства DKST-50
* P.Lyubasov
*v2.0
*01.2010
*v2.1-50
*6.07.2010 by LBS
*  loop power on, 12V by default
*v 2.2-50
*5.03.2011
*  Valid Traps. English log.
*v2.3-60
*14.06.2012
* dkst60 support (dkst50.25)
*v2.4-60
*26.10.2012
*  changed 'cut' threshold 4000 to 4500 Ohm, 'short' hyst to 50
    (it was buggy 200 vs threshold 100)
*v2.3-50
*7.11.2012
  curdet_power_switch(), curdet_start_reset() for ext. control
v2.4-50
2.04.2013
  explicit control of loop power by logic.c
v2.5-70
1.07.2013
  cosmetic rewrite, dksf70 support
v2.6-70
21.01.2014
  update of table channel check in curdet_snmp_get(), curdet_snmp_set()
  to be compatible from .table.N designation in ..._MIB.py
  cosmetic interface changes curdet_real_i,v,r instead of real_i,v,r
v2.7-70
11.06.2014
  support of 70.1.2 board
v2.8-70
30.07.2014
  url-encoded api (json-p)
v2.9-70
3.12.2014
  changed Log messages, Smoke sensor
*/


#include "platform_setup.h"
#include "cur_det\cur_det.h"

#ifdef CUR_DET_MODULE

#include "eeprom_map.h"
#include "plink.h"
#include <stdio.h>

#ifndef AD0DR
#define AD0DR AD0GDR // LPC2366-2136 difference in IAR header files
#endif


#define CURDET_SIGNATURE_VALUE 52955447

char curdet_adc_i_channel;
char curdet_adc_v_channel;

void curdet_start_reset(void);

// setup data
__no_init struct curdet_setup_s curdet_setup;
// filter
unsigned filter_i;
unsigned filter_v;
unsigned short accum_i;
unsigned short accum_v;
unsigned char  accum_n;
unsigned char  samples_n;
unsigned short samples_i[CURDET_FILT_DEPTH];
unsigned short samples_v[CURDET_FILT_DEPTH];
// values and trap flags
unsigned curdet_real_v;
unsigned curdet_real_i;
unsigned curdet_real_r;
unsigned char cut_flag;
unsigned char short_flag;
unsigned char al_flag;
unsigned char curdet_status;
static unsigned char prev_status;
unsigned curdet_reset_timer;
unsigned char curdet_power;
volatile unsigned char curdet_power_logic_input;

#if PROJECT_CHAR=='E'
const char * const curdet_status_text[5] =
{
  "now is OK",
  "ALARM!",
  "loop is open",
  "loop is shorted",
  "loop power is off"
};
#else
const char * const curdet_status_text[5] =
{
  "Норма",
  "ТРЕВОГА!",
  "Обрыв",
  "Кор.зам.",
  "Обесточен"
};
#endif

unsigned curdet_http_get_data(unsigned pkt, unsigned more_data)
{
  char buf[768];
  char *dest = buf;
  // it's for single channel (scalar curdet_setup) only!!!
  dest+=sprintf(dest,"var packfmt={");
  PLINK(dest, curdet_setup, al_mode);
  PLINK(dest, curdet_setup, cut_mode);
  PLINK(dest, curdet_setup, short_mode);
  PLINK(dest, curdet_setup, al_cmp);
  PLINK(dest, curdet_setup, al_threshld);
  PLINK(dest, curdet_setup, cut_threshld);
  PLINK(dest, curdet_setup, short_threshld);
  PLINK(dest, curdet_setup, al_hyst);
  PLINK(dest, curdet_setup, cut_hyst);
  PLINK(dest, curdet_setup, short_hyst);
  PLINK(dest, curdet_setup, power);
  PLINK(dest, curdet_setup, voltage);
  PLINK(dest, curdet_setup, rst_period);
  PLINK(dest, curdet_setup, trap);
  PLINK(dest, curdet_setup, rst_flag);
  PSIZE(dest, sizeof curdet_setup);
  dest += sprintf(dest, "};data={status:%u,current:%u,vdrop:%u,resist:%u,",
                curdet_status, curdet_real_i, curdet_real_v, curdet_real_r);
  PDATA(dest, curdet_setup, al_mode);
  PDATA(dest, curdet_setup, cut_mode);
  PDATA(dest, curdet_setup, short_mode);
  PDATA(dest, curdet_setup, al_cmp);
  PDATA(dest, curdet_setup, al_threshld);
  PDATA(dest, curdet_setup, cut_threshld);
  PDATA(dest, curdet_setup, short_threshld);
  PDATA(dest, curdet_setup, al_hyst);
  PDATA(dest, curdet_setup, cut_hyst);
  PDATA(dest, curdet_setup, short_hyst);
  PDATA(dest, curdet_setup, power);
  PDATA(dest, curdet_setup, voltage);
  PDATA(dest, curdet_setup, rst_period);
  PDATA(dest, curdet_setup, trap);
  --dest; // remove last ','
  *dest++ = '}';
  *dest++ = ';';
  tcp_put_tx_body(pkt, (unsigned char*)buf, dest - buf);
  return 0;
}

int curdet_http_set_data(void)
{
  http_post_data((void*)&curdet_setup, sizeof curdet_setup);
  EEPROM_WRITE(&eeprom_curdet_setup, &curdet_setup, sizeof eeprom_curdet_setup);
  if(curdet_setup.rst_flag)
  {
    curdet_setup.rst_flag = 0;
    curdet_reset_timer = curdet_setup.rst_period * 1000;
  }
  http_redirect("/curdet.html");
  return 0;
}

unsigned curdet_http_get_cgi(unsigned pkt, unsigned more_data)
{
  char buf[128] = "curdet_result('error');";
  char *p = req_args;
  if(*p == 0)
  {
    if(curdet_status >= 5) goto end;
    sprintf(buf, "curdet_result('ok', %u, '%s');", curdet_status, curdet_status_text[curdet_status]);
  }
  else if(strcmp(p, "reset") == 0)
  {
    curdet_start_reset();
    sprintf(buf, "curdet_result('ok');");
  }
  //else goto end; // rudimentary code
end:
  tcp_put_tx_body(pkt, (void*)buf, strlen(buf));
  return 0;
}

/*
unsigned curdet_http_do_reset(unsigned pkt, unsigned more_data)
{
Лишние пакеты возникли из-за реализации POST хандлера
для GET запроса (img.src='/curedet_rst.cgi' это GET запрос!)
Out-of-order вероятно связано с несогласованными tcp_create_packet() и tcp_send_packet().
Seq/Ack следует формировать исключительно при посылке.

  curdet_reset_timer = curdet_setup.rst_period * 1000;
  return 0;
}
HOOK_CGI(curdet_rst,   (void*)curdet_http_do_reset,  mime_js,  HTML_FLG_GET | HTML_FLG_NOCACHE );
*/

HOOK_CGI(curdet_get,   (void*)curdet_http_get_data,  mime_js,  HTML_FLG_GET | HTML_FLG_NOCACHE );
HOOK_CGI(curdet,       (void*)curdet_http_get_cgi,   mime_js,  HTML_FLG_GET | HTML_FLG_NOCACHE );
HOOK_CGI(curdet_set,   (void*)curdet_http_set_data,  mime_js,  HTML_FLG_POST );


const unsigned char curdet_enterprise[] =
// .1.3.6.1.4.1.25728.8900.2
{SNMP_OBJ_ID, 11, // ASN.1 type/len, len is for 'enterprise' trap field
0x2b,6,1,4,1,0x81,0xc9,0x00,0xc0,0x6c,2}; // OID for "enterprise" in trap msg

unsigned char curdet_trap_data_oid[] =
{0x2b,6,1,4,1,0x81,0xc9,0x00,0xc0,0x6c,2, // table entry
0,0}; // last oid components (pre-last is variable, last is .0 for scalar oid)


void curdet_add_vbind_integer(unsigned char last_oid_component, int value)
{
  unsigned seq_ptr;
  seq_ptr = snmp_add_seq();
  curdet_trap_data_oid[sizeof curdet_trap_data_oid - 2] = last_oid_component;
  snmp_add_asn_obj(SNMP_OBJ_ID, sizeof curdet_trap_data_oid, curdet_trap_data_oid);
  snmp_add_asn_integer(value);
  snmp_close_seq(seq_ptr);
}

void curdet_make_trap(void)
{
  snmp_create_trap((void*)curdet_enterprise);
  if(snmp_ds == 0xff) return; // if can't create packet
  curdet_add_vbind_integer(1, 1);              // LP_CHANNEL
  curdet_add_vbind_integer(2, curdet_status);  // LP_STATUS
  curdet_add_vbind_integer(3, curdet_real_i);         // LP_I
  curdet_add_vbind_integer(4, curdet_real_v);         // LP_V
  curdet_add_vbind_integer(5, curdet_real_r);         // LP_R
  curdet_add_vbind_integer(7, curdet_power);   // npCurLoopTrapPower
}

void curdet_check_state(void)
{
  unsigned status;
  static systime_t time_of_change;
  // check status
  if(curdet_power==0 || curdet_reset_timer) status = CURDET_NOT_POWERED;
  else if(short_flag) status = CURDET_SHORT;
  else if(cut_flag)   status = CURDET_CUT;
  else if(al_flag)    status = CURDET_ALARM;
  else                status = CURDET_NORM;
  // filter out changes shorter than 250ms
  if(status != prev_status)
  {
    prev_status = status;
    time_of_change = sys_clock();
    return;
  }
  if(sys_clock() - time_of_change < 250 /* ms */) return;
  if(status == curdet_status) return;
  curdet_status = status;
# ifdef NOTIFY_MODULE
    unsigned mask = 0;
    switch(curdet_status)
    {
    case CURDET_NOT_POWERED:
    case CURDET_SHORT:
    case CURDET_CUT:
      mask = curdet_notify.fail;
      break;
    case CURDET_ALARM:
      mask = curdet_notify.high;
      break;
    case CURDET_NORM:
      mask = curdet_notify.norm;
      break;
    }
#   if PROJECT_CHAR=='E'
      notify(mask, "Smoke sensor (current loop): state has changed, %s", curdet_status_text[curdet_status]); // 22.08.2014, it was 'Curredt'
#   else
      notify(mask, "Датчик дыма (токовый датчик): состояние %s", curdet_status_text[curdet_status]);
#   endif
    if(mask & NOTIFY_TRAP)
    {
      if(valid_ip(sys_setup.trap_ip1)) { curdet_make_trap(); snmp_send_trap(sys_setup.trap_ip1); }
      if(valid_ip(sys_setup.trap_ip2)) { curdet_make_trap(); snmp_send_trap(sys_setup.trap_ip2); }
    }
#   ifdef SMS_MODULE
      if(mask & NOTIFY_SMS) sms_curdet_event();
#   endif
# else // NOTIFY_MODULE
    // write (sys)log, make and send trap
#   if PROJECT_CHAR=='E'
      log_printf("Smoke sensor (current loop): state has changed, %s", curdet_status_text[curdet_status]);
#   else
      log_printf("Датчик дыма (токовый датчик): состояние %s", curdet_status_text[curdet_status]);
#   endif
    if(curdet_setup.trap)
    {
      if(valid_ip(sys_setup.trap_ip1)) { curdet_make_trap(); snmp_send_trap(sys_setup.trap_ip1); }
      if(valid_ip(sys_setup.trap_ip2)) { curdet_make_trap(); snmp_send_trap(sys_setup.trap_ip2); }
    }
#endif  // NOTIFY_MODULE
}

void curdet_switch_power(int val)
{
  if(curdet_setup.power != val)
  {
    curdet_setup.power = val; // power is switched on-off in curdet_exec() main loop
    EEPROM_WRITE(&eeprom_curdet_setup.power,
       &curdet_setup.power,
       sizeof eeprom_curdet_setup.power);
  }
}

void curdet_start_reset(void)
{
  curdet_reset_timer = curdet_setup.rst_period * 1000;
}

// DKSF50 only
int curdet_snmp_get(unsigned id, unsigned char *data)
{
  unsigned ch = snmp_data.index - 1;   // update from from legacy id-coded channel 23.01.2014
  if(ch != 0) return SNMP_ERR_NO_SUCH_NAME;

  int val = 0;
  switch(id&0xfffffff0)
  {
  case 0x8310: val = 1; break;
  case 0x8320: val = curdet_status; break;
  case 0x8330: val = curdet_real_i; break;
  case 0x8340: val = curdet_real_v; break;
  case 0x8350: val = curdet_real_r; break;
  case 0x8370: val = curdet_reset_timer ? 2 : curdet_power;
  }
  if(val < 0) val = 0;
  snmp_add_asn_integer(val);
  return 0;
}

int curdet_snmp_set(unsigned id, unsigned char *data)
{
  int val;

  unsigned ch = snmp_data.index - 1;   // update from from legacy id-coded channel 23.01.2014
  if(ch != 0) return SNMP_ERR_NO_SUCH_NAME;

  if(*data != 0x02) return SNMP_ERR_BAD_VALUE; // asn type isn't INTEGER
  asn_get_integer(data, &val);
  val &= 0x7fffffff;
  snmp_add_asn_integer(val);
  switch(id&0xfffffff0)
  {
  case 0x8310:
  case 0x8320:
  case 0x8330:
  case 0x8340:
  case 0x8350:
    return SNMP_ERR_READ_ONLY;
  case 0x8370:
    switch(val)
    {
    case 0:
    case 1:
      curdet_switch_power(val);
      break;
    case 2:
      curdet_start_reset();
      break;
    default:
      return SNMP_ERR_BAD_VALUE;
    }
    break;
  default:
    return SNMP_ERR_NO_SUCH_NAME;
  }
  return 0;
}

void curdet_timer()
{
  unsigned d;

  //AD0CR_bit.SEL = 1 << CURDET_ADC_I_CHANNEL;
  //AD0CR_bit.START = 1;
  AD0CR =
        1 << curdet_adc_i_channel    // SEL
   |    CURDET_ADC_DIV <<  8         // curdet ADC divider
   |    1 << 21                      // PDN = 1
   |    1 << 24;                     // START
  do { d = AD0DR; } while((d & (1u<<31)) == 0); // wait for DONE bit
# if CPU_FAMILY == LPC17xx
    accum_i += (d >> 4) & 0xfff; // get 12-bit result field
# else
    accum_i += (d >> 6) & 0x3ff; // get 10-bit result field
# endif

  //AD0CR_bit.SEL = 1 << CURDET_ADC_V_CHANNEL;
  //AD0CR_bit.START = 1;
  AD0CR =
        1 << curdet_adc_v_channel    // SEL
   |    CURDET_ADC_DIV <<  8         // curdet ADC divider
   |    1 << 21                      // PDN = 1
   |    1 << 24;                     // START
  do { d = AD0DR; } while((d & (1u<<31)) == 0);
# if CPU_FAMILY == LPC17xx
    accum_v += (d >> 4) & 0xfff; // get 12-bit result field
# else
    accum_v += (d >> 6) & 0x3ff; // get 10-bit result field
# endif

  // filter input
  if(++accum_n==8)
  {
    accum_n = 0;

    filter_i -= samples_i[samples_n];
    samples_i[samples_n] = accum_i >> 3; // :8
    filter_i += samples_i[samples_n];
    accum_i = 0;

    filter_v -= samples_v[samples_n];
    samples_v[samples_n] = accum_v >> 3; // :8
    filter_v += samples_v[samples_n];
    accum_v = 0;

    if(++samples_n == CURDET_FILT_DEPTH) samples_n = 0;
  }
  // count reset by powercycling
  if(curdet_reset_timer) --curdet_reset_timer;
}

void curdet_exec() // board-dependant!
{
  // handle loop power
  if(curdet_setup.power != 3)
    curdet_power = curdet_setup.power; // manual
  else
    curdet_power = curdet_power_logic_input; // control from logic

#if PROJECT_MODEL == 50

  /// DKSF 50.6 only!
  unsigned cpsr = proj_disable_interrupt(); // to change IOxDIR (non-atomic in IAR!)
  if(curdet_setup.voltage) IO0SET = 1 << 17; // 1=24V 0=12V
  else                     IO0CLR = 1 << 17;
  IO0DIR |= 1 << 17;
  if(curdet_power && (curdet_reset_timer==0))
    IO0SET = 1 << 18;
  else
    IO0CLR = 1 << 18;
  IO0DIR |= 1 << 18;
  proj_restore_interrupt(cpsr);

#if CURDET_FILT_DEPTH != 16
 #error "rewrite math for CURDET_FILT_DEPTH not equal to 16"
#endif

  // compute with fixed point 24.8
  // I (mA) = ADC / 1024 * 3300 mV / 20 MAX4372T gain / 10 Ohm Rsense = ADC / 62.06
  unsigned ri = (filter_i << 8) / 62 / CURDET_FILT_DEPTH;
  // V (mV) = ADC / 1024 ADC scale * 3300 mV * 10 input R/R divider = ADC * 32.23
  unsigned rv = (filter_v << (5 + 8)) / CURDET_FILT_DEPTH;
  real_r = (rv / (ri+1/*zero division protection*/));
  if(real_r > 99999) real_r = 99999;
  real_v = rv >> 8;
  real_i = ri >> 8;

#elif PROJECT_MODEL == 60

  // handle loop power
  if(sys_clock() < 3000ULL) return; // delay after power-off in curdet_init()
  pinwrite(ENA_12V, curdet_power && (curdet_reset_timer==0) );

#if CURDET_FILT_DEPTH != 16
 #error "rewrite math for CURDET_FILT_DEPTH not equal to 16"
#endif

  // compute with fixed point 16.16
  // I (mA) = ADC / 1024 * 3300 mV / 20 MAX4373T gain / 1.5 Ohm Rsense = ADC / 9.31 = ADC * 0.1074
  unsigned ri = filter_i * 440; // 2^16/16/9.31
  // V (mV) = ADC / 1024 ADC scale * 3300 mV * 10 input R/R divider = ADC * 32.23
  unsigned rv = filter_v * 132014; // 2^16/16*32.23
  real_r = (rv / (ri+1/*zero division protection*/));
  if(real_r > 21) real_r -= 21; // R611 22E 5%
  else real_r = 0;
  if(real_r > 99999) real_r = 99999;
  real_v = rv >> 16;
  real_i = ri >> 16;

#elif PROJECT_MODEL == 70 || PROJECT_MODEL == 71

  // handle loop power
  if(sys_clock() < 3000ULL) return; // delay after power-off in curdet_init()
  pinwrite(ENA_12V_CURSENS, curdet_power && (curdet_reset_timer==0) );

#if CURDET_FILT_DEPTH != 16
 #error "rewrite math for CURDET_FILT_DEPTH not equal to 16"
#endif

  // compute with fixed point 16.16 and 12-bit ADC
  // I (mA) = ADC / 4096 * 3300 mV / 20 MAX4373T gain / 1.5 Ohm Rsense = ADC * 0.02685546875
  unsigned ri = filter_i * 110; // 2^16/16(CURDET_FILT_DEPTH)*0.02685546875
  // V (mV) = ADC / 4096 ADC scale * 3300 mV * 10 input R/R divider = ADC * 8.056640625
  unsigned rv = filter_v * 33000; // 2^16/16(CURDET_FILT_DEPTH)*8.056640625
  curdet_real_r = (rv / (ri+1/*zero division protection*/));
  if(curdet_real_r > 21) curdet_real_r -= 21; // R611 22E 5%
  else curdet_real_r = 0;
  if(curdet_real_r > 99999) curdet_real_r = 99999;
  curdet_real_v = rv >> 16;
  curdet_real_i = ri >> 16;

#else
  #error "undefined model!"
#endif

  // compute status
  unsigned value, hyst;
  struct curdet_setup_s *s = &curdet_setup;

  switch(s->al_mode)
  {
  case 0: value = curdet_real_i; break;
  case 1: value = curdet_real_v; break;
  case 2: value = curdet_real_r; break;
  default: value = 1;
  }
  hyst = al_flag ? s->al_hyst : 0;
  switch(s->al_cmp)
  {
  case 0: al_flag = value < s->al_threshld + hyst; break; // lesser then
  case 1: al_flag = value > s->al_threshld - hyst; break; // greater then
  }

  hyst = cut_flag ? s->cut_hyst : 0;
  switch(s->cut_mode)
  {
  case 0: cut_flag = curdet_real_i < s->cut_threshld + hyst; break;
  case 1: cut_flag = curdet_real_v > s->cut_threshld - hyst; break;
  case 2: cut_flag = curdet_real_r > s->cut_threshld - hyst; break;
  default: cut_flag = 0;
  }

  hyst = short_flag ? s->short_hyst : 0;
  switch(s->short_mode)
  {
  case 0: short_flag = curdet_real_i > s->short_threshld - hyst; break;
  case 1: short_flag = curdet_real_v < s->short_threshld + hyst; break;
  case 2: short_flag = curdet_real_r < s->short_threshld + hyst; break;
  default: short_flag = 0;
  }

  curdet_check_state();
}

void curdet_reset_params(void)
{
  curdet_setup.al_mode = 2;
  curdet_setup.cut_mode = 2;
  curdet_setup.short_mode = 2;
  curdet_setup.al_cmp = 0;
  curdet_setup.al_threshld = 2000;
  curdet_setup.cut_threshld = 4500; // 26.10.12
  curdet_setup.short_threshld = 100;
  curdet_setup.al_hyst = 200;
  curdet_setup.cut_hyst = 200;
  curdet_setup.short_hyst = 50; // 26.10.12
  /*   LBS 6.07.2010
  curdet_setup.power = 0;
  curdet_setup.voltage = 0;
  */
  curdet_setup.power = 1; // power on, 12V by default
  curdet_setup.voltage = 0;
  //
  curdet_setup.rst_period = 5;
  curdet_setup.trap = 1;
  EEPROM_WRITE(&eeprom_curdet_setup, &curdet_setup, sizeof eeprom_curdet_setup);
  unsigned n = CURDET_SIGNATURE_VALUE;
  EEPROM_WRITE(&eeprom_curdet_signature, &n, sizeof eeprom_curdet_signature);
}

void curdet_init(void)
{
  // restore parameters
  unsigned sign;
  EEPROM_READ(&eeprom_curdet_signature, &sign, sizeof sign);
  if(sign != CURDET_SIGNATURE_VALUE) curdet_reset_params();
  else EEPROM_READ(&eeprom_curdet_setup, &curdet_setup, sizeof curdet_setup);
  // setup ADC
#if PROJECT_MODEL == 50

  PINSEL1_bit.P0_27 = 0x01;
  PINSEL1_bit.P0_28 = 0x01;

#elif PROJECT_MODEL == 60

  PCONP_bit.PCAD = 1; // power-on ADC
  PCLKSEL0_bit.PCLK_ADC = 1; // 1:1 ADC PCLK
  PINSEL1_bit.P0_24 = 1;
  PINSEL1_bit.P0_25 = 1;
  pinclr(LIMIT_RESET); // enable auto-reset on overcurrent
  pindir(LIMIT_RESET, 1);
  pinclr(ENA_12V);
  pindir(ENA_12V,1);
  pindir(0,30,1); // LPC236x UM page 174, USB pins must have the same direction

#elif PROJECT_MODEL == 70 || PROJECT_MODEL == 71

  if(proj_hardware_model == 1)
  {
    ///// short in prototype (only one my board, because of patching) !!!
#   warning "ATTN Short in DKST70 prototype on P2.31 / MAT3 / SCL2, pin pull is disabled!"
    if(serial == ~0UL) IOCON_P2_31 = 0; // disable pull!
    /////
    curdet_adc_i_channel = 4;
    curdet_adc_v_channel = 5;
    IOCON_P1_30 = 3<<0 | 0<<3 | 0<<7 ; // func=ADC, pull=none, analog mode on (bit7=0!)
    IOCON_P1_31 = 3<<0 | 0<<3 | 0<<7 ; // func=ADC, pull=none, analog mode on (bit7=0!)
  }
  else
  {
    curdet_adc_i_channel = 1;
    curdet_adc_v_channel = 2;
    IOCON_P0_24 = 1<<0 | 0<<3 | 0<<7 ; // func[0..2]=ADC, pull=none, analog mode on (bit7=0!)
    IOCON_P0_25 = 1<<0 | 0<<3 | 0<<7 ; // func[0..2]=ADC, pull=none, analog mode on (bit7=0!)
  }
  PCONP_bit.PCAD = 1; // power-on ADC
  delay(1);
  pinclr(ENA_12V_CURSENS);
  pindir(ENA_12V_CURSENS, 1);

#else
  #error "undefined model!"
#endif
  AD0CR = 0;
  AD0CR =
   CURDET_ADC_DIV <<  8 |
   1              << 21 ;  // PDN = 1, power on
  // reset filters
  filter_v = filter_i = 0;
  accum_v = accum_i = 0;
  accum_n = samples_n = 0;
  util_fill((void*)samples_i, sizeof samples_i, 0);
  util_fill((void*)samples_v, sizeof samples_v, 0);
  // reset status
  curdet_status = curdet_power ? CURDET_NORM : CURDET_NOT_POWERED;
  prev_status = 0xff; // non-existant value
}

void curdet_event(enum event_e event)
{
  switch(event)
  {
  case E_TIMER_1ms:
    curdet_timer();
    break;
  case E_EXEC:
    curdet_exec();
    break;
  case E_INIT:
    curdet_init();
    break;
  case E_RESET_PARAMS:
    curdet_reset_params();
    break;
  }
}

#endif

#warning ****************** check HW id and CurDet operation on DKST70. Some discrepancy with board wiring, see curdet_timer() ***********
// *********** CURDET - latest FW shows HW is 70.1.1 on demo rack and my production sample, but CurDet is working! ****************
// may be ADC_I,ADC_V routes are left on AD0.4, AD0.5 (P0.30,31) ?

