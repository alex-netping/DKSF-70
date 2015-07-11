/*
v1.1
3.12.2012
  dksf213 support, spi_eeprom_read_id()
v1.2-48
4.03.2013
  dksf48 support
*/

#include "platform_setup.h"
#ifndef  SPI_EEPROM_H
#define  SPI_EEPROM_H

//Версия модуля
#define  SPI_EEPROM_VER	        1
//Сборка модуля
#define  SPI_EEPROM_BUILD	2

void spi_eeprom_init(void);
void spi_eeprom_restore_init(void);
unsigned spi_eeprom_read_id(void);
void spi_eeprom_read(unsigned addr, void *buf, unsigned len);
void spi_eeprom_write(unsigned addr, void *buf, unsigned len);


#endif

