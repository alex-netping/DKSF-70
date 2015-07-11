/*
v1.2
11.02.2010
  modified by LBS
v1.3-48
3.03.2013
  LPC17xx support
*/

#include "platform_setup.h"
#include <string.h>

#if CPU_FAMILY == LPC17xx
#define IAP_ENTRY   0x1FFF1FF1
#else
#error "undefined CPU!"
#endif

void flash_wr_func(unsigned addr, void *buf, unsigned len);

static void prepare_sect(unsigned start, unsigned stop)
{
  unsigned cmd[6];
  unsigned status[3];
  cmd[0] = 50;
  cmd[1] = start;
  cmd[2] = stop;
  unsigned s = proj_disable_interrupt();
  (*(void (*)(void*,void*))IAP_ENTRY)(cmd, status);
  proj_restore_interrupt(s);
}

void flash_lpc_erase(unsigned start, unsigned end)
{
  unsigned cmd[6];
  unsigned status[3];
  prepare_sect(start, end);
  // Erase sector
  cmd[0] = 52;
  cmd[1] = start;
  cmd[2] = end;
  cmd[3] = SYSTEM_CCLK / 1000;
  unsigned s = proj_disable_interrupt();
  (*(void (*)(void*,void*))IAP_ENTRY)(cmd, status);
  proj_restore_interrupt(s);
}

void flash_lpc_write(unsigned addr, void *buf, unsigned len)
{
  unsigned stack[256/4]; // 256 byte buffer, word-aligned
  unsigned addr1, addr2, size_b;
  addr1 = addr & ~0xFFu;
  addr2 = addr &  0xFFu;
  do {
    size_b = 256 - addr2;
    if (size_b >= len && len <= 256) size_b = len;  // LBS this legacy needs to be simplified!
    len -= size_b;
    memcpy(stack, (void*)addr1, 256);
    memcpy((char*)stack + addr2, buf, size_b);
    flash_wr_func(addr1, stack, 256);
    addr1 += 256; addr2 = 0;
    buf = (void*)((unsigned)buf + size_b);
  } while(len);
}

// dont't writes over sector boundary!
// addr must be 256b aligned
// buf must be word-aligned
// len must be 256, 512, 1024, 4096 (for LPC17xx)
void flash_wr_func(unsigned addr, void *buf, unsigned len)
{
  unsigned cmd[6];
  unsigned status[3];
  unsigned sec;
#if CPU_FAMILY == LPC23xx || CPU_FAMILY == LPC21xx
  if(addr < 32768) sec = addr >> 12;
  else sec = ((addr-32768) >> 15) + 8;
#elif CPU_FAMILY == LPC17xx
  if(addr < 0x10000) sec = addr >> 12;
  else sec = ((addr - 0x10000) >> 15) + 16;
#else
# error "undefined CPU!"
#endif
  prepare_sect(sec, sec);
  //Program sector
  cmd[0] = 51;
  cmd[1] = addr;
  cmd[2] = (unsigned)buf;
  cmd[3] = len;
  cmd[4] = SYSTEM_CCLK / 1000;
  unsigned s = proj_disable_interrupt();
  (*(void (*)(void*,void*))IAP_ENTRY)(cmd, status);
  proj_restore_interrupt(s);
}


