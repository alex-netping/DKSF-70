/*
Автоматическая компоновка модулей
v3.0
19.02.2010
v3.1-48
4.03.2013
*/

enum event_e {
  E_INIT = 1,
  E_EXEC,
  E_RESET_PARAMS,
  E_RESET_PARAMS_MULTIKARTA,
  E_TIMER_1ms = 0x11,
  E_TIMER_10ms,
  E_TIMER_100ms,
  E_TIMER_1s,
  E_TIMER_10s,
  E_PARSE_TCP = 0x21,
  E_ACCEPT_TCP,
  E_SNMP_GET = 0x41,
  E_SNMP_SET = 0x42,
  E_POWERSAVING
};

void init_modules(void);
void init_basic_modules(void);
void system_event(enum event_e event, unsigned evinfo);

