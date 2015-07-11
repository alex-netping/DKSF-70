#define TERMO_MODULE		//������ ��������� ������
//#define   TERMO_DEBUG		//������ ��������� ������� � ������

/// ���-�� ��������
#define TERMO_N_CH              8    // SLA bits 3..1 range is 0..7

/// SLA ����� �������, ��� ������ ����
#define TERMO_SLA            0x90

/// ����� I2C �����������
////#define TERMO_I2C               2
#define TERMO_I2C               0

/// ������ ���������� ������ ��������, ��
#define TERMO_READ_PERIOD       6000

/// �������� ����������� �� �������� ����������, ����.� (����-�����)
#define TERMO_HYST              1

//---- ��������������� ������� ������ ������---------

/// ������� ������-������ i2c

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
