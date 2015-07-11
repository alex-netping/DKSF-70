/*
v1.2
11.02.2010
  modified by LBS
v1.3-48
3.03.2013
  LPC17xx support
*/

#include "platform_setup.h"

#ifndef  FLASH_LPC_H
#define  FLASH_LPC_H

///Версия модуля
#define  FLASH_LPC_VER	  1
///Сборка модуля
#define  FLASH_LPC_BUILD  3

// ATTENTION! check argument limitations - block alignment and size!
void flash_lpc_erase(unsigned num1, unsigned num2);
void flash_wr_func(unsigned addr, void *buf, unsigned len);
void flash_lpc_write(unsigned addr, void *buf, unsigned len);

#endif

