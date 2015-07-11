/*@{
defgroup STR
* ������ STR ������������� ������� ��� ������ �� ��������
* ������ ���������:
*\autor
*version 1.0
*\date 17.09.2007
*
*
12.2009
- ����� str_data_to_dec() � ��������� ������
������������ ���������� #include <stdio.h> � ������ .c �����:
- ����������� str_ip_to_str ��� ������ ������������ stdio.h
- ����������� str_mask_to_str ��� ������ ������������ stdio.h
v2.2
7.12.2012
  str_pzt_cpy()
*/

#include "platform_setup.h"
#ifndef  STR_H
#define  STR_H
///������ ������
#define  STR_VER	2
///������ ������
#define  STR_BUILD	2

#include <string.h>


extern uword str_error; //���� ������

//---------------- ������, ��� ����� ������������ ������� ������ ---------------------------

/*!��������� ����������� ������� ������ ������ ���� (����/�����/������� ����� � �.�) � ����������������� ������ �������� (������: "A67E")
* ����: ������ �������� ������ ������������ �����.
* ����: ������� ������ ���� � �������� ������ ����������
\param data -��������� �� ������� ������
\param str -��������� �� �������������� ������
\param len -������ ������
*/
extern void str_data_to_hex(unsigned char * data, unsigned char *str,uword len);
/*!��������� ����������� ������� ������ ������ ���� (����/�����/������� ����� � �.�) � ���������� ������ �������� (������: "68")
* ����: ������ �������� ������ ������������ �����.
* ����: ������� ������ ���� � �������� ������ ����������
\param data -��������� �� ������� ������
\param str -��������� �� �������������� ������
\param len -������ ������
*/
extern void str_data_to_dec(unsigned char * data, unsigned char *str,uword len);
/*!��������� ����������� ������� ������ ������ ���� (����/�����/������� ����� � �.�) � �������� ������ �������� (������: "101")
* ����: ������ �������� ������ ������������ �����.
* ����: ������� ������ ���� � �������� ������ ����������
\param data -��������� �� ������� ������
\param str -��������� �� �������������� ������
\param len -������ ������
*/
extern void str_data_to_bin(unsigned char * data, unsigned char *str,uword len);
/*! ��������� ��������������� ������� ������ � ������������� Boolean, �.�. :
* ���� (data&1)==1 �� ��������� � �������� ������ "ENABLE", ����� ��������� � �������� ������ "DISABLE"
\param data -������� ������
\param str -��������� �� �������������� ������
*/
extern void str_data_to_bool(uword data, unsigned char *str);
/*! ��������� ����������� ������� ������ � ������� HEX � ������.
* ��������: ������� ������ "0AbC3\0" ����� ������������� � ������ ABC3
* ����: ������ �������� ������ ������������ �����.
* ����: ��������� �� ������������ � �������� �������� ��������
* ����: � ������ ���� �������������� �� ��������, �������� ������
\param str -��������� �� ������� ������
\param data- ��������� �� ������� ������
\param len- ������ ������
*/
extern void str_hex_to_data( unsigned char *str,unsigned char * data,uword len);
/*! ��������� ����������� ������� ������ � ������� DEC � ������.
* ��������: ������� ������ "0123\0" ����� ������������� � ������ 123
* ����: ������ �������� ������ ������������ �����.
* ����: � ������ ���� �������������� �� ��������, �������� ������
\param str -��������� �� ������� ������
\param data- ��������� �� ������� ������
\param len- ������ ������
*/
extern void str_dec_to_data(unsigned char *str,unsigned char * data, uword len);
/*! ��������� ����������� ������� ������ � ������� BIN � ������.
* ��������: ������� ������ "0110\0" ����� ������������� � ������ 5
* ����: ������ �������� ������ ������������ �����.
* ����: � ������ ���� �������������� �� ��������, �������� ������
\param str -��������� �� ������� ������
\param data- ��������� �� ������� ������
\param len- ������ ������
*/
extern void str_bin_to_data(unsigned char *str, unsigned char * data, uword len);
/*!
! ��������� ����������� ������� ������ � ������� BOOLEAN (ENABLE/DISABE) � ������.
* ��������: ������� ������ EnAble ����� �������������� � ������ 1. ������� ������ DiSAble ����� �������������� � ������ 1.
* ����: ������ �������� ������ ������������ �����.
* ����: � ������ ���� �������������� �� ��������, �������� ������

\param str -��������� �� ������� ������
\param data -�������� ������
*/
extern uword str_bool_to_data(unsigned char *str);
/*! ��������� ����������� ����� ������� � ������ ������ �������:
* ������: ����� ����� 24, ������ �� ������  "255.255.255.0"
* ����: ������ �������� ������ ������������ �����.
\param data -�����
\param str -�������� ������
*/
extern void str_mask_to_str(uword data,unsigned char *str);
/*! ��������� ����������� ������ � �����:
* ������: ����� ����� 24, ������ �� ������  "255.255.255.0"
* ����: ������ �������� ������ ������������ �����.
* ����: � ������ ���� �������������� �� ��������, �������� ������
\param str -������� ������
\param data -�����
*/
extern void str_str_to_mask(unsigned char *str,unsigned char *data);

/*! ��������� ����������� IP ����� � ������ �������� � ������� xx.yy.zz.ff  (������� 192.168.0.100) :
* ����: ������ �������� ������ ������������ �����.
\param data -�����
\param str -�������� ������
*/
extern void str_ip_to_str(unsigned char *data,unsigned char *str);
/*! ��������� ����������� ������ IP ������ � ������� xx.yy.zz.ff � IP �����.
* ����: ������ �������� ������ ������������ �����.
* ����: � ������ ���� �������������� �� ��������, �������� ������
\param str -������� ������
\param data -�����
*/
extern void str_str_to_ip(unsigned char *str,unsigned char *data);

/*! ��������� ����������� MAC ����� � ������ �������� � ������� xx.yy.zz.ff  (������� 192.168.0.100) :
* ����: ������ �������� ������ ������������ �����.
\param data -�����
\param str -�������� ������
*/
extern void str_mac_to_str(unsigned char *data,unsigned char *str);

/*! ��������� ����������� ������ MAC ������ � ������� xx:yy:zz:ff:gg:hh � MAC �����
* ����: ������ �������� ������ ������������ �����.
* ����: � ������ ���� �������������� �� ��������, �������� ������
\param str -������� ������
\param data -�����
*/
extern void str_str_to_mac(unsigned char *str,unsigned char *data);


/*! ��������� ���������� ������ ������
* ����: ������ �������� ������ ������������ �����.
\param str - ������� ������
*/
extern uword str_lenght(unsigned char *str);

/*! �������� "����������" ������ � zero-terminated ������.
    target_size - ������ ������ zeroterm_str (������� ����� ��� ����������� ����)
    ���� �������� ������ ������, ��� ���������� �� target_size-1 ��������.
    LBS 09.2009
*/
int str_pasc_to_zeroterm(unsigned char *pasc_str, unsigned char *zeroterm_str, int target_size);

/*! �������� ������ � pascal + zero-terminated ������.
    pasc_str_size - ������ pasc_str (������� ����� ��� ����� � ����������� ����)
    ���� �������� ������ ������, ��� ���������� �� pasc_str_size-2 ��������.
    LBS 12.2012
*/
void str_pzt_cpy(unsigned char *pasc_str, char *src, size_t pasc_str_size);



/*! ��������� �������������� unsigned long � HEX ������
\param data- ������
\param str - ��������� �� �������� ������
*/
#define STR_LONG_TO_HEX(data,str) str_data_to_hex((unsigned char*)&data,str,4);
/*! ��������� �������������� unsigned short � HEX ������
\param data- ������
\param str - ��������� �� �������� ������
*/
#define STR_SHORT_TO_HEX(data,str) str_data_to_hex((unsigned char*)&data,str,2);
/*! ��������� �������������� unsigned char � HEX ������
\param data- ������
\param str - ��������� �� �������� ������
*/
#define STR_BYTE_TO_HEX(data,str) str_data_to_hex((unsigned char*)&data,str,1);

/*! ��������� �������������� unsigned long � DEC ������
\param data- ������
\param str - ��������� �� �������� ������
*/
#define STR_LONG_TO_DEC(data,str) str_data_to_dec((unsigned char*)&data,str,4);

/*! ��������� �������������� unsigned short � DEC ������
\param data- ������
\param str - ��������� �� �������� ������
*/
#define STR_SHORT_TO_DEC(data,str) str_data_to_dec((unsigned char*)&data,str,2);

/*! ��������� �������������� unsigned char � DEC ������
\param data- ������
\param str - ��������� �� �������� ������
*/
#define STR_BYTE_TO_DEC(data,str) str_data_to_dec((unsigned char*)&data,str,1);

/*! ��������� �������������� unsigned long � BIN ������
\param data- ������
\param str - ��������� �� �������� ������
*/
#define STR_LONG_TO_BIN(data,str) str_data_to_bin((unsigned char*)&data,str,4);

/*! ��������� �������������� unsigned short � BIN ������
\param data- ������
\param str - ��������� �� �������� ������
*/
#define STR_SHORT_TO_BIN(data,str) str_data_to_bin((unsigned char*)&data,str,2);

/*! ��������� �������������� unsigned char � BIN ������
\param data- ������
\param str - ��������� �� �������� ������
*/
#define STR_BYTE_TO_BIN(data,str) str_data_to_bin((unsigned char*)&data,str,1);

/*! ��������� �������������� ������ HEX � unsigned long
\param data- ������
\param str - ��������� �� ������� ������
*/
#define STR_HEX_TO_LONG(str,data) str_hex_to_data(str,(unsigned char*)&data,4);

/*! ��������� �������������� ������ HEX � unsigned short
\param data- ������
\param str - ��������� �� ������� ������
*/
#define STR_HEX_TO_SHORT(str,data) str_hex_to_data_to_hex(str,(unsigned char*)&data,2);

/*! ��������� �������������� ������ HEX � unsigned char
\param data- ������
\param str - ��������� �� ������� ������
*/
#define STR_HEX_TO_BYTE(str,data) str_hex_to_data(str,(unsigned char*)&data,1);

/*! ��������� �������������� ������ DEC � unsigned long
\param data- ������
\param str - ��������� �� ������� ������
*/
#define STR_DEC_TO_LONG(str,data) str_dec_to_data(str,(unsigned char*)&data,4);
/*! ��������� �������������� ������ DEC � unsigned short
\param data- ������
\param str - ��������� �� ������� ������
*/
#define STR_DEC_TO_SHORT(str,data) str_dec_to_data_to_hex(str,(unsigned char*)&data,2);
/*! ��������� �������������� ������ DEC � unsigned char
\param data- ������
\param str - ��������� �� ������� ������
*/
#define STR_DEC_TO_BYTE(str,data) str_dec_to_data((str,unsigned char*)&data,1);
/*! ��������� �������������� ������ BIN � unsigned long
\param data- ������
\param str - ��������� �� ������� ������
*/
#define STR_BIN_TO_LONG(str,data) str_bin_to_data(str,(unsigned char*)&data,4);
/*! ��������� �������������� ������ BIN � unsigned short
\param data- ������
\param str - ��������� �� ������� ������
*/
#define STR_BIN_TO_SHORT(str,data) str_bin_to_data_to_hex(str,(unsigned char*)&data,2);
/*! ��������� �������������� ������ BIN � unsigned char
\param data- ������
\param str - ��������� �� ������� ������
*/
#define STR_BIN_TO_BYTE(str,data) str_bin_to_data((str,unsigned char*)&data,1);

#define STR_BOOL_TO_LONG(str) 1
#define STR_LONG_TO_BOOL(str,data)

#define STR_BOOL_TO_DATA(str,data) str_bool_to_data(str,data)
#define STR_MASK_TO_STR(data,str) str_mask_to_str(data,str)
#define STR_STR_TO_MASK(str,data) str_str_to_mask(str,data)
#define STR_IP_TO_STR(data,str) str_ip_to_str(data,str)
#define STR_STR_TO_IP(str,data) str_str_to_ip(str,data)
#define STR_MAC_TO_STR(data,str) str_mac_to_str(data,str)
#define STR_STR_TO_MAC(str,data) str_str_to_mac(str,data)
#define STR_LENGHT(str) str_lenght(str)


#endif
//}@
