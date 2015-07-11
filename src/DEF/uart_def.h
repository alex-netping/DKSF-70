#define UART_MODULE
#include "uart\uart.h"

#define UART_RX_BUF_LEN_FACTOR  9 // buf len = 2 ^ factor
#define UART_TX_BUF_LEN_FACTOR  8 // buf len = 2 ^ factor

#define UART_RX_BUF_LEN         (1<<UART_RX_BUF_LEN_FACTOR)  // must be power of 2
#define UART_TX_BUF_LEN         (1<<UART_TX_BUF_LEN_FACTOR)  // must be power of 2
#if UART_RX_BUF_LEN < 64
#error "Too short buffers! (flow control)"
#endif

