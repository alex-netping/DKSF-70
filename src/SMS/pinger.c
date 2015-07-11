

struct echo_s {
  char type;
  char code;
  unsigned short checksum;
  unsigned short id;
  unsigned short sequence;
  char data[32];
};

systime_t next_ping_time;

void pinger_exec(void)
{
  if(sys_time() < next_ping_time) return;
  if(!valid_ip(sms_setup.pinger_ip)) return;

}