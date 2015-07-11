
extern unsigned tcpcom_soc;
extern unsigned uart_to_net_overflow;

void tcpcom_reinit(void);
void tcpcom_init(void);
void tcpcom_event(enum event_e event, unsigned evdata_tcp_soc_n);
