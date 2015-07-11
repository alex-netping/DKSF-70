/*
 * Модуль NTP.c
v 2.3
22.02.2010
v 2.4
16.03.2010
v 2.5
31.03.2010
 english clock adjust msg
v 2.6-51,52
20.09.2010
  removed legacy PARAMETERS references
v 2.7-51,52
20.09.2010
  ntp_time_is_actual()
  starting time bugfix
v 2.8-200
18.02.2011
  short-time reboot persistence
v2.9-52
31.03.2011
  russian DST is removed
  ntp_status added, ntp_exec() rewrite, 1 min timeout if no responce from servers
v2.10-48
8.04.2013
  ntp_calendar() argument meaning is changed
  DST flag addded
  RTC adopted
v2.11-48
18.06.2013
  bugfix of ntp to rtc saving
  ntp_calendar() renamed to ntp_calendar_tz(), to avoid confusion with old variant
*/

#include "platform_setup.h"


#ifdef NTP_MODULE
	
#ifndef NTP_DEBUG
	#undef DEBUG_SHOW_TIME_LINE
	#undef DEBUG_MSG			
	#undef DEBUG_PROC_START
	#undef DEBUG_PROC_END
    #undef DEBUG_INPUT_PARAM
    #undef DEBUG_OUTPUT_PARAM	

	#define DEBUG_SHOW_TIME_LINE
	#define DEBUG_MSG			
	#define DEBUG_PROC_START(msg)
	#define DEBUG_PROC_END(msg)
    #define DEBUG_INPUT_PARAM(msg,val)	
    #define DEBUG_OUTPUT_PARAM(msg,val)	
#endif


#define NTP_SYNC_PERIOD  1024                           //  period of  NTP  request, sec
//#define NTP_SYNC_PERIOD  25

#define NTP_1MS   ((1LL<<32)/1000)                      //  step of NTP timestamp per 1ms

__no_init unsigned long long local_time;                //  local NTP timestamp, 00:00:00 1.01.1970 starting point
__no_init unsigned long long local_time_inv;            //  local NTP check for RAM persistence during reboot
__no_init unsigned long long ntp_time_prev @ "DATA_Z";  //  NTP timestamp from previous request
unsigned long long ntp_step = NTP_1MS;                  //  step of NTP timestamp per one local timer tick (imperfect 1ms)
__no_init signed long long ntp_steering  @ "DATA_Z";    //  correction step, steers local clock to NTP clock
__no_init unsigned since_last_correction @ "DATA_Z";    //  ms elapsed from last correction
unsigned short ntp_req_id = 0x3333;                     //  request serial id
unsigned char ntp_state = 1;
__no_init systime_t   ntp_timeout @ "DATA_Z";
unsigned char DST_flag = 0;                             // DST active
enum ntp_status_e ntp_status = NTP_STATUS_TIME_NOT_SET; // status of NTP client

void ntp_timer(void)
{
  local_time += ntp_step + ntp_steering;
  local_time_inv = ~local_time; // inversed, to check RAM-persistent values after reboot
  ++ since_last_correction;
}

void send_ntp_req(unsigned char *ip)
{
  uword pkt = udp_create_packet();
  if(pkt == 0xff) return;

  unsigned char buf[48];
  util_fill(buf, sizeof buf, 0);
  buf[0] = 4<<3 | 3;  //  VN=4; Mode=3
  ntp_req_id += 1;
  buf[47] = ntp_req_id & 255; // LSB of xmit timestamp
  buf[46] = ntp_req_id >> 8;

  ip_get_tx_header(pkt);
  util_cpy(ip, ip_head_tx.dest_ip, 4);
  ip_put_tx_header(pkt);

  udp_get_tx_header(pkt);
  udp_tx_head.src_port[0] = 0;
  udp_tx_head.src_port[1] = 123;
  udp_tx_head.dest_port[0] = 0;
  udp_tx_head.dest_port[1] = 123;
  udp_put_tx_header(pkt);

  udp_tx_body_pointer = 0;
  udp_put_tx_body(pkt, buf, sizeof buf);

  udp_send_packet(pkt, sizeof buf);
}

unsigned long long abs64(signed long long val)
{
  return val < 0 ? -val : val;
}

void parse_ntp(void)
{
  udp_get_rx_header();
  if(((udp_rx_head.dest_port[0] << 8) | udp_rx_head.dest_port[1]) != 123) return;
  if(((udp_rx_head.src_port[0] << 8) | udp_rx_head.src_port[1]) != 123) return;
  unsigned char buf[64];
  udp_rx_body_pointer = 0;
  udp_get_rx_body(buf, sizeof buf);
  if((buf[0] & 7) != 4) return; // packet isn't answer
  // check request id
  if(buf[31] != (ntp_req_id & 255)) return; // LSB of Originate timestamp
  if(buf[30] != (ntp_req_id >> 8)) return;
  // extract timestamp
  unsigned long long ntp_time = 0;
  for(int i=40; i<48; ++i)
    ntp_time = (ntp_time << 8 ) | buf[i];
#ifdef RTC_MODULE
  save_to_rtc((unsigned)(ntp_time >> 32) - (unsigned)TIMEBASE_DIFF);
#endif
  // compute correction step
  /*
  delta = local_time - ntp_time;
  ntp_step_correction += -delta * 1.75 / steps_elapsed_from_last_correction;
  */
  // __disable_interrupt(); // freeze timer-advanced values
  unsigned cpsr = proj_disable_interrupt(); // LBS 16.03.2010
  signed long long delta = local_time - ntp_time;
  if(abs64(delta) > ((2LL * 60) << 32 ) ) // > 2 minute
  {
    // set hard
#if  PROJECT_CHAR != 'E'
    log_printf("переустановка...");
    local_time = ntp_time;
    log_printf("...часов");
#elif PROJECT_CHAR == 'E'
    log_printf("local clock...");
    local_time = ntp_time;
    log_printf("...is adjusted to NTP clock");
#endif
    ntp_step = (1LL<<32)/1000;
    ntp_steering = 0;
    ntp_time_prev = 0;
  }
  else
  {
    // tune local time pace
    if(ntp_time_prev == 0)
    {
      ntp_step = (1LL<<32)/1000; // default step
    }
    else
    {
      unsigned long long ntp_step_new = (ntp_time - ntp_time_prev) / since_last_correction;
      ntp_step = (ntp_step + ntp_step_new)>>1;
    }
    // steer clock faster/slower to compensate local-NTP delta
    ntp_steering = -delta/(NTP_SYNC_PERIOD*1000);
  }
  ntp_time_prev = ntp_time;
  since_last_correction = 0;
  // __message ntp_time:%x;__message local_time:%x;__message " "
  /*
  log_printf("ntp delta:%c%5u", delta<0?'-':' ', (unsigned int)(abs64(delta) / MS_DIVIDER));
  */
  // __enable_interrupt();
  proj_restore_interrupt(cpsr); // LBS 16.03.2010
  // reset ntp request cycle
  ntp_status = ntp_state <= 2 ? NTP_STATUS_SET_FROM_SERVER_1 : NTP_STATUS_SET_FROM_SERVER_2 ;
  ntp_timeout = sys_clock() + NTP_SYNC_PERIOD*1000;
  ntp_state = 1;
}



#define ip1 sys_setup.ntp_ip1
#define ip2 sys_setup.ntp_ip2

void ntp_exec(void)
{
  systime_t time = sys_clock();
  if(time < ntp_timeout) return;
  ntp_steering = 0;
  if(!valid_ip(ip1) && !valid_ip(ip2))
  {
    ntp_status = NTP_STAUS_NO_SETUP;
    return;
  }
  if(!ntp_time_is_actual())
    ntp_status = NTP_STATUS_TIME_NOT_SET;
  switch(ntp_state)
  {
  case 1:
  case 2:
    if(!valid_ip(ip1))
    {
      ntp_state = 3;
      break;
    }
    send_ntp_req(ip1);
    ntp_timeout = time + 4000;
    ntp_state += 1;
    break;
  case 3:
  case 4:
    if(!valid_ip(ip2))
    {
      ntp_state = 5;
      break;
    }
    send_ntp_req(ip2);
    ntp_timeout = time + 4000;
    ntp_state += 1;
    break;
  case 5:
    ntp_status = NTP_STATUS_NO_REPLY;
    ntp_timeout = time + 60*1000;
    ntp_state = 1;
    break;
  }
}

/*
void ntp_exec(void)
{
  static union { // static! old value is retained!
    unsigned char ip[4];
    unsigned ip32;
  };
  systime_t time = sys_clock();
  if(sys_clock() < ntp_timeout) return;
  switch(ntp_state)
  {
  case 1:
    ntp_steering = 0;
    util_cpy(sys_setup.ntp_ip1, ip, 4);
    if(ip32 == 0 || ip32 == 0xffffffff)
    {
      ntp_state = 3; break;
    }
    send_ntp_req(ip);
    ntp_timeout = time + 4000;
    ntp_state = 2;
    break;
  case 2:
    send_ntp_req(ip);
    ntp_timeout = time + 4000;
    ntp_state = 3;
    break;
  case 3:
    util_cpy(sys_setup.ntp_ip2, ip, 4);
    if(ip32 == 0 || ip32 == 0xffffffff)
    {
      ntp_state = 1;
      ntp_timeout = time + NTP_SYNC_PERIOD*1000;
      break;
    }
    send_ntp_req(ip);
    ntp_timeout = time + 4000;
    ntp_state = 4;
    break;
  case 4:
    send_ntp_req(ip);
    ntp_timeout = time + NTP_SYNC_PERIOD*1000;
    ntp_state = 1;
    break;
  }
}
*/

void ntp_init(void)
{
  ntp_state = 1;
  ntp_timeout = sys_clock() + 10*1000;; // 10s pause after reboot before fist request
  ntp_step = (1LL<<32)/1000;
  ntp_steering = 0;
#ifdef RTC_MODULE
  if(RTCAUX_bit.RTC_OSCF == 0)
  { // no RTC clock fault, RTC time probably is valid
    unsigned long long local_time_from_rtc = (get_from_rtc() + TIMEBASE_DIFF) << 32;
    unsigned s = proj_disable_interrupt();
    if( local_time != ~local_time_inv // cold start, no valid time in RAM
    ||  ntp_time_is_actual() == 0 // wrong time evristics
    ||  abs64(local_time_from_rtc - local_time) > (0x0101ULL<<24) ) // > ~ 1.001s diff
    {
      local_time = local_time_from_rtc; // set time in RAM from RTC
      local_time_inv = ~local_time; // valid time 'signature'
    }
    proj_restore_interrupt(s);
  }
#endif
  unsigned s = proj_disable_interrupt();
  if(local_time != ~local_time_inv) // cold start, no correct time in RAM
  {
    local_time = (unsigned long long)TIMEBASE_DIFF << 32;
    local_time_inv = ~local_time; // valid time 'signature'
  }
  proj_restore_interrupt(s);
}


int ntp_dst(struct tm *date)
{
   /* Russian DST algorithm (c) 2007 P.V.Lyubasov, gm.luxtech@gmail.com */
   int m, d, w, cd;
   m = date->tm_mon + 1;  // tm_mon is 0..11
   if(m < 3 || m > 10) return 0;
   if(m > 3 && m < 10) return 1;

   // m = 3 or 10
   w = date->tm_wday; // tm_wday is 0..6 since Sunday
   if(w==0) w = 7;    // w = 1..7 since Monday
   d = date->tm_mday; //  tm_mday is 1-31
   cd = d; // current date of month
   while(d > 1) // retrace calendar to 1st date of month
   {
      --d; --w;
      if(w == 0) w = 7;
   }
   while(w != 7)  // find 1st sunday
   {
      ++w; ++d;
   }
   while(d+7 <= 31) d+=7;  // find last sunday

   // m = 3 or 10
   if(m == 3)
   {  // march
      if(cd > d) return 1; // in period by date
      if(cd == d && date->tm_hour >= 2) return 1; // date = last sunday, 2:00:00 and after
   }
   else if(m == 10)
   { // october
      if(cd < d) return 1; // in period by daye
      if(cd == d && date->tm_hour <  2) return 1; //  date = last summer, up to 01h59m59s last sunday
   }
   return 0; // march or october,  not in summer time range
}


#define YEAR0                   1900
#define EPOCH_YR                1970
#define SECS_DAY                (24L * 60L * 60L)

//#define LEAPYEAR(year)          (!((year) % 4) && (((year) % 100) || !((year) % 400)))
#define LEAPYEAR(year)          (((year) & 3) == 0) // 100 and 400 years exceptions are not practical for embedded system

//#define YEARSIZE(year)          (LEAPYEAR(year) ? 366 : 365)
#define YEARSIZE(year)           ((year) & 3 ? 365 : 366)

const static unsigned char _ytab[2][12] =
{
  {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
  {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}
};

// equivalent of gm_time() from standart C runtime lib

struct tm *ntp_gmtime_r(const time_t time, struct tm *tmbuf)  // not *time, but time, arg by value!
{
  unsigned long dayclock, dayno;
  int year = EPOCH_YR;
  unsigned long tmp;

  dayclock = (unsigned long) time % SECS_DAY;
  dayno = (unsigned long) time / SECS_DAY;

  tmbuf->tm_sec = dayclock % 60;
  tmbuf->tm_min = (dayclock % 3600) / 60;
  tmbuf->tm_hour = dayclock / 3600;
  tmbuf->tm_wday = (dayno + 4) % 7; // Day 0 was a thursday

  while (dayno >= (tmp = (unsigned long) YEARSIZE(year)))
  {
    dayno -= tmp; // -= YEARSIZE(year);
    year++;
  }
  tmbuf->tm_year = year - YEAR0;
  tmbuf->tm_yday = dayno;
  tmbuf->tm_mon = 0;
  while (dayno >= (tmp = (unsigned long) _ytab[LEAPYEAR(year)][tmbuf->tm_mon]) )
  {
    dayno -= tmp; // _ytab[LEAPYEAR(year)][tmbuf->tm_mon];
    tmbuf->tm_mon++;
  }
  tmbuf->tm_mday = dayno + 1;
  tmbuf->tm_isdst = 0;

  return tmbuf;
}



struct tm *ntp_calendar_tz(int use_timezone_and_dst)
{
  __no_init static struct tm date @ "DATA_Z";
  time_t time_s;

  time_s = (unsigned)(local_time >> 32);
  if(time_s >= (unsigned)TIMEBASE_DIFF) time_s -= (unsigned)TIMEBASE_DIFF;
  if(use_timezone_and_dst)
  {
    time_s += sys_setup.timezone * 3600;
    if(sys_setup.dst) time_s += 3600;
  }
  ntp_gmtime_r(time_s, &date);
  return &date; // pointer to global static var
}

int ntp_time_is_actual(void)
{
  return (unsigned long)(local_time>>32) > ((100U * 365U) + 24U) * 24U * 3600U; // примерно 1.01.2000 (високосные дни учтены наобум)
}

unsigned ntp_event(enum event_e event)
{
  switch(event)
  {
  case E_TIMER_1ms:
    ntp_timer();
    break;
  case E_EXEC:
    ntp_exec();
    break;
  case E_INIT:
    ntp_init();
    break;
  }
  return 0;
}

#endif

