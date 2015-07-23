
#include "platform_setup.h"
#include "eeprom_map.h"
#include "plink.h"
#include "bootlink.h"

#include <stdio.h>

__no_init struct bootldr_data_s bootldr_data @ 0x10007F40;

/*
 up to 48.1.7; 70.2.7 located at eeprom 0x3200
const unsigned sys_setup_sign_2 = 0x12a55d97; // with snmp_port aux parameter, with notification_email aux parameter
const unsigned mib2_signature   = 0x33941784; // mibII data in separate eeprom area, legacy
*/
const unsigned sys_setup_sign      = 0x12a53d96;
const unsigned sys_setup_sign2     = 0x12a53d97; // with snmp_port aux parameter
const unsigned sys_setup_sign3     = 0x12a59e98; // with notification_email aux parameter
const unsigned sys_setup_sign4     = 0x12a59e99; // with telnet_port aux parameter
const unsigned sys_setup_sign_mib2 = 0x12a59ea0; // with mib2 variables sysName,sysLocation,sysContact
const unsigned sys_setup_sign_dns  = 0x12a55d98; // dns stuff


unsigned char mac_link_status;
unsigned char phy_link_status;
unsigned char sys_phy_number;
unsigned char main_reload;
unsigned char main_reboot;

__no_init struct sys_setup_s sys_setup;


void word_bit_write(unsigned *addr, unsigned bit_n, int value)
{
  if(value) *addr |=  (1U << bit_n);
  else      *addr &=~ (1U << bit_n);
}

unsigned print_dev_name_fw_ver(char *dest)
{
  return sprintf(dest, "%s v%u.%u.%u.%c-%u", device_name,
    PROJECT_MODEL, PROJECT_VER, PROJECT_BUILD, PROJECT_CHAR, PROJECT_ASSM);
}

/*
SNMP v2.5 FW upload handler
block_num is 0-based!
data points to the block oh FW data received in snmp varbind
*/
void load_fw_block(unsigned block_num, unsigned char *data)
{
  if(block_num >= FIRMWARE_SIZE / FW_UPDATE_BLOCK) return;
  if(block_num == 0) flash_lpc_erase(FW_UPDATE_SECTOR_START, FW_UPDATE_SECTOR_END);
  flash_lpc_write(FIRMWARE_START + block_num * FW_UPDATE_BLOCK, data, FW_UPDATE_BLOCK);
}

void load_mw_block(unsigned block_num, unsigned char *data)
{
  load_fw_block(block_num, data);
}


unsigned short update_fw_crc_calc(unsigned addr, unsigned size)
{
  unsigned block_size, i, j, data, tmp;
  unsigned short crc = 0x4C7F;
  unsigned char buf[64];
  do {
    wdt_reset();
    block_size = size > 64 ? 64 : size;
    size -= block_size;
    // read prepared fw from temp location (from update area)
    util_cpy((unsigned char*)addr, buf, block_size);
    // calc crc
    for (j=0;j<block_size;j++){
      i=0;
      data=buf[j];
      do{
        tmp=(crc ^ data) & 1;
        crc >>= 1;
        if (tmp) {crc ^= 0xA001;}
        data>>=1;
      }while (!(++i & 0x8));
    }
    addr+=64;
  } while(size);
  return crc;
}

void start_fw_update(unsigned npconf_crc)
{
  bootldr_data.crc16 = update_fw_crc_calc(FIRMWARE_START, FIRMWARE_SIZE);
  bootldr_data.signature = BOOTLDR_SIGNATURE_START_UPDATE;
  main_reboot = 1;
}

void start_fw_update_with_ext_crc(unsigned short crc)
{
  bootldr_data.crc16 = crc;
  bootldr_data.signature = BOOTLDR_SIGNATURE_START_UPDATE;
  main_reboot = 1;
}

void start_mw_update(void)
{
  main_reboot = 1;
}

int sys_http_fw_update_set(void) // board-dependant
{
  struct {
    unsigned flags;
    unsigned offset;
    unsigned len;
  } head;
  unsigned char buf[512];
  if(http.post_content_length < sizeof head * 2) return -1; // in this CGI, where is pure hex data, no 'data=' prefix
  http_post_data_part(req, (void*)&head, sizeof head);
  if((head.flags & 0xffff) != 0xdeba) return -9; // check signature
  switch((head.flags >> 16) & 0xff)
  {
  case 0:
    if(head.len > sizeof buf) return -2;
    if(http.post_content_length != (sizeof head + head.len) * 2) return -2; // check POSTed data length
    if(head.offset & 255) return -3; // check block alignment
    if(head.offset > 256*1024 - head.len) return -4; // check block address and size // board-dependant
    util_fill(buf, sizeof buf, 0xff);
    http_post_data_part(req + (sizeof head)*2, buf, head.len);
    if(head.offset == 0)
      flash_lpc_erase(FW_UPDATE_SECTOR_START, FW_UPDATE_SECTOR_END);
    flash_lpc_write(FIRMWARE_START + head.offset, buf, head.len); // board-dependant
    break;
  case 5:
    start_fw_update_with_ext_crc(head.offset & 0xffff); // original fw CRC transfered in offset field
    break;
  }
  http_reply(200,""); // dummy code, this proc slways sends 200 OK
  return 0;
}


// sets SN from snmp request (internal use only, wrong asn.1 4-byte arg)
void set_sn(unsigned char *sn)
{
  if(serial != 0xffffffff) return;
  unsigned char bufTmp[4];
  bufTmp[0] = sn[3];
  bufTmp[1] = sn[2];
  bufTmp[2] = sn[1];
  bufTmp[3] = sn[0];
  flash_lpc_write((unsigned)&serial, bufTmp, 4);
}

void set_mac(unsigned char *mac)
{
  util_cpy(mac, sys_setup.mac, 6);
  EEPROM_WRITE(&eeprom_sys_setup.mac, &sys_setup.mac, 6);
  main_reload = 1;
}

void set_ip(unsigned char *ip)
{
  util_cpy(ip, sys_setup.ip, 4);
  EEPROM_WRITE(&eeprom_sys_setup.ip, &sys_setup.ip, 4);
  main_reload = 1;
}

#warning TODO problematic socet manipulations on reload
void reload_proc(void)
{
  // http HACK!!!
  if (tcp_socket[http.tcp_session].tcp_state!=TCP_LISTEN)
  {
    main_reload = 1; // repeat later
    return;
  }
  // break SSE push connection
  if(sse_sock != 0xff && tcp_socket[sse_sock].tcp_state != TCP_RESERVED)
     tcp_send_flags(TCP_MSK_ACK | TCP_MSK_RST | TCP_MSK_PSH, sse_sock);
#ifdef TCPCOM_MODULE
  if(tcpcom_soc < 0xfe)
   if(tcp_socket[tcpcom_soc].tcp_state != TCP_LISTEN)
     tcp_send_flags(TCP_MSK_ACK | TCP_MSK_RST | TCP_MSK_PSH, tcpcom_soc);
#endif
  // clear and re-init tcp sockets
  // http HACK!!!
  tcp_clear_connection(http.tcp_session);
  tcp_socket[http.tcp_session].used = 0;
  if(sse_sock != 0xff)
  {
    tcp_clear_connection(sse_sock);
    tcp_socket[sse_sock].used = 0;
  }
  http_init();
#ifdef TCPCOM_MODULE
  if(tcpcom_soc < 0xfe)
  {
    tcp_socket[tcpcom_soc].used = 0;
    tcpcom_init();
  }
#endif
#ifdef SENDMAIL_MODULE
  // break sendmail connection
  if(tcp_cli.state != TCPC_IDLE)
    tcp_cli_rst_and_clear();
#endif
  set_hardware_mac_address(sys_setup.mac);
  ip_reload();
  main_reload = 0;
}

void soft_reboot(void)
{
  // reboot after 1s pause for request completition (http session, udp reply etc.)
  static unsigned hard_reboot_time = 0;
  if(hard_reboot_time == 0)
    hard_reboot_time = sys_clock_100ms + 10;
  else
    if(sys_clock_100ms > hard_reboot_time)
      reboot_proc();
  // break SSE push connection
  if(sse_sock != 0xff && tcp_socket[sse_sock].tcp_state != TCP_RESERVED)
  {
    tcp_send_flags(TCP_MSK_ACK | TCP_MSK_RST | TCP_MSK_PSH, sse_sock);
    tcp_clear_connection(sse_sock);
    tcp_socket[sse_sock].used=0;
    sse_sock = 0xff;
  }
#ifdef TCPCOM_MODULE
  // break TCP-COM connection
  if(tcpcom_soc < 0xfe)
  {
    tcp_send_flags(TCP_MSK_ACK | TCP_MSK_RST | TCP_MSK_PSH, tcpcom_soc);
    tcp_clear_connection(tcpcom_soc);
    tcp_socket[tcpcom_soc].used=0;
    tcpcom_soc = 0xff;
  }
#endif
#ifdef SENDMAIL_MODULE
  // break sendmail connection
  if(tcp_cli.state != TCPC_IDLE)
    tcp_cli_rst_and_clear();
#endif
}

int sys_http_show_sn(unsigned int id, unsigned char *dest)
{
  if(serial == 0xffffffff)
  {
    return sprintf((char*)dest, "SN: N/A");
  }
  else
  {
    unsigned d, d3, d2, d1;
    d =  serial & 0x1fffffff;
    d3 = d / 1000000;
    d2 = d / 1000 - d3 * 1000;
    d1 = d - d3 * 1000000 - d2 * 1000;
    return sprintf((char*)dest, "SN: %03u %03u %03u", d3, d2, d1);
  }
}

#undef  PLINK
#define PLINK(dest, var, field) { #field, (unsigned)&((struct var##_s *)0)->field, sizeof var.field }

struct plink_s {
  char *field;
  unsigned short offset;
  unsigned short len;
};
const struct plink_s plink_format[] = {
    PLINK(s, sys_setup, mac), // not presented on http page (dksf51.3+)
    PLINK(s, sys_setup, ip),
    PLINK(s, sys_setup, gate),
    PLINK(s, sys_setup, mask),
    PLINK(s, sys_setup, dst), // 8.04.2013 LBS
    PLINK(s, sys_setup, http_port),

    PLINK(s, sys_setup, uname),
    PLINK(s, sys_setup, passwd),
    PLINK(s, sys_setup, community_r),
    PLINK(s, sys_setup, community_w),
    PLINK(s, sys_setup, filt_ip1),
    PLINK(s, sys_setup, filt_mask1),
    /*
    PLINK(s, sys_setup, filt_ip2),
    PLINK(s, sys_setup, filt_mask2),
    PLINK(s, sys_setup, filt_ip3),
    PLINK(s, sys_setup, filt_mask3),
    */
    PLINK(s, sys_setup, nf_disable), // 11.12.2014
//  PLINK(s, sys_setup, powersaving), // 16.02.2015

    PLINK(s, sys_setup, trap_ip1),
    PLINK(s, sys_setup, trap_ip2),

    PLINK(s, sys_setup, ntp_ip1),
    PLINK(s, sys_setup, ntp_ip2),
    PLINK(s, sys_setup, timezone),

    PLINK(s, sys_setup, syslog_ip1),
    /*
    PLINK(s, sys_setup, syslog_ip2),
    */
    PLINK(s, sys_setup, facility),
    PLINK(s, sys_setup, severity),

    PLINK(s, sys_setup, snmp_port),
    PLINK(s, sys_setup, notification_email),

    PLINK(s, sys_setup, hostname), // since 48.1.8
    PLINK(s, sys_setup, location),
    PLINK(s, sys_setup, contact),

#ifdef DNS_MODULE
    PLINK(s, sys_setup, dns_ip1), // since 48.1.8
//  PLINK(s, sys_setup, dns_ip2),
    PLINK(s, sys_setup, trap_hostname1),
    PLINK(s, sys_setup, trap_hostname2),
    PLINK(s, sys_setup, ntp_hostname1),
    PLINK(s, sys_setup, ntp_hostname2),
    PLINK(s, sys_setup, syslog_hostname1)
//  PLINK(s, sys_setup, syslog_hostname2)
#endif
};

unsigned make_plink(char *s, const struct plink_s *fmt, int fmt_n)
{
  int i;
  char *s0 = s;
  for(i=0;i<fmt_n;++i,++fmt)
  {
    s += sprintf(s, "%s:{offs:%d,len:%d},", fmt->field, fmt->offset, fmt->len);
  }
  return (unsigned)(s-s0);
}

unsigned sys_http_setup_get(unsigned pkt, unsigned more_data)
{
  char buf[1024];
  char *s = buf;

  switch(more_data)
  {
  case 0:
    s += sprintf(s,"var packfmt={");
    s += make_plink(s, plink_format, sizeof plink_format / sizeof plink_format[0]);
    PSIZE(s, sizeof sys_setup);

    s += sprintf(s,"};\r\n");
    tcp_put_tx_body(pkt, (unsigned char*)buf, s - buf);
    return 1;


  case 1:
    s+=sprintf(s, "var data={serial:\""); // from boot sector @ 0x0ffc
    s+=sys_http_show_sn(0, (unsigned char*)s); // ATTN! Update sys_http_setup_get() for serialnum:%u, serial
    s+=sprintf(s, "\",serialnum:%u,", serial);
    PDATA_MAC (s, sys_setup, mac);
    PDATA_IP  (s, sys_setup, ip);
    PDATA_IP  (s, sys_setup, gate);
    PDATA_MASK(s, sys_setup, mask);
    PDATA     (s, sys_setup, dst); // 8.04.2013 LBS
    PDATA     (s, sys_setup, http_port);

    PDATA_PASC_STR(s, sys_setup, uname);
    PDATA_PASC_STR(s, sys_setup, passwd);
    PDATA_PASC_STR(s, sys_setup, community_r);
    PDATA_PASC_STR(s, sys_setup, community_w);

    PDATA_IP  (s, sys_setup, filt_ip1);
    PDATA_MASK(s, sys_setup, filt_mask1);

    ///PDATA_IP  (s, sys_setup, filt_ip2);
    ///PDATA_MASK(s, sys_setup, filt_mask2);
    ///PDATA_IP  (s, sys_setup, filt_ip3);
    ///PDATA_MASK(s, sys_setup, filt_mask3);

    PDATA     (s, sys_setup, nf_disable); // 11.12.2014
//  PDATA(s, sys_setup, powersaving); // 16.02.2015

    PDATA_IP  (s, sys_setup, trap_ip1);
    PDATA_IP  (s, sys_setup, trap_ip2);

    PDATA_IP  (s, sys_setup, ntp_ip1);
    PDATA_IP  (s, sys_setup, ntp_ip2);
    PDATA_SIGNED(s, sys_setup, timezone); // 20.09.2011

    PDATA_IP  (s, sys_setup, syslog_ip1);
    ///PDATA_IP  (s, sys_setup, syslog_ip2);
    PDATA     (s, sys_setup, facility);
    PDATA     (s, sys_setup, severity);

    PDATA     (s, sys_setup, snmp_port);
    PDATA_PASC_STR(s, sys_setup, notification_email);
    tcp_put_tx_body(pkt, (unsigned char*)buf, s - buf);
    return 2;

  case 2:
#if HTTP_INPUT_DATA_SIZE<2400
#warning "Attn! big POST in settings.html, req array in http2.c has to be 2400 bytes or so!"
#endif
    PDATA_PASC_STR(s, sys_setup, hostname),
    PDATA_PASC_STR(s, sys_setup, location),
    PDATA_PASC_STR(s, sys_setup, contact),

#ifdef DNS_MODULE
    PDATA_IP(s, sys_setup, dns_ip1);
//  PDATA_IP(s, sys_setup, dns_ip2);
    PDATA_PASC_STR(s, sys_setup, trap_hostname1);
    PDATA_PASC_STR(s, sys_setup, trap_hostname2);
    PDATA_PASC_STR(s, sys_setup, ntp_hostname1);
    PDATA_PASC_STR(s, sys_setup, ntp_hostname2);
    PDATA_PASC_STR(s, sys_setup, syslog_hostname1);
//  PDATA_PASC_STR(s, sys_setup, syslog_hostname2);
#endif
    --s; // del last ','
    *s++ = '}'; *s++ = ';';
    s += sprintf(s, "data_rtc=%u;", (unsigned)(local_time >> 32) - (unsigned)TIMEBASE_DIFF );
    s += sprintf(s, "uptime_100ms=%u;", sys_clock_100ms);
    tcp_put_tx_body(pkt, (unsigned char*)buf, s - buf);
    return 0;

  default:
    return 0;
  } // switch
}

unsigned sys_http_devname_get(unsigned pkt, unsigned more)
{
  char buf[384];
  char *s = buf;
  sys_setup.hostname[sizeof sys_setup.hostname-1] = 0; // zterm prot-n
  sys_setup.location[sizeof sys_setup.location-1] = 0;
  s += sprintf(s, "var devname='%s';\n"
        "var fwver='v%u.%u.%u.%c-%u';\n"
        "var hwmodel=%u;"
        "var hwver=%u;"
        "var nf_disable=%u;" // 11.12.2014
        "var sys_name=\"%s\";"
        "var sys_location=\"",
        device_name,
        PROJECT_MODEL, PROJECT_VER, PROJECT_BUILD, PROJECT_CHAR, PROJECT_ASSM,
        PROJECT_MODEL,
        proj_hardware_detect(),
        sys_setup.nf_disable, // 11.12.2014
        sys_setup.hostname + 1 // hostname need no escaping
        );
  s += str_escape_for_js_string(s, (char*)sys_setup.location + 1, 64);
  *s++ = '"'; *s++ = ';';
  tcp_put_tx_body(pkt, (void*)buf, s - buf);
  return 0;
}

unsigned sys_http_devname_menu_get(unsigned pkt, unsigned more)
{
  sys_http_devname_get(pkt, more);
  // tail-pumping of menu.js
  // semi-hack
  extern struct page_s _htmlhdr_menu_js;
  http.page = &_htmlhdr_menu_js;
  return 1;
}

int sys_http_setup_set(void)
{
  unsigned char  old_mac[6];
  unsigned char  old_ip[4];
  unsigned short old_http_port;

  memcpy(old_mac, sys_setup.mac, 6);
  memcpy(old_ip, sys_setup.ip, 4);
  old_http_port = sys_setup.http_port;

  http_post_data((void *)&sys_setup, sizeof sys_setup);

  memcpy(sys_setup.mac, old_mac, 6); // 31.05.2010 mac is removed from settings.html page, mask out and restore it

#ifdef DNS_MODULE
  dns_resolve(eeprom_sys_setup.trap_hostname1, sys_setup.trap_hostname1);
  dns_resolve(eeprom_sys_setup.trap_hostname2, sys_setup.trap_hostname2);
  dns_resolve(eeprom_sys_setup.ntp_hostname1, sys_setup.ntp_hostname1);
  dns_resolve(eeprom_sys_setup.ntp_hostname2, sys_setup.ntp_hostname2);
  dns_resolve(eeprom_sys_setup.syslog_hostname1, sys_setup.syslog_hostname1);
#endif

  unsigned cpsr = proj_disable_interrupt();
  EEPROM_WRITE(&eeprom_sys_setup, &sys_setup, sizeof sys_setup);
  proj_restore_interrupt(cpsr);

  const static char loc[] = "/settings.html";  // 26.01.2011 remove ip and port from redirect if not needed
  if(memcmp(sys_setup.ip, old_ip, 4) != 0
  || sys_setup.http_port != old_http_port )
    http_redirect_to_addr(sys_setup.ip, sys_setup.http_port, (void*)loc);
  else
    http_redirect((void*)loc);
  return 0;
}

int sys_http_ip_set(void)
{
  sys_http_setup_set();
  main_reload = 1;
  return 0;
}

int sys_http_reboot_set(void)
{
  http_redirect("/index.html");
  main_reboot = 1;
  return 0;
}

// ATTN! Update sys_http_show_sn() for web-set SN!
// Update sys_http_setup_get() for serialnum:%u, serial
// Update index.html
int sys_http_serial_set(void)
{
  if(serial == 0xffffffff)
  {
    unsigned ser = tcp_socket[http.tcp_session].ack; // it's, essentially, random TCP SEQ from PC
    ser &= 0x1fffffff; // trim to decimal 1 mln.
    flash_lpc_write((unsigned)&serial, (void*)&ser, 4);
    memcpy(sys_setup.mac+2, &serial, 4);
    EEPROM_WRITE(eeprom_sys_setup.mac, sys_setup.mac, sizeof sys_setup.mac);
    main_reboot = 1;
  }
  http_redirect("/index.html");
  return 0;
}

HOOK_CGI(devname,   (void*)sys_http_devname_get,  mime_js,  HTML_FLG_GET | HTML_FLG_NOCACHE );
HOOK_CGI(devname_menu, (void*)sys_http_devname_menu_get,  mime_js,  HTML_FLG_GET | HTML_FLG_NOCACHE );
HOOK_CGI(setup_get, (void*)sys_http_setup_get,    mime_js,  HTML_FLG_GET | HTML_FLG_NOCACHE );
HOOK_CGI(setup_set, (void*)sys_http_setup_set,    mime_js,  HTML_FLG_POST );
HOOK_CGI(ip_set,    (void*)sys_http_ip_set,       mime_js,  HTML_FLG_POST );
HOOK_CGI(reboot,    (void*)sys_http_reboot_set,   mime_js,  HTML_FLG_POST );
HOOK_CGI(update_set,(void*)sys_http_fw_update_set,mime_js,    HTML_FLG_POST );
HOOK_CGI(newserial, (void*)sys_http_serial_set, mime_js,  HTML_FLG_POST );


void sys_setup_reset(void)
{
#if PROJECT_CHAR == 'E'
  log_printf("System settings will be reset to defaults!");
#else
  log_printf("Выполняется сброс настроек прибора к начальному стостоянию!");
#endif
  unsigned cpsr = proj_disable_interrupt();
  #define ss sys_setup
  util_fill((void*)&sys_setup, sizeof sys_setup, 0);
  ss.mac[0] = 0x00; ss.mac[1] = 0xA2; util_cpy((void*)&serial, ss.mac+2, 4);
  ss.ip[0] = 192; ss.ip[1] = 168; ss.ip[2] = 0; ss.ip[3] = 100;
  ss.mask = 24;
  ss.http_port = 80;
  memcpy(ss.uname,  "\x05visor", 1+5);    // double-check both lengths!
  memcpy(ss.passwd, "\x04ping",  1+4); // double-check both lengths!
  memcpy(ss.community_r, "\x06SWITCH", 1+6);
  memcpy(ss.community_w, "\x06SWITCH", 1+6);
  ss.timezone = 3;   // MSK
  ss.facility = 16;  // 16 = local use 0
  ss.severity = 5;   // 5 = notice: normal but significant condition
#ifdef DNS_MODULE
  memcpy(ss.ntp_hostname1, (void*)"\x0Entp.netping.ru", 1+14); // 12.11.2014
#endif
  EEPROM_WRITE(&eeprom_sys_setup, &ss, sizeof ss);
  EEPROM_WRITE(&eeprom_sys_setup_signature, &sys_setup_sign, 4);
  #undef ss
  proj_restore_interrupt(cpsr);
}

/*
 up to 48.1.7; 70.2.7
void sys_setup_2_reset(void)
{
  sys_setup.snmp_port = 161;
  util_fill(sys_setup.notification_email, sizeof sys_setup.notification_email, 0);
  EEPROM_WRITE(&eeprom_sys_setup, &sys_setup, sizeof eeprom_sys_setup);
  EEPROM_WRITE(&eeprom_sys_setup_signature2, &sys_setup_sign_2, sizeof eeprom_sys_setup_signature2);
}

void prj_mib2_reset(void)
{
  char z = 0;
  EEPROM_WRITE(eeprom_sysContact, &z, 1);
  EEPROM_WRITE(eeprom_sysName, &z, 1);
  EEPROM_WRITE(eeprom_sysLocation, &z, 1);
  EEPROM_WRITE(&eeprom_mib2_signature, &mib2_signature, sizeof eeprom_mib2_signature);
}

void sys_init(void)
{
  unsigned sign, sign2;
  EEPROM_READ(&eeprom_sys_setup_signature, &sign, sizeof sign);
  if(sign != sys_setup_sign) sys_setup_reset();
  else EEPROM_READ(&eeprom_sys_setup, &sys_setup, sizeof sys_setup);

  EEPROM_READ(&eeprom_sys_setup_signature2, &sign2, sizeof sign2);
  if(sign2 != sys_setup_sign_2) sys_setup_2_reset();

  EEPROM_READ(&eeprom_mib2_signature, &sign, sizeof sign);
  if(sign != mib2_signature) prj_mib2_reset();
}
*/


void sys_setup_reset_snmp_port_only(void)
{
  sys_setup.snmp_port = 161;
  EEPROM_WRITE(&eeprom_sys_setup.snmp_port, &sys_setup.snmp_port, sizeof eeprom_sys_setup.snmp_port);
  EEPROM_WRITE(&eeprom_sys_setup_signature2, &sys_setup_sign2, sizeof eeprom_sys_setup_signature2);
}

void sys_setup_reset_notification_email_only(void)
{
  util_fill((void*)sys_setup.notification_email, sizeof sys_setup.notification_email, 0);
  EEPROM_WRITE(eeprom_sys_setup.notification_email, sys_setup.notification_email, sizeof eeprom_sys_setup.notification_email);
  EEPROM_WRITE(&eeprom_sys_setup_signature3, &sys_setup_sign3, sizeof eeprom_sys_setup_signature3);
}

void sys_setup_reset_telnet_port_only(void)
{
  sys_setup.telnet_port = 23;
  EEPROM_WRITE(&eeprom_sys_setup.telnet_port, &sys_setup.telnet_port, sizeof eeprom_sys_setup.telnet_port);
  EEPROM_WRITE(&eeprom_sys_setup_signature4, &sys_setup_sign4, sizeof eeprom_sys_setup_signature4);
}

void sys_setup_reset_mib2_only(void)
{
  util_fill(sys_setup.hostname, sizeof sys_setup.hostname * 3, 0); // 3 fields
  EEPROM_WRITE(&eeprom_sys_setup.hostname, sys_setup.hostname, sizeof sys_setup.hostname * 3); // 3 fields
  EEPROM_WRITE(&eeprom_sys_setup_signature_mib2, &sys_setup_sign_mib2, sizeof eeprom_sys_setup_signature_mib2);
}

void sys_setup_reset_dns_only(void)
{
  const unsigned size = (char*)sys_setup.syslog_hostname2 + sizeof sys_setup.syslog_hostname2 - (char*)sys_setup.dns_ip1;
  memset(sys_setup.dns_ip1, 0, size);
  EEPROM_WRITE(eeprom_sys_setup.dns_ip1, &sys_setup.dns_ip1, size);
  EEPROM_WRITE(&eeprom_sys_setup_signature_dns, &sys_setup_sign_dns, sizeof eeprom_sys_setup_signature_dns);
}

void sys_init(void)
{
  unsigned sign;
  EEPROM_READ(&eeprom_sys_setup_signature, &sign, sizeof sign);
  if(sign != sys_setup_sign) sys_setup_reset();
  else EEPROM_READ(&eeprom_sys_setup, &sys_setup, sizeof sys_setup);

  EEPROM_READ(&eeprom_sys_setup_signature2, &sign, sizeof sign);
  if(sign != sys_setup_sign2) sys_setup_reset_snmp_port_only();

  EEPROM_READ(&eeprom_sys_setup_signature3, &sign, sizeof sign);
  if(sign != sys_setup_sign3) sys_setup_reset_notification_email_only();

  EEPROM_READ(&eeprom_sys_setup_signature4, &sign, sizeof sign);
  if(sign != sys_setup_sign4) sys_setup_reset_telnet_port_only();

  EEPROM_READ(&eeprom_sys_setup_signature_mib2, &sign, sizeof sign);
  if(sign != sys_setup_sign_mib2) sys_setup_reset_mib2_only();

  EEPROM_READ(&eeprom_sys_setup_signature_dns, &sign, sizeof sign);
  if(sign != sys_setup_sign_dns) sys_setup_reset_dns_only();
}


// concurrent use of external i2c bus for different modules and not-i2c-compliant sensors

unsigned char i2c_bus_semaphore;

int i2c_accquire(unsigned char who)
{
  if(i2c_bus_semaphore == who) return 1; // LBS 5.07.2010
  if(i2c_bus_semaphore != 0) return 0;
  i2c_bus_semaphore = who;
  return 1;
}

void i2c_release(unsigned char who)
{
  if(i2c_bus_semaphore == who) i2c_bus_semaphore = 0;
}

/*
__monitor void blink_parameters_reset(void) // DKST 160.1.4
{
  PWM1TCR = 2; // stop, zero TC&PC
  PWM1MCR = 0; // disable any match action
  PWM1PR = SYSTEM_CCLK / 5; // PCLK = CCLK = 44 236 800 Hz -> PWM clock ~ 2.5Hz
  PWM1TCR = 1; // enable count in Timer mode
  FIO0CLR = 1<<16;
  while(PWM1TC < 7*5)
  {
    led_control(CPU_LED_N, PWM1TC&1);
  }
}
*/

void _sys_event(enum event_e event)
{
  switch(event)
  {
  case E_RESET_PARAMS:
    sys_setup_reset();
    /*
      up to 48.1.7; 70.2.7
    sys_setup_2_reset();
    prj_mib2_reset();
    */
    sys_setup_reset_snmp_port_only();
    sys_setup_reset_notification_email_only();
    sys_setup_reset_telnet_port_only();
    sys_setup_reset_mib2_only();
    sys_setup_reset_dns_only();
    break;
  }
}

