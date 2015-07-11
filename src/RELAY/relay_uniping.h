/*
v1.1-70
3.06.2013
  completed rewrite based on dksf48
v1.2-52
12.08.2013
  rewrite for dksf 52.7
  added flip command for url (relay.cgi?r2=f), snmp (set mode = -1)
v1.3-52
7.11.2013
  critical bugfix in relay_save_and_log()
v1.4-70
28.07.2014
  extended, json-p compatible url-encoded API
v1.5-70
19.07.2014
 json-p CGI bugfixes
*/

enum relay_mode_e {
  RELAY_MODE_MANUAL_OFF = 0,
  RELAY_MODE_MANUAL_ON  = 1,
  RELAY_MODE_WDOG       = 2,
  RELAY_MODE_SCHED      = 3,
  RELAY_MODE_SCHED_WDOG = 4,
  RELAY_MODE_LOGIC      = 5,
  RELAY_MAX_USED_MODE
};

struct relay_setup_s {
  unsigned char name[32]; // pasc + zterm string
  enum relay_mode_e mode;     // source of control
  unsigned char reserved1;
  unsigned char reserved2;
  unsigned char reset_time;
  char reserved3[28];
};

extern struct relay_setup_s relay_setup[RELAY_MAX_CHANNEL];

int get_relay_state(unsigned ch, int apply_wdog_reset, int apply_forced_reset);

// if any of sockets connected to watchdog wdog_ch has no ac power, function returns 0
//int pingable_by_wdog(unsigned wdog_ch);
void relay_forced_reset(unsigned ch, unsigned reset_time_10_ticks_per_sec, int polarity, char const *via);
void relay_save_and_log(int only_ch, char const *via);

int pingable_by_wdog(unsigned ch); // called from watchdog.c
int relay_is_controlled_by_schedule(unsigned ch); // called from wtimer.c

int relay_snmp_set(unsigned id, unsigned char *data);
int relay_snmp_get(unsigned id, unsigned char *data);

void relay_early_init(void);
void relay_init(void);
void relay_event(enum event_e event);
