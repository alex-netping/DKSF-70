
struct smoke_setup_s
{
  unsigned char name[32];
  unsigned char ow_addr[8];
  unsigned char flags;
  unsigned char reserved1[3];
  unsigned reserved2[6];
};

enum smoke_satus_e {
  SMOKE_STATUS_NORM   = 0,
  SMOKE_STATUS_ALARM  = 1,
  SMOKE_STATUS_OFF    = 4,
  SMOKE_STATUS_FAILED = 5,
  SMOKE_STATUS_UNKNOWN = 0xff
};

extern struct smoke_setup_s smoke_setup[SMOKE_MAX_CH];

extern enum smoke_satus_e   smoke_status[SMOKE_MAX_CH];
extern unsigned             smoke_reset_time[SMOKE_MAX_CH];

extern char const * const smoke_status_text[6];

int smoke_snmp_get(void);
int smoke_snmp_set(void);

void smoke_start_reset(unsigned ch);

char const *smoke_get_status_text(unsigned ch, int message_length_type);
unsigned smoke_summary_short(char *dest);

void smoke_init(void);
void smoke_event(enum event_e event);

