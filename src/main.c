//#include <assert.h>
#include "platform_setup.h"


volatile systime_t sys_clock_counter;
volatile unsigned sys_clock_100ms = 1; // starts from not zero value
unsigned char counter_10ms;
unsigned char counter_100ms;
unsigned char counter_1s;

void HardFault_Handler(void)  { for(;;); }
void MemManage_Handler(void)  { for(;;); }
void BusFault_Handler(void)   { for(;;); }
void UsageFault_Handler(void) { for(;;); }

systime_t sys_clock(void)
{
  unsigned s = proj_disable_interrupt();
  systime_t t = sys_clock_counter;
  proj_restore_interrupt(s);
  return t;
}

void SysTick_Handler(void)
{
  ++ sys_clock_counter;
  system_event(E_TIMER_1ms, 0);
  if(++counter_10ms == 10)
  {
    counter_10ms = 0;
    system_event(E_TIMER_10ms, 0);
    if(++counter_100ms == 10)
    {
      counter_100ms = 0;
      ++ sys_clock_100ms;
      system_event(E_TIMER_100ms, 0);
      if(++counter_1s == 10)
      {
        counter_1s = 0;
        system_event(E_TIMER_1s, 0);
      }
    }
  }
}

void systick_init(void)
{
  STRELOAD = SYSTEM_CCLK / 1000 - 1;
  STCTRL = 1<<2 // from cclk
    | 1<<1 // enable interr
    | 1<<0; // enable count
}


void  main(void)
{
  __disable_interrupt();
  VTOR = 0x1000;  // remap vectors to firmware image
  wdt_on();
  wdt_reset();
  main_reboot = 0;
  main_reload = 0;
  led_pin_init(); // init LED pins
  led_pin(CPU_LED, 0); // switch off by low-level routine
  proj_init_clocks();
  led_pin(CPU_LED, 1); // basic hardware init ok, switch on LED by low-level routine
  init_basic_modules();   // low-level init of EEPROM-related modules

/*
  ////// debuggg - clear eeprom flash
# include "eeprom_map.h"
  unsigned char cbuf[256];
  util_fill(cbuf, sizeof cbuf, 0xff);
  for(unsigned a=0; a<32768; a+=256) EEPROM_WRITE(a, cbuf, 256);
  led_generation(150, 850, 255);
  for(;;){ WDT_RESET;}
  //////
*/

  int param_reset_activated = 0;
  if(reset_button()) // set default params before init
  {
    param_reset_activated = 1;
    system_event(E_RESET_PARAMS,0);
  }
  sys_init(); // prepare sys_setup

  /*********************
/// test 5
#warning "correct version in plathorm_setup!"
#warning "remove debuggggg"
#define LED 4,25
  pindir(LED, 1);
  for(;;)
  {
    pinset(LED);
    for(int i=0;i<4000000;++i);
    pinclr(LED);
    for(int i=0;i<1000000;++i);
  }
************/
/*
/// test 18
#warning "correct version in plathorm_setup!"
#warning "remove debuggggg"
  // check periph. clock
#define LED 4,25
  pindir(LED, 1);
  for(;;)
  {
    pinset(LED);
    delay_us(1000); // 5000 us = 5ms = 200Hz
    pinclr(LED);
    delay_us(4000);
  }
*/
/*
  ///// test 19,20 (SCL,SDA)
#warning "remove debuggggg"
  // check periph. clock
//#define SDA 4,20
  swi2c_init_pin();
  for(;;)
  {
    swi2c_sda(0,1);
    delay_us(2);
    delay_us(2);
    swi2c_sda(0,0);
    delay_us(4);
  }
*/
  // special service version, clear EEPROM completely (32kb)
#if PROJECT_CHAR=='W'
  unsigned char buf[255];
  util_fill(buf, sizeof buf, 0xff);
  for(unsigned addr=0;addr<65536; addr += sizeof buf)
    spi_eeprom_write(addr, buf, sizeof buf);
  for(;;) {}
#endif

  init_modules(); // init modules, prepare hw, install ISRs etc.
  systick_init();

/*****
/// test 6
#warning "correct version in plathorm_setup!"
#warning "remove debuggggg"
#define LED 4,25
  pindir(LED, 1);
  for(;;)
  {
    pinset(LED);
    for(int i=0;i<4000000;++i);
    pinclr(LED);
    for(int i=0;i<4000000;++i);
  }
****/

/*
#warning remove debugggg - scl rate test
for(;;)
{
  swi2c_scl(0,1);
  delay_us(2);
  swi2c_scl(0,0);
  delay_us(1);
  delay_us(1);
}
*/

  __enable_interrupt();
  log_enable_syslog = 1; // now network modules is functional, enable syslog
  // now LED module is init-ed ...
  led(CPU_LED, 1);
  if(param_reset_activated)
    led_generation(250, 250, 10); // signal 'parameter reset'
  else
    led_generation(100, 100, 3);  // signal 'firmware start'

  /// main loop ////
  while(1)
  {
    wdt_reset();
    if(main_reboot) reboot_proc();
    if(main_reload) reload_proc();
    system_event(E_EXEC, 0);
/*
#warning "remove debug test 24"
#define LED202 4,25
pindir(LED202,1);
swi2c_sda(0, 0);
delay(1);
swi2c_sda(0, 1);
delay_us(3);
int d = swi2c_sda_in(0);
pinwrite(LED202, d);
swi2c_sda(0, 1);
delay_us(30);
*/

  }
} // main()


#warning TODO проверить переход в другую подсеть, не зависает ли
