/*
* Модуль SNMP / MIB
*\autor P.Lyubasov
*v 2.5
*11.02.2010
*v 2.6-162
*14.04.2010
*mac as index in snmp table
*snmp_add_asn_unsigned(), snmp_add_asn_unsigned64()
*mib-ii handler for switch corrected
*v 2.7-162
*15.05.2010
*byte order of SN in snmp_find_device()
*IpAddress type
*v 2.8-162-51
* dksf51 #if added
*30.06.2010
*unsigned asn_get_unsigned()
*v2.9
*7.08.2010
*11.06.2010
* community bug in make_trap()
*v2.11-53
*12.06.2010
* specific-trap field value is changed from 0 to 1
*v2.12-53
*14.08.2010
* oid arrays are filled with 0xffff "background" for correct .0 scalars processing
* updated oid_cmp() for .0 scalars, match_index() function added
* minor bug in MIB-II (sysObjID print format)
*15.08.2010 merged 162-16p,53 untested
*v2.13-50
*28.09.2010 by LBS
*  in snmp_find_device(), proj_const[] was removed
*v2.14-50
*2.03.2012 by LBS
*  ported reboot_snmp_get(), reboot_snmp_set() from snmp.c v2.17-162
*v2.15-200
*6.12.2010
* added snmp_add_vbind_integer32()
* not used now!
*v2.16-52
*11.03.2011
* added variable UDP port for SNMP agent
*v2.17-50
*20.08.2012
*  add_vbind_email()
*v2.18-60
*10.10.2012
* bugfix in snmp_find_device()
*v2.18-50
*22.11.2012
*  removed arp_table[] reference
*  full MIB-2 ...system.sysXXXXX support
*  ignore Responce PDU
*v2.19-48
*4.04.2013
*  stdlib used, dev_sn replaced to &serial
*v2.19-52
*30.08.2013
* rewrite MIB-2 sysName, sysLocation, sysContact
*v2.20-48
*30.10.2013
* cosmetic snmp_send_trap(), don't send notif.e-mail if field is empty
*v2.21-707
*16.01.2014
* cosmetic interface changes
*v2.21-70
*19.03.2014
* ifDescr.1 added for NetPing
v2.22-70
15.05.2014
  notify module support
*/

#include "platform_setup.h"
#ifndef  SNMP_H
#define  SNMP_H
///Версия модуля
#define  SNMP_VER	2
///Сборка модуля
#define  SNMP_BUILD	22


//---------------- Раздел, где будут определяться константы модуля -------------------------
///UDP порт протокола SNMP
#define SNMP_PORT       0xA100 //161 reversed bytes
///UDP порт на который отправляются SNMP trap
#define SNMP_TRAP_PORT  0xA200 //162 reversed bytes
#define SNMP_BASE_SIGN 0x55AA

//Типы ASN1
#define SNMP_TYPE_BOOLEAN		1
#define SNMP_TYPE_INTEGER		2
#define SNMP_TYPE_BIT_STRING	        3
#define SNMP_TYPE_OCTET_STRING	        4
#define SNMP_TYPE_NULL			5
#define SNMP_OBJ_ID			6
#define SNMP_TYPE_SEQUENCE		16
#define SNMP_TYPE_IP_ADDRESS		0x40 //  LBS 15.05.2010
#define SNMP_TYPE_COUNTER		0x41
#define SNMP_TYPE_COUNTER32     	0x41
#define SNMP_TYPE_GAUGE 		0x42  // LBS 14.04.2010
#define SNMP_TYPE_GAUGE32 		0x42
#define SNMP_TYPE_COUNTER64     	0x46
#define SNMP_TYPE_UINTEGER32     	0x47

#define SNMP_PDU_GET                    0xA0  // "impied sequence" object type
#define SNMP_PDU_GET_NEXT               0xA1
#define SNMP_PDU_SET                    0xA3
#define SNMP_PDU_RESPONCE               0xA2
#define SNMP_PDU_TRAP_V1                0xA4
#define SNMP_PDU_TRAP_V2                0xA7

//Возможные номера ошибок
#define  SNMP_ERR_NO_ERROR		0
#define  SNMP_ERR_TOO_BIG               1
#define  SNMP_ERR_NO_SUCH_NAME	        2
#define  SNMP_ERR_BAD_VALUE		3
#define  SNMP_ERR_READ_ONLY		4
#define  SNMP_ERR_GEN_ERR		5
// доп.номера ошибок для внутреннего пользования
#define SNMP_ERR_WAITING_RESPONCE       6
#define SNMP_ERR_TIMEOUT                7
#define SNMP_ERR_DNS_IP                 8 // no valid ip known
#define SNMP_ERR_NO_HOST                9 // no host or ip in setup

//---------------- Раздел, где будут определяться структуры модуля -------------------------

struct oid_tree_s {     // LBS 12.2009
  unsigned short oid;
  unsigned char  root;  // index of previous node in OID  (.xxx.yyy.zzz.root.this_oid.nnn.eee)
  unsigned char  next;  // lexicographically next leaf index, ==0 if it's the last leaf or if it's not the leaf
  unsigned char  level; // index of this node in OID (depth in oid tree)
  unsigned char  flags;
  unsigned short id;    // id of resource, represented by tree leaf
};

// global data for SNMP Set/Get/GetNext handlers
struct snmp_data_s {     // LBS 14.04.2010
  unsigned id;           // resource id from leaf of OID tree
  unsigned short varid;  // table column id (based on, but not equal to, OID element before table index)
  unsigned short index;  // table index (number)
  unsigned char  octidx[6];// table index MAC etc., octet string up to 6 byte // NOT USED YET - 15.04.2010
  unsigned char  pdu;    // request PDU type, ASN.1 (get-request, set-request, get-next-request)
  unsigned char *rxdata; // pointer to data in varbind in set-request pdu
};

__no_init extern struct snmp_data_s snmp_data;

// oid tree node flags

#define OID_TREE_TABLE_LEGACY 0x01  // this variable is table column (legacy, index ORed with resource id)
#define OID_TREE_TABLE_INDEX  0x02

//---------------- Раздел, где будут определяться глобальные переменные модуля -------------

//---------------- Раздел, где будут определяться функции модуля ---------------------------

void snmp_init(void);
void snmp_exec(void);
void snmp_create_trap(unsigned char *enterprise);

/*! Процедура добавляет ASN1  в созданный SNMP trap пакет
* Прим: Код процедуры генериться если установлен флажок SNMP_TRAP_SUPPORT
\param type - тип объекта. Может быть :
   SNMP_TYPE_BOOLEAN	
   SNMP_TYPE_INTEGER
   SNMP_TYPE_BIT_STRING
   SNMP_TYPE_OCTET_STRING
   SNMP_TYPE_NULL
   SNMP_OBJ_ID
   SNMP_TYPE_SEQUENCE
\param len - размер объекта
\param data -указатель на данные которые будут добавлены в объект ASN1
*/
extern void snmp_add_asn_obj(uword type,uword len, unsigned char *data);


void snmp_send_trap(unsigned char *ip);

unsigned int get_asn_len(unsigned char* ptr);
uword get_len_type(unsigned short len);
void snmp_send_response(void);
unsigned short skip_head(void);
extern void get_len_asn1(void);
uword make_oid(unsigned char *buf);
void set_oid(void);
void get_string(unsigned char *ptr1, unsigned char *ptr2);
void send_error(uword type);

/*! Идентификатор пакета собираемого SNMP ответа
*/
extern uword snmp_ds;


// LBS 07.2009

/*! adds signed integer in ASN.1 format - LBS 08.2009
*/
void snmp_add_asn_timestamp(unsigned val);

/*! adds ASN.1 encoded signed integer val to output packet (snmp_ds)
*/
void snmp_add_asn_integer(int val);

/*! adds unsigned 32-bit in ASN.1 format - LBS 04.2010
    type = added ASN type code, raw byte (Counter32, Gauge32, Unsigned32 etc.)
*/
void snmp_add_asn_unsigned(unsigned char type, unsigned val); // 04.2010

/*! adds unsigned 64-bit in ASN.1 format - LBS 04.2010
    type = added ASN type code, raw byte (Counter64, Gauge64, Unsigned64 etc.)
*/
void snmp_add_asn_unsigned64 (unsigned char type, unsigned long long v); // 04.2010


/*! adds count raw bytes to output packet (snmp_ds)
    use it like this: snmp_add_raw_bytes(4, 0x04, 2, 'a', 'b');
*/
void snmp_add_raw_bytes(int count, ... );

/*! Adds asn.1 sequence to output packet.
   Returns position of sequence content
   Use it in conjuction with snmp_close_seq()
*/
unsigned snmp_add_seq(void);

/*! Writes 2-byte length to asn.1 sequence.
   content_pos points to start of sequence content.
   Use it in conjuction with snmp_add_seq()
*/
void snmp_close_seq(unsigned content_pos);


/*! Writes varbind with ASN.1 INTEGER value
   oid points to ASN.1 OID OBJECT (with 1-byte index == 0..127!!!)
   index substituted as new table index in copy of oid[] (up to 2^16-1 !!!!)
   value is varbind value

   НЕ ПРОВЕРНО И НЕ ОТЛАЖЕНО!

*/
// void snmp_add_vbind_integer(unsigned char *oid, unsigned short index, int value);


/*! adds varbind to trap packet, pointed by global index snmp_ds
    enterprise = TRAP enterprise, full OID object, used as oid
    last_oid_component - must be <128, concatenated at the end of enterprise
    value - varbind value
*/
void snmp_add_vbind_integer32(const unsigned char *enterprise, unsigned char last_oid_component, int value);

/*! custom handler to return BER Null value.
    May be used as dummy get and set handler.
*/
void snmp_handler_get_null(upointer addr, uword struct_type);

/*! отдаёт некоторые стандартные ветки mib-2
*/
void mibii_get_handler(upointer addr, uword struct_type);
int mibii_set_handler(unsigned id, unsigned char *data);


// parse ASN.1 OID at *data, put components in oid[], number of components write to *oid_len
unsigned asn_decode_oid(unsigned char *data, unsigned short *oid, unsigned *oid_len);

/*! Parse ASN.1 integer value from buffer p, place into value.
    Returns length of ASN.1 data in buffer p for this integer.
*/
unsigned asn_get_integer(unsigned char *p, int *value);

/*! Parse ASN.1 unsigned value from buffer p, place into value.
    Checks type, if no match, *value = 0
    Returns length of ASN.1 data in buffer p for this integer.
*/
unsigned asn_get_unsigned(unsigned char *p, unsigned *value);

void parse_snmp(void);

/*! after processing of rx snmp packet,
    bit 0 = read granted
    bit 1 = write granted
    bit 2 = update/setup granted
*/
extern unsigned char community_check_result;

/*! community is pascal string
*/
unsigned char check_comunity(unsigned char *community);

/*  reboot SNMP handler (branch .npReboot (911) )
*/
int reboot_snmp_get(unsigned id, unsigned char *data);
int reboot_snmp_set(unsigned id, unsigned char *data);

/*  Decodes ASN.1 variable data field length.
    Returns number of bytes to skip on *p (length of length field).
*/
unsigned asn_get_length(unsigned char *p, unsigned *length);

void add_vbind_email(void); // LBS 20/08/2012

#endif

