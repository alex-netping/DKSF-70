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

#ifdef HW_I2C_MODULE

#define CON_AA    1<<2
#define CON_SI    1<<3
#define CON_STOP  1<<4
#define CON_START 1<<5
#define CON_I2EN  1<<6

struct i2c_dev_s {
  unsigned conset;
  unsigned stat;
  unsigned dat;
  unsigned addr;
  unsigned sclh;
  unsigned scll;
  unsigned conclr;
};

// target acknoledge flag (=1 if transfer is ok)
unsigned char i2c_ack;


//Описание процедур модуля

struct i2c_dev_s *iic_base(int ch)
{
  switch(ch)
  {
  case 0: return (void*) &I2C0CONSET;
  case 1: return (void*) &I2C1CONSET;
#if CPU_FAMILY != LPC21xx
  case 2: return (void*) &I2C2CONSET;
#endif
  default: return (void*) ~0U;

  }
}

void hw_i2c_init_one(unsigned i2c_number, unsigned freq)
{
  struct i2c_dev_s *iic = iic_base(i2c_number);
  unsigned halftimimg = SYSTEM_PCLK / freq / 2;
  iic->sclh = halftimimg >> 8;
  iic->scll = halftimimg & 255;
  iic->conset = CON_I2EN;
  iic->conset = CON_STOP;
}

#if !(defined(HW_I2C0_USED) || defined(HW_I2C1_USED) || defined(HW_I2C2_USED))
#error "Define which i2c is used!"
#endif

void hw_i2c_init(void)
{
  hw_i2c_init_pins(); // incl. 5V power enable and timer setup

#ifdef HW_I2C0_USED
  hw_i2c_init_one(0, 100000);
#endif

#ifdef HW_I2C1_USED
  hw_i2c_init_one(1, 100000);
#endif

#ifdef HW_I2C2_USED
  hw_i2c_init_one(2, 100000);
#endif

}

void iic_wait(struct i2c_dev_s *iic, unsigned mask, unsigned state) // state = 0 or 1, not mask
{
  I2C_TIMER_TCR = 2; // stop, zero TC&PC
  I2C_TIMER_TCR = 1; // enable counting
  unsigned state_mask = state * mask;
  for(;;)
  {
    if((iic->conset & mask) == state_mask) { i2c_ack = 1; return; } // ok
    if(I2C_TIMER_TC >= 10)                 { i2c_ack = 0; return; } // 1ms timeout (0.1 x 10)
  }
}

#define WAIT_SI()   iic_wait(iic, CON_SI,   1)
#define WAIT_STOP() iic_wait(iic, CON_STOP, 0)
/*
void iic_start(struct i2c_dev_s *iic, unsigned char devaddr, int rw)
{
  int n, att;
  iic->conset = CON_STOP;
  iic->conclr = CON_SI;
  WAIT_STOP(); // wait while STOP transmitted (bit is cleared by LPC HW)
  if(!i2c_ack) return; // bus failure
  for(att=0; att<2; ++att) // 2 conseq. attempts
  {
    iic->conset = CON_START;
    WAIT_SI();
    if(iic->stat == 0x08 || iic->stat == 0x10) // START or REP.START is ok
    {
      iic->dat = devaddr | (rw & 1);
      iic->conclr = CON_START | CON_SI;
      n=10; while(--n);
      WAIT_SI();
      if(iic->stat == 0x18 || iic->stat == 0x40)
      { // slave addr acknoledged, OK
        i2c_ack = 1; // OK
        return;
      }
    }
    iic->conset = CON_STOP;
    iic->conclr = CON_SI; // clear SI
    WAIT_STOP();
  }
  i2c_ack = 0; // no ACK
}

void hw_i2c_read(int ch, int devaddr, void *vbuf, unsigned len)
{
  struct i2c_dev_s *iic = iic_base(ch);
  char *buf = vbuf;

  if(len==0) return;
  iic_start(iic, devaddr, 1);
  if(!i2c_ack) return;
  do
  {
    if (len == 1) iic->conclr = CON_AA; // on last byte clear ACK
    else          iic->conset = CON_AA; //
    iic->conclr = CON_SI;
    WAIT_SI();
    if(iic->stat == 0x50 || iic->stat == 0x58) ; // data byte tx ok
    else
    { // bus failure
      iic->conset = CON_STOP;
      iic->conclr = CON_SI;
      WAIT_STOP();
      i2c_ack = 0; // show error (WAIT_STOP() sets i2c_ack!)
      return;
    }
    *buf++ = iic->dat;
  }
  while(--len);
  iic->conset = CON_STOP;
  iic->conclr = CON_SI; // clear SI
  WAIT_STOP();
}

void hw_i2c_write(int ch, int devaddr, void *vbuf, unsigned len)
{
  struct i2c_dev_s *iic = iic_base(ch);
  char *buf = vbuf;

  if(len == 0) return;
  iic_start(iic, devaddr, 0);
  if(!i2c_ack) return;
  do
  {
    iic->dat = *buf++;
    iic->conclr = CON_SI;
    WAIT_SI();
    if(iic->stat != 0x28) // anithing except "data byte transmitted, ACK received"
    {
      iic->conset = CON_STOP;
      iic->conclr = CON_SI;
      WAIT_STOP();
      i2c_ack = 0; // show error (WAIT_STOP() sets i2c_ack!)
      break;
    }
  }
  while(--len);
  iic->conset = CON_STOP;
  iic->conclr = CON_SI;
  WAIT_STOP();
}


*/

void iic_start(struct i2c_dev_s *iic, unsigned char devaddr, int rw)
{
  int n, att;
  // bug LBS 1.06.2010
  /*
  I20CONSET=1<<4; // STOP
  I20CONCLR=0x08;
  */
  iic->conset = 1<<4; // STOP
  iic->conclr = 0x08;
  //
  WAIT_STOP(); // wait while STOP transmitted (cleared by LPC HW)
  if(!i2c_ack) return; // bus failure
  /////for(att=0; att<4; ++att) // 4 conseq. attempts
  for(att=0; att<1; ++att) /// DEBUGGGGG
  {
    iic->conset = 1<<5; // START
    WAIT_SI();
    if(iic->stat == 0x08 || iic->stat == 0x10) // START or REP.START is ok
    {
      iic->dat = devaddr | (rw & 1);
      iic->conclr = 0x28;  // clear STA, clear SI
      n=10; while(--n);
      WAIT_SI();
      if(iic->stat == 0x18 || iic->stat == 0x40)
      { // slave addr acknoledged, OK
        i2c_ack = 1; // OK
        return;
      }
    }
    iic->conset = 1<<4; // STOP
    iic->conclr = 0x08; // clear SI
    WAIT_STOP();
  }
  i2c_ack = 0; // no ACK
}


void hw_i2c_read(int ch, int devaddr, unsigned char *buf, unsigned short len)
{

  struct i2c_dev_s *iic = iic_base(ch);

  if(len==0) return;
  iic_start(iic, devaddr, 1);
  if(!i2c_ack) return;
  do
  {
    if (len==1) iic->conclr = 1<<2; // set/clear ACK
    else iic->conset = 1<<2;
    iic->conclr = 0x08; // clear SI
    WAIT_SI();
    if(iic->stat == 0x50 || iic->stat == 0x58) ; // data byte tx ok
    else
    { // bus failure
      iic->conset = 1<<4; // stop
      iic->conclr = 0x08; // clear SI
      WAIT_STOP();
      i2c_ack = 0;
      return;
    }
    *buf++ = iic->dat;
  }
  while(--len);
  iic->conset = 1<<4; // stop
  iic->conclr = 0x08; // clear SI
  WAIT_STOP();
}

void hw_i2c_write(int ch, int devaddr, unsigned char *buf, unsigned short len)
{
  struct i2c_dev_s *iic = iic_base(ch);

  if(len==0) return;
  iic_start(iic, devaddr, 0);
  if(!i2c_ack) return;
  do
  {
    iic->dat = *buf++;
    iic->conclr = 0x08; // clear SI
    WAIT_SI();
    if(iic->stat != 0x28)
    { // anithing except "data byte transmitted, ACK received"
      iic->conset = 1<<4; // stop
      iic->conclr = 0x08; // clear SI
      WAIT_STOP();
      i2c_ack = 0;
      return;
    }
  }
  while(--len);
  iic->conset = 1<<4; // stop
  iic->conclr = 0x08; // clear SI
  WAIT_STOP();
}




void hw_i2c_event(enum event_e event)
{
  if(event == E_INIT)
    hw_i2c_init();
}

#endif // HW_I2C_MODULE


