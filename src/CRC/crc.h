/* ������ CRC ������������ ��� ������� ����������� ���� ��� TCP/IP ����� � ��� ������ PARAMETERS
*\autor
*version 1.0
*\date 19.07.2007
*
* v 1.1
* 08.05.2010 by LBS
*   place_checksum()
*
* v 1.2
* 21.05.2010 by LBS
*   crc16_reset()
*   crc16_incremental_calc()
*v1.3
*10.11.2012
*  place_checksum() rewrite, now calc_and_place_checksum()
*/

#include "platform_setup.h"
#ifndef  CRC_H
#define  CRC_H

#define CRC_VER     1
#define CRC_BUILD   3


#define CRC16_POLINOM		0xA001			

///���������� ���������� ��� �������� ������������� ����������� ����� TCP/IP
extern unsigned long CRC16;

///���������� ���������� ��� �������� �������������/�������� ����������� ����� ��� ������ parameters
extern unsigned short crc16;


/*! ������� ��������� CRC ��� TCP/IP ����� (�� RFC 1071)
����. ������ ���� ������� ����� ����� �� ������� DKSF 50 (������ crc.c ������� crc16_sum)
����. ������� ��������� CRC � ���������� CRC16
\param buf ��������� �� ����� � ������
\param len ������ ������ � ������
*/
void crc_calc(unsigned char *buf,uword len); // �� ����� ���� ��� chechsum, � �� CRC - LBS

/*! ������� ��������� CRC ��� TCP/IP ����� (�� RFC 1071)
������� �������� ���������� crc_calc �� ����������� ����������� ����� �� ������ nic
��� ��������� ������ �� NIC ������� ���������� ������� ������� CRC_GET_NIC_DATA
����.������� ��������� CRC � ���������� CRC16
\param addr ����� ������ ������ � NIC
\param len  ������ ������ � ������
*/
void crc_calc_nic(upointer addr,unsigned short len); // �� ����� ���� ��� chechsum, � �� CRC - LBS

/*! ������� ���������� CRC ��� TCP/IP ����� (�� RFC 1071)
������� ��������� ��������� �������� ������� CRC.
����. ������ ���� ������� ����� ����� �� ������� DKSF 50 (������ crc.c ������� crc16_get)
����. ������� ��������� CRC �� ���������� CRC16
\return ����������� �������� CRC16
*/

unsigned int crc_get(void); // �� ����� ���� ��� chechsum, � �� CRC - LBS

/*! ������� ��������� CRC16 ��� ������ Parameters
����. ������ ���� ������� ����� ����� �� ������� DKSF 50 (������ crc.c ������� update_crc16)
������� ��������� CRC � ���������� crc16
\param buf ��������� �� �����
\param len ������ ������
*/
void crc_param(unsigned char *buf,uword len); // � ��� ��� ������������� CRC - LBS

// LBS 08.05.2010
void place_checksum(void *vbuf, unsigned len, unsigned char *checksum);
// LBS 21.05.2010
void crc16_reset(unsigned short *crc16p);
void crc16_incremental_calc(unsigned short *crc16p, unsigned char *addr, unsigned size);
// LBS 11.11.2012
void calc_and_place_checksum(void *buf, unsigned len, unsigned char *checksum);


void checksum_reset(void *cs_ptr);
void checksum_incremental_pseudo(unsigned char *dst_ip, unsigned char *src_ip, char protocol, unsigned ip_payload_length);
void checksum_incremental_calc(void *buf, unsigned len);
void checksum_place(void *cs_ptr);

#endif

