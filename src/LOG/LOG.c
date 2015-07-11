/*
* LOG
* by P.Lyubasov
*v1.5
*22.02.2010
*v1.6
*31.05.2010
* minor #if #else adjustment for old/new platform
* english startup msg
*v1.6.1-200
*21.12.2010
* cosmetic edit
*v1.7-50
*12.08.2010 by LBS
* void log_clear(void)  // merged from  DKSF_53.1.7 LBS 8.07.2010
*v1.7-52
*30.08.2011
* char via_web[], via_snmp[] moved here from project.c
* cosmetic edit
*v1.8-50
*28.09.2010 by LBS
*  removed prj_const[]
*1.8-52
*5.05.2012
*  via_url[] added
*v1.9-50
*20.08.2012
* added SN and notification email into SysLog message
*v1.10-60
*11.10.2012
* removed legacy, notif.email ported
*v1.10-213
*15.10.2012
* dksf213 support (sans html)
* print_date()
* English days of week
*v1.11-48
*15.04.2013
* argument of ntp_calendar() has changed
*v1.11-50
*17.06.2013
* bugfix in log_printf()
*v1.15-52
*15.08.2013
* hostname
* SSEvents - unsuccessful, TODO UTF-8 encoding
*/

#include "platform_setup.h"
#ifdef LOG_MODULE
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "eeprom_map.h"


#if PROJECT_CHAR == 'E'
const char via_web[]  = "by web-interface";
const char via_url[]  = "by cgi call";
const char via_snmp[] = "by SNMP";
#else
const char via_web[]  = "через веб-интерфейс";
const char via_url[]  = "вызовом cgi";
const char via_snmp[] = "через SNMP";
#endif

void log_read(unsigned addr, unsigned char *buf, unsigned size);
void log_write(unsigned addr, const unsigned char *buf, unsigned size);
unsigned log_wrap_addr(unsigned addr);
void send_syslog(struct tm *date, char *message);
void send_sse(unsigned char *buf, unsigned len);

// updated to 0x72,0x83,0x94,0xA5 in log_init()
// to be different from constatnt in FW update data in EEPROM
unsigned char log_marker[4] = {0x71,0x82,0x93,0xA4};

const static unsigned char crlf[2] = {'\r','\n'};

unsigned char log_enable_syslog = 0; // syslog enable flag, is set after initialisation of (network) modules

__no_init int log_pointer; // EEEPROM addr of log end marker

/*
unsigned addr=LOG_START;
unsigned marker_n = 0;
unsigned buf_n = 0;
*/

/*
from_addr - начальный адрес сканирования
marker - 4-ба
*/

//
//unsigned char log_debug[LOG_SIZE];
//

void log_read(unsigned addr, unsigned char *buf, unsigned size)
{
  EEPROM_READ(addr, buf, size);
  ////util_cpy(log_debug+addr-LOG_START, buf, size);
}

void log_write(unsigned addr, const unsigned char *buf, unsigned size)
{
  if(addr < LOG_START) return;
  if(addr + size - 1 > LOG_END) return;
  EEPROM_WRITE(addr, (void*)buf, size);
  ////util_cpy((void*)buf, log_debug+addr-LOG_START, size);
}

static int find_marker(unsigned from_addr, unsigned len, const unsigned char *marker, unsigned marker_len)
{
  unsigned addr = from_addr & ~(LOG_BLOCK_SIZE-1);
  unsigned buf_offs = from_addr & (LOG_BLOCK_SIZE-1);
  unsigned char buf[LOG_BLOCK_SIZE];
  int marker_pos, marker_n;

  if(len==0) return -1;
  LOG_READ(addr, buf, sizeof buf);
  marker_n = 0; // LBS 11.2009 !!!! не было инициализации - БАГ!
  for(;;)
  {
    if(buf[buf_offs] == marker[marker_n])
    {
      if(marker_n == 0) marker_pos = addr + buf_offs;
      if(marker_n == marker_len-1) return marker_pos;
      ++ marker_n;
    }
    else
    {
      marker_n = 0;
    }
    if(++buf_offs >= sizeof buf)
    {
      buf_offs = 0;
      addr += sizeof buf;
      if(addr >= LOG_END) addr = LOG_START;
      LOG_READ(addr, buf, sizeof buf);
    }
    if(--len == 0) return -1;
  }
  // marker is absent
  // hard reset of eeprom buffer
}

//// also called from notify.c!
void log_wrapped_write(unsigned char *txt, int len)
{
  unsigned end = log_pointer + len - 1;
  if(end <= LOG_END)
  {
    LOG_WRITE(log_pointer, txt, len);
  }
  else
  {
    unsigned n = LOG_END - log_pointer + 1;
    LOG_WRITE(log_pointer, txt, n);
    LOG_WRITE(LOG_START, txt + n, len - n);
  }
}

/*******************************

алгоритм добавления
- найти (взять) позицию END маркера
- с начала маркера записать сообщение, CR LF
- записать END маркер
- запомнить в RAM позицию END маркера

алгоритм распечатки:
- найти END // перед end маркером - последние сообщения кольца
- с этого адреса найти CR LF // пропустить испорченную строку, на которую наехали новые сообщения
- напечатать с текущего адреса до END // напечатать всё

инициализация кольцевого лога:
END FF FF FF FF ... FF FF CR LF
^ LOG_START           LOG_END ^

********************************/


const char wdname[7][4] =
#if PROJECT_CHAR == 'E'
    {"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"};
#else
    {"Вс", "Пн", "Вт", "Ср", "Чт", "Пт", "Сб"};
#endif

unsigned print_date(char *buf, struct tm *date) // 15.10.2012
{
   return sprintf((char*)buf, "%02d.%02d.%02d %c%c %02d:%02d:%02d.%03u",
                      date->tm_mday,        //  tm_mday is 1-31
                      date->tm_mon + 1,     // tm_mon is 0..11
                      date->tm_year % 100,  // tm_year is year
                      wdname[date->tm_wday][0], wdname[date->tm_wday][1], // wday is 0..6 since Sunday
                      date->tm_hour,
                      date->tm_min,
                      date->tm_sec,
                      ((unsigned)local_time) / MS_DIVIDER //  millisecounds
                      );
}

#pragma __printf_args
int log_printf(char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
#warning "big data in stack"
  unsigned char buf[512]; // 27.11.2010, it was 192
  struct tm *date = ntp_calendar_tz(1); // 15.04.2013, different args, show local time
  unsigned len = print_date((char*)buf, date);
  buf[len++] = ' ';
  buf[len] = 0;  // 17.06.2013 ++ removed
  char *message = (char*)&buf[len]; // skip timestamp
  len += vsprintf(message, fmt, args);

  send_syslog(date, message); // send_syslog() needs only body, w/o textual timestamp
  /////send_sse(buf, len);

  buf[len++] = '\r'; // mark end of message for weblog ring memory
  buf[len++] = '\n';
  util_cpy(log_marker, &buf[len], sizeof log_marker); // add wrap marker
  log_wrapped_write(buf, len + sizeof log_marker);
  log_pointer = log_wrap_addr(log_pointer + len);
  va_end(args);
  return len;
}

unsigned log_wrap_addr(unsigned addr)
{
  return addr <= LOG_END ? addr : LOG_START + (addr - (LOG_END+1));
}

int log_wrap_diff(unsigned start, unsigned end)
{
  int diff = end - start;
  return diff >= 0 ? diff : LOG_SIZE + diff ;
}

#define LOG_BUFLEN 1024 // enlarged from [512], 9.05.2013

// http handler data pumping stuff
static int current_addr;
static int log_len_to_print = 0;
//

unsigned log_http_handler_get(unsigned pkt, unsigned more_data)
{
  char buf[LOG_BUFLEN];
  unsigned len;
  if(more_data == 0 || log_len_to_print == 0)
  {
    current_addr = find_marker(log_pointer, LOG_SIZE, crlf, 2); // finf first CRLF after END marker (skip overwritten string)
    current_addr = log_wrap_addr(current_addr+2); // skip CRLF
    log_len_to_print = log_wrap_diff(current_addr, log_pointer);
  }
  len = current_addr + LOG_BUFLEN <= LOG_END ? LOG_BUFLEN : LOG_END - current_addr + 1;
  if(len > log_len_to_print) len = log_len_to_print;
  LOG_READ(current_addr, (unsigned char*)buf, len);
  tcp_put_tx_body(pkt, (unsigned char*)buf, len);
  current_addr = log_wrap_addr(current_addr+len);
  log_len_to_print -= len;
  return log_len_to_print;
}


#ifdef HTTP_MODULE

//HOOK_CGI(log,  (void*)log_http_handler_get,  mime_js,  HTML_FLG_GET | HTML_FLG_NOCACHE );
HOOK_CGI(log,  (void*)log_http_handler_get,  "text/plain; charset=windows-1251",  HTML_FLG_GET | HTML_FLG_NOCACHE );

#endif


void log_init(void)
{
  log_enable_syslog = 0; // disable network part (syslog) till initialization of UDP module

  // modify log_marker to be different from constatnt in FW data
  if(log_marker[0] == 0x71) for(int i=0;i<4;++i) log_marker[i]++;
  //
  log_pointer = find_marker(LOG_START, LOG_SIZE, log_marker, sizeof log_marker);
  if(log_pointer == -1)
  {
    LOG_WRITE(LOG_END - sizeof crlf + 1, crlf, sizeof crlf);
    LOG_WRITE(LOG_START, log_marker, sizeof log_marker);
    log_pointer = LOG_START;
  }
#ifdef HTTP_MODULE
  log_len_to_print = 0;
#endif
  char devname_buf[128];
  print_dev_name_fw_ver(devname_buf); // sprintf z-terminate himself
#if   PROJECT_CHAR !='E'
  log_printf("Начало работы (перезагрузка) %s", devname_buf);
#elif PROJECT_CHAR =='E'
  log_printf("Firmware start, %s", devname_buf);
#endif
}

const unsigned char month_name[12][4] = { "Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec" };

void send_syslog_to_ip(unsigned char *part1, unsigned char *part2, unsigned char *part3, unsigned char *ip)
{
  unsigned pkt = udp_create_packet();
  if(pkt==0xFF) return;

  ip_get_tx_header(pkt);
  util_cpy(ip, ip_head_tx.dest_ip, 4);
  ip_put_tx_header(pkt);

  udp_get_tx_header(pkt);
  udp_tx_head.src_port[0] = 514 >> 8;
  udp_tx_head.src_port[1] = 514 & 255; // SYSLOG port
  udp_tx_head.dest_port[0] = 514 >> 8;
  udp_tx_head.dest_port[1] = 514 & 255; // SYSLOG port
  udp_put_tx_header(pkt);

  udp_tx_body_pointer = 0;
  int len1 = strlen((char*)part1);
  if(len1>64) len1 = 64;
  udp_put_tx_body(pkt, part1, len1);
  int len2 = strlen((char*)part2);
  if(len2 > 768) len2 = 768;
  udp_put_tx_body(pkt, part2, len2);
  int len3 = strlen((char*)part3);
  if(len3 > 64) len3 = 64; // 11.10.2012, it was len2>64
  udp_put_tx_body(pkt, part3, len3);

  udp_send_packet(pkt, len1 + len2 + len3);
}

void send_syslog(struct tm *date, char *message)
{
  if(!log_enable_syslog) return;

  unsigned char head[128], tail[64], ipstr[16];
  extern const unsigned serial;
  str_ip_to_str(sys_setup.ip, (char*)ipstr);
  sys_setup.hostname[sizeof sys_setup.hostname - 1] = 0; // z-term protection
  sprintf((char*)head, "<%u>%3s %2u %02u:%02u:%02u.%03u %s NetPing: ",
              (sys_setup.facility<<3) | sys_setup.severity,
              month_name[date->tm_mon],
              date->tm_mday,
              date->tm_hour,
              date->tm_min,
              date->tm_sec,
              ((unsigned)local_time) / MS_DIVIDER, //  millisecounds
              sys_setup.hostname[0] ? sys_setup.hostname + 1 : ipstr
            );
#ifndef NOTIFY_MODULE
  sys_setup.notification_email[sizeof sys_setup.notification_email - 1] = 0; // z-term protection
  sprintf((char*)tail, " [ SN %u %s%s]",
             serial,
             sys_setup.notification_email + 1,
             sys_setup.notification_email[0] ? " " : ""
            );
#else
  tail[0] = 0;
#endif
  if(valid_ip(sys_setup.syslog_ip1)) send_syslog_to_ip(head, (unsigned char*)message, tail, sys_setup.syslog_ip1);
  if(valid_ip(sys_setup.syslog_ip2)) send_syslog_to_ip(head, (unsigned char*)message, tail, sys_setup.syslog_ip2);
}

/*
#error TODO for SSE it needs to be encoded as UTF-8
void send_sse(unsigned char *msg, unsigned len)
{
  if(tcp_socket[sse_sock].tcp_state != TCP_ESTABLISHED) return;
  unsigned pktlen = (len + 80) & 0xffffff00;
  if(pktlen == 0) pktlen = 1;
  unsigned pkt = tcp_create_packet_sized(sse_sock, pktlen << 8);
  if(pkt == 0xff) return;
  char *hdr = "event: log\n""data: ";
  tcp_tx_body_pointer = 0;
  tcp_put_tx_body(pkt, (void*)hdr, strlen(hdr));
  tcp_put_tx_body(pkt, msg, len);
  tcp_put_tx_body(pkt, "\n\n", 2);
  tcp_send_packet(sse_sock, pkt, tcp_tx_body_pointer);
}
*/

void log_clear(void)  // merged from  DKSF_53.1.7 LBS 8.07.2010
{
  unsigned char buf[128];
  util_fill(buf, sizeof buf, 0);
  unsigned addr=LOG_START;
  unsigned len=LOG_SIZE;
  unsigned chunk;
  do
  {
    chunk = len > sizeof buf ? sizeof buf : len;
    len -= chunk;
    EEPROM_WRITE(addr, buf, chunk);
    addr += chunk;
  } while(len);
  log_init();
}

unsigned log_event(enum event_e event)
{
  switch(event)
  {
  case E_INIT:
    log_init();
    break;
  }
  return 0;
}


#endif
