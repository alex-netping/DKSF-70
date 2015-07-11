
#define SNMP_MODULE		//Флажок включения модуля
//#define SNMP_DEBUG		//Флажок включения отладки в модуле


//---- Переопределение внешних связей модуля----------

///Внешняя процедура перезагрузки параметров устройства
#define SNMP_RELOAD_HANDLER (main_reload = 1)
///Внешняя процедура перезагрузки устройства
#define SNMP_RESET_HANDLER (main_reboot = 1)

#define SNMP_CRC_CALC(buf,len) calc_snmp_crc(buf,len)

#define SNMP_CRC crc16

///Размер блока обновления ПО
#define SNMP_FW_UPDATE_BLOCK 512
///Стартовый адрес с которого размещается обновление ПО
#define SNMP_FW_START 0x40000
///Размер области обновления ПО
#define SNMP_FW_SIZE  0x3F000 // 258048
///Размер блока обновления таблицы сообщений
#define SNMP_MW_UPDATE_BLOCK 512
///Стартовый адрес с которого размещается таблица сообщений
#define SNMP_MW_START 0x40000
///Размер области таблицы сообщений
#define SNMP_MW_SIZE  0x3F000 // 126976

/*! Процедура загрузки блока обновления ПО номер n в область обновления ПО
\param n - номер блока обновления ПО
*/
#define SNMP_LOAD_FW_BLOCK(n) load_fw_block(n);

///Процедура обновления ПО
#define SNMP_START_FW_UPDATE  start_fw_update();

/*! Процедура загрузки блока таблицы сообщений номер n в область таблицы сообщений
\param n - номер блока таблицы сообщений
*/
#define SNMP_LOAD_MW_BLOCK(n) load_mw_block(n);

///Процедура обновления таблицы сообщений
#define SNMP_START_MW_UPDATE start_mw_update();

#define SNMP_AGENT_IP sys_setup.ip

/*! Процедура изменения серийного номера устройства
\param sn -указатель на серийный номер
*/
#define SNMP_SET_SN(sn) set_sn(sn);
/*! Процедура изменения MAC устройства
\param mac - указатель на mac адрес
*/
#define SNMP_SET_MAC(mac) set_mac(mac);

/*! Процедура изменения IP устройства
\param IP - указатель на IP адрес
*/
#define SNMP_SET_IP(ip) set_ip(ip);


//// LBS revised OID search version

// #define MAX_OID_LEN 16

#define MAX_OID_LEN 18 // LBS 9.04.2010 mac index


#include "snmp\snmp.h"
