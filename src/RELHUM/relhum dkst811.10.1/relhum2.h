/*
Relative humidity sensing
SHT1x (FOST02) sensor

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

enum relhum_status_e {
  RH_STATUS_OK     = 0,
  RH_STATUS_FAILED = 1
};

extern enum relhum_status_e rh_status;
extern int rh_real_h;
extern int rh_real_t;

// resets serial interface of the humidity sensor chip, to release shared SDA line
void relhum_cancel(void);
// check safe range, send notifications
void rh_check_status(void);

int relhum_snmp_get(unsigned id, unsigned char *data);
void relhum_init(void);
void relhum_event(enum event_e event);
