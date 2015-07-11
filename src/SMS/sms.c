/*
v1.5-200
15.08.2011
  "sim busy" error resolved
  two dest phone numbers for sms sending
v1.6-201
19.08.2011
  IGT, EMEROFF inversed or not with auto identification
v1.7-200
10.10.2011
  emergency halt of the module
  reboot limiter
  'sim busy' retry limiter
  sms number per hour limiter
  accurate sms_init()
  accurate sms_sate checking for all operations before modem responce parsing
v1.8-201
25.10.2011
  English version
v1.9-213
7.12.2012
  sms_reset_params_mkarta()
v1.10-50
10.04.2013
  enlarged outgoing sms queue
  bugfix 'sms storm' after 8 messages enqued
  bugfix in sms_send_from_queue() for N phone numbers
v1.10-213
4.01.2013
  ignition rewrite, debug log modifications
v1.11-213
6.02.2013
  ignition, on-off rewrite
v1.12-48
  sys_setup.hostname used instead of sms_setup.hostname
  DNS adaptation
v1.13-707
  clear rx and del sms queues on module init and restart, in sms_init()
  respond to caller's phone number
v1.12-70
19.03.2014
  sms page update: restart button, debug log checkbox, last error
v1.13-201
24.03.2014
  both CMS/CME sim not inserted
v1.14-70
5.06.2014
  runtime 52/900 adaptation (by proj_hardware_detect() model in gsm_model variable)
v1.15-54
17.06.2014
  runtime 52/900 adaptation  (by proj_hardware_detect() model in gsm_model variable) of shutdown part
v1.16-54 (testing for 201 on dkst51.1.9)
12.11.2014
  using at+cmgd=1,4 instead of scanning sms purge during init for SIM900
v1.17-201
29.01.2015
  enchanced modem Init sequence, deleted /r, ESCs from init strings, big ATs only for SIM900
*/

#include "platform_setup.h"

#ifdef SMS_MODULE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "plink.h"
#include "eeprom_map.h"

#define SMS_UART_RX_TIMEOUT  200
#define WAIT_MORE_CYCLES      20
#define SMS_BAUDRATE       19200

//const unsigned sms_signature = 946100743;
//const unsigned sms_signature = 94610074; // four dest phones 7.11.2012
// 10.04.2013, break param compatibility if MAX_DEST_PHONE has changed from 4
const unsigned sms_signature = 94610070 + MAX_DEST_PHONE;
const unsigned sms_periodic_time_signature = 94610330; // periodic field in sms_setup, signature also is in sms_setup;

#define UxRBR    U1RBR
#define UxTHR    U1THR
#define UxDLL    U1DLL
#define UxDLM    U1DLM
#define UxFCR    U1FCR
#define UxLCR    U1LCR
#define UxMCR    U1MCR
#define UxLSR    U1LSR
#define UxLCR    U1LCR
#define UxIER    U1IER
#define UxIIR    U1IER
#define UxISR    UART1_IRQHandler
void UART1_IRQHandler(void);
#define UxIRQ 6 // LPC1778 UART 1 Irq Id // don't use NVIC_UART1 !!! it's exception number, another thing!
//#define UxIRQ VIC_UART1 // LPC23xx IAR definition (bit 7 in interrupt source registers, table 87 in LPC23xx User Manual)

#warning ATTN! use of two separate UARTs!
#undef UART_MODULE // DKST70 has separate UART for ext. serial port

struct sms_setup_s sms_setup;

char gsm_started; // =1 from IGT pulse to powered down state after AT^SMSO
char gsm_shutdown_req; // request to shut down if modem is busy now
unsigned short gsm_shutdown_timer; // 10ms tick, delay 1s from URC '^SHUTDOWN' to power down state,
// or 10s delay if module is halted by sms_emergency_halt flag

char sms_tx_buf[256];
unsigned sms_tx_idx;

char sms_rx_buf[512];
unsigned sms_rx_idx;
unsigned sms_rx_idx_debug;

#ifndef UART_MODULE
char sms_rx_uart_buf[256];
unsigned sms_rx_uart_idx;
#endif

systime_t   sms_rx_timeout = ~0ULL;
static char wait_more = 0;
static char wait_more_counter = WAIT_MORE_CYCLES;
systime_t   sim_busy_waiting = ~0;

unsigned char sms_at_debug;
char sms_rx_at_debug_buf[512];

char sms_ussd_responce[128];
char sms_ussd_responce_ready;
int  sms_sig_level;
int  sms_gsm_registration;
char sms_last_error[64];

unsigned char sms_gsm_failed = 0xff;
systime_t sms_gsm_test_time;
unsigned char sim_busy_counter; // limiting SIM Busy attempt counter
unsigned char sms_emergency_halt = 0; // Very Bad Error flag, halt, don't restart!


#define SMS_IDX_QUEUE_LEN 20
#define SMS_Q_LEN 16
#define SMS_Q_MSG_LEN 160

char sms_tx_queue[SMS_Q_LEN][SMS_Q_MSG_LEN];
char sms_tx_tel_queue[SMS_Q_LEN][16];   // telephone number to respond or 0 if unsolicited notification
char sms_rx_queue[SMS_IDX_QUEUE_LEN];   // indeces of received sms in modem's SMS memory
char sms_del_queue[SMS_IDX_QUEUE_LEN];  // indeces of delete candidates in modem's SMS memory

char single_dest_phone[16];             // contains caller phone number while sending respond sms (copied from head of tx queue)

unsigned sms_msg_serial_num;            // serial of sent unsolicited sms for device lifecycle  (stored in eeeprom, two page-size buffers)

unsigned short ignition_timer = 0;
unsigned short ignition_timeout = 0;

#define SMS_REBOOT_LIMIT  10        // max 10 reboots per 1 hour
#define SMS_REBOOT_LIMIT_THRESHOLD  3600
#define SMS_REBOOT_LIMIT_DEC        (SMS_REBOOT_LIMIT_THRESHOLD /  SMS_REBOOT_LIMIT)

int sms_reboot_limiter = SMS_REBOOT_LIMIT_THRESHOLD;
unsigned sms_reboot_counter = 0;

#define SMS_LIMIT_MSG_PER_HOUR    60
#define SMS_LIMIT_THRESHOLD       3600000
#define SMS_LIMIT_DEC             (SMS_LIMIT_THRESHOLD / SMS_LIMIT_MSG_PER_HOUR)

int sms_limit_counter = SMS_LIMIT_THRESHOLD;

///static const char init_sequence_zero[] = "\r\r\r\rATE0\\Q0\r"; // MC52i and external 52
///static const char init_sequence_zero[] = "\r\r\r\rATE0\r";  // SIM900, \Q not supported, default flow ctrl is off
#warning "This is inappropriate for external modem, we should turn flow ctrl off"
//static const char init_sequence_zero[] = "\r\r\r\rATE0\r";  // let's believe flow control=default off for internal modem (MC52 and SIM900)
static const char init_sequence_zero[] = "ATE0\r";
//static const char init_sequence_one[] = "\rAT+CMGF=1;+CNMI=1,1\r";
static const char init_sequence_one[] = "AT+CMGF=1;+CNMI=1,1\r";
static const char smso_52[] = "\x1b""AT^smso\r"; // MC52i // for SIM900 it's anothe cmd, and another URC - TODO // used with battery power
static const char smso_900[] = "AT+cpowd=1\r"; // SIM900 normal power down
static char *smso = "AT\r";

short int receive_idx;             // index of sms for moving from read to delete queue after read is completed
short int send_idx;                // index of sms for moving to del queue after sms sending is completed
short int sms_purge_idx;           // index of sms in modem memory for purge at startup
unsigned char phone_idx;           // index of currenth dest phone number while sending sms, currently 0 or 1

enum sms_state_e
{
  SMS_STOPPED,
  SMS_START,
  SMS_IGT_AT,
  SMS_IGT_CFUN,
  SMS_IGT_CFUN_CHECK,
  SMS_EMEROFF_DEASSERT,
  SMS_IGT_ASSERT,
  SMS_INIT_0,
  SMS_INIT_1,
  SMS_INIT_2,
  SMS_INIT_3,
  SMS_INIT_PURGE,
  SMS_IDLE,
  SMS_BUSY_READ,
  SMS_BUSY_SEND,
  SMS_BUSY_GET_INFO,
  SMS_BUSY_USSD,
  SMS_CHECK_SEND_ERR,
  SMS_PURGE,
  SMS_DEBUG_1,
  SMS_DEBUG_2,
  SMS_DEBUG_3,
  SMS_SHUTDOWN_STARTED
} sms_state;

void sms_uart_rx_parse(void);
int  sms_parse_command(char *cmd_str, char *calling_phone);
static int final_code_ok(void);
static int final_code_error(void);
void delete_sms(int idx);
void send_sms(unsigned char *phone);
void sms_state_watchdog(void);


#ifndef UART_MODULE

void UART1_IRQHandler(void)
{
  unsigned reg, iir, lsr;
  iir = UxIIR;
  switch((iir >> 1) & 7)
  {
  case 0x03: // rx error
    lsr = UxLSR; // read to clear interrupt
    reg = UxRBR; // read from fifo and drop byte with error
    break;
  case 0x02: // rx fifo threshold
  case 0x06: // rx fifo timeout
    lsr = UxLSR;
    if((lsr & 1) == 0)
    {
      reg = UxRBR; // silicon bug? clears CTI interrupt with LSR.DA = 0 !
      break;
    }
    if(lsr & 1) sms_rx_timeout = sys_clock() + SMS_UART_RX_TIMEOUT;
    while(lsr & 1) // rx fifo is not empty
    {
      reg = UxRBR; // read data
      if(sms_rx_uart_idx < sizeof sms_rx_uart_buf - 1) // if place available, save it
        sms_rx_uart_buf[sms_rx_uart_idx++] = reg;
      lsr = UxLSR;
    }
    sms_rx_uart_buf[sms_rx_uart_idx] = 0; // z-term
    if(sms_rx_uart_idx >= sizeof sms_rx_uart_buf - 48)
      UxMCR &=~ 0x02; // de-assert RTS, disable rx flow
    break;
  } // switch
}

#endif // ndef UART_MODULE



void sms_halt_module(void)
{
  sms_emergency_halt = 1;
  gsm_emeroff(0);
#if PROJECT_CHAR == 'E'
  log_printf("SMS function is stopped. Unrecoverable error!");
#else
  log_printf("SMS модуль остановлен, неустранимая ошибка!");
#endif
  gsm_led(0);
  sms_gsm_failed = 2;
  sms_trap_gsm_alive();
}

static int match(const char *buf)
{
  char *s = sms_rx_buf;
  if(*s == '\r') ++s;
  if(*s == '\n') ++s;
  return memcmp(s, buf, strlen(buf)) == 0;
}

static int contains(const char *pattern)
{
  int i, m = 0;
  for(i=0; i<sms_rx_idx; ++i)
  {
    if(sms_rx_buf[i] == pattern[m]) ++m;
    else m = 0;
    if(pattern[m] == 0) return 1;
  }
  return 0;
}

static int final_code_ok(void)
{
  return sms_rx_idx >= 6 && strcmp(sms_rx_buf + sms_rx_idx - 6, "\r\nOK\r\n") == 0;
}

static int final_code_error(void)
{
  return sms_rx_idx >= 9 && strcmp(sms_rx_buf + sms_rx_idx - 9, "\r\nERROR\r\n") == 0;
}

void sms_uart_exec(void)
{
#ifndef UART_MODULE
  if(sms_rx_uart_idx > 0)
  {
    nvic_disable_irq(UxIRQ);
    sms_rx_idx += strlccpy(sms_rx_buf + sms_rx_idx, sms_rx_uart_buf, 0, sizeof sms_rx_buf - sms_rx_idx); // always returns only len of copied part (excl term 0)
    sms_rx_uart_idx = 0;
    sms_rx_uart_buf[0] = 0;
    nvic_enable_irq(UxIRQ);
    UxMCR |= 0x02; // assert RTS, restore flow
  }
#else
#error "tuned for direct UART interface w/interrupts"
#endif

  if(sms_rx_idx != 0 && sys_clock() > sms_rx_timeout) // "no new data" timeout
  {
    sms_rx_buf[sms_rx_idx] = 0; // zterm

    // log errors and debug - moved from parse, 4.01.2013
    if((sms_setup.flags & SMS_DEBUG_LOG) && sms_rx_idx_debug < sms_rx_idx)
    {
      log_printf("modem [%d]: '%s'", sms_state, sms_rx_buf + sms_rx_idx_debug);
      sms_rx_idx_debug = sms_rx_idx;
    }

    wait_more = 1;
    if( final_code_ok()
    ||  strcmp(sms_rx_buf, "\r\n> ") == 0
    || (memcmp(sms_rx_buf, "\r\n+C", 4) == 0 && sms_rx_buf[sms_rx_idx - 1] == '\n')
    ||  final_code_error()
    || (sms_state == SMS_SHUTDOWN_STARTED && contains("^SHUTDOWN\r") ) )
      wait_more = 0;

    if(wait_more && wait_more_counter)
    {
      if((sms_setup.flags & SMS_DEBUG_LOG)
      && wait_more_counter == WAIT_MORE_CYCLES)
        log_printf("SMS debug: waiting final result code");
      --wait_more_counter;
      sms_rx_timeout = sys_clock() + 300; // postpone parsing, wait more data
    }
    else
    {
      sms_uart_rx_parse();
      sms_rx_idx = 0;
      sms_rx_idx_debug = 0;
      wait_more_counter = WAIT_MORE_CYCLES;
    }
  }

  for(;;)
  {
    if((UxLSR & 0x40) == 0) break; // THRE not empty, continue output on next main loop cycle
    if(sms_tx_buf[sms_tx_idx] == 0) break; // end of tx string (z-terminated) is reached
    if(sms_tx_idx >= sizeof sms_tx_buf - 1) break; // end of buffer
    UxTHR = sms_tx_buf[sms_tx_idx++]; // transmit byte
  }

}

void sms_uart_init(void) // board-dependant
{
  ///// pins, UART power and clock are inited in gsm_pins_init() in project.c
  UxIER = 0; // disable uart interrupts
  UxLCR = 1 << 7 | // DLAB, map on divider regs
          3 << 0;  // 8 bits
  UxDLL = (SYSTEM_PCLK / 16 / SMS_BAUDRATE) & 0xff;
  UxDLM = (SYSTEM_PCLK / 16 / SMS_BAUDRATE) >> 8;
  UxLCR &= 0x7f; // clear DLAB, map off divider regs
  UxFCR |= 1 << 0 | // enable FIFO
           2 << 6;  // FIFO trigger depth = 8
  UxFCR |= 3 << 1;  // reset FIFOs (self-clearing bits)
  ///UxMCR |= 3 << 7;  // enable hardware flow control
#ifndef UART_MODULE
  sms_rx_uart_idx = 0;
  sms_rx_uart_buf[0] = 0;
# if CPU_FAMILY == LPC23xx
  vic_install_isr(UxIRQ, UxISR, 2);
# endif
  nvic_set_pri(UxIRQ, 2);
  nvic_enable_irq(UxIRQ);
  UxIER = 1<<2 | 1<<0; // RX data available+RX Timeout, RX error
#endif // ndef UART_MODULE
  /*
#warning "rem debug"
  UxMCR |= 1<<4; // enable loopback
  */
}

void sms_clear_rx_buf(void)
{
  sms_rx_idx = 0;
  sms_rx_buf[0] = 0;
}

void send_tx_buffer(void)
{
  sms_tx_buf[sizeof sms_tx_buf - 1] = 0; // z-term protection
  sms_tx_idx = 0; // initiate tx
  if(sms_setup.flags & SMS_DEBUG_LOG) log_printf("AT cmd [%d]:'%s'", sms_state, sms_tx_buf);
}

void send_printf(char *fmt, ... )
{
  va_list args;
  va_start(args, fmt);
  sms_tx_buf[0] = 0; // empty string, safe if vsprintf() fails to parse format
  vsprintf(sms_tx_buf, fmt, args);
  send_tx_buffer();
}

// enqueues sms and dest_phone; dest_phone may be 0 or ""
void sms_q_text(char *txt, char *dest_phone)
{
  for(int i=0; i<SMS_Q_LEN; ++i)
  {
    if(sms_tx_queue[i][0] == 0)
    {
      strlcpy(sms_tx_queue[i], txt, sizeof sms_tx_queue[0]);
      if(dest_phone != 0)
        strlcpy(sms_tx_tel_queue[i], dest_phone, sizeof sms_tx_tel_queue[0]);
      else
        sms_tx_tel_queue[i][0] = 0;
      break;
    }
  }
}

// removes first sms from queue
void sms_q_advance(void)
{
  if(sms_tx_queue[0][0] == 0) return;
  memmove(sms_tx_queue[0], sms_tx_queue[1], sizeof(sms_tx_queue) - sizeof sms_tx_queue[0] );
  memset(sms_tx_queue[SMS_Q_LEN - 1], 0, sizeof sms_tx_queue[0]); // it was wrong arg order, LBS 10.04.2013
  memmove(sms_tx_tel_queue[0], sms_tx_tel_queue[1], sizeof sms_tx_tel_queue - sizeof sms_tx_tel_queue[0]);
  memset(sms_tx_tel_queue[SMS_Q_LEN - 1], 0, sizeof sms_tx_tel_queue[0]);
}

// place to queue of indeces
void enqueue(char *q, char idx)
{
  for(int i=0; i<SMS_IDX_QUEUE_LEN; ++i)
  {
    if(q[i] == 0)
    {
      q[i] = idx;
      return;
    }
  }
}

// advance queue of indeces
char dequeue(char *q)
{
  char first = q[0];
  if(first == 0) return 0;
  memcpy(q, q+1, SMS_IDX_QUEUE_LEN - 1);
  q[SMS_IDX_QUEUE_LEN-1] = 0;
  return first;
}

// parsing input from modem, also state engine
void sms_uart_rx_parse()
{
  int sms_idx;
  char *tel, *cmd, *s, *z;

  /* moved to sms_uart_exec()
  // log errors and debug
  if(sms_setup.flags & SMS_DEBUG_LOG)
  {
    log_printf("modem [%d]: '%s'", sms_state, sms_rx_buf);
  }
  */

  if(match("+CME ERROR: SIM blocked") // try repeat // 10.01.2015
  || match("+CME ERROR: SIM busy")
  || match("+CMS ERROR: SIM busy") )
  { // initiate resend of command from tx_buf after some wait period
    sim_busy_waiting = sys_clock() + 3000;
    return;
  }
  else
  { // reset limiting SIM Busy attempt counter
    sim_busy_counter = 0;
  }

  if(match("+CME ERROR:") || match("+CMS ERROR:")) // rewrite 19.03.2014
  {
    char *errs = sms_rx_buf;
    if(*errs == '\r') ++errs;
    if(*errs == '\n') ++errs;
    strlccpy(sms_last_error, errs, '\r', sizeof sms_last_error);
    // if no debug, pass errors to log
    if((sms_setup.flags & SMS_DEBUG_LOG) == 0)
    {
      sms_tx_buf[sizeof sms_tx_buf - 1] = 0; // z-term prot-n
      sms_rx_buf[sizeof sms_rx_buf < 256 ? sizeof sms_rx_buf - 1 : 255] = 0; // zterm protection, limit length for weblog
      log_printf("GSM error '%s', command='%s'", errs, sms_tx_buf);
    }
    // halt if no SIM
    if(match("+CME ERROR: SIM not inserted")
    || match("+CMS ERROR: SIM not inserted")) // bug in SIM900 FF, CMS instead of CME
    {
      sms_halt_module();
      return;
    }
  }

  // process debug request
  if(sms_at_debug)
  {
    memcpy(sms_rx_at_debug_buf, sms_rx_buf, sizeof sms_rx_at_debug_buf);
    sms_rx_at_debug_buf[sizeof sms_rx_at_debug_buf - 1] = 0;
    sms_at_debug = 0; // LBS 22.02.2011
    return;
  }
  // at command completition reached, process pending shutdown request
  if(gsm_shutdown_req)
  {
    gsm_shutdown_req = 0;
    log_printf("GSM shutdown from busy state");
    send_printf((void*)smso);
    return;
  }
  // process Unsolocited Result Codes
  if(match("+CMTI: "))
  {
    sms_idx = atoi(sms_rx_buf + 14); // \r\n+CMTI: "ME", nn - read nn
    //enqueue(sms_rx_queue, sms_idx);
    enqueue(sms_emergency_halt == 0 ? sms_rx_queue : sms_del_queue, sms_idx);
  }
  else // process modem reply according to SMS engine state
  switch(sms_state)
  {
  case SMS_SHUTDOWN_STARTED:
    if(contains("^SHUTDOWN") || contains("NORMAL POWER DOWN"))
    {
      gsm_shutdown_timer = 100 + 1; // 10ms tick
    }
    break;
  case SMS_IGT_CFUN:
    if(final_code_ok()) // check answer on repeating 'ESC at CR'
    {
      send_printf("AT+cfun=1,1\r"); // reboot modem
      ignition_timer = 500; // 10ms tick, wait reboot
      sms_state = SMS_IGT_CFUN_CHECK;
    }
    break;
  case SMS_IGT_CFUN_CHECK:
    // drop any output (OK, ^SYSSTART etc.), just wait 5s and test with AT
    break;
  case SMS_INIT_0: // check modem responding via com
    if(final_code_ok()) // ... untill it will be responded with OK
    {
      ignition_timer = 0; // stop querying
      send_printf( gsm_model == 52 ? "AT^sm20=1,0\r" : "AT\r"); // ext.error reporting // only for MC52i, SIM900 and other - dummy big AT for autobaud
      sms_state = SMS_INIT_1;
    }
    else
    {
      // do nothing, wait sms state wdog reset
    }
    break;
  case SMS_INIT_1:
    send_printf("AT+cmee=2\r"); // +CSMS ERROR reporting
    sms_state = SMS_INIT_2;
    break;
  case SMS_INIT_2:
    send_printf((char*)init_sequence_one); // send init string
    sms_state = SMS_INIT_3;
    break;
  case SMS_INIT_3:
    if(final_code_ok())
    {
      if(gsm_model == 52)
      { // MC52i
        sms_purge_idx = 1;
        delete_sms(sms_purge_idx);
        sms_state = SMS_INIT_PURGE;
      }
      else
      { // SIM900
        send_printf("AT+cmgd=1,4\r"); // SIM900 supports 'delete all stored SMS'
        sms_purge_idx = 250; // skip index scan
        sms_state = SMS_INIT_PURGE;
      }
    }
    else
      log_printf("GSM init error");
    break;
  case SMS_INIT_PURGE:
    if(sms_purge_idx < 6)
    {
      delete_sms(++sms_purge_idx);
    }
    else
    {
#if PROJECT_CHAR == 'E'
      log_printf("GSM modem is initialized and ready for communication");
#else
      log_printf("GSM модем готов к работе");
#endif
      sms_state = SMS_IDLE;
    }
    break;
  case SMS_BUSY_READ:
    if(match("+CMGR: "))
    {
      if(sms_rx_idx < 16) // CRLF+CMGR: 0,,0 - reading from empty sms slot, wrong sms cell index
      {
        sms_state = SMS_IDLE;
        break;
      }
      // get source tel. number (reply to source number is not used now)
      tel = sms_rx_buf + 23; // \r\n+CMGR: "REC UNREAD","+79136159985"  - skip to src phone
      for(z = tel; *z && *z!='"'; ++z) {} // scan to " at the end of tel. number
      if(*z == 0) { sms_state = SMS_IDLE; break; } // rotten input, no " after number
      *z++ = 0; // skip " and zterm src teleph number

      // skip to command body after LF, trim remainder of input
      for(z = z; *z && *z!='\n'; ++z) {} // scan to LF at the end of 1st line of URC
      if(*z == 0) { sms_state = SMS_IDLE; break; } // rotten input, no LF at end of 1st string
      cmd = z + 1; // skip this LF
      for(z = cmd; *z && *z!='\r'; ++z) {} // scan to CR at the end of sms body
      *z = 0; // z-term

      sms_parse_command(cmd, tel);
      enqueue(sms_del_queue, receive_idx);
      receive_idx = 0;
      sms_state = SMS_IDLE;
    }
    break;
  case SMS_BUSY_SEND:
    if(match("> "))
    {
      // pop first sms from queue
      sms_tx_queue[0][sizeof(sms_tx_queue[0])-1] = 0; // safe z-term
      send_printf("%s\x1a", sms_tx_queue[0]); // type sms body and Ctrl-Z
      strlcpy(single_dest_phone, sms_tx_tel_queue[0], sizeof single_dest_phone);
      sms_q_advance(); // remove message from RAM queue
    }
    else
    if(match("+CMGW: "))
    { // get index of writed sms, send it
      send_idx = atoi(sms_rx_buf + 8); // \r\n+CMGW: nnn, get nnn, atoi() skips leading space
      phone_idx = 0; // now starting value 0 // 5.02.2014
      goto start_sending_sms; // the same code for first and over phone numbers
    }
    break;
  case SMS_BUSY_GET_INFO:
    if(match("+CREG: "))
    {
      sms_gsm_registration = atoi(sms_rx_buf + 11);
      send_printf("AT+csq\r");
    }
    else
    if(match("+CSQ: "))
    {
      sms_sig_level = atoi(sms_rx_buf + 7);
      sms_state = SMS_IDLE;
    }
    break;
  case SMS_BUSY_USSD:
    if(match("+CUSD: "))
    {
      for(s=sms_rx_buf; *s && *s!='"'; ++s) {}
      if(*s==0) { sms_state = SMS_IDLE; break; }
      ++s;
      for(z=s; *z && *z!='"'; ++z) {}
      if(*z==0) { sms_state = SMS_IDLE; break; }
      *z = 0;
      decode_ussd(s);
      sms_state = SMS_IDLE;
    }
    break;
  case SMS_CHECK_SEND_ERR:
    if( match("ERROR") || match("+CMS ERROR: ") ||  match("+CME ERROR: ") )
    {
#if PROJECT_CHAR == 'E'
      log_printf("SMS sending not completed, error");
#else
      log_printf("SMS сбой отправки");
#endif
    }

  start_sending_sms:

    if(single_dest_phone[0] != 0)
    { // send to caller
      if(phone_idx == 0 && sms_emergency_halt == 0) // initial val = 0
      { // send
        ++phone_idx;
        send_sms((unsigned char*)single_dest_phone);
        sms_state = SMS_CHECK_SEND_ERR;
      }
      else
      { // delete
        enqueue(sms_del_queue, send_idx); // sms was sent to all dest phones, move it to delete queue
        send_idx = 0;
        sms_state = SMS_IDLE;
      }
    }
    else
    { // send to pre-programmed phones
      for(; phone_idx < MAX_DEST_PHONE; ++phone_idx)
        if(sms_setup.dest_phone[phone_idx][0] != 0)
          break; // find not empty phone
      if(phone_idx < MAX_DEST_PHONE && sms_emergency_halt == 0)
      { // send to this dest phone number
        sms_setup.dest_phone[phone_idx][15] = 0; // zterm protection, pasc-zterm string
//// #warning remove debugggggggggggggg
        send_sms(sms_setup.dest_phone[phone_idx] + 1);
        /*
log_printf("debugggggg: sending smsto %s", sms_setup.dest_phone[phone_idx]);
send_printf("AT\r");
        */
        ++ phone_idx;
        sms_state = SMS_CHECK_SEND_ERR;
      }
      else
      { // all dest phones processed, clear message
        enqueue(sms_del_queue, send_idx); // sms was sent to all dest phones, move it to delete queue
        send_idx = 0;
        sms_state = SMS_IDLE;
      }
    }
    break;
  case SMS_PURGE:
    sms_state = SMS_IDLE;
    break;
  } // if .. else switch(sms_state) {};
}


// send unsolicited message (it isn't used for responce to sms command)
void sms_msg_printf(char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);

  char msg[384];
  unsigned char name[64];

  ++sms_msg_serial_num;
  // wrilte to two buffers, kinda wear levelling
  EEPROM_WRITE(eeprom_sms_msg_serial[sms_msg_serial_num & 1], &sms_msg_serial_num, sizeof sms_msg_serial_num);

  str_pasc_to_zeroterm(sys_setup.hostname, name, sizeof name);
  sprintf(msg, "%s (%u) ",
      name[0] ? name : "NETPING",
      sms_msg_serial_num);
  vsprintf(msg + strlen(msg), fmt, args);
  msg[159] = 0; // limit sms length

  if(sms_state == SMS_SHUTDOWN_STARTED
  || sms_state == SMS_STOPPED
  || sms_emergency_halt)
  {
    log_printf("GSM is down, sms dropped: %s", msg);
    return;
  }

  sms_q_text(msg, 0); // enqueue message
}

void delete_sms(int idx)
{
  send_printf("AT+cmgd=%d\r", idx);
}

void start_ussd(void)
{
  strcpy(sms_ussd_responce, "-");
  if(sms_setup.ussd_string[0] == 0 || sms_setup.ussd_string[0] == 0xff ) return;
  if(sms_state != SMS_IDLE) return;
  sms_state = SMS_BUSY_USSD;
  sms_setup.ussd_string[sizeof sms_setup.ussd_string - 1] = 0;
  send_printf("AT+cusd=1,\"%s\"\r", sms_setup.ussd_string + 1);
}

void ask_gsm_info(void)
{
  sms_gsm_registration = sms_emergency_halt ? 254 : 255; // halted or updating
  sms_sig_level = 255; // = updating info
  if(sms_state != SMS_IDLE) return;
  sms_state = SMS_BUSY_GET_INFO;
  send_printf("AT+creg?\r");
}

void startup_sequence(void)
{
  static char at_1_count = 0;

  if(sms_state > SMS_INIT_0) return;
  if(ignition_timer) return;
  switch(sms_state)
  {
  case SMS_START:
    at_1_count = 0;
    // проваливаемся
  case SMS_IGT_AT:
    sms_clear_rx_buf();
    ///send_printf("\x1b""AT\r");
    send_printf("AT\r"); // 29.01.2015 from at+cmgw, it will return OK the same as from idle, AT will be written as message and deleted during SMS_PURGE phase
    ignition_timer = 100; // 10ms tick
    sms_state = SMS_IGT_CFUN;
    break;
  case SMS_IGT_CFUN:
    if(++at_1_count < 5)
      sms_state = SMS_IGT_AT;
    else
    { // no reply on AT, forced hardware reboot or cold start
      gsm_emeroff(0); // assert
      ignition_timer = 10; // 10ms tick
      sms_state = SMS_EMEROFF_DEASSERT;
    }
    break;
  case SMS_EMEROFF_DEASSERT:
    gsm_emeroff(1);
    ignition_timer = 50; // 10ms tick
    sms_state = SMS_IGT_ASSERT;
    break;
  case SMS_IGT_ASSERT:
#if PROJECT_CHAR != 'E'
    log_printf("Запуск GSM модема (IGT)");
#else
    log_printf("Start of GSM modem (IGT)");
#endif
    gsm_igt(0); // assert
    if(gsm_model == 52)
      ignition_timer = 20; // 10ms tick // MC52i
    else
      ignition_timer = 100; // 10ms tick  // SIM900
    sms_state = SMS_INIT_0;
    break;
  case SMS_IGT_CFUN_CHECK:
  case SMS_INIT_0:
    gsm_igt(1); // deassert
    sms_clear_rx_buf();
    send_printf((char*)init_sequence_zero);
    ignition_timer = 200; // 10ms tick
    sms_state = SMS_INIT_0;
    break;
  }
}

void gsm_alive_test()
{
  systime_t time = sys_clock();
  if(sms_state == SMS_IDLE && time > sms_gsm_test_time)
  {
    sms_state = SMS_BUSY_GET_INFO;
    sms_gsm_test_time = time + 180000; // 3 min
    sms_gsm_registration = 255; // indicate updating
    sms_sig_level = 255; // indicate updating
    send_printf("AT+creg?\r");
  }
}

void sms_send_from_queue(void)
{
  if(sms_state != SMS_IDLE) return;
  if(sms_tx_queue[0][0] == 0) return; // egressing sms queue is empty
  int i;
  for(i=0; i<MAX_DEST_PHONE; ++i)
    if(sms_setup.dest_phone[i][0] != 0)
      break;
  if(i == MAX_DEST_PHONE && sms_tx_tel_queue[0][0] == 0)
  { // no dest numbers to send to
    sms_q_advance(); // remove sms from tx queue
  }
  else
  {
    sms_state = SMS_BUSY_SEND;
    send_printf("AT+cmgw=\"000000\"\r"); // start 'write sms to memory' command
  }
}

void sms_read_received_from_queue(void)
{
  if(sms_state != SMS_IDLE) return;
  int rx_idx = dequeue(sms_rx_queue);
  if(rx_idx == 0) return;
  sms_state = SMS_BUSY_READ;
  receive_idx = rx_idx;
  send_printf("AT+cmgr=%u\r", rx_idx); // request sms read
}

void send_sms(unsigned char *phone)
{
  sms_limit_counter -= SMS_LIMIT_DEC;
  if(sms_limit_counter > 0)
  {
    send_printf("AT+cmss=%d,\"%s\"\r", send_idx, phone);
  }
  else
  {
#if PROJECT_CHAR == 'E'
    log_printf("SMS: too many outgoing messages (>%u per hour)", SMS_LIMIT_MSG_PER_HOUR);
#else
    log_printf("СМС сообщения отправляются слишком часто (>%u в час)!", SMS_LIMIT_MSG_PER_HOUR);
#endif
    sms_halt_module();
  }
}

void sms_purge_del_queue(void)
{
  if(sms_state != SMS_IDLE) return;
  int del_idx = dequeue(sms_del_queue);
  if(!del_idx) return;
  sms_state = SMS_PURGE;
  delete_sms(del_idx);
}

void sms_resend_on_sim_busy(void)
{
  if(sys_clock() > sim_busy_waiting)
  {
    sim_busy_waiting = ~0ULL;
    if(++sim_busy_counter < 30)
    {
      send_tx_buffer();
    }
    else
    {
      #if PROJECT_CHAR == 'E'
        log_printf("SMS: repeating unrecoverable SIM Busy error!");
      #else
        log_printf("SMS: повторяющаяся ошибка SIM Busy!");
      #endif
      sms_halt_module();
    }
  }
}

void sms_exec(void)
{
  if(sms_emergency_halt) return; // harakiri executed!
  sms_state_watchdog();
  // ignition_sequence();
  startup_sequence();
  sms_uart_exec();
  sms_resend_on_sim_busy();
  gsm_alive_test();
  sms_purge_del_queue();
  sms_read_received_from_queue();
  sms_send_from_queue();
}

void sms_timer_10ms(void)
{
  if(ignition_timer > 0) --ignition_timer;
  ++ ignition_timeout;
  if(gsm_shutdown_timer > 1)
    --gsm_shutdown_timer;
  else
  if(gsm_shutdown_timer == 1)
  {
    gsm_shutdown_timer = 0;
    sms_state = SMS_STOPPED;
  }
}

void sms_timer_1s(void)
{
  if(sms_reboot_limiter < SMS_REBOOT_LIMIT_THRESHOLD) ++sms_reboot_limiter;
  if(sms_limit_counter  < SMS_LIMIT_THRESHOLD) sms_limit_counter += 1000;
}

void sms_reset_params(void)
{
  util_fill((void*)&sms_setup, sizeof sms_setup, 0);
  strcpy((char*)sms_setup.ussd_string, "\x05*100#");
  sms_setup.pinger_period = 30;
  EEPROM_WRITE(&eeprom_sms_setup, &sms_setup, sizeof eeprom_sms_setup);
  EEPROM_WRITE(&eeprom_sms_signature, &sms_signature, 4);
}

#if PROJECT_MODEL == 200 || PROJECT_MODEL == 213

void sms_reset_params_mkarta(void)
{
  util_fill((void*)&sms_setup, sizeof sms_setup, 0);
  str_pzt_cpy(sms_setup.hostname, "NetPing", sizeof sms_setup.hostname);
  str_pzt_cpy(sms_setup.dest_phone[0], "+79210000005",  sizeof sms_setup.dest_phone[0]);
  str_pzt_cpy(sms_setup.ussd_string, "*100#", sizeof sms_setup.ussd_string);
  get_ip("172.22.55.21", sms_setup.pinger_ip);
  sms_setup.pinger_period = 30;
  sms_setup.event_mask = SMS_EVENT_PWR | SMS_EVENT_ETHERNET | SMS_EVENT_BATTERY | SMS_EVENT_PINGER;
  EEPROM_WRITE(&eeprom_sms_setup, &sms_setup, sizeof eeprom_sms_setup);
  EEPROM_WRITE(&eeprom_sms_signature, &sms_signature, 4);
}

#endif

void sms_init(void)
{
  if(gsm_model == 52) smso = (char*)smso_52;
  if(gsm_model == 900) smso = (char*)smso_900;
  // read setup
  unsigned sign;
  EEPROM_READ(&eeprom_sms_signature, &sign, 4);
  if(sign != sms_signature)
  {
    sms_reset_params();
  }
  EEPROM_READ(&eeprom_sms_setup, &sms_setup, sizeof sms_setup);
  // reset (new) periodic setup
  EEPROM_READ(&eeprom_sms_periodic_time_signature, &sign, 4);
  if(sign != sms_periodic_time_signature)
  {
    memset(sms_setup.periodic_time, 0, sizeof sms_setup.periodic_time);
    EEPROM_WRITE(eeprom_sms_setup.periodic_time, sms_setup.periodic_time, sizeof eeprom_sms_setup.periodic_time);
    EEPROM_WRITE(&eeprom_sms_periodic_time_signature, &sms_periodic_time_signature, sizeof eeprom_sms_periodic_time_signature);
  }
  // read tx message serial number
  unsigned a,b; // get last message serial id
  EEPROM_READ(eeprom_sms_msg_serial[0], &a, sizeof a);
  EEPROM_READ(eeprom_sms_msg_serial[1], &b, sizeof b);
  if(a == ~0) a = 0;
  if(b == ~0) b = 0;
  sms_msg_serial_num = a > b ? a : b;
  // hardware init
  gsm_pins_init();
  gsm_igt(1);
  gsm_emeroff(1);
  sms_uart_init();
#ifndef BATTERY_MODULE
  delay(20);
  gsm_power(1);
#endif
  // reset uart communication and parser
  sms_tx_idx = 0;
  sms_tx_buf[0] = 0;
  sms_rx_idx = 0;
  sms_rx_idx_debug = 0;
  sms_rx_buf[0] = 0;
  sms_rx_timeout = ~0LL;
  wait_more_counter = WAIT_MORE_CYCLES;
  sim_busy_waiting = ~0LL;
  // reset modem protocol
  receive_idx = 0;
  send_idx = 0;
  sms_purge_idx = 1;
  phone_idx = 0;
  sms_ussd_responce_ready = 0;
  // clear old outgoing messages, clear sms queues
  memset(sms_tx_queue, 0, sizeof sms_tx_queue);
  memset(sms_tx_tel_queue, 0, sizeof sms_tx_tel_queue);
  memset(sms_rx_queue, 0, sizeof sms_rx_queue);
  memset(sms_del_queue, 0, sizeof sms_del_queue);
  // start init, reset state
  ignition_timer = 600; ///////// postpone startup, 10ms tick
  sms_state = SMS_START;
/////log_printf("sending CR LF CR LF");
//////send_printf("\r\n\r\n");
  // reset GSM network watchdog
  sms_gsm_test_time = sys_clock() + 20000;
  sms_gsm_registration = 255;
  sms_sig_level = 255; // = updating info
  // reset debug grabber
  sms_at_debug = 0;
  // turn GSM LED off
  gsm_led(0);

  // init pinger (in sms_cmd.c) once (sms_init() used multiple times for restart)
  static char pinger_inited = 0;
  if(pinger_inited == 0)
    pinger_init();
  pinger_inited = 1;

  // restore unrecoverable  halt
  sms_emergency_halt = 0;
  // mark last error  obsolete
#if PROJECT_CHAR == 'E'
  if(sms_last_error[0] != 0 && strstr(sms_last_error, "restart)") == 0)
     strcat(sms_last_error, " (occuried before modem restart)");
#else
  if(sms_last_error[0] != 0 && strstr(sms_last_error, "(до") == 0)
     strcat(sms_last_error, " (до рестарта модема)");
#endif
}

#ifdef HTTP_MODULE

unsigned sms_debug_http_set(void)
{
  http_post_data((void*)sms_tx_buf, 16); // it was 50, 14.05.2015
  log_printf("GSM debug command: '%s'", sms_tx_buf);
  sms_tx_idx = 0;
  sms_rx_buf[0] = 0;
  sms_rx_idx = 0;
  sms_rx_idx_debug = 0;
  sms_at_debug = 1;
  http_redirect("/at.html");
  return 0;
}

unsigned sms_debug_http_get(unsigned pkt, unsigned more_data)
{
  char data[1024];
  sms_rx_at_debug_buf[sizeof sms_rx_at_debug_buf - 1] = 0; // protect
  unsigned len = sprintf(data, "<html><body><pre>\r%s\r</pre></body></html>\r", sms_rx_at_debug_buf);
  tcp_put_tx_body(pkt, (unsigned char*)data, len);
  return 0;
}

void sms_modem_log_switch(unsigned pkt, unsigned log_state)
{
  if(log_state) sms_setup.flags |= SMS_DEBUG_LOG;
  else sms_setup.flags &=~ SMS_DEBUG_LOG;
  EEPROM_WRITE(&eeprom_sms_setup, &sms_setup, sizeof sms_setup);
  char data[192];
  unsigned len = sprintf(data, "<html><body><pre>GSM modem AT logging switched %s!</pre></body></html>\r",
                         log_state ? "on" : "off");
  tcp_put_tx_body(pkt, (unsigned char*)data, len);
}

unsigned sms_modem_log_on(unsigned pkt, unsigned more_data)
{
  sms_modem_log_switch(pkt, 1);
  return 0;
}

unsigned sms_modem_log_off(unsigned pkt, unsigned more_data)
{
  sms_modem_log_switch(pkt, 0);
  return 0;
}

unsigned sms_restart_http_set(void)
{
#if PROJECT_CHAR == 'E'
  log_printf("Manual reboot of GSM modem via web");
#else
  log_printf("Ручной рестарт GSM модема через веб-интерфейс");
#endif
  sms_init();
  http_redirect("/sms.html");
  return 0;
}

HOOK_CGI(gsmlog,      (void*)sms_modem_log_on,      mime_html,  HTML_FLG_GET | HTML_FLG_NOCACHE );
HOOK_CGI(gsmnolog,    (void*)sms_modem_log_off,     mime_html,  HTML_FLG_GET | HTML_FLG_NOCACHE );
HOOK_CGI(atget,       (void*)sms_debug_http_get,    mime_html,  HTML_FLG_GET | HTML_FLG_NOCACHE );
HOOK_CGI(at,          (void*)sms_debug_http_set,    mime_js,    HTML_FLG_POST );
HOOK_CGI(gsm_restart, (void*)sms_restart_http_set,  mime_js,    HTML_FLG_POST );

#endif // ifdef HTTP_MODULE

void sms_state_watchdog(void)
{
  static systime_t timeout;
  static enum sms_state_e old_sms_state = SMS_IDLE;
  systime_t time = sys_clock();
  if(sms_state != old_sms_state)
  {
    timeout = time + 30000;
    old_sms_state = sms_state;
  }
  if(sms_state == old_sms_state
  && sms_state >= SMS_INIT_0  // 6.01.2013
  && sms_state != SMS_IDLE
  && sms_state != SMS_SHUTDOWN_STARTED // 17.02.2013
  && time > timeout)
  {
    if(sms_gsm_failed != 1)
    {
      sms_gsm_failed = 1; // 7.02.2014
      sms_trap_gsm_alive();
    }
    gsm_led(0); // 25.02.2014
    sms_reboot_limiter -= SMS_REBOOT_LIMIT_DEC;
    if(sms_reboot_limiter > 0)
    {
#if PROJECT_CHAR == 'E'
      log_printf("SMS function hangs, restart of SMS module!");
#else
      log_printf("Зависание модуля СМС, перезапуск");
#endif
      ++sms_reboot_counter;
      sms_init();
    }
    else
    {
#if PROJECT_CHAR == 'E'
      log_printf("SMS: too many hangs (>%u per hour)!", SMS_REBOOT_LIMIT);
#else
      log_printf("Слишком частые аварийные перезапуски модуля СМС (>%u в час)!", SMS_REBOOT_LIMIT);
#endif
      sms_halt_module();
    }
  }
  if(sms_state == SMS_IDLE // ready for use
  && (sms_gsm_registration == 1 || sms_gsm_registration==5)) // registered in home gsm netwk or roaming
  {
    if(sms_gsm_failed != 0)
    {
      sms_gsm_failed = 0;
      sms_trap_gsm_alive();
    }
    gsm_led(1); // 25.02.2014
  }
}

int sms_modem_is_ready(void)
{
  return sms_state == SMS_IDLE;
}

int sms_modem_is_busy(void)
{
  return sms_state > SMS_IDLE;
}

int gsm_modem_is_stopped(void)
{
  return sms_state == SMS_STOPPED;
}

int gsm_modem_is_shutting_down(void)
{
  return sms_state == SMS_SHUTDOWN_STARTED;
}

void gsm_soft_shutdown(void)
{
  gsm_shutdown_timer = 0;
  if(sms_emergency_halt)
  {
    // force string to uart (using 16 bytes of FIFO)
    log_printf("GSM shutdown from emergency halted state");
    char *p = (void*)smso;
    while(*p) UxTHR = *p++;
    gsm_shutdown_timer = 10000; // 10ms tick, 10s 'blind' wait before forced poweroff
    sms_state = SMS_SHUTDOWN_STARTED;
  }
  else
  if(sms_state == SMS_IDLE)
  {
    log_printf("GSM shutdown from idle state");
    send_printf((void*)smso);
    gsm_shutdown_timer = 1000; // 10ms tick, 10s 'blind' wait before forced poweroff
    sms_state = SMS_SHUTDOWN_STARTED;
  }
  else
  if(sms_state == SMS_START) // delay before start is in progress, IGT was not engaged
  {
    log_printf("GSM startup is canceled before ignition");
    sms_state = SMS_STOPPED;
  }
  else
  {
    log_printf("GSM is busy, shutdown requested");
    gsm_shutdown_req = 1;
  }
}

int sms_event(enum event_e event)
{
  switch(event)
  {
  case E_EXEC:
    sms_exec();
    etherstatus_exec();
    pinger_exec();
#ifndef NOTIFY_MODULE
    sms_cmd_periodic_exec();
#endif
    break;
  case E_TIMER_10ms:
    sms_timer_10ms();
    break;
  case E_TIMER_1s:
    sms_timer_1s();
    break;
  case E_RESET_PARAMS:
    sms_reset_params();
    break;
#if PROJECT_MODEL == 200 || PROJECT_MODEL == 213
  case E_RESET_PARAMS_MULTIKARTA:
    sms_reset_params_mkarta();
    break;
#endif
  }
  return 0;
}

#endif

#warning TODO сообщение об отбросе очереди СМСок при ре-ините, или их досылка
#warning TODO прочистка памяти при рестарте интерферирует с приёмом свежих СМС
