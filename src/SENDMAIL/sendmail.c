/*
v1.2-70
25.11.2014
  bugfix in tcp_cli_parsing(), port filtering, it was replying on ANY unknown to it dst (local) port
v1.3-70
11.02.2015
  ignore errors on session closing
  reset TCP Client errors
  session timeout
  tcp issue for dups longer than original
  clear numeric ip if empty fqnd posted from sendmail.html page
  enable checkbox
v1.4-52/201/202
10.06.2015
  sendmail_tx_buf rewrite
  Date: header added
  crossing dup segment rewrite in TCP Client
*/

#include "platform_setup.h"
#include "eeprom_map.h"
#include "plink.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

const unsigned sendmail_signature = 300448764;
const unsigned sendmail_cc_signature = 100558995;
struct sendmail_setup_s sendmail_setup;

void tcp_cli_connect(unsigned char *ip, unsigned short port);
void tcp_cli_puts(char *s); // place in tx buffer and send
void tcp_cli_send_data(int data_len); // just send tcp_cli.tx_data buffer

void tcp_cli_clear(void);
void tcp_cli_rst_and_clear(void);

unsigned char sendmail_smtp_ip[4];
unsigned short sm_server_code;
char *sm_request = ""; // text just for for error reporting

#define TCP_CLI_WINDOW 4096

#define TCP_FIN 0x01
#define TCP_SYN 0x02
#define TCP_RST 0x04
#define TCP_PSH 0x08
#define TCP_ACK 0x10

struct tcp_hdr_s {
  unsigned short port_src;
  unsigned short port_dst;
  unsigned seq;
  unsigned ack;
  unsigned char  offset; // top 4 bits used; len of hdr in words
  unsigned char  flags;
  unsigned short window;
  unsigned short checksum;
  unsigned short urg_ptr;
  unsigned char  option[4];
};

struct tcp_client_s tcp_cli;

enum sendmail_state_e sm_state;

// 100ms ticks, session must be completed before this time (from tcp open to tcp closed or tcp error)
unsigned sm_session_timeout = ~0U;
// flag, correctly formatted complete responce from server has received
int sm_responce_ready;

char sendmail_tx_buf[256*6]; // queue of outgoing e-mails
char *sendmail_tail = sendmail_tx_buf; // tail of queue, next after last z-term char

void sendmail(char *subj, char *body, struct tm *d)
{
  // Date: Tue, 2 Jun 2015 16:50:07 +0700
  const char wdays[7][4] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
  const char month[12][4] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
  char date[40];
  sprintf(date, "%s, %u %s %04u %02u:%02u:%02u %+03d00",
    wdays[d->tm_wday], d->tm_mday, month[d->tm_mon], d->tm_year + 1900,
    d->tm_hour, d->tm_min, d->tm_sec, sys_setup.timezone);

  unsigned total_len = strlen(date) + strlen(subj) + strlen(body) + 3; // +3 zeroterm 0s
  if(sendmail_tail + total_len <= sendmail_tx_buf + sizeof sendmail_tx_buf)
  {
    sendmail_tail += sprintf(sendmail_tail, "%s%c%s #%08x%c%s",
          date, 0, subj, (unsigned)(local_time>>22), 0, body) + 1;  // with 'random' element in subj
  }
  else
  {
    char s[48];
    strlcpy(s, subj, 40);
    if(strlen(s) != strlen(subj)) // 13.08.2014
      strcat(s, "...");
    log_printf("No room for new mail message \"%s\"", s);
  }
}

void sm_remove_message(void)
{
  if(sendmail_tx_buf[0] == 0) return;
  char *s = sendmail_tx_buf;
  s += strlen(s) + 1; // date
  s += strlen(s) + 1; // subj
  s += strlen(s) + 1; // body
  unsigned new_len = sendmail_tail - s;
  unsigned del_msg_len = s - sendmail_tx_buf;
  memmove(sendmail_tx_buf, s, new_len);
  sendmail_tail -= del_msg_len;
  memset(sendmail_tail, 0, del_msg_len);
}

enum sendmail_state_e sendmail_state;
__no_init unsigned short tcp_cli_port;

unsigned short tcp_cli_next_local_port(void)
{
  return 37888 + (++tcp_cli_port & 1023);
}

static const char b64[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

unsigned sm_place_base64(char *dst, char *src, unsigned src_len)
{
  char *p = src;
  char *q = dst;
  unsigned d, pad = 0, dst_len = 0;
  for(;;)
  {
    if(src_len == 0) break;
    pad = 2;
    d = *p++ << 16; --src_len;
    if(src_len) { d |= *p++ << 8; --pad; --src_len;}
    if(src_len) { d |= *p++; --pad; --src_len;}
    *q++ = b64[(d >> 18) & 63];
    *q++ = b64[(d >> 12) & 63];
    *q++ = b64[(d >>  6) & 63];
    *q++ = b64[(d      ) & 63];
    dst_len += 4;
    if(pad) break;
  }
  *q = 0;
  if(pad >= 1) q[-1] = '=';
  if(pad == 2) q[-2] = '=';
  return dst_len;
}

void sendmail_exec_and_parsing(void)
{
  static unsigned sm_retry_time = 300; // 30c startup pause
  if(sys_clock_100ms < sm_retry_time) return; // wait pause before  repeat after fail

  int session_timed_out = sys_clock_100ms > sm_session_timeout;
  if(session_timed_out || tcp_cli.state == TCPC_ERROR)
  {
    if(session_timed_out)
      tcp_cli_rst_and_clear(); // if timed out, send RST to server and reset client to TCPC_IDLE
    else
      tcp_cli_clear(); // if error, just reset tcp client to TCPC_IDLE
    if(sm_state != SM_CLOSING) // on SM_CLOSING ignore errors, like RST from server)
    { // retry mail sending after pause
      sm_state = SM_IDLE;
      sm_session_timeout = ~0U;
      sm_retry_time = sys_clock_100ms + 300; // 30s pause between retries (infinite)
      return;
    }
  }

  // wait TX completition
  if(tcp_cli.tx_state != TX_FREE) return;

  if(sm_state > SM_CONNECT && sm_state < SM_CLOSING)
  { // wait server reply
    // check it's last line (nnn<sp>blablabla\r\n)
    if(tcp_cli.rx_data_len < 6) return;
    tcp_cli.rx_data[tcp_cli.rx_data_len] = 0; // z-term
    char *p = tcp_cli.rx_data + tcp_cli.rx_data_len - 2;
    if(p[0] != '\r' || p[1] != '\n') return;
    while(p > tcp_cli.rx_data && p[-1] != '\n') --p; // scan to beginning of last line
    if(p[3] != ' ') return; // last line starts nnn<sp>, non-last starts nnn-
    sm_server_code = atoi(p);
    sm_responce_ready = 1;
  }

  char *p, *s;
  unsigned len, chunk_len;
  char buf[128];

  switch(sm_state)
  {
  case SM_IDLE:
    if(sendmail_tx_buf[0] != 0)
      sm_state = SM_CONNECT;
    return;
  case SM_CONNECT:
    if((sendmail_setup.flags & SM_FLG_ENABLE_SM) == 0 || !valid_ip(sendmail_smtp_ip))
    {
      sm_remove_message();
      sm_state = SM_IDLE;
      return;
    }
    sm_request = "connect";
    tcp_cli_connect(sendmail_smtp_ip, sendmail_setup.port);
    sm_session_timeout = sys_clock_100ms + 200;
    sm_state = SM_EHLO;
    break;
  case SM_EHLO:
    if(tcp_cli.state != TCPC_ESTABLISHED) return;
    if(sm_server_code != 220) goto error; // check connect prompt
    sm_request = "EHLO";
    tcp_cli_puts("EHLO netping-device\r\n");
    if(sendmail_setup.user[0])
      sm_state = SM_AUTH;
    else
      sm_state = SM_MAIL_FROM; // if credentials empty, skip AUTH
    break;
  case SM_AUTH:
    if(sm_server_code != 250) goto error; // check EHLO reply
    s = strstr(tcp_cli.rx_data, "250 AUTH ");
    if(!s) s = strstr(tcp_cli.rx_data, "250-AUTH ");
    if(!s) goto error;
    char auth_str[128];
    strlccpy(auth_str, s, '\n', sizeof auth_str);
    if(strstr(auth_str, "PLAIN"))
    {
      sm_request = "AUTH PLAIN";
      len = sprintf(buf, "%c%s%c%s", 0, sendmail_setup.user+1, 0, sendmail_setup.passwd+1);
      p = tcp_cli.tx_data;
      p += sprintf(p, "AUTH PLAIN ");
      p += sm_place_base64(p, buf, len);
      *p++ = '\r'; *p++ = '\n';
      tcp_cli_send_data(p - tcp_cli.tx_data);
      sm_state = SM_MAIL_FROM;
    }
    else if(strstr(auth_str, "LOGIN"))
    {
      sm_request = "AUTH LOGIN";
      strcpy(tcp_cli.tx_data, "AUTH LOGIN\r\n");
      tcp_cli_send_data(-1);
      sm_state = SM_USERNAME;
    }
    else
      goto error;
    break;
  case SM_USERNAME:
    if(sm_server_code != 334) goto error; // check AUTH LOGIN reply
    sm_request = "Username";
    p = tcp_cli.tx_data;
    p += sm_place_base64(p, (char*)sendmail_setup.user+1, sendmail_setup.user[0]);
    *p++ = '\r'; *p++ = '\n';
    tcp_cli_send_data(p - tcp_cli.tx_data);
    sm_state = SM_PASSWD;
    break;
  case SM_PASSWD:
    if(sm_server_code != 334) goto error; // check AUTH LOGIN reply
    sm_request = "Password";
    p = tcp_cli.tx_data;
    p += sm_place_base64(p, (char*)sendmail_setup.passwd+1, sendmail_setup.passwd[0]);
    *p++ = '\r'; *p++ = '\n';
    tcp_cli_send_data(p - tcp_cli.tx_data);
    sm_state = SM_MAIL_FROM;
    break;
  case SM_MAIL_FROM:
    if(sendmail_setup.user[0])
    {
      if(sm_server_code != 235) goto error; // if credentials was sent, check AUTH PLAIN reply
    }
    else
    {
      if(sm_server_code != 250) goto error; // check EHLO response
    }
    sm_request = "MAIL FROM";
    tcp_cli_send_data(sprintf(tcp_cli.tx_data, "MAIL FROM:<%s>\r\n", sendmail_setup.from + 1));
    sm_state = SM_RCPT_TO;
    break;
  case SM_RCPT_TO:
    if(sm_server_code != 250) goto error; // check MAIL FROM reply
    sm_request = "RCPT TO";
    tcp_cli_send_data(sprintf(tcp_cli.tx_data, "RCPT TO:<%s>\r\n", sendmail_setup.to + 1));
    sm_state = SM_RCPT_CC_1;
    break;
  case SM_RCPT_CC_1:
    if(!(sm_server_code == 250 || sm_server_code==251 || sm_server_code==252)) goto error; // check RCPT TO reply
    if(sendmail_setup.cc_1[0] != 0)
    {
      sm_request = "RCPT TO (CC 1)";
      tcp_cli_send_data(sprintf(tcp_cli.tx_data, "RCPT TO:<%s>\r\n", sendmail_setup.cc_1 + 1));
      sm_state = SM_RCPT_CC_2;
      break;
    }
    // no break
  case SM_RCPT_CC_2:
    if(!(sm_server_code == 250 || sm_server_code==251 || sm_server_code==252)) goto error; // check RCPT TO reply
    if(sendmail_setup.cc_2[0] != 0)
    {
      sm_request = "RCPT TO (CC 2)";
      tcp_cli_send_data(sprintf(tcp_cli.tx_data, "RCPT TO:<%s>\r\n", sendmail_setup.cc_2 + 1));
      sm_state = SM_RCPT_CC_3;
      break;
    }
    // no break
  case SM_RCPT_CC_3:
    if(!(sm_server_code == 250 || sm_server_code==251 || sm_server_code==252)) goto error; // check RCPT TO reply
    if(sendmail_setup.cc_3[0] != 0)
    {
      sm_request = "RCPT TO (CC3)";
      tcp_cli_send_data(sprintf(tcp_cli.tx_data, "RCPT TO:<%s>\r\n", sendmail_setup.cc_3 + 1));
      sm_state = SM_DATA;
      break;
    }
    // no break
  case SM_DATA:
    if(!(sm_server_code == 250 || sm_server_code==251 || sm_server_code==252)) goto error; // check RCPT TO reply
    sm_request = "DATA";
    tcp_cli_puts("DATA\r\n");
    sm_state = SM_BODY;
    break;
  case SM_BODY:
    if(sm_server_code != 354) goto error; // check DATA reply
    sm_request = "data enter";
    p = tcp_cli.tx_data;
    p += sprintf(p,
           "From:<%s>\r\n"
           "To:<%s>\r\n",
              sendmail_setup.from + 1,
              sendmail_setup.to + 1);
    if(sendmail_setup.cc_1[0]) p += sprintf(p, "CC: <%s>\r\n", sendmail_setup.cc_1 + 1);
    if(sendmail_setup.cc_2[0]) p += sprintf(p, "CC: <%s>\r\n", sendmail_setup.cc_2 + 1);
    if(sendmail_setup.cc_3[0]) p += sprintf(p, "CC: <%s>\r\n", sendmail_setup.cc_3 + 1);
    s = sendmail_tx_buf;
    p += sprintf(p, "Date: %s\r\n", s);
    s += strlen(s) + 1; // skip date, get to subj
    p += sprintf(p, "Subject: ");
    len = strlen(s); // len of subj
    for(;;)
    {
      p += sprintf(p, "=?Windows-1251?B?");
      chunk_len = 42;
      if(chunk_len > len) chunk_len = len;
      p += sm_place_base64(p, s, chunk_len);
      p += sprintf(p, "?=\r\n");
      s += chunk_len;
      len -= chunk_len;
      if(len == 0) break;
      else *p++ = ' '; // ident from 2nd string
    }
    s += 1; // skip zeroterm of subj, get to the body
    p += sprintf(p, "Content-Type: text/plain; charset=Windows-1251\r\n"
                    "Content-Transfer-Encoding: base64\r\n"
                    "\r\n");
    if(s[0] == 0)
    {
      *p++ = '\r'; *p++ = '\n'; // only subj, empty body
    }
    else
    {
      for(len = strlen(s); len > 0;)
      {
        chunk_len = 57;
        if(chunk_len > len) chunk_len = len;
        p += sm_place_base64(p, s, chunk_len);
        len -= chunk_len; s += chunk_len;
        *p++ = '\r'; *p++ = '\n';
      }
    }
    p += sprintf(p, "\r\n.\r\n");
    // initiate tx in TCP client
    tcp_cli_send_data(p - tcp_cli.tx_data);
    sm_state = SM_QUIT;
    break;
  case SM_QUIT:
    if(sm_server_code != 250) goto error; // check data enter reply
    sm_request = "QUIT";
    tcp_cli_puts("QUIT\r\n");
    sm_state = SM_CLOSING;
    break;
  case SM_CLOSING:
    if(tcp_cli.state == TCPC_IDLE)
    {
      // remove message from mail tx queue
      sm_remove_message();
      // mail transfer completed
      sm_session_timeout = ~0U;
      sm_state = SM_IDLE;
    }
    break;
  } // switch(sm_state)
  if(sm_responce_ready)
  { // 'consume' rx data, reset rx completed flag
    tcp_cli.rx_data_len = 0;
    sm_responce_ready = 0;
  }
  return;
error:
#if PROJECT_CHAR != 'E'
  log_printf("sendmail: в ответ на %s получено %s", sm_request, tcp_cli.rx_data);
  log_printf("sendmail: сообщение отброшено");
#else
  log_printf("sendmail: request %s replied error %s", sm_request, tcp_cli.rx_data);
  log_printf("sendmail: message has dropped");
#endif
  sm_request = "QUIT (after error)";
  tcp_cli_puts("QUIT\r\n");
  sm_state = SM_CLOSING;
  tcp_cli.rx_data_len = 0;
  sm_responce_ready = 0;
}

void tcp_cli_puts(char *s)
{
  strlcpy(tcp_cli.tx_data, s, sizeof tcp_cli.tx_data);
  tcp_cli.tx_data_p = tcp_cli.tx_data;
  tcp_cli.tx_data_len = strlen(s);
  tcp_cli.tx_state = TX_NEW;
}

void tcp_cli_send_data(int data_len)
{
  tcp_cli.tx_data_len = data_len == -1 ? strlen(tcp_cli.tx_data) : data_len;
  tcp_cli.tx_data_p = tcp_cli.tx_data;
  tcp_cli.tx_state = TX_NEW;
}

void memswap(void *a, void *b, unsigned len)
{
  char *p = a;
  char *q = b;
  char c;
  while(len--)
  {
    c = *p;
    *p++ = *q;
    *q++ = c;
  }
}

void tcp_cli_send_constructed_ack(struct ip_hdr_s *aip, struct tcp_hdr_s *atcp)
{
  unsigned pkt = ip_create_packet_sized(256, TCP_PROT);
  if(pkt == 0xff) return;

  struct mac_header *amac = nic_ref(NIC_RX_PACKET, 0);
  struct mac_header *mac =  nic_ref(pkt, 0);
  struct ip_hdr_s   *ip =   ip_ref(pkt, -1);
  struct tcp_hdr_s  *tcp =  tcp_ref(pkt, -1);
  memcpy(mac, amac, 64);
  memswap(mac->dest_mac, mac->src_mac, 6);
  memswap(ip->dst_ip, ip->src_ip, 4);
  memswap(&tcp->port_dst, &tcp->port_src, 2);
  tcp->offset = 5 << 4; // 20 bytes, no tcp options
  tcp->window = TCP_CLI_WINDOW;
  tcp->flags = TCP_ACK;
  tcp->seq = atcp->ack;
  tcp->ack = atcp->seq + 1;
  checksum_reset(&tcp->checksum);
  checksum_incremental_calc(ip->src_ip, 8); // first 2 words of pseudoheader
  checksum_incremental_pseudo(ip->dst_ip, ip->src_ip, TCP_PROT, 20);
  checksum_incremental_calc(tcp, 20);
  checksum_place(&tcp->checksum);

  nic_send_packet(pkt, 14 + 20 + 20, 1);
}

void tcp_cli_tx(void)
{
  unsigned pkt = ip_create_packet_sized(14 + 20 + 24 + (tcp_cli.tx_state == TX_FREE ? 0 : tcp_cli.tx_data_len), TCP_PROT); // headers + len, tcp proto
  if(pkt == 0xff) return;
  struct ip_hdr_s  *ip = ip_ref(pkt, -1);
  *(unsigned*)ip->dst_ip = tcp_cli.ip32;
  struct tcp_hdr_s *tcp = tcp_ref(pkt, -1);
  tcp->port_dst = htons(tcp_cli.port);
  tcp->port_src = htons(tcp_cli.my_port);
  tcp->seq = htonl(tcp_cli.my_seq);
  tcp->ack = htonl(tcp_cli.peer_seq);
  tcp->offset = 5<<4;
  tcp->flags = tcp_cli.flags;
  tcp->window = htons(TCP_CLI_WINDOW);
  tcp->urg_ptr = 0;
  unsigned len = 20; // TCP hdr size
  if(tcp->flags & TCP_SYN)
  { // MSS option для устранения фрагментации на IP
    tcp->option[0]=0x02;
    tcp->option[1]=0x04;
    tcp->option[2]=0x05;
    tcp->option[3]=0xB4;
    tcp->offset = 6 << 4; // 24 byte header, 1 word of option
    len += 4;
  }
  if(tcp_cli.tx_state != TX_FREE)
  {
    if(tcp_cli.tx_data_len != 0) tcp->flags |= TCP_PSH;
    memcpy((char*)tcp + len, tcp_cli.tx_data_p, tcp_cli.tx_data_len);
    len += tcp_cli.tx_data_len;
    tcp_cli.tx_state = TX_KEEP;
  }
  checksum_reset(&tcp->checksum);
  checksum_incremental_pseudo(ip->dst_ip, ip->src_ip, TCP_PROT, len);
  checksum_incremental_calc(tcp, len); // tcp header and data
  checksum_place(&tcp->checksum);
  ip_send_packet(pkt, len, 1);
}

// cancel TX, send RST with 'connected' SEQ and ACK, then clears session
void tcp_cli_rst_and_clear(void)
{
  tcp_cli.tx_state = TX_FREE;
  tcp_cli.flags = TCP_ACK | TCP_RST;
  tcp_cli_tx();
  tcp_cli_tx();
  tcp_cli_clear();
}

void tcp_cli_clear(void)
{
  tcp_cli.state = TCPC_IDLE;
  tcp_cli.tx_state = TX_FREE;
  tcp_cli.ip32 = 0; // stop rx parsing
  tcp_cli.timeout = ~0U; // stop resends in tcp_cli_exec()
}

void tcp_cli_connect(unsigned char *ip, unsigned short port)
{
  tcp_cli.rx_data_len = 0; // clear rx buffer from prev. connection data
  tcp_cli.rx_data[0] = 0;
  memcpy(&tcp_cli.ip32, ip, 4);
  tcp_cli.port = port;
  tcp_cli.my_port = tcp_cli_next_local_port();
  tcp_cli.my_seq = htonl((unsigned)sys_clock()); // swap bytes to get ISN dispersed
  tcp_cli.flags = TCP_SYN;
  tcp_cli.retry = 0;
  /// tcp_cli.timeout = 0; // inint sending // 25.11.2014
  tcp_cli_tx();
  tcp_cli.timeout = sys_clock_100ms + (1<<tcp_cli.retry) * 2;
  ++ tcp_cli.retry;
  tcp_cli.state = TCPC_SYN_SENT;
}

void tcp_cli_write(void *data, unsigned len)
{
  if(tcp_cli.tx_state != TX_FREE
  || len > 1460)
    return;
  tcp_cli.tx_data_p = data;
  tcp_cli.tx_data_len = len;
  tcp_cli.tx_state = TX_NEW;
}

void tcp_cli_exec(void)
{
  static char skip = 0;
  if(++skip < 19) return;
  skip = 0;

  if(tcp_cli.timeout > sys_clock_100ms) return;

  switch(tcp_cli.state)
  {
  case TCPC_SYN_SENT:
  case TCPC_ESTABLISHED:
  case TCPC_LAST_ACK:
    if(tcp_cli.retry < 4)
    { // resend on timeout
      tcp_cli_tx();
      tcp_cli.timeout = sys_clock_100ms + (1<<tcp_cli.retry) * 2;
      ++ tcp_cli.retry;
    }
    else
    { // max resends, no reply from peer
      tcp_cli.flags = TCP_RST;
      tcp_cli.tx_state = TX_FREE;
      tcp_cli_tx();
      tcp_cli_clear();
      tcp_cli.state = TCPC_ERROR;
      // sm_exec_and_parsing() will read TCPC_ERROR in superloop and immediately will call tcp_cli_clear()
    }
    break;
  } // switch state
}


void tcp_cli_parsing(void)
{
  struct ip_hdr_s *ip = ip_ref(NIC_RX_PACKET, -1);
  if(*(unsigned*)ip->src_ip != tcp_cli.ip32) return;

  struct tcp_hdr_s *tcp = tcp_ref(NIC_RX_PACKET, -1);

  if(tcp->port_dst != htons(tcp_cli.my_port) ) // 25.11.2014
    return; // not our local port
  if( tcp->port_src != htons(tcp_cli.port) )
  { // unknown connection to our local port
    if(tcp->flags & TCP_FIN)
    { // TIME_WAIT hack (actually, used by active close, which is not used by smtp client)
      tcp_cli_send_constructed_ack(ip, tcp);
    }
    return;
  }

  unsigned rx_body_len = ip_rx_body_length - (tcp->offset >> 4) * 4;

  if(tcp->flags & TCP_RST)
  {
    tcp_cli_clear();
    tcp_cli.state = TCPC_ERROR;
    return;
  }

  if((tcp->flags & TCP_SYN)
  && (tcp->flags & TCP_ACK)
  && tcp->ack == htonl(tcp_cli.my_seq + 1) )
  {
    tcp_cli.my_seq += 1;
    tcp_cli.peer_seq = htonl(tcp->seq) + 1 + rx_body_len;
    tcp_cli.flags = TCP_ACK;
    tcp_cli.state = TCPC_ESTABLISHED;
    ///tcp_cli.timeout = 0; // init sending
    tcp_cli_tx(); // send ACK, and possibly re-send first our data as side-effect, without disturbing resend timeout // 25.11.2014
    return;
  }

  if(tcp->flags & TCP_ACK)
  {
    unsigned rx_ack = htonl(tcp->ack);
    if(tcp_cli.state == TCPC_ESTABLISHED)
    {
      if(rx_ack == tcp_cli.my_seq) // code is restricted to only 1 sent outstanding segment
      { // fast rexmit request from peer
        tcp_cli.timeout = 0; // initiate resending in tcp_cli_exec()
      }
      if(rx_ack == tcp_cli.my_seq + tcp_cli.tx_data_len)
      {
        tcp_cli.my_seq = rx_ack;
        tcp_cli.tx_state = TX_FREE;
        tcp_cli.retry = 0;
        tcp_cli.timeout = ~0U;
      }
    }
    else
    if(tcp_cli.state == TCPC_LAST_ACK)
    {
      if(rx_ack == tcp_cli.my_seq + 1) // FIN ACK confirmed by peer with ACK (passive close ending)
        tcp_cli_clear();
    }
  }

#warning experimental

  // this for 'crossing' rx segment - dup segment which is *longer* than 'original'
  // SEQ wrapping ignored, because of low probability (~1E-6 per sendmail session)

  /*
  int segm_len = htonl(tcp->seq) + rx_body_len - tcp_cli.peer_seq; // fresh data size
  //if( segm_len <= 0 // dup, already received // this produces dups from netping after receiving empty segment with no data!
  if( segm_len < 0 // dup, already received // this works good!
  ||  segm_len > rx_body_len ) // gap in rx stream, prev. seq lost
  { // wrong (unexpected) seq received from peer, initiate fast rexmit request
    tcp_cli.timeout = 0;
    return;
  }
  */

  unsigned seq = htonl(tcp->seq);
  if( seq > tcp_cli.peer_seq  // gap in stream, prev. segment was lost
  ||  seq + rx_body_len < tcp_cli.peer_seq  ) // already received, no new data in this segment
  { // wrong (unexpected) seq received from peer, initiate fast rexmit request
    tcp_cli.timeout = 0;
    return;
  }
  int new_data_len = seq + rx_body_len - tcp_cli.peer_seq;

  /*
  if(htonl(tcp->seq) != tcp_cli.peer_seq) // 24.11.2015
  { // wrong (unexpected) seq received from peer, initiate fast rexmit request
    tcp_cli.timeout = 0;
    return;
  }
  */

  // valid segment from peer received, with expected peer seq

  if(tcp->flags & TCP_FIN) // passive close (initiated by peeer)
  {
    ++ tcp_cli.peer_seq; // confirm FIN
    tcp_cli.flags |= TCP_FIN; // setup reply FIN ACK
    tcp_cli.state = TCPC_LAST_ACK; // after 'automatic' FIN ACK reply go to LAST ACK state
  }

  if(new_data_len != 0)
  {
    tcp_cli.peer_seq += new_data_len; // advance ack for peer
    // get rx data with rx_buf protection
    unsigned n;
    if(tcp_cli.rx_data_len + new_data_len + 1 <= sizeof tcp_cli.rx_data)
      n = new_data_len;
    else
      n = sizeof tcp_cli.rx_data - 1 - tcp_cli.rx_data_len;
    // accumulate data
    // in case of segm_len != rx_body_len skip duplicated part of rx data
    memcpy(tcp_cli.rx_data + tcp_cli.rx_data_len, tcp_ref(NIC_RX_PACKET, rx_body_len - new_data_len), n);
    tcp_cli.rx_data_len += n;
    tcp_cli.rx_data[n] = 0; // z-term

    //tcp_cli_parse_data(); // old our data acking from peer already processed above, can post new data
    sendmail_exec_and_parsing();
  }

  if( tcp_cli.tx_state == TX_NEW // need to send fresh posted data
  || (tcp_cli.flags & TCP_FIN) ) // need to send flag
  // (fast) rexmit controlled not here, but by tcp_cli.timeout = 0
  {
    tcp_cli.retry = 0;
    tcp_cli_tx();
    tcp_cli.timeout = sys_clock_100ms + (1<<tcp_cli.retry) * 2;
    ++ tcp_cli.retry;
  }
  else
  if(rx_body_len != 0) // no new our data to send, but it was data from peer, need to ACK (or reject) it
  {
    // our outstanding data was confirmed (and removed) by received packet upper in code
    tcp_cli_tx();
  }

}
/*
  unsigned char  fqdn[64];
  unsigned short port;
  unsigned short reserved;
  unsigned char  user[48];
  unsigned char  passwd[32];
  unsigned char  from[48];
  unsigned char  to[48];
  unsigned char  reports[64];
  */

unsigned sendmail_http_get(unsigned pkt, unsigned more_data)
{
  char buf[864];
  char *dest = buf;
  dest+=sprintf(dest,"var packfmt={");
  PLINK(dest, sendmail_setup, fqdn);
  PLINK(dest, sendmail_setup, port);
  PLINK(dest, sendmail_setup, flags);
  PLINK(dest, sendmail_setup, user);
  PLINK(dest, sendmail_setup, passwd);
  PLINK(dest, sendmail_setup, from);
  PLINK(dest, sendmail_setup, to);
  PLINK(dest, sendmail_setup, cc_1);
  PLINK(dest, sendmail_setup, cc_2);
  PLINK(dest, sendmail_setup, cc_3);
  PLINK(dest, sendmail_setup, reports);
  PSIZE(dest, sizeof sendmail_setup); // must be the last // alignment!
  dest+=sprintf(dest, "};\nvar data={");
  PDATA_PASC_STR(dest, sendmail_setup, fqdn);
  PDATA(dest, sendmail_setup, port);
  PDATA(dest, sendmail_setup, flags);
  PDATA_PASC_STR(dest, sendmail_setup, user);
  PDATA_PASC_STR(dest, sendmail_setup, passwd);
  PDATA_PASC_STR(dest, sendmail_setup, from);
  PDATA_PASC_STR(dest, sendmail_setup, to);
  PDATA_PASC_STR(dest, sendmail_setup, cc_1);
  PDATA_PASC_STR(dest, sendmail_setup, cc_2);
  PDATA_PASC_STR(dest, sendmail_setup, cc_3);
  PDATA_PASC_STR(dest, sendmail_setup, reports);
  --dest; // remove last comma
  *dest++='}'; *dest++=';';
  tcp_put_tx_body(pkt, (unsigned char*)buf, dest - buf);
  return 0;
}


int sendmail_http_set(void)
{
  http_post_data((void*)&sendmail_setup, sizeof sendmail_setup);
  // no ip[4] is posted for sendmail, only fqdn. It's necessary to clear numeric ip if got empty fqdn!
  // dns_resove() depends on posted all-zero ip!
  if(sendmail_setup.fqdn[0] == 0) memset(sendmail_smtp_ip, 0, sizeof sendmail_smtp_ip);
  dns_resolve(eeprom_sendmail_setup.fqdn, sendmail_setup.fqdn);
  EEPROM_WRITE(&eeprom_sendmail_setup, &sendmail_setup, sizeof eeprom_sendmail_setup);
  http_redirect("/sendmail.html");
  return 0;
}


HOOK_CGI(sendmail_get,  sendmail_http_get,        mime_js, HTML_FLG_GET | HTML_FLG_NOCACHE );
HOOK_CGI(sendmail_set,  sendmail_http_set,        mime_js, HTML_FLG_POST );

void sendmail_param_reset(void)
{
  memset(&sendmail_setup, 0, sizeof sendmail_setup);
  sendmail_setup.port = 25;
  sendmail_setup.flags |= SM_FLG_ENABLE_SM | SM_FLG_SIGNATURE_BIT;
  EEPROM_WRITE(&eeprom_sendmail_setup, &sendmail_setup, sizeof eeprom_sendmail_setup);
  EEPROM_WRITE(&eeprom_sendmail_signature, &sendmail_signature, sizeof eeprom_sendmail_signature);
}

void sendmail_param_reset_cc_only(void)
{
  memset(sendmail_setup.cc_1, 0, sizeof sendmail_setup.cc_1 * 3);
  EEPROM_WRITE(&eeprom_sendmail_setup.cc_1, &sendmail_setup.cc_1, sizeof eeprom_sendmail_setup.cc_1 * 3);
  EEPROM_WRITE(&eeprom_sendmail_cc_signature, &sendmail_cc_signature, sizeof eeprom_sendmail_cc_signature);
}

void sendmail_init(void)
{
  unsigned sign;
  EEPROM_READ(&eeprom_sendmail_signature, &sign, sizeof sign);
  if(sign != sendmail_signature)
    sendmail_param_reset();
  // reset new flags member, 1 bit used as validness signature
  if((sendmail_setup.flags & SM_FLG_SIGNATURE_BIT) == 0)
    sendmail_setup.flags |= SM_FLG_ENABLE_SM | SM_FLG_SIGNATURE_BIT;
  //
  EEPROM_READ(&eeprom_sendmail_cc_signature, &sign, sizeof sign);
  if(sign != sendmail_cc_signature)
    sendmail_param_reset_cc_only();
  EEPROM_READ(&eeprom_sendmail_setup, &sendmail_setup, sizeof sendmail_setup);
  dns_add(sendmail_setup.fqdn, sendmail_smtp_ip);
}

void sendmail_event(enum event_e event)
{
  switch(event)
  {
  case E_EXEC:
    tcp_cli_exec();
    sendmail_exec_and_parsing();
    break;
  case E_RESET_PARAMS:
    sendmail_param_reset();
    sendmail_param_reset_cc_only();
    break;
  }
}

#warning повторный ACK вместо потерянного СИН-АСК ответа РС приходит в виде АСК без СИН. Возможно, потом повторяется СИН-АСК. Что делать?
