/*
v1.5-200
15.08.2011
  "sim busy" error resolved
  two dest phone numbers for sms sending
v1.6-201
19.08.2011
  IGT, EMEROFF inversed or not with auto identification
v1.7-200
10.10.2011
  emergency halt of the module
  reboot limiter
  'sim busy' retry limiter
  sms number per hour limiter
  accurate sms_init()
  accurate sms_sate checking for all operations before modem responce parsing
v1.8-201
25.10.2011
  English version
v1.9-213
7.12.2012
  sms_reset_params_mkarta()
v1.10-50
10.04.2013
  enlarged outgoing sms queue
  bugfix 'sms storm' after 8 messages enqued
  bugfix in sms_send_from_queue() for N phone numbers
v1.10-213
4.01.2013
  ignition rewrite, debug log modifications
v1.11-213
6.02.2013
  ignition, on-off rewrite
v1.12-48
  sys_setup.hostname used instead of sms_setup.hostname
  DNS adaptation
v1.13-707
  clear rx and del sms queues on module init and restart, in sms_init()
  respond to caller's phone number
v1.12-70
19.03.2014
  sms page update: restart button, debug log checkbox, last error
v1.13-201
24.03.2014
  both CMS/CME sim not inserted
v1.14-70
5.06.2014
  runtime 52/900 adaptation (by proj_hardware_detect() model in gsm_model variable)
v1.15-54
17.06.2014
  runtime 52/900 adaptation  (by proj_hardware_detect() model in gsm_model variable) of shutdown part
v1.16-54 (testing for 201 on dkst51.1.9)
12.11.2014
  using at+cmgd=1,4 instead of scanning sms purge during init for SIM900
v1.17-200
10.01.2015
  voltage request via modem
v1.17-201
29.01.2015
  enchanced modem Init sequence, deleted /r, ESCs from init strings, big ATs only for SIM900
v1.18-201
24.02.2015
  Rewrite to use +CMGS to send SMS immediately, w/o writing to memory then multiple sending
  SIM900 Call Ready polling before start
*/

struct sms_setup_s {
  unsigned char   hostname[32];
  unsigned char   dest_phone[MAX_DEST_PHONE][16];
  unsigned char   ussd_string[16];
  unsigned        event_mask;
  unsigned char   pinger_ip[4];
  unsigned short  pinger_period; // s
  unsigned short  flags;
  unsigned char   pinger_hostname[64]; // 24.10.2013
  unsigned char   periodic_time[32];  // 26.03.2014 // signed by periodic_time_signature if SMS eeprom area
}; // if >254bytes = trouble, see eeprom.map.h!

extern struct sms_setup_s sms_setup;

#define SMS_EVENT_PWR             0x0001
#define SMS_EVENT_ETHERNET        0x0002
#define SMS_EVENT_BATTERY         0x0004
#define SMS_EVENT_PINGER          0x0008

#define SMS_EVENT_IO_BASE         0x0010
#define SMS_EVENT_IO_1            0x0010
#define SMS_EVENT_IO_2            0x0020
#define SMS_EVENT_IO_3            0x0040
#define SMS_EVENT_IO_4            0x0080

#define SMS_EVENT_THERMO          0x0100

#define SMS_DEBUG_LOG             0x8000

extern const char via_sms[]; // aux text for logs
extern unsigned char powersaving;

#define MAXWORDS 7
#define WORDLEN  48
extern char  words[MAXWORDS][WORDLEN];
void upcase(char *s); // uppercase s in place
void split(char *s); // fills words

extern char sms_responce[128];       // sms command reply buffer
extern char sms_ussd_responce[128];
extern char sms_ussd_responce_ready;
extern unsigned char sms_gsm_failed; // gsm operation status ok(0), failed(1), halted(2)
extern int  sms_gsm_registration;
extern int  sms_sig_level;
extern systime_t sms_gsm_test_time;  // next +creg test timestamp in epoch ms
extern unsigned sms_reboot_counter;  // counter of modem reboots from device power-up
extern unsigned char sms_emergency_halt; // flag of halt on unrecoverable error
extern char sms_last_error[64];
extern unsigned char sms_at_debug;

void sms_collate_dest_ph_numbers(void);

void start_ussd(void);
int  start_ussd_with_req(char *request);
void ask_gsm_info(void);
int get_pinger_status(void);

void sms_q_text(char *txt, char *dest_phone);

void sms_trap_gsm_alive(void);

void sms_io_event(int ch);
void sms_thermo_event(int ch);
void sms_relhum_event(void);
void sms_curdet_event(void);
void sms_pwr_event(int ch, char event_code);
void sms_battery_event(unsigned event);

unsigned sms_snmp_get(void);
unsigned sms_snmp_set(unsigned id, unsigned char *data);

void sms_at_debug_start(char *s);
void sms_msg_printf(char *fmt, ...);
void ucs2_to_win1251(char *src, char *dest, unsigned dest_len);
void win1251_to_ucs2(char *src, char *dest, unsigned dest_len);
void decode_ussd(char *s);

void gsm_power(int onoff);
int sms_modem_is_ready(void);
int gsm_modem_is_busy(void);
int gsm_modem_is_stopped(void);
int gsm_modem_is_shutting_down(void);
void gsm_soft_shutdown(void);

void etherstatus_exec(void);
void pinger_exec(void);
void sms_cmd_periodic_exec(void);

void pinger_init(void);
void sms_init(void);

int  sms_event(enum event_e event);
