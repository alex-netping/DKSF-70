
#include "platform_setup.h"

const char device_name[] =
#if PROJECT_MODEL == 70
"UniPing Server Solution v3/SMS";
#elif PROJECT_MODEL == 71
"UniPing Server Solution v3";
#endif

__root struct {
  char start_label[16];
  unsigned short model;
  unsigned short version;
  unsigned short build;
  unsigned char  letter;
  unsigned char  assm;
} ver_data_for_npu_maker = { "6Gkj0Snm1c9mAc55", PROJECT_MODEL, PROJECT_VER,
            PROJECT_BUILD, PROJECT_CHAR, PROJECT_ASSM };

////////////////////////////// serial number /////////////////////////////////

__no_init unsigned serial @ 0x1000-4;

///////////////////////////// CPU NVIC //////////////////////////////////////

void nvic_set_pri(unsigned irq_n, unsigned pri)
{
  ((char*)&IP0)[irq_n] = pri << 3;
}

void nvic_enable_irq(unsigned irq_n)
{
  (&SETENA0)[irq_n >> 5] = 1 << (irq_n & 0x1f);
}

void nvic_disable_irq(unsigned irq_n)
{
  (&CLRENA0)[irq_n >> 5] = 1 << (irq_n & 0x1f);
}

///////////////////////////// CPU WDT ////////////////////////////////////////

// LPC17xx
// WDT clock is fixed 500kHz/4=125kHz

void wdt_on(void)
{
  if(serial == ~0U) return;
  WDTC = 125000 * 8; // 8s
  WDMOD = 3;
  unsigned s = proj_disable_interrupt();
  WDFEED = 0xAA;
  WDFEED = 0x55;
  proj_restore_interrupt(s);
}

void wdt_reset(void)
{
  if(serial == ~0U) return;
  unsigned s = proj_disable_interrupt();
  WDFEED = 0xAA;
  WDFEED = 0x55;
  proj_restore_interrupt(s);
}

///////////////////////////// reboot ////////////////////////////////////////

void reboot_proc(void)
{
  //AIRCR |= 1<<2; // SYSRESETREQ bit in System Control registers
  AIRCR = 0x05FA << 16 | 1<<2 ;
  for(;;);
}

///////////////////////////// clocks /////////////////////////////////////////

void proj_init_clocks(void)
{
  // if necessary, spin up quartz OSC
  if(SCS_bit.OSCSTAT == 0)
  {
    //SCS_bit.OSCRANGE = 0;
    SCS_bit.OSCEN = 1;
    while(SCS_bit.OSCSTAT == 0) ;
  }
  // Switch clock(s) from PLL to main OSC (i.e IRC or quartz)
  CCLKSEL_bit.CCLKSEL = 0;
  // Disable PLL
  PLL0CON_bit.PLLE = 0;
  PLL0FEED = 0xAA;
  PLL0FEED = 0x55;
  // Select main clock source (IRC or Quartz)
  CLKSRCSEL_bit.CLKSRC = 1; // Quartz
  // Set PLL settings
  PLL0CFG_bit.MSEL = 4-1;
  PLL0CFG_bit.PSEL = 1;
  PLL0FEED = 0xAA;
  PLL0FEED = 0x55;
  // Enable PLL
  PLL0CON_bit.PLLE = 1;
  PLL0FEED = 0xAA;
  PLL0FEED = 0x55;
  // Wait for the PLL to achieve lock
  while(PLL0STAT_bit.PLOCK == 0) ;
  // Set divider settings
  CCLKSEL_bit.CCLKDIV = 1;
  PCLKSEL_bit.PCLKDIV = 2;
  // Flash Accel Config
  #if SYSTEM_CCLK < 20000000
   FLASHCFG = (0x0UL<<12) | 0x3AUL;
  #elif SYSTEM_CCLK < 40000000
   FLASHCFG = (0x1UL<<12) | 0x3AUL;
  #elif SYSTEM_CCLK < 60000000
   FLASHCFG = (0x2UL<<12) | 0x3AUL;
  #elif SYSTEM_CCLK < 80000000
   FLASHCFG = (0x3UL<<12) | 0x3AUL;
  #elif SYSTEM_CCLK < 100000000
   FLASHCFG = (0x4UL<<12) | 0x3AUL;
  #endif
  // Switch clocks to the PLL
  CCLKSEL_bit.CCLKSEL = 1;
}


/////////////////////////// interrupts ////////////////////////////////////////

unsigned proj_disable_interrupt(void)
{
  unsigned primask = __get_PRIMASK();
  __set_PRIMASK(primask|1);
  return primask;
}
void proj_restore_interrupt(unsigned primask)
{
  __set_PRIMASK(primask);
}

void nvic_int_enable(unsigned exception_number)
{
  unsigned n = exception_number - NVIC_WDT;
  unsigned *regarray = (void*)&SETENA0;
  regarray[n >> 5] = 1U << (n & 31);
}

void nvic_int_disable(unsigned exception_number)
{
  unsigned n = exception_number - NVIC_WDT;
  unsigned *regarray = (void*)&CLRENA0;
  regarray[n >> 5] = 1U << (n & 31);
}

void nvic_clr_pend(unsigned exception_number)
{
  unsigned n = exception_number - NVIC_WDT;
  unsigned *regarray = (void*)&CLRPEND0;
  regarray[n >> 5] = 1U << (n & 31);
}

void nvic_int_pri(unsigned exception_number, unsigned priority_level)
{
  unsigned n = exception_number - NVIC_WDT;
  char *regarray = (void*)&IP0;
  regarray[n] = priority_level << 3;
}

////////////////////// GPIO driver LPC17xx ///////////////////////////////////


void pindir(int port, int pin, int dir_bit_value)
{
  unsigned addr = 0x20098000 + port * 0x20;
  /*
  *bit2m(addr, pin) = dir_bit_value;
  */
  unsigned intmask = proj_disable_interrupt();
  if(dir_bit_value)  *(unsigned*)addr |=  (1U<<pin);
  else     *(unsigned*)addr &=~ (1U<<pin);
  proj_restore_interrupt(intmask);
}

void pinset(int port, int pin)
{
  unsigned addr = 0x20098018 + port * 0x20;
  *(unsigned*)addr = 1U<<pin;
}

void pinclr(int port, int pin)
{
  unsigned addr = 0x2009801c + port * 0x20;
  *(unsigned*)addr = 1U<<pin;
}

void pinwrite(int port, int pin, unsigned value)
{
  unsigned addr = value ? 0x20098018 : 0x2009801c;
  addr += port * 0x20;
   *(unsigned*)addr = 1U<<pin;
}

unsigned pinread(unsigned port, unsigned bit)
{
  unsigned addr = 0x20098014 + port * 0x20;
  return ((*(unsigned*)addr) >> bit) & 1U;
}

///////////////////////////// reset button //////////////////////////////////

#define DEFAULT 4,18

int reset_button(void)
{
  return pinread(DEFAULT) == 0;
}

//////////////////////////////// LEDs //////////////////////////////////////////

#define LED2 4,22
#define LED4 3,13
#define POWERLED 4,25

void led_pin_init(void)
{
  IOCON_P4_22_bit.OD = 1; // LED2
  pindir(LED2, 1);
  pinset(LED2);
  IOCON_P3_13_bit.OD = 1; // LED4
  pindir(LED4, 1);
  pinset(LED4);
  // static power LED (LED Green D202)
  pindir(POWERLED, 1);
  pinset(POWERLED);
}

void led_pin(enum leds_e ledn, int state)
{
  if(ledn == CPU_LED)
  {
    pinwrite(LED2, state ? 0 : 1);
    pinwrite(LED4, state ? 0 : 1);
  }
}

///////////////////////// Ethernet  //////////////////////////////////////

#define PHY_RST 1,3

void phy_reset_line_init(void)  { pindir(PHY_RST, 1); }
void phy_reset_line_clear(void) { pinclr(PHY_RST); }
void phy_reset_line_set(void)   { pinset(PHY_RST); }

////////////////////// HW detect /////////////////////////////////////////

unsigned char proj_hardware_model;
unsigned short gsm_model;

#warning ATTN call proj_hardware_detect() on startup!
unsigned proj_hardware_detect(void)
{
  if(proj_hardware_model == 0)
  {
    PCONP_bit.PCAD = 1; // power-on ADC
    IOCON_P0_23 = 1<<0 | 0<<3 | 0<<7 ; // func=ADC, pull=none, analog mode on (bit7=0!)
    delay(1);
    AD0CR =
           1 << 0                       // SEL channel
      |    CURDET_ADC_DIV <<  8         // ADC divider from PCLK
      |    1 << 21                      // PDN = 1
      |    0 << 24;                     // START
    delay(1);
    AD0CR |= 1<<24; // start
    unsigned d, v;
    do { d = AD0GDR; } while((d & (1u<<31)) == 0); // wait for DONE bit
    unsigned u = (d >> 4) & 0xfff; // get 12-bit result field
    for(v=1; v<7; ++v)
      if(u < 372 * v) // 372 = 0.3v/3.3v*4096
        break;
    proj_hardware_model = v;
    if(v == 1) gsm_model = 52;
    else gsm_model = 900;
  }
  return proj_hardware_model;
}

/////////////////////////// relays ///////////////////////////////////////

#define RELAY1 0,12

void relay_pin_init(void)
{
  pindir(RELAY1, 1);
}

void relay_pin(unsigned relay_n, int state)
{
  pinwrite(RELAY1, state);
}

//////////////////////////// hw i2c /////////////////////////////////////

#define ENA_5V_I2C 3,26

void hw_i2c_init_pins(void)
{
  // enable ext. i2c bus power
  pinset(ENA_5V_I2C);
  pindir(ENA_5V_I2C, 1);
/*
#warning remove debug
  pindir(4,20,1);
  pindir(4,21,1);
  for(;;)
  {
    pinset(4,20);
    pinset(4,21);
    delay(1);
    pinclr(4,20);
    pinclr(4,21);
    delay(1);
  }
*/
  // enable i2c pins
  // func=2 pull=no hys=1 inv=0 slew=slow od=1
  IOCON_P4_20 = IOCON_P4_21 = 2<<0 | 0<<3 | 1<<5 | 0<<6 | 0<<9 | 1<<10;
  // setup i2c timeout timer
  // 0.1ms prescaled clock is used in hw_i2c.c
  PCONP_bit.PCTIM2 = 1;
  /* LPC2366 - PCLKSEL0 = (PCLKSEL0 & ~(3<<4)) | (1<<4);  // T1 clock = CCLK */
  I2C_TIMER_PR = SYSTEM_PCLK / 10000;
}

///////////////////////// sw i2c /////////////////////////////////////////

#define CPU_SCL  4,21 // SCL out
#define CPU_SDA  4,20 // SDA in/out

#define ENA_5V_I2C 3,26

void swi2c_init_pin(void)
{
  pinset(CPU_SCL);
  pindir(CPU_SCL, 1);
  pinset(CPU_SCL);
  pindir(CPU_SCL, 1);

  // config i2c pins, open drain
  // func=0 pull=no hys=1 inv=0 slew=slow od=1
  IOCON_P4_20 = IOCON_P4_21 = 0<<0 | 0<<3 | 1<<5 | 0<<6 | 0<<9 | 1<<10;

  pinset(ENA_5V_I2C);
  pindir(ENA_5V_I2C, 1);
}

void swi2c_scl(int ifnum, int state)
{
  pinwrite(CPU_SCL, state);
}

void swi2c_sda(int ifnum, int state)
{
  pinwrite(CPU_SDA, state);
  pindir(CPU_SDA, 1);
}

int swi2c_sda_in(int ifnum)
{
  pindir(CPU_SDA, 0);
  for(int i=0; i<10; ++i) ;
  return pinread(CPU_SDA);
}

//////////////////////////////// 1W /////////////////////////////////////

#define OW_OUT 0,0
#define OW_IN  0,1

void ow_init_pin(void)
{
  pinset(OW_OUT);
  pindir(OW_OUT, 1);
}

void ow_out(int state)
{
  pinwrite(OW_OUT, state);
}

int ow_in(void)
{
  return pinread(OW_IN);
}

/////////////////////////////// IO LINES ////////////////////////////////

#define ENA_12V  3,25

// ATTN! THIS DEFINITIONS IS ***NOT*** USED IN DRIVERS!!!!

// IO pinouts of DKST70.1 prototype has checked positively on 4.06.2013

#define OUTPUT(n)  4,2+n // open sink, inverting via mosfet
#define INPUT(n)   4,10+n

const unsigned saved_io_sign = 0xaa548220;
__no_init unsigned saved_io;
__no_init unsigned char saved_io_lvl[8]; // retains state via hot restart
__no_init unsigned char saved_io_dir[8];

void io_pin_mark_saved(void)
{
  saved_io = saved_io_sign;
}

void io_hardware_init(void)
{
  pinset(ENA_12V);
  pindir(ENA_12V, 1);
  // if signature ok, quick restore of state before hot restart
  if(saved_io == saved_io_sign)
  {
    for(int i=0; i<8; ++i)
    {
      if(saved_io_dir[i] == 0) io_set_level(i, 1); // input
      else io_set_level(i, saved_io_lvl[i]); // output
    }
  }
  else
  {
    for(int i=0; i<8; ++i)
      io_set_level(i, 1); // def = input
  }
  // init low switch control pins
  for(int i=0; i<8; ++i)
    pindir(OUTPUT(i), 1);
}

int  io_read_level(unsigned ch)
{
  if(ch > 7) return 0;
  return pinread(INPUT(ch)) == 0; // inverted by comparator
}

void io_set_level(unsigned ch, unsigned level)
{
  if(ch > 7) return;
  saved_io_lvl[ch] = level;
  // mosfet with open sink (inversion!)
  pinwrite(OUTPUT(ch), !level);
}

void io_set_dir(unsigned ch, unsigned dir)
{
  if(ch > 7) return;
  saved_io_dir[ch] = dir;
  // open sink outputs with independent input, Input equivalent to Output Hi
  if(dir == 0)
  {
    // mosfet with open sink (inversion!)
    pinwrite(OUTPUT(ch), 0);
  }
}

////////////////////////// UART //////////////////////////////////////////

void uart_pins_init(void)
{
  PCONP_bit.PCUART2 = 1;
  IOCON_P0_10 = 1<<0 | 2<<3 | 1<<5 ; // UART2 TX func, pull-up, hyst on
  IOCON_P0_11 = 1<<0 | 2<<3 | 1<<5 ; // UART2 RX func, pull-up, hyst on
  /*
#warning "remove debuggg"
#warning "correct C letter in fw name!"
  */
  IOCON_P1_19 = 6<<0 | 1<<10;  // U2_OE func, no pulls, open drain mode
  U2RS485CTRL |= 1<<4 | 1<<5; // enable hw tx ctrl, enable inversion (=1 on tx)
}

void rs485_tx(unsigned flag)
{
  // empty, hardware control on U2_OE
}

//////////////////////////// SMS IGT/EMEROFF driver //////////////////////

#define ENA_4V2     2,3
#define IGT         3,29
#define EMEROFF     3,28
#define CTS         3,18

void gsm_power(int onoff)
{
  pinwrite(ENA_4V2, onoff);
  pindir(ENA_4V2, 1);
}

void gsm_pins_init(void)
{
  IOCON_P3_16 = 3<<0 | 2<<3 | 1<<5 ; // UART1 TX func, pull-up, hyst on
  IOCON_P3_17 = 3<<0 | 2<<3 | 1<<5 ; // UART1 RX func, pull-up, hyst on
  pindir(IGT, 1);
  pindir(EMEROFF, 1);
  // CTS line,  P3.18 on DKSF48
  IOCON_P3_18_bit.MODE = 1; // pull-down
}

int gsm_read_cts(void)
{
  return pinread(CTS);
}

void gsm_igt(int level)
{
  pinwrite(IGT, !level); // inverted by transistor
}

void gsm_emeroff(int level)
{
  pinwrite(EMEROFF, !level); // inverted by transistor
}

void gsm_led(int state)
{
  // unused in dkst70
}

//////////////////////////////////////////////////////////////////////////



