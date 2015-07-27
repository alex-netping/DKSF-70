#include "platform_setup.h"


int snmp_data_handler(unsigned pdu_mode, unsigned short id, unsigned char *p)
{
  if((id & 0xfff0) == 0xebb0)
  {
    switch(pdu_mode)
    {
    case 0xA0:
    case 0xA1:
      return reboot_snmp_get(id, p);
    case 0xA3:
      return reboot_snmp_set(id, p);
    }
  }

  if((id & 0xff00) == 0x0300)
  {
    switch(pdu_mode)
    {
    case 0xA0:
    case 0xA1:
      mibii_get_handler(id, 0);
      return 0;
    case 0xA3:
      return mibii_set_handler(id, p);
    }
  }

#ifdef RELAY_MODULE
  if((id & 0xff00) == 0x5500) // Relay
  {
    switch(pdu_mode)
    {
    case 0xA0:
    case 0xA1:
      return relay_snmp_get(id, p);
    case 0xA3:
      return relay_snmp_set(id, p);
    }
  }
#endif

#ifdef IR_MODULE
  if((id & 0xff00) == 0x7900) // IR
  {
    switch(pdu_mode)
    {
    case 0xA0:
    case 0xA1:
      return ir_snmp_get(id, p);
    case 0xA3:
      return ir_snmp_set(id, p);;
    }
  }
#endif

#ifdef CUR_DET_MODULE
  if((id & 0xff00) == 0x8300) // CURDET
  {
    switch(pdu_mode)
    {
    case 0xA0:
    case 0xA1:
      return curdet_snmp_get(id, p);
    case 0xA3:
      return curdet_snmp_set(id, p);
    }
  }
#endif

# ifdef  RELHUM_MODULE
  if((id & 0xff00) == 0x8400) // RELHUM
  {
    switch(pdu_mode)
    {
    case 0xA0:
    case 0xA1:
      return relhum_snmp_get(id, p);
    case 0xA3:
      return SNMP_ERR_READ_ONLY;
    }
  }
# endif

#ifdef TERMO_MODULE
  if((id & 0xff00) == 0x8800) // TERMO
  {
    switch(pdu_mode)
    {
    case 0xA0:
    case 0xA1:
      return termo_snmp_get(id, p);
    case 0xA3:
      return SNMP_ERR_READ_ONLY;
    }
  }
#endif

#ifdef IO_MODULE
  if((id & 0xff00) == 0x8900) // IO
  {
    switch(pdu_mode)
    {
    case 0xA0:
    case 0xA1:
      return io_snmp_get(id, p);
    case 0xA3:
      return io_snmp_set(id, p);
    }
  }
#endif

#ifdef SMS_MODULE
  if((id & 0xff00) == 0x3800) // GSM
  {
    switch(pdu_mode)
    {
    case SNMP_PDU_GET:
    case SNMP_PDU_GET_NEXT:
      return sms_snmp_get();
    case SNMP_PDU_SET:
      return sms_snmp_set(id, p);
    }
  }
#endif

#ifdef SMOKE_MODULE
  if((id & 0xff00) == 0x8200)
  {
    switch(pdu_mode)
    {
    case SNMP_PDU_GET:
    case SNMP_PDU_GET_NEXT:
      return smoke_snmp_get();
    case SNMP_PDU_SET:
      return smoke_snmp_set();
    }
  }
#endif

  return SNMP_ERR_NO_SUCH_NAME;
}


void init_basic_modules(void)
{
  delay_init();
  io_hardware_init(); // fast restore of IO lines!
  proj_hardware_detect();
  led_init();
  spi_eeprom_init();
}

void init_modules(void)
{
  relay_init();
  ntp_init(); // before log - 8.04.2013
  log_init();
  mac236x_init();
  phy_ksz8863_init();
  nic_init();
  ip_init();
  icmp_init();
  udp_init();
  dns_init();
  tcp_init();
  http_init();
  ping_init();
  watchdog_init();
  wtimer_init();
  io_init();
  swi2c_init();
  termo_init();
  ir_init();
#if PROJECT_MODEL == 70
  uart_init();
  tcpcom_init();
  sms_init();
#endif
  curdet_init();
  smoke_init();
  setter_init();
  logic_init();
  ow_init();
  relhum_init();
  pwrmon_init();
  sendmail_init();
  notify_init();
}


void system_event(enum event_e event, unsigned evdata)
{
  led_event(event);
  _sys_event(event);
//  mac236x_event(event); // it's empty
  phy_ksz8863_event(event);
  nic_event(event);
  ip_event(event);
  ip_event(event);
  udp_event(event);
  tcp_event(event);
  dns_event(event);
  http_event(event, evdata);
  ntp_event(event);
  //icmp_event(event); // it's empty
  ping_event(event);
  watchdog_event(event);  // wdog only
  wtimer_event(event);
  io_event(event);
  termo_event(event);
  relay_event(event);
  ir_event(event);
#if PROJECT_MODEL == 70
  uart_event(event);
  tcpcom_event(event, evdata);
  sms_event(event);
#endif
  curdet_event(event);
  smoke_event(event);
  logic_event(event);
  setter_event(event);
  ow_event(event);
  relhum_event(event);
  sendmail_event(event);
  notify_event(event);
}

