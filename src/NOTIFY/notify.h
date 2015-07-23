#define NOTIFY_LOG       1
#define NOTIFY_SYSLOG    2
#define NOTIFY_EMAIL     4
#define NOTIFY_SMS       8
#define NOTIFY_TRAP     16

#define NOTIFY_COMMON_ALL_EVENTS    1
#define NOTIFY_COMMON_ALL_CHANNELS  2

struct range_notify_s {
  unsigned short high;
  unsigned short norm;
  unsigned short low;
  unsigned short fail;
  unsigned short report;
  unsigned short reserved0;
  unsigned char  flags;
  unsigned char  reserved1[3];
};

struct binary_notify_s {
  unsigned char legend_high[16];
  unsigned char legend_low[16];
  unsigned short high;
  unsigned short low;
  unsigned short report;
  unsigned short reserved0;
  unsigned reserved1[6];
};

struct relay_notify_s {
  unsigned short on_off;
  unsigned short report;
  unsigned reserved[3];
};

struct relhum_notify_s {
  unsigned short h_high;
  unsigned short h_norm;
  unsigned short h_low;
  unsigned short fail;
  unsigned short report;
  unsigned short t_high;
  unsigned short t_norm;
  unsigned short t_low;
  unsigned char  flags;
  unsigned char  reserved0;
  unsigned char  reserved1[6];
};

extern struct range_notify_s    thermo_notify[TERMO_N_CH];
extern struct binary_notify_s   io_notify[IO_MAX_CHANNEL];
extern struct relay_notify_s    relay_notify[RELAY_MAX_CHANNEL];
extern struct range_notify_s    curdet_notify;
#ifdef RELHUM_MAX_CH
extern struct relhum_notify_s   relhum_notify[RELHUM_MAX_CH];
#else
extern struct range_notify_s    relhum_notify;
#endif
extern struct range_notify_s    smoke_notify[SMOKE_MAX_CH];
extern struct range_notify_s    pwrmon_notify[PWRMON_MAX_CH];

#pragma __printf_args
void notify(unsigned mask, char *fmt, ...);

void notify_init(void);
void notify_event(enum event_e event);

