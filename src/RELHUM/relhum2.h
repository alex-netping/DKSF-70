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
v3.0
22.06.2015
  multiple 1w sensors
*/

struct relhum_setup_s {
  unsigned char name[32];
  unsigned char ow_addr[8]; // 23.01.2014
  unsigned char rh_high;
  unsigned char rh_low;
  signed   char t_high;
  signed   char t_low;
  unsigned reserved[5];
};

extern struct relhum_setup_s relhum_setup[RELHUM_MAX_CH];

enum relhum_status_e { // this is communication status, not safe range status (which is rh_status_h)
  RH_STATUS_FAILED = 0,
  RH_STATUS_OK    = 1
};

struct relhum_state_s {
  unsigned char rh;
  unsigned char rh_status;
  signed   char t;
  unsigned char t_status;
  unsigned char error;
};

extern struct relhum_state_s relhum_state[RELHUM_MAX_CH];

// check safe range, send notifications
void rh_check_status(unsigned ch);

int relhum_snmp_get(unsigned id, unsigned char *data);
void relhum_init(void);
void relhum_event(enum event_e event);

/////////////// this is legacy shim! remove! ********
extern unsigned rh_real_h, rh_status_h;
extern int rh_real_t;
