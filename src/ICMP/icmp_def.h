
#define ICMP_MODULE		//������ ��������� ������
//#define ICMP_DEBUG		//������ ��������� ������� � ������

//---- ��������������� ������� ������ ������----------
///������� ������� CRC
#define ICMP_CALC_CRC(buf, len)   crc_calc(buf, len)
///������� ������������ �������� CRC
#define ICMP_GET_CRC crc_get()

///������� ������� ���������� ICMP_Echo_reply ������
// replying to incoming Echo Request is in icmp.c!
#define ICMP_PARSING  do{/*ping_parsing();*/}while(0)


#include "icmp\icmp.h"
