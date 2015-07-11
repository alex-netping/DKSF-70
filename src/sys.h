
#include "platform_setup.h"
#ifndef  SYS_H
#define  SYS_H

extern unsigned char phy_link_status; // 0 = link down; 10,100 = link up if 1 phy; bits 0 and 1 = link up if 2 phy in system
extern unsigned char sys_phy_number;
extern unsigned char main_reload; // new params apply request (incl. network setup)
extern unsigned char main_reboot; // reboot request

void word_bit_write(unsigned *addr, unsigned bit_n, int value);

struct sys_setup_s {
  unsigned char mac[6];
  unsigned char ip[4];
  unsigned char gate[4];
  unsigned char mask;
  unsigned char dst; // 8.04.2013 LBS
  unsigned short http_port;

  unsigned char uname[18];
  unsigned char passwd[18];
  unsigned char community_r[18];
  unsigned char community_w[18];
  unsigned char filt_ip1[4];
  unsigned char filt_mask1;
  /*
  unsigned char filt_ip2[4];
  unsigned char filt_mask2;
  unsigned char filt_ip3[4];
  unsigned char filt_mask3;
  */
  unsigned char nf_disable;    // 11.12.2014 dksf 70; repurposed field; no need for init
  unsigned char reserved[9];   // 11.12.2014 dksf 70

  unsigned char trap_ip1[4];
  unsigned char trap_ip2[4];

  unsigned char ntp_ip1[4];
  unsigned char ntp_ip2[4];
  signed   char timezone;

  unsigned char syslog_ip1[4];
  unsigned char syslog_ip2[4];
  unsigned char facility;
  unsigned char severity;

  unsigned short snmp_port; // 11.03.2011 LBS
  unsigned char  notification_email[48];

  unsigned short telnet_port; // 1.11.2012 LBS

  unsigned char hostname[64]; // added here 6.10.2013
  unsigned char contact [64];
  unsigned char location[64];

  unsigned char dns_ip1[4];
  unsigned char dns_ip2[4];
  unsigned char trap_hostname1[64]; // 7.05.2013, added here 6.10.2013
  unsigned char trap_hostname2[64];
  unsigned char ntp_hostname1[64];
  unsigned char ntp_hostname2[64];
  unsigned char syslog_hostname1[64];
  unsigned char syslog_hostname2[64];
};

extern struct sys_setup_s sys_setup;


void load_fw_block(unsigned block_num, unsigned char *data);
void load_mw_block(unsigned block_num, unsigned char *data);
//void calc_snmp_crc(unsigned char* buf,unsigned int len);
void start_fw_update(unsigned crc);
void start_mw_update(void);
void set_sn(unsigned char *sn);
void set_mac(unsigned char *mac);
void set_ip(unsigned char *ip);
void reload_proc(void);
//void reboot_proc(void); // moved to project.c
//int  sys_http_show_sn(unsigned int id, unsigned char *dest);

unsigned print_dev_name_fw_ver(char *dest);

int i2c_accquire(unsigned char who);
void i2c_release(unsigned char who);

void sys_init(void);
void _sys_event(enum event_e event);

#endif

