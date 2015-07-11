#define SENDMAIL_MODULE

struct sendmail_setup_s {
  unsigned char  fqdn[64];
  unsigned short port;
  unsigned char  flags; // 11.02.2015
  unsigned char  reserved2;
  unsigned char  user[48];
  unsigned char  passwd[32];
  unsigned char  from[48];
  unsigned char  to[48];
  unsigned char  reports[64];
  unsigned char  cc_1[48];
  unsigned char  cc_2[48];
  unsigned char  cc_3[48];
};
extern struct sendmail_setup_s sendmail_setup;

// flags
#define SM_FLG_ENABLE_SM          0x01
#define SM_FLG_SIGNATURE_BIT      0x80

extern unsigned char sendmail_smtp_ip[4];

void sendmail(char *subj, char *body);

void tcp_cli_parsing(void);
void sendmail_init(void);
void sendmail_event(enum event_e event);
