/*
* hardware i2c for LPC21xx/23xx
* by P.Lyubasov
* v1.3
* v1.8
* 1.06.2010
*   bugfix in iic_start()
* v1.9-70
* 30.05.2013
*   some cosmetic rewrite and unification
*   board dependencies moved to project.c
*/

#include "platform_setup.h"
#ifndef  HW_I2C_H
#define  HW_I2C_H

//Версия модуля
#define  HW_I2C_VER	  1
//Сборка модуля
#define  HW_I2C_BUILD	  9


/// флаг успешной операции
extern unsigned char i2c_ack;

void hw_i2c_init(void);
//void hw_i2c_read(int ch, int devaddr, void *vbuf, unsigned len);
void hw_i2c_read(int ch, int devaddr, unsigned char *buf, unsigned short len);
//void hw_i2c_write(int ch, int devaddr, void *vbuf, unsigned len);
void hw_i2c_write(int ch, int devaddr, unsigned char *buf, unsigned short len);


#endif

