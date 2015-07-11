/*
Relative humidity sensing
SHT1x (FOST02) sensor, 1W sensor since v2.0

P.Lyubasov
v1.1
20.03.2010
v1.2
5.07.2010
 bug if below -40C corrected
v1.3
14.02.2011
  minor timing
v1.4
17.04.2012
  cosmetic rewrite and relhum_cancel()
v1.5
11.11.2012
  RH safe range notification, parameters
v1.5-60
26.04.2013
  cpu unload
v2.0-70
24.01.2014
  1w version
  driver moved to ow.c
v2.1-70
  hr_status_check() call moved from ow_scan() in ow.c to rh_exec()
v2.2-70
15.05.2014
  notify support
v2.3-70
30.07.2014
  url-encoded api (json-p)
v2.4-70
24.11.2014
  JSON-P returns relhum_result(...) instead of rh_result(...)
*/

#define RH_I2C_ADDR_BYTE  0x40   // Si7005 chip

struct relhum_setup_s {
  unsigned char rh_high;
  unsigned char rh_low;
  unsigned char flags;
  unsigned char reserved0;
  unsigned char ow_addr[8]; // 23.01.2014
};

extern struct relhum_setup_s relhum_setup;

enum relhum_status_e { // this is communication status, not safe range status (which is rh_status_h)
  RH_STATUS_FAILED = 0,
  RH_STATUS_OK    = 1
};

extern enum relhum_status_e rh_status; // status of communication to sensor
extern unsigned char rh_status_h; // 0-3 status of fail, below, in, above safe range
extern int rh_real_h;
extern int rh_real_t;
extern int rh_real_t_100; // *100

// resets serial interface of the humidity sensor chip, to release shared SDA line
void relhum_cancel(void);
// check safe range, send notifications
void rh_check_status(void);

int relhum_snmp_get(unsigned id, unsigned char *data);
void relhum_init(void);
void relhum_event(enum event_e event);
