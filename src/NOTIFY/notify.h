#define NOTIFY_LOG       1
#define NOTIFY_SYSLOG    2
#define NOTIFY_EMAIL     4
#define NOTIFY_SMS       8
#define NOTIFY_TRAP     16

struct range_notify_s {
  unsigned short high;
  unsigned short norm;
  unsigned short low;
  unsigned short fail;
  unsigned short report;
  unsigned short reserved0;
  unsigned reserved1;
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

extern struct range_notify_s    thermo_notify[TERMO_N_CH];
extern struct binary_notify_s   io_notify[IO_MAX_CHANNEL];
extern struct relay_notify_s    relay_notify[RELAY_MAX_CHANNEL];
extern struct range_notify_s    curdet_notify;
extern struct range_notify_s    relhum_notify;

#pragma __printf_args
void notify(unsigned mask, char *fmt, ...);

void notify_init(void);
void notify_event(enum event_e event);

