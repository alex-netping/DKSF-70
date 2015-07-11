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

#include "platform_setup.h"
#ifdef SW_I2C_MODULE

#ifndef DELAY_T_LOW

// approx 125kHz SCL clock
#define DELAY_T_LOW()  delay_us(3)
#define DELAY_T_HIGH() delay_us(3)
#define DELAY_HOLD()   delay_us(2)
#define DELAY_IDLE()   delay_us(5)
#define DELAY_T_LOW_MINUS_HOLD()  delay_us(2)
#define DELAY_T_HIGH_MINUS_HOLD() delay_us(2)

#endif


#define SDA(x) swi2c_sda(ifnum, x)
#define SCL(x) swi2c_scl(ifnum, x)

/*
#define DELAY_1()     delay_us(1)
#define DELAY_6()     delay_us(1)
#define DELAY_13()    delay_us(2)
#define DELAY_LOW()   delay_us(4)
#define DELAY_HIGH()  delay_us(3)
*/

#define DELAY_1()     delay_1()
#define DELAY_6()     delay_6()
#define DELAY_13()    delay_13()
#define DELAY_LOW()   delay_us(2)
#define DELAY_HIGH()  delay_us(2)

void delay_1(void)
{
__no_operation();
__no_operation();
__no_operation();
__no_operation();
__no_operation();

__no_operation();
}

void delay_6(void)
{
 for(int i=14; --i;) ;
}

void delay_13(void)
{
 for(int i=30; --i;) ;
}


unsigned char i2c_ack;

void swi2c_init(void)
{
  swi2c_init_pin();
  i2c_ack = 0;
}

/*
void swi2c_write_byte(int ifnum, int d)
{
  for(int i=0; i<8; ++i)
  {
    SCL(0);
    DELAY_HOLD(); // LBS 5.07.2010
    SDA(d & 0x80); d<<=1;
    DELAY_T_LOW_MINUS_HOLD();
    SCL(1);
    DELAY_T_HIGH();
  }
  SCL(0);
  DELAY_HOLD(); // LBS 5.07.2010
  SDA(1); // release SDA for RX of ACK
  DELAY_T_LOW_MINUS_HOLD();
  swi2c_ack = !swi2c_sda_in(ifnum); // ACK bit is inverse
  SCL(1);
  DELAY_T_HIGH();
}
*/

void swi2c_write_byte(int ifnum, int d)
{
  for(int i=0; i<8; ++i)
  {
    SCL(0);
    DELAY_HOLD(); // LBS 5.07.2010
    SDA(d & 0x80); d<<=1;
    DELAY_T_LOW_MINUS_HOLD();
    SCL(1);
    DELAY_T_HIGH();
  }
  SCL(0);
  DELAY_HOLD(); // LBS 5.07.2010
  SDA(1); // release SDA for RX of ACK
  DELAY_T_LOW_MINUS_HOLD();
  SCL(1);
  DELAY_HOLD();
  i2c_ack = !swi2c_sda_in(ifnum); // ACK bit is inverse
  DELAY_T_HIGH_MINUS_HOLD();
}


/*
unsigned char swi2c_read_byte(int ifnum, int ack)
{
  unsigned char d = 0;
  for(int i=0; i<8; ++i)
  {
    SCL(0);
    DELAY_HOLD(); // LBS 5.07.2010
    SDA(1);
    DELAY_T_LOW_MINUS_HOLD();
    d<<=1; d |= swi2c_sda_in(ifnum) ? 1 : 0 ;
    SCL(1);
    DELAY_T_HIGH();
  }
  SCL(0);
  DELAY_HOLD(); // LBS 5.07.2010
  SDA(!ack); // ACK is inverse bit
  DELAY_T_LOW_MINUS_HOLD();
  SCL(1);
  DELAY_T_HIGH();
  return d;
}
*/

unsigned char swi2c_read_byte(int ifnum, int ack)
{
  unsigned char d = 0;
  for(int i=0; i<8; ++i)
  {
    SCL(0);
    DELAY_HOLD(); // LBS 5.07.2010
    SDA(1);
    DELAY_T_LOW_MINUS_HOLD();
    SCL(1);
    DELAY_HOLD();
    d<<=1; d |= swi2c_sda_in(ifnum) ? 1 : 0 ;
    DELAY_T_HIGH_MINUS_HOLD();
  }
  SCL(0);
  DELAY_HOLD(); // LBS 5.07.2010
  SDA(!ack); // ACK is inverse bit
  DELAY_T_LOW_MINUS_HOLD();
  SCL(1);
  DELAY_T_HIGH();
  return d;
}

// must be used after   swi2c_stop() or swi2c_send_byte() for correct bus timing !!!
void swi2c_start(int ifnum)
{
  // check SDA is released
  i2c_ack = swi2c_sda_in(ifnum);
  if(!i2c_ack) return;
  SCL(1); // LBS 5.07.2010 - enshure SCL level for start
  // in low-speed, t SU;STA = 4.7us vs t HIGH = 4.0us, so add 1us delay
  delay_us(1);
  // start
  SDA(0);
  /* LBS 5.07.2010
  // start hold 4.0 / 0.6 / 0.26 us
  DELAY_HOLD();
  */
  DELAY_T_HIGH(); // if SCL was 0, it's required full t LOW
}

void swi2c_rep_start(int ifnum)
{
  // complete last clock period (from SCL high)
  SCL(0);
  DELAY_HOLD(); // LBS 5.07.2010
  SDA(1);
  DELAY_T_LOW_MINUS_HOLD();
  SCL(1);
  DELAY_T_HIGH();
  // check SDA is released
  i2c_ack = swi2c_sda_in(ifnum);
  if(!i2c_ack) return;
  // in low-speed, t SU;STA = 4.7us vs t HIGH = 4.0us, so add 1us delay
  delay_us(1);
  // START
  SDA(0);
  // start hold 4.0 / 0.6 / 0.26 us
  DELAY_HOLD();
}

// must be used after swi2c_read_byte() or after swi2c_write_byte()  for correct bus timing&protocol
void swi2c_stop(int ifnum)
{
  // complete last clock period (from SCL high)
  SCL(0);
  DELAY_HOLD(); // LBS 5.07.2010
  SDA(0);
  DELAY_T_LOW_MINUS_HOLD();
  SCL(1);
  // t SU;STOP = 4.0 / 0.6 / 0.26 us ( == t HIGH)
  DELAY_T_HIGH();
  // STOP
  SDA(1);
  // t BUF   4.7 / 1.3 / 0.5 us
  DELAY_IDLE();
}

void swi2c_op(int ifnum, unsigned char addr_byte, void *buf, unsigned len)
{
  unsigned char *p = buf;
  swi2c_start(ifnum);
  if(!i2c_ack) return;
  swi2c_write_byte(ifnum, addr_byte);
  if(i2c_ack)
  {
    for(int i=len; i--;)
    {
      if((addr_byte & 1) == 0)
      {
        swi2c_write_byte(ifnum, *p++);
        if(!i2c_ack) break;
      }
      else *p++ = swi2c_read_byte(ifnum, i!=0);
    }
  }
  swi2c_stop(ifnum);
}

void swi2c_pointer_op(int ifnum, unsigned char addr_byte, unsigned char pointer, void *buf, unsigned len)
{
  unsigned char *p = buf;
  swi2c_start(ifnum);
  if(!i2c_ack) return;
  swi2c_write_byte(ifnum, addr_byte & 0x7f); // write pointer // 26.01.2014
  if(!i2c_ack) goto end;
  swi2c_write_byte(ifnum, pointer);
  if(!i2c_ack) goto end;
  if(addr_byte & 1)
  {
    swi2c_rep_start(ifnum);
    if(!i2c_ack) goto end;
    swi2c_write_byte(ifnum, addr_byte);
    if(!i2c_ack) goto end;
  }
  for(int i=len; i--;)
  {
    if((addr_byte & 1) == 0)
    {
      swi2c_write_byte(ifnum, *p++);
      if(!i2c_ack) break;
    }
    else
      *p++ = swi2c_read_byte(ifnum, i!=0);
  }
end:
  swi2c_stop(ifnum);
}


void aswi2c_start(unsigned ifnum) // indeed, rep start with preparing sda down front
{
  DELAY_1();  // prepare rep start or stop errata on bus
  SDA(1);
  DELAY_LOW(); // also, bus free time after 'errata' stop
  SCL(1);
  DELAY_HIGH();
  SDA(0);  // real start
  DELAY_HIGH();
  SCL(0);
}

void aswi2c_write_byte(unsigned ifnum, char byte)
{
  for(int i=0; i<8; ++i)
  {
    DELAY_1();
    SDA(byte & 0x80);
    byte <<= 1;
    DELAY_LOW();
    SCL(1);
    DELAY_HIGH();
    SCL(0);
  }
  DELAY_1();
  SDA(1);
  DELAY_LOW();
  SCL(1);
  DELAY_1();
  i2c_ack = !swi2c_sda_in(ifnum);
  DELAY_HIGH();
  SCL(0);
}

char aswi2c_read_byte(unsigned ifnum, int ack)
{
  char byte = 0;
  DELAY_1();
  SDA(1);
  for(int i=0; i<8; ++i)
  {
    DELAY_LOW();
    SCL(1);
    DELAY_1(); // ???
    byte = (byte<<1) | swi2c_sda_in(ifnum);
    DELAY_HIGH();
    SCL(0);
  }
  DELAY_1();
  SDA(!ack);
  DELAY_LOW();
  SCL(1);
  DELAY_HIGH();
  SCL(0);
  return byte;
}

void aswi2c_stop(unsigned ifnum)
{
  DELAY_1();
  SDA(0);
  DELAY_LOW();
  SCL(1);
  DELAY_HIGH();
  SDA(1);
}




void swi2c_event(enum event_e event)
{
  if(event == E_INIT) swi2c_init();
}



#endif
