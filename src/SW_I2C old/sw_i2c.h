/*
v2.2
15.03.2010
v2.3
05.04.2010
 DKSF162 driver
v2.4-50
5.07.2010
 corrected timing diagramm (hold after SCL(0), enshure SCL(1) before START)
v2.5-70
30.09.2013 (~)
  modified read algorithm, sampling point has moved, DELAY_T_HIGH_MINUS_HOLD added
  TODO check timing/freq of i2c bus
v2.6-70
22.01.2014
  void swi2c_pointer_op() added for sensor op convivience
*/

#ifndef I2C_RWBIT_WRITE
#define I2C_RWBIT_WRITE   0
#define I2C_RWBIT_READ    1
#endif

void swi2c_sda(int ifnum, int state);
int swi2c_sda_in(int ifnum);

extern unsigned char swi2c_ack;
void swi2c_op(int ifnum, unsigned char addr_byte, void *buf, unsigned len);
void swi2c_pointer_op(int ifnum, unsigned char addr_byte, unsigned char pointer, void *buf, unsigned len);
void swi2c_event(enum event_e event);
void swi2c_init(void);

/*
unsigned i2c_accquire(unsigned arg);
void i2c_release(void);
*/
