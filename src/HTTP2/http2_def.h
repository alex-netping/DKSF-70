
#define HTTP_MODUL		//������ ��������� ������
//#define HTTP_DEBUG		//������ ��������� ������� � ������

//---- ��������������� ������� ������ ������----------

// HTTP v2 (by LBS) -------------------------------

#define TCP_CONN_IS_CLOSED(socket) (tcp_socket[socket].tcp_state == TCP_LISTEN)

// ��������� http �������� � ������ (�������� ���. ������������ - cpu flash ��� eeprom)
#define HTML_RES_IS_IN_CPU_FLASH
/*#define HTML_RES_IS_IN_EEPROM*/
#define HTTP_RES_START 0x20000



#include "http2\http2.h"

