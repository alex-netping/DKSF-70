#include "crc\crc.h"

/*#define MODULE2 &crc_struct
#define MODULE2_INITS  CRC_INITS
#define MODULE2_EXECS  CRC_EXECS
#define MODULE2_TIMERS CRC_TIMERS*/
#define CRC_MODUL		//������ ��������� ������
//#define CRC_DEBUG		//������ ��������� ������� � ������

//---- ��������������� ������� ������ ������----------
/*! ������� ������� ��������� � ������ NIC
������� ����������� ���� �� NIC � ����� � RAM
\param addr ����� ����� � NIC
\param buf ����� ������ � RAM
\param len ������ ���� ��� ���������
*/
#define CRC_GET_NIC_DATA(addr,buf,len) NIC_READ_BUF(addr,buf,len)


