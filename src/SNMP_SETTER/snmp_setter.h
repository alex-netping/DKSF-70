
struct setter_setup_s {
  unsigned char name[32]; // pasc-zterm string
  unsigned char oid[64]; // pasc-zterm string
  unsigned char ip[4];
  unsigned char hostname[64]; // pasc-zterm string // DNS, 18.10.2013
  unsigned char community[30]; // pasc-zterm string
  unsigned short port;
  int      value_on;
  int      value_off;
  unsigned char reserved[16]; // 18.10.2013
};

enum setter_state_e {
  SETTER_STATE_IDLE,
  SETTER_STATE_ATTEMPT_1,
  SETTER_STATE_ATTEMPT_2,
  SETTER_STATE_ATTEMPT_3
};

struct setter_state_s {
  enum setter_state_e state;
  unsigned request_id;
  systime_t timeout;
  char value_idx;
  char err_status;
};

extern struct setter_state_s setter_state[SETTER_MAX_CH];

void setter_send(unsigned ch, unsigned onoff);

void setter_parsing(void);
void setter_init(void);
void setter_event(enum event_e event);
