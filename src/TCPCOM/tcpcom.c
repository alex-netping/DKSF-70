#include "platform_setup.h"

#ifdef TCPCOM_MODULE

#define TCPCOM_WIN_UPDATE_PERIOD 50 // ms

unsigned tcpcom_soc = 0xfe;
systime_t tcpcom_send_time;
// diag error flags (dbug)
unsigned char flow_ctrl_failed = 0;
unsigned uart_to_net_overflow = 0; // both directions rx and tx

#define UART_TX_SPACE() (UART_TX_BUF_LEN - uart_tx_counter)

void tcpcom_accept_tcp(unsigned soc_n)
{
  if(soc_n != tcpcom_soc) return;
  if(uart_setup.uart_usage != UART_USAGE_TCP_COM) return;
  tcp_socket[soc_n].window = UART_TX_SPACE();
  tcp_socket[soc_n].closing_time = sys_clock() + 120000; // inactvity T/O
  tcpcom_send_time = sys_clock(); // update ACK send time
  char buf[UART_RX_BUF_LEN];
  uart_rx_read_buf(buf, UART_RX_BUF_LEN); // flush rx
}

void tcpcom_parsing(unsigned soc_n)
{
  if(soc_n != tcpcom_soc) return;
  ////if((uart_setup.flags & REMCOM_ENABLE_COM_PORT) == 0) return;
  if(uart_setup.uart_usage != UART_USAGE_TCP_COM) return;

  unsigned char buf[UART_TX_BUF_LEN];

  if(tcp_rx_data_length > UART_TX_SPACE())
  { // if no space, drop data (window flow ctrl dont work)
    flow_ctrl_failed |= 1;
    uart_to_net_overflow += 1;
    return;
  }
  tcp_rx_body_pointer = 0;
  tcp_get_rx_body(buf, tcp_rx_data_length);
  uart_tx_write_buf((void*)buf, tcp_rx_data_length);
  tcp_socket[tcpcom_soc].window = UART_TX_SPACE(); // update window before ACK will be sent
  tcp_socket[tcpcom_soc].closing_time = sys_clock() + 120000; // inactvity T/O
  tcpcom_send_time = sys_clock(); // update ACK send time
}

void tcpcom_exec(void)
{
  static char cpu_unload;
  if(++cpu_unload < 11) return;
  cpu_unload = 0;

  ///if((uart_setup.flags & REMCOM_ENABLE_COM_PORT) == 0) return;
  if(uart_setup.uart_usage != UART_USAGE_TCP_COM) return;

  unsigned char buf[UART_RX_BUF_LEN];

  if(tcpcom_soc >= 0xfe) return;

  struct tcp_socket_s *soc = &tcp_socket[tcpcom_soc];

  if(soc->tcp_state != TCP_ESTABLISHED) return;

  unsigned uart_rx_len = uart_rx_counter; // copy volatile data

  int send_data_flag = uart_rx_len >= uart_setup.rx_packet_len
    || (uart_rx_counter != 0 && sys_clock() > get_uart_rx_time() + uart_setup.rx_packet_timeout) ;

  if(soc->window != UART_TX_SPACE())
  {
    if(sys_clock() > tcpcom_send_time + uart_setup.rx_packet_timeout || uart_tx_counter == 0) // rx_packet_timeout used as window update t/o
    {
      if(send_data_flag == 0)
      {
        soc->window = UART_TX_SPACE();
        tcp_send_flags(TCP_MSK_ACK, tcpcom_soc); // update TCP window
        tcpcom_send_time = sys_clock(); // update ACK send time
      }
      else
        ; // window will be sent with data packet
    }
  }

  if(send_data_flag)
  {
    unsigned pkt = tcp_create_packet(tcpcom_soc);
    if(pkt == 0xff)
    {
      uart_to_net_overflow += 1;
      return;
    }
    if(uart_rx_len)
    {
      uart_rx_read_buf((void*)buf, uart_rx_len);
      tcp_tx_body_pointer = 0;
      tcp_put_tx_body(pkt, buf, uart_rx_len);
    }
    soc->window = UART_TX_SPACE();
    tcp_send_packet(tcpcom_soc, pkt, uart_rx_len);
    tcpcom_send_time = sys_clock(); // update ACK send time
  }
}

void tcpcom_reinit(void)
{
  if(tcpcom_soc < 0xfe)
  {
    struct tcp_socket_s *soc = &tcp_socket[tcpcom_soc];
    if(soc->tcp_state != TCP_LISTEN)
      tcp_send_flags(TCP_MSK_ACK | TCP_MSK_RST, tcpcom_soc);
    tcp_clear_connection(tcpcom_soc);
    soc->listen_port[0] = uart_setup.listen_port >> 8;
    soc->listen_port[1] = uart_setup.listen_port & 0xff;
  }
}

void tcpcom_init(void)
{
  tcpcom_soc = tcp_open(uart_setup.listen_port);
}

void tcpcom_event(enum event_e event, unsigned evdata_tcp_soc_n)
{
  switch(event)
  {
  case E_EXEC:
    tcpcom_exec();
    break;
  case E_PARSE_TCP:
    tcpcom_parsing(evdata_tcp_soc_n);
    break;
  case E_ACCEPT_TCP:
    tcpcom_accept_tcp(evdata_tcp_soc_n);
    break;
  }
}

#warning "ATTN big data in stack!"

#endif // TCPCOM_MODULE
