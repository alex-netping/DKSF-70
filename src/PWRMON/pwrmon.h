
struct pwrmon_setup_s {
  unsigned char  name[32]; // pasc. string
  unsigned char  ow_addr[8];
  unsigned short uv1;
  unsigned short ov1;
  unsigned short t1;
  unsigned short t12;
  unsigned short uv2;
  unsigned short ov2;
  unsigned short t2;
  // len from start 54
  unsigned char  reserved[42];
  // len 96
};

struct pwrmon_state_s {
  char comm_status;
  char refresh; // flag: necessary to read setup data from sensor. Set after sensor re-connect or ow_addr change.
  // stats data; 32-bit counters read on pwrmon_state_s.refresh flag
  unsigned cnt1uv;
  unsigned cnt1ov;
  unsigned cnt2uv;
  unsigned cnt2ov;
  unsigned cnt3uv;
  unsigned short v;
  unsigned short f;
  // setup data (alignment match sensor's); it's read from sensor on pwrmon_state_s.refresh flag
  unsigned short uv1;
  unsigned short ov1;
  unsigned short t1;
  unsigned short t12;
  unsigned short reserved1;
  unsigned short reserved2;
  unsigned short uv2;
  unsigned short ov2;
  unsigned short t12_copy;
  unsigned short t2;
  unsigned short reserved3;
  unsigned short reserved4;
  unsigned short t2_copy;
  //
  char write_sensor_setup; // flag: start saving of above sensor setup data into the sensor
};

extern struct pwrmon_state_s pwrmon_state[PWRMON_MAX_CH];
extern struct pwrmon_setup_s pwrmon_setup[PWRMON_MAX_CH];

extern const char pwrmon_msg_fail[];

void pwrmon_parse_setup_data_from_sensor(unsigned ch, unsigned char *buf);
void pwrmon_parse_full_stats_from_sensor(unsigned ch, unsigned char *buf);
void pwrmon_parse_short_stats_from_sensor(unsigned ch, unsigned char *buf);
void pwrmon_set_comm_status(unsigned ch, int ok_flag);

