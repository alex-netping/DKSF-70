/*
v1.1
3.12.2012
  dksf213 support, spi_eeprom_read_id()
v1.2-48
4.03.2013
  dksf48 support
*/

#include "platform_setup.h"

#ifdef SPI_EEPROM_MODULE


void eeprom_chip_init(void);

#if PROJECT_MODEL==35 || PROJECT_MODEL==253 || PROJECT_MODEL == 200 || PROJECT_MODEL==213

#define CS_ON()  (IO0CLR=1<<16)  // board dependant
#define CS_OFF() do { IO0SET=1<<16; delay_us(1); } while(0)  // board dependant

void spi_eeprom_init(void)    // board dependant
{
  CS_OFF();
  IO0DIR |= 1<<16;
  PINSEL1_bit.P0_16 = 0;    // CS pin for SPI EEPROM - GPIO

  PCONP |= 1<<8;            // power on spi
  for(int i=0;i<50;++i);    // some delay

  S0SPCR = (1<<5);          // SPI mode 0, Master, MSB first, 8 bit, Int disabled
  S0SPINT = 1;              // clear interrupt

  PCLKSEL0_bit.PCLK_SPI = 1;    // SPI PCLK == CCLK (1:1)
  S0SPCCR =  (SYSTEM_CCLK / 1800000) + 1;   // ~ 1.8MHz SPI CLK
  PINSEL0_bit.P0_15 = 3;        // SCK pin function
  PINSEL1_bit.P0_17 = 3;        // MISO function
  PINSEL1_bit.P0_18 = 3;        // MOSI function
  // запретить подтяжку
  PINMODE0_bit.P0_15 = 2;
  PINMODE1_bit.P0_16 = 2;
  PINMODE1_bit.P0_17 = 2;
  PINMODE1_bit.P0_18 = 2;

  eeprom_chip_init();
}

void spi_eeprom_restore_init(void)
{
  // восстановить подтяжку
  PINMODE0_bit.P0_15 = 2;
  PINMODE1_bit.P0_16 = 2;
  PINMODE1_bit.P0_17 = 2;
  PINMODE1_bit.P0_18 = 2;
  // восстановить default gpio
  unsigned q = proj_disable_interrupt();
  IO0DIR &=~ (1<<16);           // CS pin
  proj_restore_interrupt(q);
  PINSEL0_bit.P0_15 = 0;        // SCK pin -> GPIO
  PINSEL1_bit.P0_17 = 0;        // MISO pin -> GPIO
  PINSEL1_bit.P0_18 = 0;        // MOSI pin -> GPIO
}

inline void spi_write(unsigned char b)
{
  S0SPDR = b;
  do {} while(S0SPSR_bit.SPIF == 0);
}

inline unsigned char spi_read(void)
{
  S0SPDR = 0;
  do {} while(S0SPSR_bit.SPIF == 0);
  return S0SPDR;
}

#elif PROJECT_MODEL == 48 || PROJECT_MODEL == 70 || PROJECT_MODEL == 71

#define EEPROM_SCK  0,15
#define EEPROM_CS   0,16
#define EEPROM_MISO 0,17
#define EEPROM_M0SI 0,18

#define CS_ON()  pinclr(EEPROM_CS)
#define CS_OFF() pinset(EEPROM_CS)


inline void spi_write(unsigned char b)
{
  SSP0DR = b;
  do {} while(SSP0SR_bit.BSY);
  unsigned dummy = SSP0DR; // LPC17xx remove rx word from fifo!
}

inline unsigned char spi_read(void)
{
  SSP0DR = 0;  do {} while(SSP0SR_bit.BSY);
  return SSP0DR;
}

void spi_eeprom_init(void)     // board dependant
{
  CS_OFF();
  pindir(EEPROM_CS,  1);

  pindir(EEPROM_SCK, 1);
  pindir(EEPROM_M0SI,1);

  PCONP_bit.PCSSP0 = 1;     // power on spi
  for(int i=0;i<50;++i);    // some delay

  SSP0CR0 = 7<<0;           // 8 bit, SPI protocol, SPI mode 0, clock = prescaled : 1
  SSP0CPSR = (SYSTEM_CCLK / 1800000) + 1;   // ~ 1.8MHz SPI CLK
  SSP0CR1 = 1<<1;           // master mode, enable ssp

  IOCON_P0_15_bit.FUNC = 2;     // SSP0 SCK
  IOCON_P0_17_bit.FUNC = 2;     // SSP0 MISO
  IOCON_P0_18_bit.FUNC = 2;     // SSP0 MOSI

  // 25LC512 wake-up from powerdown
  delay(2);
  CS_ON();
  spi_write(0xab); // RDID command (must be first)
  spi_write(0x55); // dummy addr
  spi_write(0x55);
  spi_read();
  spi_read();
  unsigned id = spi_read();
  CS_OFF();
  if(id != 0x29) for(;;); // panic
  eeprom_chip_init();
}

#else
#error " undefined SPI EEPROM pins!"
#endif


int eeprom_is_busy()
{
  CS_ON();
  spi_write(5); // Read Status command
  int d = spi_read();
  CS_OFF();
  return d & 1; // Write In Progress bit
}

void eeprom_chip_init(void)
{
  while(eeprom_is_busy()) {}
  CS_ON();
  spi_write(6); // write enable command
  CS_OFF();
  CS_ON();
  spi_write(1); // write status cmd
  spi_write(0); // disable write protection
  CS_OFF();
}

void spi_eeprom_read(unsigned addr, void *buf, unsigned len)
{
  if(len==0) return;
  while(eeprom_is_busy()) {}
  CS_ON();
  spi_write(3);
  spi_write(addr >> 8);
  spi_write(addr & 0xff);
  char *p = buf;
  do {
    *p++ = spi_read();
  } while(--len);
  CS_OFF();
}

void start_write(unsigned addr)
{
  while(eeprom_is_busy()) {}
  CS_ON();
  spi_write(6); // write enable command
  CS_OFF();
  CS_ON();
  spi_write(2); // write  cmd
  spi_write(addr >> 8);
  spi_write(addr & 0xff);
}

void spi_eeprom_write(unsigned addr, void *buf, unsigned len)
{
  if(len==0) return;
  start_write(addr);
  char *p = buf;
  do {
    spi_write(*p++);
    ++addr;
    --len;
    if((addr & (EEPROM_PAGE_SIZE-1)) == 0)
    {
      if(len)
      {
        CS_OFF(); // write page
        start_write(addr); // start next page
      }
    }
  } while(len);
  CS_OFF(); // write last page
}

unsigned spi_eeprom_read_id(void) // LBS 3.12.2012
{
  while(eeprom_is_busy()) {}
  CS_ON();
  spi_write(0xab); // RDID command
  spi_write(0x55); // dummy addr
  spi_write(0x55);
  unsigned d = spi_read();
  CS_OFF();
  return d;
}

#endif // SPI_EEPROM_MODULE




