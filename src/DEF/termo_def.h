#define TERMO_MODULE		//Флажок включения модуля
//#define   TERMO_DEBUG		//Флажок включения отладки в модуле

/// Кол-во датчиков
#define TERMO_N_CH              8    // SLA bits 3..1 range is 0..7

/// SLA адрес датчика, без адреса чипа
#define TERMO_SLA            0x90

/// номер I2C контроллера
////#define TERMO_I2C               2
#define TERMO_I2C               0

/// период повторения опроса датчиков, мс
#define TERMO_READ_PERIOD       6000

/// половина гистерезиса на границах диапазонов, град.С (плюс-минус)
#define TERMO_HYST              1

//---- Переопределение внешних связей модуля---------

/// макросы чтения-записи i2c

/*
#define TERMO_IIC_READ(sla, buf, len)     hw_i2c_read(TERMO_I2C, sla, buf, len)
#define TERMO_IIC_WRITE(sla, buf, len)    hw_i2c_write(TERMO_I2C, sla, buf, len)
#define TERMO_IIC_ACK                     i2c_ack
*/
#define TERMO_IIC_WRITE(sla, buf, len)    swi2c_op(TERMO_I2C, (sla)&0xFE, buf, len)
#define TERMO_IIC_READ(sla, buf, len)     swi2c_op(TERMO_I2C, (sla)|0x01, buf, len)
#define TERMO_IIC_ACK                     i2c_ack
#define TERMO_IIC_SCL(level)              swi2c_scl(TERMO_I2C, level)  // LBS 14.02.2011 v1.7-50

#include "termo\termo.h"
