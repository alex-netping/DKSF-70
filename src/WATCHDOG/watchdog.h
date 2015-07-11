/*
v2.0
25.03.2013
  rewrite, external relay logic, smart ping module
v2.1-48
15.04.2013
  rewrite for dksf48
v2.2-52
12.08.2013
  dns integration
v2.3-52/201
3.09.2014
  saving of copy of channel name to relay_setup[].name
*/

#ifndef WATCHDOG_H
#define WATCHDOG_H

enum pwr_reset_logic_mode {
    A_and_B_and_C = 0, // сброс питания  в случае, если недоступен любой из адресов (A)(B)(C).
    A_or_B_or_C,       // сброс питания в случае одновременной не доступности всех Ip-адресов
    A_or_B_and_C,      // сброс питания в случае, если одновременно недоступен адрес (А) и недоступен любой из адресов (B) или (C)
    A_not_B_or_C       // сброс питания в случае, если недоступен адрес (А) но доступен любой из адресов (B) или (C)
};

struct wdog_setup_s {
   unsigned        signature;
   unsigned char   name[32];
   unsigned char   ip0[4];
   unsigned char   ip1[4];
   unsigned char   ip2[4];
   unsigned short  poll_period;
   unsigned short  ping_timeout;
   unsigned short  reset_time;
   unsigned short  reboot_pause;
   unsigned char   max_retry;
   unsigned char   doubling_pause_resets;
   unsigned char   reset_mode;                // ==0 power down, ==1 power up, exactly 0 or 1 (bitwise xor used)
   unsigned char   active;                    // bits 0 1 2 = ip A,B,C
   unsigned char   logic_mode;
   unsigned char   reserved[3];
#ifdef DNS_MODULE
   unsigned char   hostname0[64];
   unsigned char   hostname1[64];
   unsigned char   hostname2[64];
#endif
};

struct wdog_state_s {
   systime_t next_ping_time;
   systime_t reset_end_time;
   systime_t reboot_end_time;
   unsigned short reset_count;              // statistics
   unsigned short repeating_resets_count;   // repeating resets
   unsigned short poll_count_after_reset;
   unsigned char  resetting_disabled;       // if fail detected, don't reset - LBS 31.05.2010
   unsigned char  pings_in_progress;        // boolean flag, pinging is not completed
   unsigned char  output;                   // for routing to relay
   unsigned ping_mask;                      // actual ping channel mask of started pings
};

extern struct wdog_state_s wdog_state[WDOG_MAX_CHANNEL];
extern struct wdog_setup_s wdog_setup[WDOG_MAX_CHANNEL];


void watchdog_init(void);
int watchdog_event(enum event_e event);

#endif // WATCHDOG_H
