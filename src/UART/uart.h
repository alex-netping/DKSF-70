
#ifndef _UART_H_
#define _UART_H_

struct uart_setup_s {
  unsigned char  ip[4]; // client access filter, one fixed ip
  unsigned short listen_port; // it was udp_port
  unsigned short rx_packet_len;
  unsigned short rx_packet_timeout;  // in ms
  unsigned char  flags;
  unsigned char  baudrate;    // unit = 1200 bit/s
  unsigned char  uart_usage;
  unsigned char  reserved[63];
};

//  bits in flags:

#define REMCOM_ENABLE_COM_PORT 0x01
#define REMCOM_USE_FIXED_IP    0x02
#define UART_ODD_PARITY      0x04
#define UART_EVEN_PARITY     0x08
#define UART_SELECT_RS485    0x10 // for UniPing Server Solution
#define UART_2_STOP_BITS     0x20
#define UART_7_BIT_WORD      0x40

enum uart_parity_e {
  NO_PARITY = 0,
  ODD_PARITY = UART_ODD_PARITY,
  EVEN_PARITY = UART_EVEN_PARITY
};

enum uart_usage_e {
  UART_USAGE_DISABLED = 0,
  UART_USAGE_UDP_COM,
  UART_USAGE_TCP_COM,
  UART_USAGE_ENERGOMERA,
  UART_USAGE_GSM_MODEM
};


extern volatile unsigned uart_rx_counter;
extern volatile unsigned uart_tx_counter;

extern struct uart_setup_s uart_setup;

systime_t get_uart_rx_time(void);
int uart_rx_data_ready(unsigned rx_timeout_ms);
unsigned uart_rx_read_buf(char *data, unsigned len);
unsigned uart_tx_write_buf(char *data, unsigned len);
void uart_hardware_init(int speed, int bits, enum uart_parity_e parity, int stops);

void uart_init(void);
void uart_event(enum event_e event);

#endif // _UART_H_
