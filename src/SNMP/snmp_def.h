
#define SNMP_MODULE		//������ ��������� ������
//#define SNMP_DEBUG		//������ ��������� ������� � ������


//---- ��������������� ������� ������ ������----------

///������� ��������� ������������ ���������� ����������
#define SNMP_RELOAD_HANDLER (main_reload = 1)
///������� ��������� ������������ ����������
#define SNMP_RESET_HANDLER (main_reboot = 1)

#define SNMP_CRC_CALC(buf,len) calc_snmp_crc(buf,len)

#define SNMP_CRC crc16

///������ ����� ���������� ��
#define SNMP_FW_UPDATE_BLOCK 512
///��������� ����� � �������� ����������� ���������� ��
#define SNMP_FW_START 0x40000
///������ ������� ���������� ��
#define SNMP_FW_SIZE  0x3F000 // 258048
///������ ����� ���������� ������� ���������
#define SNMP_MW_UPDATE_BLOCK 512
///��������� ����� � �������� ����������� ������� ���������
#define SNMP_MW_START 0x40000
///������ ������� ������� ���������
#define SNMP_MW_SIZE  0x3F000 // 126976

/*! ��������� �������� ����� ���������� �� ����� n � ������� ���������� ��
\param n - ����� ����� ���������� ��
*/
#define SNMP_LOAD_FW_BLOCK(n) load_fw_block(n);

///��������� ���������� ��
#define SNMP_START_FW_UPDATE  start_fw_update();

/*! ��������� �������� ����� ������� ��������� ����� n � ������� ������� ���������
\param n - ����� ����� ������� ���������
*/
#define SNMP_LOAD_MW_BLOCK(n) load_mw_block(n);

///��������� ���������� ������� ���������
#define SNMP_START_MW_UPDATE start_mw_update();

#define SNMP_AGENT_IP sys_setup.ip

/*! ��������� ��������� ��������� ������ ����������
\param sn -��������� �� �������� �����
*/
#define SNMP_SET_SN(sn) set_sn(sn);
/*! ��������� ��������� MAC ����������
\param mac - ��������� �� mac �����
*/
#define SNMP_SET_MAC(mac) set_mac(mac);

/*! ��������� ��������� IP ����������
\param IP - ��������� �� IP �����
*/
#define SNMP_SET_IP(ip) set_ip(ip);


//// LBS revised OID search version

// #define MAX_OID_LEN 16

#define MAX_OID_LEN 18 // LBS 9.04.2010 mac index


#include "snmp\snmp.h"
