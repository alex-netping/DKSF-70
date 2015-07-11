
#define IR_MODULE
#define IR_COMMANDS_N 16

// v3

#define IR_IIC_CH    0
#define IR_IIC_ADDR  0x4e

#define IR_IIC_WRITE(sla, buf, len)    swi2c_op(IR_IIC_CH, (sla)&0xFE, buf, len)
#define IR_IIC_READ(sla, buf, len)     swi2c_op(IR_IIC_CH, (sla)|0x01, buf, len)
#define IR_IIC_ACK                     i2c_ack

#include "ir2/ir.h"
