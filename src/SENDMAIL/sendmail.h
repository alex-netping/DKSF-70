
enum sendmail_state_e {
  SM_IDLE,
  SM_CONNECT,
  SM_EHLO,
  SM_AUTH,
  SM_USERNAME,
  SM_PASSWD,
  SM_MAIL_FROM,
  SM_RCPT_TO,
  SM_RCPT_CC_1,
  SM_RCPT_CC_2,
  SM_RCPT_CC_3,
  SM_DATA,
  SM_BODY,
  SM_QUIT_AFTER_CHECK,
  SM_QUIT,
  SM_CLOSING
};

enum tcp_cli_state_e {
  TCPC_IDLE,
  TCPC_CONNECT,
  TCPC_SYN_SENT,
  TCPC_ESTABLISHED,
  TCPC_LAST_ACK,
  TCPC_ERROR
};

enum tx_status_e {
  TX_FREE,
  TX_KEEP,
  TX_NEW
};

struct tcp_client_s {
  enum tcp_cli_state_e state;
  unsigned ip32;
  unsigned short port;
  unsigned short my_port;
  unsigned peer_seq; // next expected rx seq
  unsigned my_seq; // outstanding tx segment starting seq
  unsigned short sent_len; // len of last sent segment
  char rx_data[768];
  unsigned short rx_data_len;
  char tx_data[1400];
  char *tx_data_p;
  unsigned short tx_data_len;
  unsigned char flags;
  unsigned char retry;
  enum tx_status_e tx_state;
  unsigned timeout;
};

extern struct tcp_client_s tcp_cli;

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

void sendmail(char *subj, char *body, struct tm *d);

void tcp_cli_parsing(void);
void tcp_cli_rst_and_clear(void);

void sendmail_init(void);
void sendmail_event(enum event_e event);
