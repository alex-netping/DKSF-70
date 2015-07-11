/*
v1.1-50
9.11.2012
 split rx,tx buf len size
v1.2-70
23.07.2013
  LPC17xx rewrite
*/

#include "platform_setup.h"

#ifdef UART_MODULE

#include <stdio.h>
#include "eeprom_map.h"
#include "plink.h"

void uart_interrupt(void);

//#define UxMCR U2MCR // UART2 has no flow control lines and MCR register
#define UxLSR U2LSR
#define UxLSR_bit U2LSR_bit
#define UxLCR U2LCR
#define UxLCR_bit U2LCR_bit
#define UxDLL U2DLL
#define UxDLM U2DLM
#define UxTHR U2THR
#define UxIER U2IER
#define UxIIR U2IIR
#define UxRBR U2RBR
#define UxFCR U2FCR
#define UxTER_bit U2TER_bit
// UARTx IRQ ID in LPC17xx
#define Ux_IRQ 7 // 7 = UART2

#if CPU_FAMILY == LPC17xx
void UART2_IRQHandler(void)
{
  uart_interrupt();
}
#endif

struct uart_setup_s uart_setup;
const unsigned uart_signature = 45840296;

unsigned short fcr;


char uart_rx_buf[UART_RX_BUF_LEN];
char uart_tx_buf[UART_TX_BUF_LEN];
volatile unsigned uart_rx_consumer;
volatile unsigned uart_rx_producer;
volatile unsigned uart_rx_counter;
volatile unsigned uart_tx_consumer;
volatile unsigned uart_tx_producer;
volatile unsigned uart_tx_counter;


#if CPU_FAMILY == LPC17xx

unsigned uart_rx_read_buf(char *data, unsigned len)
{
  nvic_disable_irq(Ux_IRQ);
  unsigned n;
  for(n=0; n < len && uart_rx_counter > 0; ++n)
  {
    *data++ = uart_rx_buf[ uart_rx_consumer++ & (UART_RX_BUF_LEN-1) ];
    --uart_rx_counter;
  }
  nvic_enable_irq(Ux_IRQ);
# ifdef  UxMCR
  UxMCR |= 0x02; // assert RTS, enable rx flow
# endif
  return n;
}

unsigned uart_tx_write_buf(char *data, unsigned len)
{
  unsigned n;
  nvic_disable_irq(Ux_IRQ);
  for(n=0; n<len; ++n)
  {
    if(uart_tx_counter == UART_TX_BUF_LEN) break;
    uart_tx_buf[ uart_tx_producer++ & (UART_TX_BUF_LEN-1) ] = *data++;
    ++uart_tx_counter;
  }
  while((UxLSR & 0x20) && uart_tx_counter) // tx fifo has free place
  {
    rs485_tx(1);
    UxTHR = uart_tx_buf[ uart_tx_consumer++ & (UART_TX_BUF_LEN-1) ];
    --uart_tx_counter;
  }
  nvic_enable_irq(Ux_IRQ);
  return n; // writed to tx buf
}

#else

unsigned uart_rx_read_buf(char *data, unsigned len)
{
  unsigned n;
  for(n=0; n < len && uart_rx_counter > 0; ++n)
  {
    __disable_interrupt();
#error this cond-n is in conflict with loop cond-n
    if(uart_rx_counter == 0)
    {
      UxMCR |= 0x02; // assert RTS, enable rx flow
      break;
    }
    *data++ = uart_rx_buf[ uart_rx_consumer++ & (UART_RX_BUF_LEN-1) ];
	  --uart_rx_counter;
	  __enable_interrupt();
  } // for
  __enable_interrupt();
  return n;
}

unsigned uart_tx_write_buf(char *data, unsigned len)
{
  unsigned n;
  __disable_interrupt();
  for(n=0; n<len; ++n)
  {
    __disable_interrupt();
    if(uart_tx_counter == UART_TX_BUF_LEN) break;
    uart_tx_buf[ uart_tx_producer++ & (UART_TX_BUF_LEN-1) ] = *data++;
    ++uart_tx_counter;
    __enable_interrupt();
  }
  __disable_interrupt();
  while((UxLSR & 0x20) && uart_tx_counter) // tx fifo has free place
  {
    rs485_tx(1);
    UxTHR = uart_tx_buf[ uart_tx_consumer++ & (UART_TX_BUF_LEN-1) ];
    --uart_tx_counter;
  }
  __enable_interrupt();
  return n; // writed to tx buf
}

#endif // CPU rx/tx

systime_t uart_rx_time; // don't use it directly in main loop!!!

systime_t get_uart_rx_time(void)
{
  unsigned s = proj_disable_interrupt();
  systime_t t = uart_rx_time;
  proj_restore_interrupt(s);
  return t;
}

/*
*  uart Rx flow control - manual, per uart_rx_buf[] fill state
*  uart Tx flow control - automatic (CTSAuto)
*/

void uart_interrupt(void)
{
  unsigned reg, iir, lsr;
  for(;;)
  {
    iir = UxIIR;
    if(iir & 1) return;
    switch(iir & 0x0f)
    {
    case 0x06: // rx error
      lsr = UxLSR; // read to clear interrupt
      reg = UxRBR; // read from fifo and drop byte with error
      break;
    case 0x04: // rx fifo threshold
    case 0x0c: // rx fifo timeout
      lsr = UxLSR;
      if((lsr & 1) == 0)
      {
        reg = UxRBR; // silicon bug? clears CTI interrupt with LSR.DA = 0 !
        break;
      }
      while(lsr & 1) // rx fifo is not empty
      {
        reg = UxRBR; // read data
        if(uart_rx_counter < UART_RX_BUF_LEN) // if place available, save it
        {
          uart_rx_buf[ uart_rx_producer++ & (UART_RX_BUF_LEN-1) ] = reg;
          ++uart_rx_counter;
        }
        lsr = UxLSR;
      }
#     ifdef  UxMCR
      if(uart_rx_counter >= UART_RX_BUF_LEN-48)
      {
        UxMCR &=~ 0x02; // de-assert RTS, disable rx flow
      }	
#     endif
      uart_rx_time = sys_clock();	
      break;
    case 0x02: // tx empty
      while(uart_tx_counter)
      {
        UxTHR = uart_tx_buf[ uart_tx_consumer++ & (UART_TX_BUF_LEN-1) ];
        --uart_tx_counter;
        if((UxLSR & (1<<5)) == 0) break; // tx fifo is full
      }
      break;
    default:
      return;
    } // switch
    break; // from for, no repeated read of iir
  } // for
}

void uart_hardware_init(int speed, int bits, enum uart_parity_e parity, int stops)
{
  unsigned cpsr = proj_disable_interrupt(); /// настройку выполн€ть с ***закрытыми прерывани€ми***
  // ќбнуление рабочих параметров
  uart_rx_consumer = uart_rx_producer = uart_rx_counter =
  uart_tx_consumer = uart_tx_producer = uart_tx_counter = 0;
  uart_pins_init();
  //  онфигураци€ сигналов управлени€ COM-порта
  // ≈сли устройство будет с управлением потоком, то нужно будет подключать
  // регистры управлени€ модемом(пока можно отложить)
  UxIER = 0x00; // disable all UART interrupts
  // ”становка скорости с округлением
#if CPU_FAMILY == LPC21xx
  // divider  = (SYSTEM_CCLK / PCLK_DIV=1) / (16 * baudrate_bps) // LPC213x
  unsigned div = (SYSTEM_CCLK<<4) / (16 * speed); // 4-bit fixed point
#elif CPU_FAMILY == LPC17xx
  unsigned div = (SYSTEM_PCLK<<4) / (16 * speed); // 4-bit fixed point
#else
#error "unsupported, check clocks"
#endif
  if(div & 8) div += 16; // round up/down to integer (4-bit fixed point)
  div >>= 4; // remove fraction part
  UxLCR_bit.DLAB=1; //Enable DLAB //открыли доступ к U0DLL, U0DLM
  UxDLL = div;
  UxDLM = div >> 8;
  // UxLCR_bit.DLAB=0; //Disable DLAB //залочили обратно U0DLL, U0DLM - залочитс€ само записью LCR
  //”становка режима
  unsigned char  lcr = 0;
  if(bits == 7)  lcr |= 2; // word length, 7 or 8
  else           lcr |= 3;
  if(stops == 2) lcr |= 1<<2;
  if(parity == EVEN_PARITY) lcr |= 1<<3 | 1<<4; // ebable parity, even parity
  if(parity == ODD_PARITY)  lcr |= 1<<3 | 0<<4; // enable parity, odd parity
  UxLCR = lcr;
  //”становка FIFO - разрешение и размер.
  // FCR - write-only регистр!!!
  UxFCR = 0x01; // enable the FIFO's, FIFO depth resets to 14
  UxFCR = 0x07; // reset rx and tx FIFOs
  fcr = UxFCR = 0x81; // set FIFO trigger level 8 byte
  // –азрешить передатчик
  //UxTER_bit.TxEn = 1;
  UxTER_bit.TXEN = 1;
#if CPU_FAMILY == LPC21xx || CPU_FAMILY == LPC23xx
  // Install ISR
  vic_install_isr(VIC_UART1, uart_interrupt, 1);
#elif CPU_FAMILY == LPC17xx
  nvic_set_pri(Ux_IRQ, 15);
  nvic_enable_irq(Ux_IRQ);
#endif
  // Enable  Interrupts in UART hardware
  // UxIER = 3; // RXDA, THRE
  UxIER = 7; // RXDA, THRE, Rx Line Status
  proj_restore_interrupt(cpsr); /// восстановить прерывани€
}



unsigned uart_http_get_data(unsigned pkt, unsigned more_data)   /// CGI дл€ выдачи данных веб-интерфейсу
{
  char buf[768];
  char *dest = buf;
  dest+=sprintf((char*)dest,"var packfmt={");
  PLINK(dest, uart_setup, ip);
  PLINK(dest, uart_setup, listen_port);
  PLINK(dest, uart_setup, rx_packet_len);
  PLINK(dest, uart_setup, rx_packet_timeout);
  PLINK(dest, uart_setup, flags);
  PLINK(dest, uart_setup, baudrate);
  PLINK(dest, uart_setup, uart_usage);
  PSIZE(dest, sizeof uart_setup);
  dest+=sprintf(dest, "};\r\nvar data={");
  PDATA_IP(dest, uart_setup, ip);
  PDATA(dest, uart_setup, listen_port);
  PDATA(dest, uart_setup, rx_packet_len);
  PDATA(dest, uart_setup, rx_packet_timeout);
  PDATA(dest, uart_setup, flags);
  PDATA(dest, uart_setup, baudrate);
  PDATA(dest, uart_setup, uart_usage);
  dest += sprintf(dest, "tx_overflows:%u, proj_assm:%u};", /*tx_overflow_event_counter*/ uart_to_net_overflow, PROJECT_ASSM); // // both directions rx and tx
  tcp_put_tx_body(pkt, (unsigned char*)buf, dest - buf);
  return 0;
}

int uart_http_set_data(void)
{
  http_post_data((void*)&uart_setup, sizeof uart_setup);
  EEPROM_WRITE(&eeprom_uart_setup, &uart_setup, sizeof uart_setup);

#ifdef TCPCOM_MODULE
  tcpcom_reinit();
#endif

  //- убить данные в FIFO, запретить передачу
  uart_init(); /// переинициализировать модуль с новыми параметрами

  http_redirect("/com.html");
  return 0;
}

HOOK_CGI(uart_get,   (void*)uart_http_get_data,  mime_js,  HTML_FLG_GET | HTML_FLG_NOCACHE );
HOOK_CGI(uart_set,   (void*)uart_http_set_data,  mime_js,  HTML_FLG_POST );


int uart_rx_data_ready(unsigned rx_timeout_ms)
{
  return uart_rx_counter > 0 && sys_clock() > get_uart_rx_time() + rx_timeout_ms;
}

void uart_flush(void)
{ // TODO enchance this!
  volatile char reg;
  unsigned cpsr = proj_disable_interrupt();
  while(UxLSR & 1) // rx fifo is not empty
    reg = UxRBR; // read data
  uart_rx_consumer =
  uart_rx_producer =
  uart_rx_counter =
  uart_tx_consumer =
  uart_tx_producer =
  uart_tx_counter = 0;
  proj_restore_interrupt(cpsr);
}

void uart_timer_1ms(void)
{
  if(UxLSR_bit.TEMT) rs485_tx(0);
}

void uart_reset_params(void)
{
  util_fill((void*)&uart_setup, sizeof uart_setup, 0);
  uart_setup.rx_packet_len = 96;      /// фиксированый размер, поле disabled в веб-интерфейсе
  uart_setup.rx_packet_timeout = 100;
  uart_setup.baudrate = 9600/1200;

  EEPROM_WRITE(&eeprom_uart_setup, &uart_setup, sizeof eeprom_uart_setup);
  EEPROM_WRITE(&eeprom_uart_signature, &uart_signature, sizeof eeprom_uart_signature);
}

void uart_init(void)
{
  unsigned sign;
  uart_pins_init();
  EEPROM_READ(&eeprom_uart_signature, &sign, sizeof sign);
  if(sign != uart_signature)
    uart_reset_params();
  EEPROM_READ(&eeprom_uart_setup, &uart_setup, sizeof uart_setup);
  uart_hardware_init(
     uart_setup.baudrate * 1200,
     uart_setup.flags & UART_7_BIT_WORD ? 7 : 8,
     (enum uart_parity_e)(uart_setup.flags & (UART_ODD_PARITY|UART_EVEN_PARITY)),
     uart_setup.flags & UART_2_STOP_BITS ? 2 : 1
       );
/*
#warning "remove debugggggg - uart loopback"
  U2MCR |= 1<<4; // Looopback
*/
}

void uart_event(enum event_e event)
{
  switch(event)
  {
  case E_TIMER_1ms:
    uart_timer_1ms();
    break;
  case E_RESET_PARAMS:
    uart_reset_params();
    break;
  }
}


#endif // UART_MODULE
