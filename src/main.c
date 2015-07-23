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

#if PROJECT_MODEL == 48
  if( (RSID & (1<<4|1<<2)) // intentional reboot by firmware or WDT reset
  && bootldr_data.relay_state == (bootldr_data.relay_state_xor ^ RELAY_XOR) // persistance data is valid
  && T1TC == 0 && T1PC < 15000 ) // time from reboot less than 1.25ms // board depandant! see T1 init in bootloader!
  {
    relay_pin_restore(bootldr_data.relay_state); // quick restore of relay pin state
    relay_pin_init(); // enable gpio outputs (empty)
  }
  RSID = 0x3f; // clear RSID
#endif

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

  // special service version, clear EEPROM completely (64kb)
#if PROJECT_CHAR=='W'
  unsigned char buf[255];
  util_fill(buf, sizeof buf, 0xff);
  for(unsigned addr=0;addr<65536; addr += sizeof buf)
    spi_eeprom_write(addr, buf, sizeof buf);
  for(;;) {}
#endif

  init_modules(); // init modules, prepare hw, install ISRs etc.
  systick_init();
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
    //if(main_reboot) reboot_proc();
    if(main_reboot) soft_reboot();
    if(main_reload) reload_proc();
    system_event(E_EXEC, 0);
  }
} // main()


#warning TODO проверить переход в другую подсеть, не зависает ли
