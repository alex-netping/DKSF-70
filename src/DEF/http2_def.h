
#define HTTP_MODULE		//������ ��������� ������
//#define HTTP_DEBUG		//������ ��������� ������� � ������

//---- ��������������� ������� ������ ������----------

// HTTP v2 (by LBS) -------------------------------

#define TCP_CONN_IS_CLOSED(socket) (tcp_socket[socket].tcp_state == TCP_LISTEN)

#define HTTP_INPUT_DATA_SIZE 3072

// ��������� http �������� � ������ (�������� ���. ������������ - cpu flash ��� eeprom)
#define HTML_RES_IS_IN_CPU_FLASH
/*#define HTML_RES_IS_IN_EEPROM*/
//#define HTML_RES_START 0x40000



#include "http2\http2.h"

