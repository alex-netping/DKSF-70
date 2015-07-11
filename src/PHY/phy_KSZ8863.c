/*
v1.5 by LBS
4.02.2011
  LINK_STATUS_LED_INIT() is added
v1.6
21.11.2011
  pull-down of unused RX_ER pin in RMII
v1.7
  monitoring of Link on poth PHYs
v1.8
19.03.2014
  sys_phy_number
  don't scan if no 8863 detected
*/

#include "platform_setup.h"


/*
void phy_ksz8863rll_init(void)
{

  // LBS 21.11.2011 - pull down RX_ER RMII pin (source of rx errors in EMI conditions)
 // PINMODE2_bit.P1_14 = 3;                           // ENET_RX_ER  (8041 ISO)

}
*/
volatile unsigned char phy_ksz8863_detected;
unsigned rr1,rr2;

void phy_ksz8863_exec(void)
{
  static int skip = 0;
  if(++skip < 253) return;
  skip = 0;

  unsigned link;
  rr1 = lpc23xx_smi_read(0x0101);
  rr2 = lpc23xx_smi_read(0x0201);
  link =  (rr1 >> 2) & 1;
  link |= ((rr2 >> 2) & 1) << 1;
  phy_link_status = link;
}

void phy_ksz8863_init(void)
{
  lpc23xx_smi_init();
  if(lpc23xx_smi_read(0x0102) == 0x0022
  && lpc23xx_smi_read(0x0103) == 0x1430)
  {
    phy_ksz8863_detected = 1;
    sys_phy_number = 2;
  }
}

void phy_ksz8863_event(enum event_e event)
{
 if(event == E_EXEC)
   if(phy_ksz8863_detected)
     phy_ksz8863_exec();
}

#warning "TODO - 8863 powersaving"
