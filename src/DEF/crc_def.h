#include "crc\crc.h"

#define CRC_MODULE		//������ ��������� ������
//#define CRC_DEBUG		//������ ��������� ������� � ������

//---- ��������������� ������� ������ ������----------
/*! ������� ������� ��������� � ������ NIC
������� ����������� ���� �� NIC � ����� � RAM
\param addr ����� ����� � NIC
\param buf ����� ������ � RAM
\param len ������ ���� ��� ���������
*/
#define CRC_GET_NIC_DATA(addr,buf,len) NIC_READ_BUF(addr,buf,len)


