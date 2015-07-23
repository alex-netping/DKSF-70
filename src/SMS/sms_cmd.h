/*
v1.5-200
15.08.2011
  "sim busy" error resolved
  two dest phone numbers for sms sending
v1.6-200
13.10.2011
  removed battery stuff (separate module)
  escape quotes in ussd responce
v1.7-200
3.02.2012
  unsigned format %u in termo SMS message now changed to signed %d
v1.8-201
10.04.2012
  decode_ussd() bugfix for ANSI (not UCS) replies
v1.9-201
28.08.2012
  sms_thermo_event() is modified, added return to safe range
  pinger is disabed while powersaving is on
v1.10-213
5.09.2012
 dksf213 support (sans http)
 get_pinger_status()
v1.11-50
22.01.2013
  sms_io_event(), for IO lines 1..4 only
v1.11-200
17.05.2013
  io_cmd() bugfix for LnP command
v1.12-50
7.06.2013
  cmd_ir() added
v1.11-70
  eth link status for dkst70 (two ports)
v1.12-70
29.01.2014
  requesting RH via SMS (command H?)
  requesting T via SMS (command Tn?)
  IR command changed from Tn to Kn
v1.13-70
3.02.2014
  relay mode rewrite, partial port from dksf48
v1.20-48
29.10.2013
  some rewrite of cmd_pwr()
  cmd_pwr_backup() rewrite
v2.0-707
 additions from 707 (reply to calling phone)
v2.1-201
19.03.2014
26.03.2014
  runtime-detected 1 or 2 phy enet link reports
  last_gsm_error output
  periodic sms
v1.14-70
15.05.2014
  sms_relhum_event() added
v2.1-707
22.08.2014
  use of str_escape_for_js_string() in USSD processing
v2.2-48
5.11.2014
  bugfixed double " escaping in ussd
v2.3-70
17.12.2014
  sys_setup.nf_disable adoption
  relay notifications
  sendsms.cgi
  npGsmSendSms snmp api
v2.4-70
10.02.2015
  bugfix in sms_snmp_set(), no returned data
v2.4-70
2.06.2015
  nf_disable in the all sms_xxx_event() routines
v2.5-70
16.06.2015
  cmd_ir() bugfix
  pinger_exec() fix, SMS reqest possible if SMS notif. switched off; powersaving don't stop poll
  changed relay state reply: P1[mode][+/-] to show actual relay state
*/
