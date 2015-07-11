/*
logic v2
v2.0.0
15.08.2011
v2.1.0
23.08.2011
v2.2-52
28.05.2012
  IR command
v2.3-60
15.06.2012
  cur_loop sensor
v2.4-50
21.11.2012
  reset signal actions
  snmp setter integration
v2.5-50
2.04.2013
  modified control of curdet power
v2.4-60
21.05.2013
  dns integration
v2.6-50
21.06.2013
  LOGIC_IO_LINES_NUMBER def in logic_def.h and it's check in logic.c, used in io web page
v2.7-70
4.07.2013
  ping v2 adopted
v2.7-60
12.08.2013
  bugfix in logic_pinger_http_set_data(), wrong macro
  own dns init in logic_pinger_init()
v2.8-52
26.09.2013
  bugfix: ping_state[n].max_retry was not set
v2.9-48
30.10.2013
  merge with dksf50, incl. usable http stuff
  rewrite of strtup and RESET signal support
  DNS compatibility
v2.10-70
11.06.2014
  RH in termostat
  flip support
*/


#ifndef _LOGIC_H_
#define _LOGIC_H_

#define LOGIC_MAX_RULES 8
#define TSTAT_MAX_CHANNEL 2

enum logic_run_state_e {
  LOGIC_BOOT_DELAY,
  LOGIC_RESET_ON,
  LOGIC_RESET_OFF,
  LOGIC_PAUSE,
  LOGIC_RUN
};

struct logic_setup_s {
  unsigned char flags;
  unsigned char input;
  unsigned char condition;
  unsigned char action;
  unsigned char output;
  unsigned char reserved1[3];
  unsigned char reserved2[8];
};

#define RULE_ACTIVE  0x01
#define RULE_TRIGGER 0x02

extern struct logic_setup_s logic_setup[LOGIC_MAX_RULES];

struct tstat_setup_s {
  signed char setpoint;
  signed char hyst;
  unsigned char sensor_no; // zero-based
  char reserved[5];
};

extern unsigned char logic_flags;

#define LOGIC_RUNNING 0x80

struct logic_pinger_setup_s {
  union {
    unsigned char  ip[4];
    unsigned       ip32;
  };
  unsigned char  reserved1[4];
  unsigned short period;  // s
  unsigned short timeout; // ms
  unsigned char  reserved2[48];
#ifdef DNS_MODULE
  unsigned char  hostname[64];
#endif
};

struct logic_pinger_state_s {
  systime_t next_time;
  unsigned char attempt_counter;
  unsigned char result;
};

#define LOGIC_MAX_PINGER 2
#define LOGIC_PING_ATTEMPTS 8

extern struct logic_pinger_setup_s logic_pinger_setup[LOGIC_MAX_PINGER];
extern struct logic_pinger_state_s logic_pinger_state[LOGIC_MAX_PINGER];


extern unsigned short logic_relay_output;
extern unsigned short logic_io_output;


void logic_init(void);
void logic_event(enum event_e event);

#endif // _LOGIC_H_
