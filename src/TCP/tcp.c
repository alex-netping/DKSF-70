/*
*rewrite by LBS
*v1.5
*17.02.2010
*v1.9-52
*24.03.2011
* cosmetic rewrite, some bugfix
*v2.0-52
*25.05.2011
*  major rewrite, retransmission, send queue (max 2 in flight)
v2.1-52
5.03.2012
  ip_free_packet() is called instead of nic_free_packet()
v2.2-50
14.11.2012
  adapted for ip3.c
v2.2-213
5.9.2012
  dksf213 support (sans http)
v2.2-60
24.02.2013
  tcp window for tcpcom flow ctrl
  handling of rx data by upper netwk layer is modified (tcp_active_session argument)
v2.3-60
9.05.2013
  removed FIN_WAIT_2 state, for browser tcp hang mitigation
v2.4-60
28.05.2013
  removed PSH flag in SYN packet
v2.5-52
14.08.2013
  tcp_create_packet_sized(), SSEvents support (TCP_RESERVED state)
v2.6-60
7.11.2014
  bugfixed wrong segment processing on rx, just ACK last received peer seq number (it will be dup ack or fast rexmit for peer)
*/

#include "platform_setup.h"
#ifdef TCP_MODULE
#include <string.h>

//---------------- Раздел, где будут определяться глобальные переменные модуля -------------
///Глобальная переменная возвращающая номер активной tcp сесии для внешнего обработчика
uword tcp_active_session;
///Таблица сокетов
struct tcp_socket_s tcp_socket[TCP_MAX_SOCKETS];
///Структуры для хранения заголовков TCP
struct tcp_header tcp_head_rx,tcp_head_tx;
upointer tcp_tx_body_pointer,tcp_rx_body_pointer;
unsigned tcp_rx_data_length;

void tcp_clear_connection(int sock_n);

/// DEBUGGG LBS waiting conn
#define CONN_Q_LEN 16

struct waiting_conn_s {
  unsigned char src_ip[4];
  unsigned char src_port[2];
  unsigned char dst_port[2];
  unsigned int  seq;
  unsigned short timer;
  unsigned char active;
} conn_queue_data[CONN_Q_LEN];

unsigned char conn_queue[CONN_Q_LEN];

void conn_queue_timer(void);

// end waiting conn


// DEBUGGG test data
unsigned r_count, r_count_drop;
unsigned rclock;
unsigned rindex;
/*
unsigned char random, rnda[] = {
179,253,12,210,34,115,33,246,166,195,77,6,44,48,110,122,123,79,123,
76,67,209,68,72,168,101,222,20,20,79,62,6,14,22,226,17,1,31,94,216,
179,179,248,21,211,52,227,217,128,125,34,151,34,181,151,141,193,161,
53,173,203,231,82,42,223,30,111,35,10,202,155,14,105,231,216,247,30,
185,36,129,30,185,237,83,93,53,70,241,182,159,176,191,164,237,22,156,
148,35,37,46,110,229,208,130,9,111,145,105,239,129,148,151,166,148,
236,114,82,102,243,119,186,9,47,245,115,198,120,222,119,139,128,198,
28,39,131,109,179,184,219,56,167,154,25,82,230,178,229,238,122,155,
212,139,83,201,122,201,68,105,4,227,233,207,250,203,232,111,100,127,
4,109,160,175,23,191,147,68,246,177,23,169,56,241,142,185,188,60,
173,124,106,15,188,41,237,193,18,206,82,214,244,209,223,61,92,219,183,
32,145,210,62,173,152,248,135,247,27,128,214,92,145,172
};
int lossy_test_drop(void)
{
  ++r_count;
  rindex = rindex+rclock+1;
  random = rnda[ rindex % (sizeof(rnda) / sizeof(rnda[0])) ];
     // p. drop switched off in mac_2366!!!
  if(random < (255 / 10) ) // 10% probability
  {
    ++r_count_drop;
    return 1;
  }
  return 0;
}
*/
// end rexmit
/*
#warning "remove debuggg"
int debug_tx_drop(void)
{
  unsigned ttt = sys_clock() & 15;
  int drop = ttt == 3 || ttt == 11;
  if(drop)
    ++drop;
  return drop;
}
*/

void tcp_transmit_pkt(unsigned sock_n, unsigned pkt, unsigned len, unsigned seq);
void tcp_send_flags(uword flags,uword socket);
void move_to_listen(uword socket);


void long_to_mas(unsigned long in,unsigned char *out);
unsigned long mas_to_long(unsigned char *in);

// compare tcp sequence numbers, wrapping aware
int ack_ge_seq(signed long long ack, signed long long seq)
{
  signed long long d = ack - seq;
  if(d < -1000000LL)  ack += 0x100000000LL;
  if(d >  1000000LL)  seq += 0x100000000LL;
  return ack >= seq;
}


#define REXMIT_LEN 3*TCP_MAX_SOCKETS

struct rexmit_s {
  unsigned char  sock_n;
  unsigned char  pkt;
  unsigned short len;
  unsigned       seq;
  unsigned       seq_end;
  systime_t      resend_time;
  unsigned char  rexmit_count;
  char           reserved[3];
} rexmit[REXMIT_LEN];


void add_send_q(unsigned sock_n, unsigned q_idx)
{
  unsigned char *send_q = tcp_socket[sock_n].send_q;
  for(int i=0; i<sizeof send_q; ++i)
    if(send_q[i] == 0xff)
    {
      send_q[i] = q_idx;
      return;
    }
}

void send_q_exec(void)
{
  struct tcp_socket_s *soc = tcp_socket;
  for(int i=0; i<TCP_MAX_SOCKETS; ++i, ++soc)
  {
    if(!soc->used) continue;
    int q_idx = soc->send_q[0];
    if(q_idx != 0xff)
    {
      struct rexmit_s *q = &rexmit[q_idx];
      tcp_transmit_pkt(q->sock_n, q->pkt, q->len, q->seq);
      util_cpy(soc->send_q + 1, soc->send_q, sizeof soc->send_q - 1);
      soc->send_q[sizeof soc->send_q - 1] = 0xff;
    }
  }
}

// remove element pointed by q
void rexmit_del(struct rexmit_s *q)
{
  /*
  char *a = (char*) q;
  char *b = (char*)(q+1);
  while(b < (char*)&rexmit[REXMIT_LEN])
    *a++ = *b++;
  rexmit[REXMIT_LEN-1].sock_n = 0xff;
  */
  q->sock_n = 0xff;
}

// add rexmit pool element
int rexmit_add(unsigned sock_n, unsigned pkt, unsigned len, unsigned seq, unsigned seq_delta)
{
  struct rexmit_s *q = rexmit;
  for(int n=0;n<REXMIT_LEN;++n,++q)
  {
    if(q->sock_n == 0xff)
    {
      q->sock_n = sock_n;
      q->pkt = pkt;
      q->len = len;
      q->seq = seq;
      q->seq_end = seq + seq_delta;
      if(tcp_socket[sock_n].out_in_flight < 2)
        q->resend_time = sys_clock() + TCP_REXMIT_TIMEOUT_us;
      else
        q->resend_time = ~0ULL;
      q->rexmit_count = 0;
      tcp_socket[sock_n].out_in_flight += 1;
      return n;
    }
  }
  return 0xff;
}

// drops acknoleged packets from NIC memory
void rexmit_ack(unsigned sock_n, unsigned ack)
{
  struct rexmit_s *q = rexmit;
  for(int n=0;n<REXMIT_LEN;++n,++q)
  {
    if(q->sock_n != sock_n) continue;
    if(ack_ge_seq(ack, q->seq_end))
    {
      ip_free_packet(q->pkt);
      rexmit_del(q);
      tcp_socket[sock_n].out_in_flight -= 1;
    }
  }
}

// check timeouts, resend, drop if obsolete
void rexmit_exec(void)
{
  systime_t time = sys_clock();
  struct rexmit_s *q = rexmit;
  for(int n=0;n<REXMIT_LEN;++n,++q)
  {
    if(q->sock_n == 0xff) continue;
    if(time > q->resend_time)
    {
      tcp_transmit_pkt(q->sock_n, q->pkt, q->len, q->seq);
      if(q->rexmit_count > TCP_MAX_RETRY)
      {
        tcp_clear_connection(q->sock_n);
        return; // re-exec rexmit on next main cycle, queue was modified by tcp_clear_conn-n()
      }
      else
      {
        ++ q->rexmit_count;
        q->resend_time = time + (TCP_REXMIT_TIMEOUT_us << q->rexmit_count);
      }
    } // if timeout
  } // for rexmit
}

void rexmit_purge(unsigned sock_n)
{
  struct rexmit_s *q = rexmit;
  for(int n=0;n<REXMIT_LEN;++n,++q)
  {
    if(q->sock_n == sock_n)
    {
      ip_free_packet(q->pkt);
      rexmit_del(q);
    }
  }
  tcp_socket[sock_n].out_in_flight = 0;
  util_fill(tcp_socket[sock_n].send_q, sizeof tcp_socket[sock_n].send_q, 0xff);
}

void rexmit_init(void)
{
  util_fill((void*)rexmit, sizeof rexmit, 0xff); // ff->sock_n i.e. unused
}

#warning "ПРИВЕСТИ В ПОРЯДОК РЕИНИЦИАЛИЗАЦИЮ СЕТИ - HTTP, NIC etc"

void tcp_init(void)
{
  struct tcp_socket_s *soc = tcp_socket;
  for(int i=0; i<TCP_MAX_SOCKETS; ++i, ++soc)
  {
    soc->used = 0;
    util_fill(soc->send_q, sizeof soc->send_q, 0xff); // init send queue
  }
  // DEBUGGGG LBS rexmit
  rexmit_init();
  // LBS waiting conn
  for(int i=0;i<CONN_Q_LEN;++i)
    conn_queue[i]=0xFF;
}

unsigned tcp_create_packet_sized(unsigned tcp_socket_num, unsigned size)
{
  struct tcp_socket_s *cur_socket;
  cur_socket = &tcp_socket[tcp_socket_num];

  // заголовок IP
  uword pkt = ip_create_packet_sized(size, TCP_PROT);
  if(pkt == 0xff) return pkt;
  util_cpy(cur_socket->socket_ip, ip_head_tx.dest_ip, 4);
  util_cpy(sys_setup.ip, ip_head_tx.src_ip, 4);
  ip_put_tx_header(pkt);

  // заголовок TCP
  util_cpy(cur_socket->socket_port,tcp_head_tx.port_dest, 2);
  util_cpy(cur_socket->listen_port,tcp_head_tx.port_src, 2);
  tcp_head_tx.data_offs = (sizeof(struct tcp_header)/4)<<4;
  tcp_head_tx.window[0] = cur_socket->window >> 8;
  tcp_head_tx.window[1] = cur_socket->window & 0xff;
  tcp_head_tx.flags = TCP_MSK_ACK ;
  util_fill(tcp_head_tx.option, sizeof tcp_head_tx.option, 0);
  tcp_put_tx_head(pkt);

  return pkt;
}

unsigned tcp_create_packet(unsigned tcp_socket_num)
{
  return tcp_create_packet_sized(tcp_socket_num, 6*256);
}

void tcp_transmit_pkt(unsigned sock_n, unsigned pkt, unsigned len, unsigned seq)
{
  unsigned short temp;

  tcp_get_tx_head(pkt);

  long_to_mas(seq, tcp_head_tx.sequence_num);
  long_to_mas(tcp_socket[sock_n].ack, tcp_head_tx.ack_num);
  if(tcp_head_tx.flags & TCP_MSK_SYN)
  { // MSS option для устранения фрагментации на IP
    tcp_head_tx.option[0]=0x02;
    tcp_head_tx.option[1]=0x04;
    tcp_head_tx.option[2]=0x05;
    tcp_head_tx.option[3]=0xB4;
  }

  ip_get_tx_header(pkt);

  unsigned char pseudoheader[12];
  util_cpy(ip_head_tx.src_ip,  pseudoheader,   4);
  util_cpy(ip_head_tx.dest_ip, pseudoheader+4, 4);
  pseudoheader[8] = 0;
  pseudoheader[9] = TCP_PROT;
  pseudoheader[10]=(sizeof(struct tcp_header)+len) >> 8;
  pseudoheader[11]=(sizeof(struct tcp_header)+len)&0xff;

  tcp_head_tx.CRC[0] = 0;
  tcp_head_tx.CRC[1] = 0;
  CRC16 = 0;
  TCP_CALC_CRC(pseudoheader, 12);
  TCP_CALC_CRC(((unsigned char*)&tcp_head_tx), sizeof(struct tcp_header));

  if(len)
  {
     temp = (NIC_RAM_START+pkt)<<8;
     temp += sizeof(struct mac_header);
     temp += sizeof(struct ip_header_s);
     temp += sizeof(struct tcp_header);
     TCP_CALC_CRC_NIC(temp, len);
  }

  temp = TCP_GET_CRC;
  tcp_head_tx.CRC[0] = (temp & 0xff);
  tcp_head_tx.CRC[1] = (temp >> 8);

  tcp_put_tx_head(pkt);
  /*
#warning "remove debuggg - tx lossy 1"
  if(!debug_tx_drop())
  */
  ip_send_packet(pkt, sizeof(struct tcp_header) + len, 0);
}


void tcp_send_packet(unsigned sock_n, unsigned pkt, unsigned len)
{
  struct tcp_socket_s* sock = &tcp_socket[sock_n];

  tcp_get_tx_head(pkt);

  unsigned seq_delta = len;
  if(tcp_head_tx.flags & TCP_MSK_SYN) seq_delta += 1;
  if(tcp_head_tx.flags & TCP_MSK_FIN) seq_delta += 1;

  if(seq_delta > 0 && (tcp_head_tx.flags & TCP_MSK_SYN) == 0) // 28.05.2013
  {
    tcp_head_tx.flags |= TCP_MSK_PSH;
    tcp_put_tx_head(pkt);
  }

  if(seq_delta > 0)
  {
    int n = rexmit_add(sock_n, pkt, len, sock->seq, seq_delta);
    if(n == 0xff)
    {
      ip_free_packet(pkt);
      return;
    }
    if(tcp_socket[sock_n].out_in_flight < 2)
    {
      tcp_transmit_pkt(sock_n, pkt, len, sock->seq);
    }
    else
      add_send_q(sock_n, n);
  }
  else
  {
    tcp_transmit_pkt(sock_n, pkt, len, sock->seq);
    ip_free_packet(pkt);
  }

  sock->seq += seq_delta;
//#warning "debuggg timeout"
  sock->timeout = sys_clock() + 2000;
}

void tcp_get_rx_head(void)
{
    IP_SET_RX_BODY_ADDR(0);
    IP_GET_RX_BODY((unsigned char*)&tcp_head_rx,sizeof(struct tcp_header));
}

void tcp_get_tx_head(uword packet_id)
{
    IP_SET_TX_BODY_ADDR(0);
    IP_GET_TX_BODY(packet_id,(unsigned char*)&tcp_head_tx,sizeof(struct tcp_header));
}

void tcp_put_tx_head(uword packet_id)
{
    IP_SET_TX_BODY_ADDR(0);
    IP_PUT_TX_BODY(packet_id,(unsigned char*)&tcp_head_tx,sizeof(struct tcp_header));
}

void tcp_get_tx_body(uword packet_id,unsigned char *buf,uword len)
{
    IP_SET_TX_BODY_ADDR(sizeof(struct tcp_header)+tcp_tx_body_pointer);
    IP_GET_TX_BODY(packet_id,buf,len);
    tcp_tx_body_pointer+=len;
}

void tcp_get_rx_body(unsigned char *buf,uword len)
{
    unsigned int temp = tcp_rx_body_pointer;
    temp +=((tcp_head_rx.data_offs>>4)&0xf)*4;
    IP_SET_RX_BODY_ADDR(temp);
    IP_GET_RX_BODY(buf,len);
    tcp_rx_body_pointer += len;
}

void tcp_put_tx_body(uword packet_id,unsigned char *buf,uword len)
{
    IP_SET_TX_BODY_ADDR(sizeof(struct tcp_header)+tcp_tx_body_pointer);
    IP_PUT_TX_BODY(packet_id,buf,len);
    tcp_tx_body_pointer += len;
}

uword tcp_open(unsigned short listen_port) // 24.04.2013
{
  unsigned soc_n;
  struct tcp_socket_s* soc = tcp_socket;

  for(soc_n=0; soc_n<TCP_MAX_SOCKETS; ++soc_n, ++soc)
    if(soc->used == 0) break;
  if(soc_n == TCP_MAX_SOCKETS)
    return 0xff;
  soc->used = 1;
  soc->listen_port[0] = listen_port >> 8;
  soc->listen_port[1] = listen_port & 0xff;
  soc->window = TCP_WINDOW;
  soc->tcp_state = TCP_LISTEN;
  return soc_n;
}


void tcp_send_constructed(unsigned char *ip, unsigned char *src_port, unsigned char *dest_port,
                unsigned seq, unsigned ack, unsigned flags)
{
  //Заголовок IP уровня
  uword pkt = ip_create_packet();
  if(pkt == 0xFF) return;
  ip_head_tx.protocol = TCP_PROT;
  util_cpy(ip, ip_head_tx.dest_ip, 4);
  util_cpy(sys_setup.ip, ip_head_tx.src_ip, 4);
  ip_put_tx_header(pkt);
  //Заголовок TCP уровня
  util_cpy(src_port, tcp_head_tx.port_src, 2);
  util_cpy(dest_port, tcp_head_tx.port_dest, 2);
  // ack = incoming seq, seq = 0 as per RFC for reject if inc ACK bit = 0
  long_to_mas(ack, tcp_head_tx.ack_num);
  long_to_mas(seq, tcp_head_tx.sequence_num);
  tcp_head_tx.flags = flags;
  tcp_head_tx.window[0] = TCP_WINDOW >> 8; // abstract value, don't care for sending RSTs
  tcp_head_tx.window[1] = TCP_WINDOW & 0xff;
  tcp_head_tx.data_offs = (sizeof(struct tcp_header)/4)<<4;
  /* urg ptr ignored, static 0? */
  tcp_head_tx.option[0]=0x00;
  tcp_head_tx.option[1]=0x00;
  tcp_head_tx.option[2]=0x00;
  tcp_head_tx.option[3]=0x00;
  tcp_put_tx_head(pkt);
  // ----- send tcp
  unsigned char header[12];  // tcp checksum pseudo-hdr
  CRC16=0;
  tcp_head_tx.CRC[0] = 0;
  tcp_head_tx.CRC[1] = 0;
  util_cpy(ip_head_tx.src_ip,  &header[0], 4);
  util_cpy(ip_head_tx.dest_ip, &header[4], 4);
  header[8] = 0;
  header[9] = TCP_PROT;
  header[11] = sizeof tcp_head_tx & 0xff;
  header[10] = sizeof tcp_head_tx >> 8;
  TCP_CALC_CRC(header,12);
  TCP_CALC_CRC(((unsigned char*)&tcp_head_tx), sizeof tcp_head_tx);
  // no data, no checksum for data
  unsigned short temp = TCP_GET_CRC;
  tcp_head_tx.CRC[0] = (temp & 0xff);
  tcp_head_tx.CRC[1] = (temp >> 8);
  tcp_put_tx_head(pkt);
  /*
#warning "remove debuggg - lossy tx 2"
  if(!debug_tx_drop())
  */
  ip_send_packet(pkt, sizeof tcp_head_tx, 1); // header only, no data, remove pkt, don't resend
}


//////////////////  LBS waiting conn ////////////////

void conn_q_move(void)
{
  int i, j;
  unsigned char idx, new_q[CONN_Q_LEN];

  for(i=0,j=0; i<CONN_Q_LEN; ++i)
  {
    idx = conn_queue[i];
    if(idx!=0xFF) // skip free slots = compact queue
    {
      if(conn_queue_data[idx].active == 0) continue; // exclude from queue if not active
      new_q[j++] = idx;
    }
  }
  for(;j<CONN_Q_LEN;++j) new_q[j]=0xFF; // mark not used slots as free
  util_cpy(new_q, conn_queue, sizeof(conn_queue));
}

struct waiting_conn_s*
conn_q_get_slot(void)
{
  int n;
  struct waiting_conn_s *p = conn_queue_data;

  if(conn_queue[CONN_Q_LEN-1]!=0xFF) return 0; // no free slots
  for(n=0;n<CONN_Q_LEN;++n,++p) // find empty data slot (unordered)
    if(p->active==0)
    {
      p->active=2;
      p->timer=5; // free if will not be used
      conn_queue[CONN_Q_LEN-1] = n; // to the last ordered slot
      conn_q_move(); // compact queue
      return p;
    }
  return 0; // no free slots
}

void start_waiting_conn(void)
{
  struct tcp_socket_s *socket = tcp_socket;
  struct waiting_conn_s *conn;
  int i;

  if(conn_queue[0] == 0xFF) return; // no waiting connections
  if(conn_queue[0] >= CONN_Q_LEN) return; // check index range
  conn = &conn_queue_data[conn_queue[0]];
  if(! conn->active) { conn_q_move(); return; } // drop conn if old
  for(i=0;i<TCP_MAX_SOCKETS;++i,++socket)
  {
    if(socket->used && socket->tcp_state == TCP_LISTEN
    && memcmp(socket->listen_port, conn->dst_port, 2) == 0 )
    {
      util_fill(socket->send_q, sizeof socket->send_q, 0xff); // init send queue
      util_cpy(conn->src_port, socket->socket_port, 2);
      util_cpy(conn->src_ip, socket->socket_ip, 4);
      socket->seq += (unsigned)sys_clock()+i; // initial random SEQ
      socket->ack = conn->seq + 1; // SYN seq delta = 1
      socket->closing_time = sys_clock() + 20000; // for closing inactive connection
      ////socket->closing_time = sys_clock() + 6000; // for closing inactive connection
      tcp_send_flags(TCP_MSK_ACK | TCP_MSK_SYN, i);
      socket->tcp_state = TCP_SYN_RCVD;
      // consume incoming conn
      conn->active = 0;
      conn_q_move();
    }
  }
}


void conn_queue_add(void)
{
  unsigned seq = mas_to_long(tcp_head_rx.sequence_num);
  struct waiting_conn_s *conn = conn_queue_data;
  for(int n=0; n<CONN_Q_LEN; ++n, ++conn) // skip dup SYNs already enqued
  {
    if(conn->active && conn->seq == seq) // SYN SEQ is used as hash
      return;
  }

  conn = conn_q_get_slot();
  if(conn==0)
  {
    // send RST - reject connection
    tcp_send_constructed(ip_head_rx.src_ip, tcp_head_rx.port_dest, tcp_head_rx.port_src,
             0, mas_to_long(tcp_head_rx.sequence_num) + 1, TCP_MSK_ACK | TCP_MSK_RST);
    return;
  }

  util_cpy(ip_head_rx.src_ip, conn->src_ip, 4);
  util_cpy(tcp_head_rx.port_src, conn->src_port, 2);
  util_cpy(tcp_head_rx.port_dest, conn->dst_port, 2);
  conn->seq = mas_to_long(tcp_head_rx.sequence_num);
  // connection waiting before drop timeout, on t-out, dropped with RST
  conn->timer = 100*20; // 20s, TIMER2=10ms tick // it was 4s
  conn->active = 1;
}

void conn_queue_exec(void)
{
  struct waiting_conn_s *conn = conn_queue_data;
  for(int i=0; i<CONN_Q_LEN; ++i,++conn)
  {
    if(conn->active == 0) continue;
    if(conn->timer == 0)
    {
      // send RST
      tcp_send_constructed(conn->src_ip, conn->dst_port, conn->src_port,
           0, conn->seq + 1, TCP_MSK_ACK | TCP_MSK_RST);
      conn->active = 0;
    }
  }
}

void conn_queue_timer(void)
{
  struct waiting_conn_s *conn = conn_queue_data;
  for(int i=0; i<CONN_Q_LEN; ++i,++conn)
    if(conn->timer > 0) --conn->timer;
}


void tcp_handler(void)
{
    unsigned prev_seq, flags;

    if (ip_head_rx.protocol != TCP_PROT) return;
    tcp_get_rx_head();
/*
#warning "debugg vars"
    unsigned debug_seq = mas_to_long(tcp_head_rx.sequence_num);
    unsigned debug_ack = mas_to_long(tcp_head_rx.ack_num);
    int debug_syn = (tcp_head_rx.flags & TCP_MSK_SYN) != 0;
    int debug_fin = (tcp_head_rx.flags & TCP_MSK_FIN) != 0;
    int debug_rst = (tcp_head_rx.flags & TCP_MSK_RST) != 0;
    int debug_d_len = ip_rx_body_length - ((tcp_head_rx.data_offs & 0xF0) >> 2);

#warning "remove debugggg - lossy"
    systime_t ttt = sys_clock() & 15;
    int debug_drop = (ttt == 1 || ttt == 11 || ttt==7);
    if(debug_drop)
    {
      return;
    }
    // lossy RX
*/

    int incoming_connection_flags =
        (tcp_head_rx.flags & TCP_MSK_SYN) != 0
     && (tcp_head_rx.flags & TCP_MSK_ACK) == 0;

    tcp_rx_data_length = ip_rx_body_length - ((tcp_head_rx.data_offs & 0xF0) >> 2);
    unsigned rxseq = mas_to_long(tcp_head_rx.sequence_num);
    unsigned rxseqend = rxseq + tcp_rx_data_length;
    if(tcp_head_rx.flags & TCP_MSK_SYN) rxseqend += 1;
    if(tcp_head_rx.flags & TCP_MSK_FIN) rxseqend += 1;

    int n;
    struct tcp_socket_s* cur_socket = tcp_socket;
    for(n=0; n<TCP_MAX_SOCKETS; ++n, ++cur_socket)
    {
      if( cur_socket->used
      && cur_socket->tcp_state != TCP_RESERVED // custom state, prepared but not active 13.08.2013, used by Server-send events
      // check dest=listening port
      &&  cur_socket->listen_port[0] == tcp_head_rx.port_dest[0]
      &&  cur_socket->listen_port[1] == tcp_head_rx.port_dest[1])
      {
        if( cur_socket->tcp_state != TCP_LISTEN
        &&  memcmp(cur_socket->socket_port, tcp_head_rx.port_src, 2) == 0
        &&  memcmp(cur_socket->socket_ip,   ip_head_rx.src_ip,    4) == 0 )
        {
          // it's from connected peer, process
          break;
        }
        else
        { // not from connected peer
          if(incoming_connection_flags)
          {
            conn_queue_add();
            return;
          }
          else if(tcp_head_rx.flags & TCP_MSK_FIN)
          { // ack dup fin for connection which is probably already closed
            tcp_send_constructed(ip_head_rx.src_ip, tcp_head_rx.port_dest, tcp_head_rx.port_src,
                                 mas_to_long(tcp_head_rx.ack_num),
                                 rxseqend, TCP_MSK_ACK);
          }
        } // if else from connected
      } // if dest port == listen port
    } // for sockets

    if(n < TCP_MAX_SOCKETS)
      tcp_active_session = n;
    else
    { // no listening port or port is busy
#warning "reply by RST or DEST UNREACHABLE? (if not RST received)"
      return;
    }

    if(tcp_head_rx.flags & TCP_MSK_RST)
    { // reset packet is received
      tcp_clear_connection(tcp_active_session);
      return;
    }

    if(tcp_head_rx.flags & TCP_MSK_ACK)
    {
      cur_socket->last_ack = mas_to_long(tcp_head_rx.ack_num);
      rexmit_ack(tcp_active_session, cur_socket->last_ack);
    }

    flags = 0;
    switch (cur_socket->tcp_state)
    {
     case TCP_LISTEN:  // DO NOTHING, new connections started by conn_q
                      break;
     case TCP_SYN_RCVD:
       if(tcp_head_rx.flags & TCP_MSK_SYN)
       {
         // dup SYN received, do nothing, rexmit will resend our SYN ACK reply
       }
       else if(ack_ge_seq(cur_socket->last_ack, cur_socket->seq))
       {
         cur_socket->tcp_state = TCP_ESTABLISHED;
         system_event(E_ACCEPT_TCP, tcp_active_session); // NEW INCOMING TCP CONNECTION  // 24.04.2013
       }
       else
         break;
       // проваливаемся, обрабатываем данные если они есть
     case TCP_ESTABLISHED:
       if(rxseq == cur_socket->ack) // next legal adjucent segment
       {
         cur_socket->ack = rxseqend;
         if(tcp_rx_data_length != 0)
         {
           prev_seq = cur_socket->seq; // detect TCP reply packets in data parsing
           system_event(E_PARSE_TCP, tcp_active_session); // PARSE TCP DATA // 24.04.2013
           if(cur_socket->seq == prev_seq)
             flags = TCP_MSK_ACK; // outgoing segment was not sent, ACK required
         }
       }
       else
         flags |= TCP_MSK_ACK; // initiate fast rexmit or just ack dup // 7.11.2014
       /*
       else if(rxseqend != rxseq && ack_ge_seq(cur_socket->ack, rxseqend))
       {
         flags |= TCP_MSK_ACK;
       }
       */
       if(tcp_head_rx.flags & TCP_MSK_FIN)
       { // close from our side
         flags |= TCP_MSK_ACK | TCP_MSK_FIN;
         cur_socket->tcp_state = TCP_LAST_ACK;
       }
       if(flags)
         tcp_send_flags(flags, tcp_active_session); // send just ACK
       break;

    case TCP_LAST_ACK:
          if(ack_ge_seq(cur_socket->last_ack, cur_socket->seq))
          { // peer ACKed all in flight, we can close
            tcp_clear_connection(tcp_active_session);
          }
          break;

    case TCP_FIN_WAIT_1:
          if(ack_ge_seq(cur_socket->last_ack, cur_socket->seq))
          { // peer ACKed all in flight, we can close
               // The hack resolves tcp hangs on browser side.
               /// just like TIME-WAIT, use constructed reply for any FIN from peer if FIN's session is already cleared!
            /*
            cur_socket->tcp_state = TCP_FIN_WAIT_2;
            */
            tcp_clear_connection(tcp_active_session);
            //
          }
          break;

    case TCP_FIN_WAIT_2:
    case TCP_TIME_WAIT:
          if(tcp_head_rx.flags & TCP_MSK_FIN)
          {
            cur_socket->ack = rxseqend; // FIN segment received
            tcp_send_flags(TCP_MSK_ACK, tcp_active_session);
            // we have no Time-Wait state, just clear connection
            // в ответ на dup FIN от закрытой сессии посылаем сконструированый ACK без сессии
            // в начале этой функции, при разборе биндинга
            tcp_clear_connection(tcp_active_session);
          }
          break;
    }
    return;
}

void tcp_close_session(unsigned sock_n)
{
#warning "это надо отрабатывать в зависимости от состояния???"
  struct tcp_socket_s* sock = &tcp_socket[sock_n];
  tcp_send_flags(TCP_MSK_ACK | TCP_MSK_FIN | TCP_MSK_PSH, sock_n);
  sock->closing_time = sys_clock() + 6000;
  sock->tcp_state = TCP_FIN_WAIT_1;
}

void tcp_exec(void)
{
  rexmit_exec();
  send_q_exec();
  conn_queue_exec();
  start_waiting_conn();  // dispatch waiting connection

  systime_t time = sys_clock();
  int n;
  struct tcp_socket_s* sock = tcp_socket;
  for(n=0; n<TCP_MAX_SOCKETS; ++n, ++sock)
  {
    if(!sock->used) continue;
    if(sock->tcp_state != TCP_LISTEN
    && sock->tcp_state != TCP_RESERVED)
    {
      if(time > sock->closing_time)
      {
        if(sock->tcp_state == TCP_ESTABLISHED)
        {
          tcp_close_session(n);
          sock->closing_time = sys_clock() + 6000;
        }
        else
          tcp_clear_connection(n);
      }
      else  if(time > sock->timeout && sock->tcp_state != TCP_SYN_RCVD)
      {
        //// tcp_send_flags(TCP_MSK_ACK, n); // kinda keep-alive
        break;
      }
    } // if !TCP_LISTEN
  } // for
}

void tcp_clear_connection(int sock_n)
{
  if(sock_n >= TCP_MAX_SOCKETS) return;
  rexmit_purge(sock_n);
  tcp_socket[sock_n].tcp_state = TCP_LISTEN;
  tcp_socket[sock_n].tcp_state = (sock_n == sse_sock) ? TCP_RESERVED : TCP_LISTEN;
#ifdef HTTP_MODULE
  // it's THE HACK! // 24.03.2011
  if(sock_n == http.tcp_session)
    http.state = HTTP_IDLE;
#endif
}

void tcp_timer_10ms(void)
{ /*
  // unused now
  struct tcp_socket_s *s = tcp_socket;
  for(int i=0;i<TCP_MAX_SOCKETS;++i)
    if (s->tcp_timeout) --s->tcp_timeout;
  */
  // LBS waiting conn
  conn_queue_timer();
}

void tcp_send_flags(uword flags, uword socket)
{
  unsigned pkt = tcp_create_packet(socket);
  if(pkt == 0xff) return;
  tcp_head_tx.flags = flags;
  tcp_put_tx_head(pkt);
  tcp_send_packet(socket, pkt, 0);
}


unsigned long mas_to_long(unsigned char *in)
{
  return in[0]<<24 | in[1]<<16 | in[2]<<8 | in[3];
}

void long_to_mas(unsigned long in, unsigned char *out)
{
  out[0] = in >> 24;
  out[1] = in >> 16;
  out[2] = in >> 8;
  out[3] = in >> 0;
}

unsigned tcp_event(enum event_e event) // LBS 22.02.2010
{
  switch(event)
  {
  case E_TIMER_10ms:
    tcp_timer_10ms();
    break;
  case E_EXEC:
    tcp_exec();
    break;
  }
  return 0;
}


#warning "tcp fast rexmit"


#endif
