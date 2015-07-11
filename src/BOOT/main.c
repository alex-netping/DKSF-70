
/// boot.c
/// updated by LBS
/// 15.03.2010
/// updated by LBS
/// 1.06.2010  boot eeprom operations moved into external file, DKSF51.3 project
/// 21.12.2010 added DKSF 200
/// 5.06.2012 added DKSF60
/// 27.08.2012 eeprom-less rewrite
//  4.03.2013 DKSF 48 version, LPC1778

#include "platform_setup.h"
#include "bootlink.h"
#include <string.h>

__no_init struct bootldr_data_s bootldr_data @ 0x10007F40;

#if CPU_FAMILY == LPC17xx

void boot_init_blink_timer() // @ "BOOT" // setup for CPU LED blink with period 1s
{
  // 1Hz blink (for over 5s)
  T1TCR = 0; // stop
  T1PR = SYSTEM_PCLK / 2;  // for 0.5s blink toggle, 1s period
  T1PC = T1TC = 0;
  T1TCR = 1;   // enable counting
}

#else
# error "not implemented for this CPU!"
#endif

void boot_blink_led(void) // @ "BOOT"
{
  led_pin(CPU_LED, T1TC&1);
}

unsigned short boot_update_fw_crc_calc(void *addr, unsigned size) // @ "BOOT"
{
  unsigned i, j, data, tmp;
  unsigned short crc = 0x4C7F;
  char *p = addr;
  for (j=0; j<size; j++)
  {
    data = *p++;
    for(i=0; i<8; ++i)
    {		
      tmp = (crc ^ data) & 1;
      crc >>= 1;
      if (tmp) crc ^= 0xA001;
      data >>= 1;
    }
    if((j & 512) == 0) wdt_reset();
  }
  return crc;
}

void boot_move_firmware_to_work_area(void) // @ "BOOT"
{
  unsigned from_addr, to_addr;
  unsigned len, block_len;
  static unsigned buf[4096/4]; // not in stack, it's big!

  flash_lpc_erase(FW_SECTOR_START, FW_SECTOR_END);
  wdt_reset();
  from_addr = FIRMWARE_START;
  to_addr   = APPL_START;
  len = FIRMWARE_SIZE;
  for( ; len > 0; )
  {
    block_len = len < sizeof buf ? len : sizeof buf;
    if(len < sizeof buf)
      memset(buf, 0xff, sizeof buf);
    memcpy(buf, (void*)from_addr, block_len);
    flash_wr_func(to_addr, buf, sizeof buf);

    len -= block_len;
    from_addr += block_len;
    to_addr += block_len;

    boot_blink_led();
    wdt_reset();
  }  // for(;;)
}

void boot_goto_firmware(void) // @ "BOOT" // FW image starts from 0x1000
{
  asm ("movs r1, #1");
  asm ("lsls r1, #12"); // r1=0x1000
  asm ("ldr r0, [r1,#0]");
  asm ("msr msp, r0"); // [0x1000]->sp
  asm ("ldr r0, [r1,#4]"); // [0x1004]->pc
  asm ("bx r0");
}

int main(void) // @ "BOOT"
{
  proj_disable_interrupt();
  wdt_on();
  proj_init_clocks();
  led_pin_init();
  led_pin(CPU_LED, 1);
  boot_init_blink_timer();
  if(bootldr_data.signature == BOOTLDR_SIGNATURE_START_UPDATE)
  {
    bootldr_data.signature = 1;
    unsigned short crc = boot_update_fw_crc_calc((void*)FIRMWARE_START, FIRMWARE_SIZE);
    if(bootldr_data.crc16 == crc)
      boot_move_firmware_to_work_area();
    bootldr_data.signature = 2;
    // blink CPU led for at least 7s from start of update
    do {
      boot_blink_led();
      wdt_reset();
    } while(T1TC <= 16); // 8s
    led_pin(CPU_LED, 0); // off LED
  }
  bootldr_data.signature = 3;
  boot_goto_firmware();
  return 0;
}


#if CPU_FAMILY != LPC17xx
#error "Unknown CPU_FAMILY !"
#endif



