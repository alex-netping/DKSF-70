#include "platform_setup.h"

#define EEPROM_READ(ee_addr, ram_addr, len)   spi_eeprom_read ((unsigned)ee_addr, (void*)ram_addr, len)
#define EEPROM_WRITE(ee_addr, ram_addr, len)  spi_eeprom_write((unsigned)ee_addr, (void*)ram_addr, len)

/* ВНИМАНИЕ! В начале памяти параметры от старых прошивок.
   Не трогать, иначе сработает "импорт" параметров от старой прошивки.
   См. restore_parameters() в файле project.c */

#define _extern   //  IAR6 - extern w/o definition don't works.

_extern  __no_init  char eeprom_log[0x3000-0x200]                          @ 0xEE000200;

_extern  __no_init unsigned eeprom_sys_setup_signature                     @ 0xEE003000;
_extern  __no_init struct sys_setup_s eeprom_sys_setup                     @ 0xEE003004;

/*
  up to 48.1.7; 70.7.2
_extern  __no_init unsigned eeprom_sys_setup_signature2                    @ 0xEE003200;
_extern  __no_init unsigned eeprom_mib2_signature                          @ 0xEE003204; // sysContact, sysName, sysLocation

_extern __no_init char eeprom_sysContact[64]                               @ 0xEE003700;
_extern __no_init char eeprom_sysName[64]                                  @ 0xEE003740;
_extern __no_init char eeprom_sysLocation[64]                              @ 0xEE003780;
*/

// since 48.1.8; 70.2.7
_extern  __no_init unsigned eeprom_sys_setup_signature_mib2                @ 0xEE0037EC; // sysContact, sysName, sysLocation
_extern  __no_init unsigned eeprom_sys_setup_signature_dns                 @ 0xEE0037F0; // dns
_extern  __no_init unsigned eeprom_sys_setup_signature4                    @ 0xEE0037F4; // telnet_port
_extern  __no_init unsigned eeprom_sys_setup_signature3                    @ 0xEE0037F8; // e_mail notification
_extern  __no_init unsigned eeprom_sys_setup_signature2                    @ 0xEE0037FC; // snmp_port

_extern  __no_init struct wdog_setup_s eeprom_wdog_setup[WDOG_MAX_CHANNEL] @ 0xEE003800; // the same place, individual sign per ch

#ifdef TERMO_N_CH
_extern  __no_init unsigned  eeprom_termo_signature                        @ 0xEE003A00;
_extern  __no_init struct termo_setup_s eeprom_termo_setup[TERMO_N_CH]     @ 0xEE003A04;
#endif

#ifdef IO_MAX_CHANNEL
_extern  __no_init unsigned  eeprom_io_signature                           @ 0xEE003C00;
_extern  __no_init struct io_setup_s eeprom_io_setup[IO_MAX_CHANNEL]       @ 0xEE003C04;
_extern  __no_init unsigned  eeprom_io_signature_io34                      @ 0xEE003DFC;
#endif

#ifdef WTIMER_MODULE
_extern  __no_init unsigned  eeprom_wtimer_signature                             @ 0xEE003E00;
_extern  __no_init struct wtimer_holidays_s eeprom_wtimer_holidays               @ 0xEE003E04;
//_extern  __no_init struct wtimer_setup_s eeprom_wtimer_setup[WTIMER_MAX_CHANNEL+2] @ 0xEE003E80;
_extern  __no_init struct wtimer_setup_s eeprom_wtimer_setup[WTIMER_MAX_CHANNEL] @ 0xEE003E80; // 14.05.2015
#endif

#ifdef CUR_DET_MODULE
_extern  __no_init unsigned  eeprom_curdet_signature                       @ 0xEE004000;
_extern  __no_init struct curdet_setup_s eeprom_curdet_setup               @ 0xEE004004;
#endif

#ifdef UART_MODULE
_extern  __no_init unsigned  eeprom_uart_signature                        @ 0xEE004300;
_extern  __no_init struct    uart_setup_s eeprom_uart_setup               @ 0xEE004304;
#endif

#ifdef SMOKE_MODULE
_extern  __no_init unsigned eeprom_smoke_signature                         @ 0xEE004400-4;
_extern  __no_init struct smoke_setup_s eeprom_smoke_setup[SMOKE_MAX_CH]   @ 0xEE004400; // 64b per elem-t
#endif

#ifdef RELHUM_MODULE
#ifndef RELHUM_MAX_CH
_extern  __no_init unsigned  eeprom_relhum_signature                       @ 0xEE004600;
_extern  __no_init struct relhum_setup_s eeprom_relhum_setup               @ 0xEE004604;
#endif
#endif

#ifdef SETTER_MAX_CH
_extern __no_init unsigned eeprom_setter_signature                         @ 0xEE004700;
_extern __no_init struct setter_setup_s eeprom_setter_setup[SETTER_MAX_CH] @ 0xEE004704; // 140b per elem. (0x8c) // or 220b???
#endif

#ifdef SMS_MODULE
_extern __no_init unsigned eeprom_sms_periodic_time_signature              @  0xEE006500 - 4;
_extern __no_init unsigned eeprom_sms_signature                            @  0xEE006500;
_extern __no_init struct sms_setup_s eeprom_sms_setup                      @  0xEE006504; // <254 байт
_extern __no_init unsigned char eeprom_sms_msg_serial[2][64]               @  0xEE006600;
#endif

#ifdef LOGIC_MODULE

_extern __no_init unsigned char eeprom_logic_flags                                        @ 0xEE006800-5;
_extern __no_init unsigned eeprom_logic_signature                                         @ 0xEE006800-4;
_extern __no_init struct logic_setup_s eeprom_logic_setup[LOGIC_MAX_RULES]                @ 0xEE006800;

_extern __no_init unsigned eeprom_tstat_signature                                         @ 0xEE006b00;
_extern __no_init struct tstat_setup_s eeprom_tstat_setup[TSTAT_MAX_CHANNEL]              @ 0xEE006b04;

_extern __no_init unsigned eeprom_logic_pinger_signature                                  @ 0xEE006b00 - 4;
_extern __no_init struct logic_pinger_setup_s eeprom_logic_pinger_setup[LOGIC_MAX_PINGER] @ 0xEE006c00; // 0x40 byte per elem.

#endif

#ifdef RELAY_MODULE
_extern __no_init unsigned eeprom_relay_signature                                         @ 0xEE007800;
//_extern __no_init char     eeprom_relay_in_setup[2]                                       @ 0xEE007804;
_extern __no_init struct   relay_setup_s eeprom_relay_setup[RELAY_MAX_CHANNEL]            @ 0xEE007880;
#endif

_extern __no_init unsigned                 eeprom_notify_signature                        @ 0xEE008000;
_extern __no_init unsigned                 eeprom_notify_relay_signature                  @ 0xEE008004;
_extern __no_init unsigned                 eeprom_notify_smoke_signature                  @ 0xEE008008;
_extern __no_init unsigned                 eeprom_notify_relhum_signature                 @ 0xEE00800c; // v3 relhum
_extern __no_init unsigned                 eeprom_notify_pwrmon_signature                 @ 0xEE008010;
//_extern __no_init struct range_notify_s    eeprom_relhum_notify                           @ 0xEE008040; // pror relhum v3 (multiple 1w), don't overwrite!
_extern __no_init struct range_notify_s    eeprom_curdet_notify                           @ 0xEE008080;
_extern __no_init struct range_notify_s    eeprom_thermo_notify[TERMO_N_CH]               @ 0xEE0080c0; // 16b per elem.
_extern __no_init struct range_notify_s    eeprom_smoke_notify[SMOKE_MAX_CH]              @ 0xEE0081c0; // 16b per ch
_extern __no_init struct binary_notify_s   eeprom_io_notify[IO_MAX_CHANNEL]               @ 0xEE0082c0; // 64b per elem.
_extern __no_init struct relay_notify_s    eeprom_relay_notify[RELAY_MAX_CHANNEL]         @ 0xEE008500;
_extern __no_init struct relhum_notify_s   eeprom_relhum_notify[RELHUM_MAX_CH]            @ 0xEE008510; // 24b per elem. // since relhum v3
//                     eeprom_pwrmon_notify[PWRMON_MAX_CH] is below


_extern __no_init unsigned                 eeprom_sendmail_cc_signature                   @ 0xEE008600 - 8;
_extern __no_init unsigned                 eeprom_sendmail_signature                      @ 0xEE008600 - 4;
_extern __no_init struct sendmail_setup_s  eeprom_sendmail_setup                          @ 0xEE008600;

#ifdef RELHUM_MAX_CH // v3
_extern  __no_init unsigned  eeprom_relhum_signature                                      @ 0xEE008A00 - 4;
_extern  __no_init struct relhum_setup_s eeprom_relhum_setup[RELHUM_MAX_CH]               @ 0xEE008A00;
#endif

_extern __no_init unsigned                 eeprom_pwrmon_signature                        @ 0xEE008C00 - 4;
_extern __no_init struct range_notify_s    eeprom_pwrmon_notify[PWRMON_MAX_CH]            @ 0xEE008C00;
_extern __no_init struct pwrmon_setup_s    eeprom_pwrmon_setup[PWRMON_MAX_CH]             @ 0xEE008D00;

