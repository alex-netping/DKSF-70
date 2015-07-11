
enum sms_state_e
{
  SMS_STOPPED,
  SMS_START,
  SMS_BREAK_OFF,
  SMS_IGT_CTRL_Z,
  SMS_IGT_SMSO,
  SMS_IGT_CHECK_OFF,
  SMS_IGT_ASSERT,
  SMS_IGT_DEASSERT,
  SMS_IGT_CHECK_ON,
  SMS_INIT_0,
  SMS_INIT_1,
  SMS_INIT_2,
  SMS_INIT_3,
  SMS_INIT_PURGE,
  SMS_IDLE,
  SMS_BUSY_READ,
  SMS_BUSY_SEND,
  SMS_BUSY_GET_INFO,
  SMS_BUSY_USSD,
  SMS_CHECK_SEND_ERR,
  SMS_PURGE,
  SMS_DEBUG_1,
  SMS_DEBUG_2,
  SMS_DEBUG_3,
  SMS_SHUTDOWN_STARTED
} sms_state;

void ignition_sequence(void)
{
  if(ignition_timer) return;
  switch(sms_state)
  {
  case SMS_START:
    /*
    FIO2CLR = 1<<0;
    FIO2DIR |= 1<<0;
    PINSEL4_bit.P2_0 = 0; // P2.0(TXD1) = GPIO
    ignition_timer = 100;
    sms_state = SMS_BREAK_OFF;
    */
    if(gsm_read_cts() == 1)
    { // modem is online, reboot by AT command
      send_printf("at+cfun=1,1\r");
    }
    break;
  case SMS_BREAK_OFF:
    FIO2SET = 1<<0;
    PINSEL4_bit.P2_0 = 2; // P2.0(TXD1) = TXD1
    ignition_timer = 100;
    sms_state = SMS_IGT_CTRL_Z;
    break;
  case SMS_IGT_CTRL_Z:
#warning remove debug
log_printf("sending at CR LF at CR LF Ctl-Z CR LF");
    send_printf("at\r\nat\r\n\x1a\r\n"); // type Ctrl-Z, CRLF
    ignition_timer = 100;
#warning remove debug - testing 'net search' problem on the 51.1.8
    /////sms_state = SMS_IGT_SMSO;
sms_state = SMS_IGT_ASSERT;
    break;
  case SMS_IGT_SMSO:
    send_printf("at^smso\r\n");
    ignition_timer = 200;
    ignition_timeout = 0;
    sms_state = SMS_IGT_CHECK_OFF;
    break;
  case SMS_IGT_CHECK_OFF:
    if(gsm_read_cts() == 0)
    { // modem is powered off (no power or it's in standby)
#warning remove debug
log_printf("cts==0, modem is off");
      gsm_power(1); // switch on supply
      ignition_timer = 3000; // *10ms, 30s pause before switch on
      sms_state = SMS_IGT_ASSERT;
    }
    else if(ignition_timeout > 600) // *10ms
    { // timeout 6s for software poweroff is expired
#warning remove debug
log_printf("cts==1, modem don't go off by s/w command, reboot by emeroff");
      gsm_emeroff(0); // forcefully off or reboot (depends on modem model)
      delay(20);
      gsm_emeroff(1);
      ignition_timer = 3000; // *10ms, 30s pause before switch on
      sms_state = SMS_IGT_ASSERT;
    }
    else
      ignition_timer = 25; // repeat check after 250ms
    break;
  case SMS_IGT_ASSERT:
    gsm_igt(0); // assert IGNITION
    gsm_started = 1;
    ignition_timer = 20; // 200ms
    sms_state = SMS_IGT_DEASSERT;
    break;
  case SMS_IGT_DEASSERT:
    gsm_igt(1); // deassert IGNITION
    ignition_timer = 50; // *10ms
    ignition_timeout = 0;
#warning remove debug - testing 'net search' problem on the 51.1.8
///    sms_state = SMS_IGT_CHECK_ON;
ignition_timer = 301;
sms_state = SMS_INIT_0;
///
    break;
  case SMS_IGT_CHECK_ON:
    if(gsm_read_cts() == 1)
    {
      sms_state = SMS_INIT_0;
    }
    else if(ignition_timeout > 300) // *10ms
    {
      log_printf("Can't get CTS from GSM modem after power-up and ignition! Module is halted.");
      sms_halt_module();
    }
    else
      ignition_timer = 25; // repeat check after 250ms
    break;
  case SMS_INIT_0:
    send_printf((char*)init_sequence_zero);
    ignition_timer = 300; // *10ms = 3s repeat
    break;
  }
}
