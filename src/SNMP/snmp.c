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
#ifdef SNMP_MODULE

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "eeprom_map.h"

//#include "dksf/mib_tree.c"
#include "mib_tree.c"

#ifndef SNMP_DEBUG
	
	#undef DEBUG_SHOW_TIME_LINE
	#undef DEBUG_MSG			
	#undef DEBUG_PROC_START
	#undef DEBUG_PROC_END
	#undef DEBUG_INPUT_PARAM
        #undef DEBUG_OUTPUT_PARAM

	#define DEBUG_SHOW_TIME_LINE
	#define DEBUG_MSG(...)        // LBS 06.2009			
	#define DEBUG_PROC_START(msg)
	#define DEBUG_PROC_END(msg)
	#define DEBUG_INPUT_PARAM(msg,val)	
        #define DEBUG_OUTPUT_PARAM(msg,val)	
#endif

/* legacy
 const struct exec_queue_rec snmp_init_table[]={(upointer)snmp_init,SNMP_INIT1_PRI|LAST_REC};
 const struct module_rec snmp_struct={(upointer)snmp_init_table, NULL, NULL};
 const struct module_rec snmp_struct={NULL, NULL, NULL};
*/

unsigned snmp_ds = 0xff;
unsigned snmp_tx_error_pos;
unsigned char community_check_result = 0;
__no_init struct snmp_data_s snmp_data;

int snmp_data_handler(unsigned pdu_mode, unsigned short id, unsigned char *p);


void snmp_add_asn_obj(uword type, uword len, unsigned char *data)
{
  unsigned char tmp[6];
  uword tmp_len;

  tmp[0] = type;
  tmp_len = 4;

  if((type&0xA0)|| (len > 255))
  {
     tmp[1] = 0x82;  // asn.1 length "long form"
     tmp[2] = (len>>8);
     tmp[3] = (unsigned char)len;
  }
  else if(type == SNMP_TYPE_INTEGER)
  {  // LBS 12.2009
     unsigned v = 0;
     if(len>4) len=4;
     util_cpy((void*)data, (void*)&v, len);
     snmp_add_asn_integer(v);
     return;
  }
  else
  {
    if(len > 255) len = 255; // LBS 07.2009, crop size
    if(len > 127)
    { // asn.1 length "long form"
      tmp[1] = 0x81; // one byte value
      tmp[2] = len;  // len value
      tmp_len = 3;
    }
    else
    { // asn.1 length "short form"
      tmp[1] = len;
      tmp_len = 2;
    }
  }
  _UDP_PUT_TX_BODY(snmp_ds, tmp, tmp_len);
  // LBS 07.2009
  //if(data)_UDP_PUT_TX_BODY(snmp_ds, data, len);
  if(data && len) _UDP_PUT_TX_BODY(snmp_ds, data, len);
  //
  DEBUG_PROC_END("snmp_add_asn_obj");
}


unsigned char system_oid[] =
{ 43,6,1,4,1,0x81,0xc9,0x00 };
/*
unsigned char system_oid[] =
{ 43,6,1,4,1,0x81,0xc9,0x00,PROJECT_MODEL,PROJECT_VER,PROJECT_BUILD,PROJECT_CHAR-'A'+40,PROJECT_ASSM };
*/


#if PROJECT_MODEL==160 || PROJECT_MODEL==161 || PROJECT_MODEL==162 || PROJECT_MODEL == 163 // i.e. Switch

void mibii_get_handler(upointer addr, uword struct_type)
{
  char buf[64];
  int n;
  /*
  Аргумент addr на самом деле - Идентификатор ресурса из OID таблицы в DI файле
  Значения шестнадцатеричные!
  */
  switch(addr)
  {
  case 0x314: // ...system.sysDescr
    n = sprintf(buf, "%s, FW v%d.%d.%d.%c-%d",
        device_name,
        PROJECT_MODEL, PROJECT_VER, PROJECT_BUILD, PROJECT_CHAR, PROJECT_ASSM);
    snmp_add_asn_obj(SNMP_TYPE_OCTET_STRING, n, (void*)buf);
    break;
  case 0x332: // ...system.sysObjectID
    snmp_add_asn_obj(SNMP_OBJ_ID, sizeof system_oid, system_oid);
    break;
  case 0x333: // ...system.sysUpTime
    snmp_add_asn_timestamp(sys_clock() / 10); // 10 ms ticks
    break;
  case 0x337: // ...system.sysServices
    snmp_add_asn_integer(64+8+2); // Application + Host + Bridge
    break;
  case 0x301: // ...Lightcom.Dev_id.SYS_SN
    snmp_add_asn_integer(serial & 0x3ffff);
    break;
  default:
    snmp_add_raw_bytes(2,0x05,0); // null
    break;
  }
}
#else // NetPings
void mibii_get_handler(upointer addr, uword struct_type)
{
  unsigned char buf[64];
  int n;
  /*
  Аргумент addr на самом деле - Идентификатор ресурса из OID таблицы в DI файле
  Значения шестнадцатеричные!
  */
  switch(addr)
  {
  case 0x314: // ...system.sysDescr
    n = sprintf((char*)buf, "%s, FW v%d.%d.%d.%c-%d",
        device_name,
        PROJECT_MODEL, PROJECT_VER, PROJECT_BUILD, PROJECT_CHAR, PROJECT_ASSM);
    snmp_add_asn_obj(SNMP_TYPE_OCTET_STRING, n, (void*)buf);
    break;
  case 0x332: // ...system.sysObjectID
    snmp_add_asn_obj(SNMP_OBJ_ID, sizeof system_oid, system_oid);
    break;
  case 0x333: // ...system.sysUpTime
    snmp_add_asn_timestamp(sys_clock() / 10); // 10 ms ticks
    break;
  case 0x334: // ...system.sysContact
    //EEPROM_READ(eeprom_sysContact, buf, sizeof buf);
    snmp_add_asn_obj(SNMP_TYPE_OCTET_STRING, sys_setup.contact[0], sys_setup.contact + 1);
    break;
  case 0x335: // ...system.sysName
    //EEPROM_READ(eeprom_sysName, buf, sizeof buf);
    snmp_add_asn_obj(SNMP_TYPE_OCTET_STRING, sys_setup.hostname[0], sys_setup.hostname + 1);
    break;
  case 0x336: // ...system.sysLocation
    //EEPROM_READ(eeprom_sysLocation, buf, sizeof buf);
    snmp_add_asn_obj(SNMP_TYPE_OCTET_STRING, sys_setup.location[0], sys_setup.location + 1);
    break;
  case 0x337: // ...system.sysServices
    snmp_add_asn_integer(72); // Application host // LBS 16.04.2010, it was 64
    break;
  case 0x350: // ...system.ifNumber
    snmp_add_asn_integer(1);
    break;
  case 0x351: // ...system.ifTable.ifEntry.ifIndex
    snmp_add_asn_integer(1); // only 1 network interface
    break;
  case 0x352: // ...system.ifTable.ifDescr.ifIndex // 19.03.2014
    n = sprintf((char*)buf, "%s Enet Port", device_name);
    snmp_add_asn_obj(SNMP_TYPE_OCTET_STRING, n, (void*)buf);
    break;
  case 0x353: //  ...system.ifTable.ifEntry.ifType
    snmp_add_asn_integer(6); // ethernetCsmacd(6)
    break;
  case 0x354: // ...system.ifTable.ifEntry.ifMtu
    snmp_add_asn_integer(1514);
    break;
  case 0x355: // ...system.ifTable.ifEntry.ifSpeed
    snmp_add_asn_integer(100000000);
    break;
  case 0x303: // ...system.ifTable.ifEntry.ifMac
    snmp_add_asn_obj(SNMP_TYPE_OCTET_STRING, 6, sys_setup.mac); // 11.11.2012, removed arp_table[]
    break;
  case 0x301: // ...Lightcom.Dev_id.SYS_SN
    snmp_add_asn_integer(serial & 0x3ffff);
    break;
  default:
    snmp_add_raw_bytes(2,0x05,0); // null
    break;
  }
}

int mibii_set_handler(unsigned id, unsigned char *data)
{
  char buf[64];
  unsigned len;

  if(*data++ != SNMP_TYPE_OCTET_STRING) return SNMP_ERR_BAD_VALUE;
  data += asn_get_length(data, &len);
  if(len > sizeof buf - 2) len = sizeof buf - 2;
  memset(buf, 0, sizeof buf);
  memcpy(buf + 1, data, len);
  buf[0] = len;

  switch(id)
  {
  case 0x0334: // ...system.sysContact
    //EEPROM_WRITE(eeprom_sysContact, buf, sizeof eeprom_sysContact);
    memcpy(sys_setup.contact, buf, sizeof sys_setup.contact);
    EEPROM_WRITE(eeprom_sys_setup.contact, buf, sizeof eeprom_sys_setup.contact);
    break;
  case 0x0335: // ...system.sysName
    //EEPROM_WRITE(eeprom_sysName, buf, sizeof eeprom_sysName);
    memcpy(sys_setup.hostname, buf, sizeof sys_setup.hostname);
    EEPROM_WRITE(eeprom_sys_setup.hostname, buf, sizeof eeprom_sys_setup.hostname);
    break;
  case 0x0336: // ...system.sysLocation
    //EEPROM_WRITE(eeprom_sysLocation, buf, sizeof eeprom_sysLocation);
    memcpy(sys_setup.location, buf, sizeof sys_setup.location);
    EEPROM_WRITE(eeprom_sys_setup.location, buf, sizeof eeprom_sys_setup.location);
    break;
  default: return SNMP_ERR_READ_ONLY;
  }

  snmp_add_asn_obj(SNMP_TYPE_OCTET_STRING, len, (unsigned char*)buf + 1);
  return 0;
}

#endif // MIB-II handler



/*  LBS 08.2009 --------------------------------------------------
*/

void snmp_add_asn_timestamp(unsigned val)
{
  unsigned char buf[8];
  unsigned i, len;
  buf[0] = 0x43; // type: Application 3 (Timestamp)
  len = 0;
  // drop excessive most significant bytes as per ASN.1 BER
  for(i=0; i<3; ++i, val<<=8) // было i<4 - bug! исправлено 12.2009
  {
      if((val & 0xff800000) != 0x00000000) break;
  }
  // write value, MSB first
  for(   ; i<4; ++i, val<<=8)
  {
    buf[2+len] = val >> 24;
    ++len;
  }
  buf[1] = len;
  _UDP_PUT_TX_BODY(snmp_ds, buf, len+2);
}


/* adds signed integer in ASN.1 format - LBS 07.2009
*/
void snmp_add_asn_integer(int val)
{
  unsigned char buf[8];
  unsigned i, u, len;
  buf[0] = SNMP_TYPE_INTEGER;
  u = (unsigned)val;
  len = 0;
  // drop excessive most significant bytes as per ASN.1 BER
  for(i=0; i<3; ++i, u<<=8) // было i<4 - bug! исправлено 09.2009
  {
      if(val < 0 && (u & 0xff800000) != 0xff800000) break;
      if(val >=0 && (u & 0xff800000) != 0x00000000) break;
  }
  // write value, MSB first
  for(   ; i<4; ++i, u<<=8)
  {
    buf[2+len] = u >> 24;
    ++len;
  }
  buf[1] = len;
  _UDP_PUT_TX_BODY(snmp_ds, buf, len+2);
}

/* adds unsigned 32-bit in ASN.1 format - LBS 04.2010
   type = added ASN type code, raw byte (Counter32, Gauge32, Unsigned32 etc.)
*/
void snmp_add_asn_unsigned(unsigned char type, unsigned val)
{
  unsigned char buf[8];
  unsigned i, u, len;
  buf[0] = type;
  u = val;
  len = 0;
  // drop excessive most significant bytes as per ASN.1 BER
  for(i=0; i<3; ++i, u<<=8) // было i<4 - bug! исправлено 09.2009
  {
    if((u & 0xff000000) != 0x00000000) break;
  }
  // add zero if 'sign' bit is set
  if(u & 0x80000000)
  {
    buf[2+len] = 0; len++;
  }
  // write value, MSB first
  for(   ; i<4; ++i, u<<=8)
  {
    buf[2+len] = u >> 24;
    ++len;
  }
  buf[1] = len;
  _UDP_PUT_TX_BODY(snmp_ds, buf, len+2);
}

/* adds unsigned 64-bit in ASN.1 format - LBS 04.2010
   type = added ASN type code, raw byte (Counter64, Gauge64, Unsigned64 etc.)
*/
void snmp_add_asn_unsigned64 (unsigned char type, unsigned long long v)
{
  unsigned char buf[16];
  unsigned i, len;
  buf[0] = type;
  len = 0;
  // drop excessive most significant bytes as per ASN.1 BER
  for(i=0; i<7; ++i, v<<=8)
  {
    if((v & 0xff00000000000000) != 0) break;
  }
  // add zero if 'sign' bit is set
  if((signed long long)v < 0)
  {
    buf[2+len] = 0; len++;
  }
  // write value, MSB first
  for(   ; i<8; ++i, v<<=8)
  {
    buf[2+len] = v >> (24+32);
    ++len;
  }
  buf[1] = len;
  _UDP_PUT_TX_BODY(snmp_ds, buf, len+2);
}

void snmp_add_raw_bytes(int count, ... )
{
  va_list ap;
  int j;
  typedef unsigned char uchar;

  va_start(ap, count); //Requires the last fixed parameter (to get the address)
  unsigned char b;
  for(j=0; j<count; j++)
  {
    b = va_arg(ap, uchar); //Requires the type to cast to. Increments ap to the next argument.
    _UDP_PUT_TX_BODY(snmp_ds, &b, 1);
  }
  va_end(ap);
}

/* Adds asn.1 sequence to output packet.
   Returns position of sequence content
   Use it in conjuction with snmp_close_seq()
*/
unsigned snmp_add_seq(void)
{
  snmp_add_raw_bytes(4, 0x30, 0x82, 0, 0);
  return _UDP_TX_BODY_POINTER;
}

/* Writes 2-byte length to asn.1 sequence.
   content_pos points to start of sequence content.
   Use it in conjuction with snmp_add_seq()
*/
void snmp_close_seq(unsigned content_pos)
{
  unsigned short len = _UDP_TX_BODY_POINTER - content_pos;
  unsigned saved = _UDP_TX_BODY_POINTER;
  _UDP_TX_BODY_POINTER = content_pos - 2;
  snmp_add_raw_bytes(2, len >> 8, len & 0xFF);
  _UDP_TX_BODY_POINTER = saved;
}

/* Writes ASN.1 encoded length or oid point
   (bit 7 = "will be next byte" flag, bits 6..0 - part of value)
   Returns number of bytes added (1..3)
*/
int snmp_add_asn_number(unsigned short value)
{
  unsigned u = value;
  int len = 3;
  if(u >= 1<<14) snmp_add_raw_bytes(1, 0x80 | (u>>14)); // indirectly cast to byte
  else --len;
  if(u >= 1<< 7) snmp_add_raw_bytes(1, 0x80 | (u>> 7));
  else --len;
  snmp_add_raw_bytes(1, 0x80 | u);
  return len;
}

/* Writes varbind with ASN.1 INTEGER value
   oid points to ASN.1 OID OBJECT (with 1-byte index == 0..127!!!)
   index substituted as new table index in copy of oid[] (up to 2^16-1 !!!!)
   value is varbind value

   НЕ ПРОВЕРНО И НЕ ОТЛАЖЕНО!

*/
/*
void snmp_add_vbind_integer(unsigned char *oid, unsigned short index, int value)
{
  unsigned char oidbuf[64];
  unsigned seq_ptr;
  int oidlen;

  if(oid[0] != SNMP_OBJ_ID) return;
  oidlen = oid[1];
  if(oidlen<2) return;
  if(oidlen > sizeof oidbuf - 4) return;
  oidlen -= 1; // drop 1-byte index
  util_cpy(oid+2, oidbuf, oidlen);
  // add index to oidbuf
#error "it's bug!"
  if(index >= 1<<14) oidbuf[oidlen++] = 0x80 | (index>>14); // cast indirectly to byte
  if(index >= 1<< 7) oidbuf[oidlen++] = 0x80 | (index>> 7);
#error "it's bug!"
  oidbuf[oidlen++] = 0x80 | index; // cast indirectly to byte (get 7 LSB)
  // add warbind
  seq_ptr = snmp_add_seq();
  snmp_add_asn_obj(SNMP_OBJ_ID, oidlen, oidbuf);
  snmp_add_asn_integer(value);
  snmp_close_seq(seq_ptr);
}
*/


///// вариант с генерацией полных oid фикс. длины с 0 на месте пустых эл-тов

// include MIB tree resource (in the head of file)
// const struct oid_tree_s oid_tree[] = { ... } ;
// const unsigned short oid_tree_root_idx = ... ;

//  oid_tree[0].root = index of lexicographically first leaf
// elements of OID in use must be != 0, except the last number (i.e. scalar variable designation). It's in accordance of RFC1155.

void get_oid_from_tree(int tree_idx, unsigned short *oid, unsigned *oid_len)
{
  const struct oid_tree_s *tree = &oid_tree[tree_idx];
  util_fill((void*)oid, 2*MAX_OID_LEN, 0xff); // LBS 14.08.2010
  *oid_len = tree->level + 1;
  for(;;)
  {
    oid[tree->level] = tree->oid;
    if(tree->level == 0) break;
    tree = &oid_tree[tree->root];
  }
}


unsigned short match_index(int get_next_flag, unsigned short index, unsigned max_index) // LBS 14.08.2010
{
  if(get_next_flag)
  {
    if(index == 0xffff) index = 1;
    else index += 1;
  }
  if(index > max_index) index = max_index;
  return index;
}



int oid_cmp(int get_next_flag, unsigned short *oid_a, unsigned short *oid_b)
{
  unsigned short max_idx;
#ifdef MAX_U_PORT  // for NetSwitch only
  unsigned char mac_as_idx[6];
#endif
  int result = 0;
  for(int n=0; n<MAX_OID_LEN; ++n)
  {
    if(oid_a[n] >= 0x8000) // LBS 14.08.2010
    {
      switch(oid_a[n] & 0xf000)
      {
      case 0x8000: // legacy fixed size table with natural index
        max_idx = oid_a[n] & 0x7fff;
        oid_a[n] = match_index(get_next_flag, oid_b[n], max_idx);
        break;
#ifdef MAX_U_PORT  // for NetSwitch only
      case 0xa000: // uport index (1-based UI port number w/o cpu) - DKSF160-162
        oid_a[n] = match_index(get_next_flag, oid_b[n], MAX_U_PORT);
        break;
      case 0x9000: // index is mac (6-byte oct.string), mib handler 0 (ATU)
        for(unsigned i=0;i<6;++i) mac_as_idx[i] = oid_b[n + i];
        atu_find_next_mac(mac_as_idx); // retuns all-zero mac if no next entry
        for(unsigned i=0;i<6;++i) if(n + i < MAX_OID_LEN) oid_a[n + i] = mac_as_idx[i]; // protected index
        break;
#endif
      }
    }
    if(oid_a[n] == oid_b[n]) continue;
    if((signed short)oid_a[n] >  (signed short)oid_b[n]) { result =  1; break; } // (signed short) - LBS 14.08.2010
    if((signed short)oid_a[n] <  (signed short)oid_b[n]) { result = -1; break; }
  }
  // post-process unresolved match elements
  for(int n=0; n<MAX_OID_LEN;++n)
  {
    if(oid_a[n] >= 0x8000) // LBS 14.08.2010
    {
      switch(oid_a[n] & 0xf000)
      {
      case 0x8000: // fixed size table with natural index
      case 0xa000: // uport index (1-based UI port number w/o cpu) - DKSF160-162
        oid_a[n] = 1; // set unprocessed index to 1, first fixed table entry
        break;
#ifdef MAX_U_PORT   // for NetSwitch only
      case 0x9000: // index is mac (6-byte oct.string), mib handler 0 (ATU)
        util_fill(mac_as_idx, 6, 0xff); // six 0xff = new search, find first mac in ATU
        atu_find_next_mac(mac_as_idx);
        for(unsigned i=0;i<6;++i) if(n + i < MAX_OID_LEN) oid_a[n + i] = mac_as_idx[i]; // protected index
        break;
#endif
      } // switch
    } // if
  } // for
  return result;
}


// in_oid, out_oid -> unsigned short [MAX_OID_LEN]
int find_oid(int get_next_flag, unsigned short *in_oid, unsigned short *out_oid, unsigned *out_oid_len)
{
  int n = oid_tree_root_idx;
  int f;
  for(;;)
  {
    get_oid_from_tree(n, out_oid, out_oid_len);

    f = oid_cmp(get_next_flag, out_oid, in_oid);
    if(get_next_flag!=0 && f> 0 ) return n;
    if(get_next_flag==0 && f==0 ) return n;
    n = oid_tree[n].next;
    if(n > sizeof oid_tree / sizeof oid_tree[0]) return -1; // aux protection
    if(n==0) return -1; // next==0 -> the last oid in lexicographic list
  }
}


void snmp_create_responce_packet(void)
{
  snmp_ds = udp_create_packet();
  if(snmp_ds == 0xff) return;
  ip_get_tx_header(snmp_ds);
  util_cpy(ip_head_rx.src_ip, ip_head_tx.dest_ip, 4);
  ip_put_tx_header(snmp_ds);
  *(unsigned short*)udp_tx_head.dest_port = *(unsigned short*)udp_rx_head.src_port;
  //#warning "fixed SNMP port!"
  //*(unsigned short*)udp_tx_head.src_port  = SNMP_PORT;
  udp_tx_head.src_port[0] = sys_setup.snmp_port >> 8; // 11.03.2011
  udp_tx_head.src_port[1] = sys_setup.snmp_port & 0xff;
  udp_put_tx_header(snmp_ds);
}

void snmp_set_err(unsigned tx_body_pos, int errcode, int errindex)
{
  unsigned char t;
  if(tx_body_pos == 0) return;
  unsigned saved = udp_tx_body_pointer;
  udp_tx_body_pointer = tx_body_pos + 2;
  t = errcode;
  udp_put_tx_body(snmp_ds, &t, 1);  // implied 1-byte ASN integer (added to tx udp as 0)
  udp_tx_body_pointer = tx_body_pos + 5;
  t = errindex;
  udp_put_tx_body(snmp_ds, &t, 1);  // implied 1-byte ASN integer (added to tx udp as 0)
  udp_tx_body_pointer = saved;
}

unsigned asn_get_length(unsigned char *p, unsigned *length)
{
  int n = *p++;
  if(n < 128) { *length = n; return 1; } // short form
  else n &= 0x7f;
  // long form
  unsigned len = 0;
  for(int i=0; i<n; ++i) len = (len<<8) + *p++;
  *length = len;
  return n + 1;
}


unsigned asn_get_unsigned(unsigned char *p, unsigned *value) // LBS 30.06.2010
{
  unsigned char type;
  unsigned len;
  unsigned i, val;

  *value = 0;
  type = *p++;
  p += asn_get_length(p, &len);
  switch(type)
  {
  case SNMP_TYPE_INTEGER:
  case SNMP_TYPE_GAUGE32:
  case SNMP_TYPE_COUNTER32:
    if(len > 4) break;
    val = 0;
    for(i=0; i<len; ++i)
    {
      val <<= 8;
      val |= *p++;
    }
    *value = val;
    break;
  }
  return len + 2;
}

unsigned asn_get_integer(unsigned char *p, int *value)
{
  unsigned char b;
  unsigned len;
  int i, val;

  *value = 0;
  b = *p++;
  p += asn_get_length(p, &len);
  if(b == 0x02)
  {
    if(len <= 4)
    {
      for(i=0; i<len; ++i)
      {
        b = *p++;
        if(i==0) val = b&0x80 ? -1 : 0; // sign
        val <<= 8;
        val |= b;
      }
      *value = val;
    }
  }
  return len + 2;
}

unsigned asn_decode_oid(unsigned char *data, unsigned short *oid, unsigned *oid_len)
{
  unsigned pos = 0;
  unsigned len, n;
  unsigned char *p, *dataend;
  p = data + 1;
  p += asn_get_length(p, &len);
  dataend = p + len;
  util_fill((void*)oid, MAX_OID_LEN*2, 0xff); // LBS 14.08.2010
  if(*data == 0x06 && // type = ObjectId
     *p == 43 &&       // 1st octet = oid .1.3
     len > 1)          // len is ok
  {
    oid[pos++] = 1; oid[pos++] = 3; ++p; // fill .1.3
    do {
      n = 0;
      do { n = (n<<7) + (*p & 0x7f); } while(*p++ & 0x80); // decode 7-bit "bytes"
      if(n > 0x7fff) n = 0x7fff; // this application limitation for 15 bit
      oid[pos++] = n;
    } while(p < dataend && pos < MAX_OID_LEN);
  }
  *oid_len = pos;
  return dataend - data;
}

void snmp_add_oid(int out_oid_length, unsigned short *oid)
{
  unsigned char buf[36];
  unsigned char *p = buf;
  unsigned n;
  *p++ = 0x06;
  *p++ = 0;
  *p++ = 43;
  for(int i=2; i<out_oid_length; ++i)
  {
    n = oid[i];
    if(n & 0xC000) *p++ = (n >> 14) | 0x80;
    if(n & 0xFF80) *p++ = (n >>  7) | 0x80;
    *p++ = n & 0x7F;
  }
  buf[1] = p - &buf[2];
  udp_put_tx_body(snmp_ds, buf, buf[1] + 2);
}

int snmp_find_device(unsigned char *data)
{
  unsigned char buf[96];
  unsigned n, tmp;
  if(*data++ != SNMP_TYPE_OCTET_STRING) return 0;
  data += asn_get_length(data, &tmp); // skip length
  if(*data++ != 1) return 0; // version
  n = *data++;
  if(n > 100) n = 100;
  for(int i=0; i<n; ++i, data+=4)
    //if(dev_sn[3]==data[0] && dev_sn[2]==data[1] && dev_sn[1]==data[2] && dev_sn[0]==data[3])
    if(memcmp(data, &serial, 4)==0) // 15.05.2010 SN byte order
      return -1; // marked as already found
  // send device info
  unsigned char *p = buf;
  *p++ = 1; // ver 1
  *p++ = 0; // no error
  memcpy(p, &serial, 4); p += 4; // Little endian (Rev2 31.03.2010 Doc)
#if PROJECT_MODEL==53
  *p++ = 50&0xff;  // DKSF53 lack NPCONF support
  *p++ = 50>>8;
  *p++ = 77;
#else
  *p++ = PROJECT_MODEL & 0xff;
  *p++ = PROJECT_MODEL >> 8;
  *p++ = PROJECT_VER;
#endif
  //
  *p++ = PROJECT_BUILD;
  *p++ = PROJECT_CHAR;
  *p++ = PROJECT_ASSM;
  util_cpy(sys_setup.ip, p, 4);   p += 4;  // not in accordance with doc
  util_cpy(sys_setup.mac, p, 6);  p += 6;  // not in accordance with doc
  unsigned char *lenp = p++; // bugfix 10.10.2012, it was no length byte as required by 'netping snmp update standart'
  p += print_dev_name_fw_ver((char*)p);
  *lenp = p - lenp; // insert description length
  n = p - buf;
  snmp_add_asn_obj(SNMP_TYPE_OCTET_STRING, n, buf);
  return 1;
}


int reboot_snmp_get(unsigned id, unsigned char *data)
{
  int val = 0;
  switch(id)
  {
  case 0xebb1: val = main_reboot; break; // .npReboot.npSoftReboot
  case 0xebb2: val = main_reload; break; // .npReboot.npResetStack
  case 0xebb3: val = 0;           break; // .npReboot.npForcedReboot // reboot_proc() - forced reboot by WDT
  default:
    return SNMP_ERR_NO_SUCH_NAME;
  }
  snmp_add_asn_integer(val);
  return 0;
}

int reboot_snmp_set(unsigned id, unsigned char *data)
{
  int val = 0;
  switch(id)
  {
  case 0xebb1: main_reboot = 1; break;  // .npReboot.npSoftReboot
  case 0xebb2: main_reload = 1; break;  // .npReboot.npResetStack
  case 0xebb3: reboot_proc();   break;  // .npReboot.npForcedReboot // reboot_proc() - forced reboot by WDT
  default:
    return SNMP_ERR_NO_SUCH_NAME;
  }
  snmp_add_asn_integer(val);
  return 0;
}


// LBS 02.2010 modifications for NPCONF xxxx.lightcom.model.assm.x OIDs
const unsigned short fw_oid[] = {1,3,6,1,4,1,25728/*,0*/};

int check_fw_update_oid(unsigned pdu_mode, unsigned short *oid, unsigned oid_length)
{
  if(pdu_mode == 0xA0 || pdu_mode==0xA3) ; // Get or SetRequest
  else return 0;
  if(memcmp((void*)oid, (void*)fw_oid, sizeof fw_oid) != 0) return 0;
  if(oid_length != 10) return 0;
  switch(oid[7]) // LBS 02.2010 modifications for NPCONF xxxx.lightcom.model.assm.x OIDs
  {
  case 0:
    switch(oid[8])
    {
    case 1:
      switch(oid[9])
      {
      case 0:
        return 0xff10;
      }
      break;
    case 2:
      switch(oid[9])
      {
      case 1:
      case 2:
      case 3:
      case 4:
        return 0xff20 | oid[9];
      }
      break;
    case 3:
    case 4:
      if(pdu_mode == 0xA3 /*set*/) return 0xff00 | (oid[8]<<4);
      break;
    case 5:
      if(pdu_mode != 0xA3 /*set*/) return 0;
      switch( oid[9] )
      {
      case 0: // SN
        return 0xffa0;
      case 1: // ip
        return 0xffa1;
      case 2: // mac
        return 0xffa4;
      }
      break;
    }
    return 0;
  case PROJECT_MODEL: // oid[7]
  /// case 50: // DEBUGGGGGGG - NPCONF don't know model '53'
    if(oid[8] != PROJECT_ASSM) return 0;
    switch( oid[9] )
    {
    case 1: // ip
    case 2: // mask
    case 3: // gate
    case 4: // mac
      return 0xffa0 | oid[9];
    }
    break;
  } // switch( oid[7] )
  return 0;
}

int parse_fw_update_commands(unsigned pdu_mode, unsigned short *oid, unsigned oid_length, unsigned char *data, unsigned id)
{
  unsigned val;
  if(pdu_mode == 0xA0 || pdu_mode==0xA3) ; // Get or SetRequest
  else return 0;
  switch(id)
  {
  case 0xff10: // dev search
    return snmp_find_device(data);
  case 0xff21:
    snmp_add_asn_integer(SNMP_FW_UPDATE_BLOCK);
    break;
  case 0xff22:
    snmp_add_asn_integer(SNMP_MW_UPDATE_BLOCK);
    break;
  case 0xff23:
    snmp_add_asn_integer(1);
    break;
  case 0xff24:
    val = 0;
    if(pdu_mode == 0xA3 /*set*/)
    {
      data += asn_get_integer(data, (int*)&val);
      switch(val)
      {
      case 1:
        { SNMP_RESET_HANDLER; }
        break;
      case 2:
        { SNMP_RELOAD_HANDLER; }
        break;
      }
    }
    snmp_add_asn_integer(val);
    break;
  case 0xff30:
    if(pdu_mode != 0xA3 /*set*/) return 0;
    if( oid[9] == 0)
    {
      data += asn_get_integer(data, (int*)&val);
      ///////// TODO place CRC in eeprom!!!!!!!!!!!!!!!!!!!!!!!!!!!!
      // TODO crc check
      start_fw_update(val);
    }
    else
    {
      data++;
      data += asn_get_length(data, &val);
      if(val != SNMP_FW_UPDATE_BLOCK) return 0;
      load_fw_block(oid[9]-1, data);
    }
    snmp_add_asn_integer(0);
    break;
  case 0xff40:
    if(pdu_mode != 0xA3 /*set*/) return 0;
    if( oid[9] == 0)
    {
      data += asn_get_integer(data, (int*)&val);
      // TODO crc check
      start_mw_update();
    }
    else
    {
      data++;
      data += asn_get_length(data, &val);
      if(val != SNMP_MW_UPDATE_BLOCK) return 0;
      load_mw_block(oid[9]-1, data);
    }
    snmp_add_asn_integer(0);
    break;
  case 0xffa0:
  case 0xffa1:
  case 0xffa2:
  case 0xffa3:
  case 0xffa4:
    if(pdu_mode == 0xA3 /*set*/)
    {
      unsigned char type = data[0];
      unsigned char len =  data[1];
      unsigned char *d =   data+2;
      switch( id & 0x000f )
      {
      case 0:
        if(type != 0x04 || len != 4) return 0; // data type & len
        SNMP_SET_SN(d);
        break;
      case 1:
        if(type==0x40 || type==0x04) ; else return 0;
        if(len != 4) return 0;
        //SNMP_SET_IP(d);
        util_cpy(d, sys_setup.ip, len);
        EEPROM_WRITE(&eeprom_sys_setup.ip, d, len); // sorry, size optimization
        break;
      case 2:
        if(type != 2 || len != 1) return 0;
        sys_setup.mask = *d;
        EEPROM_WRITE(&eeprom_sys_setup.mask, d, len); // sorry, size optimization
        break;
      case 3:
        if(type != 0x40 || len != 4) return 0;
        util_cpy(d, sys_setup.gate, len);
        EEPROM_WRITE(&eeprom_sys_setup.gate, d, len);
        break;
      case 4:
        if(type != 0x04 || len != 6) return 0;
        // SNMP_SET_MAC(d);
        util_cpy(d, sys_setup.mac, len);
        EEPROM_WRITE(&eeprom_sys_setup.mac, d, len);
        break;
      }
      snmp_add_asn_obj(type, len, d);
      main_reload = 1;
    }
    else // Get
    {
      switch( id & 0x000f )
      {
      case 1: snmp_add_asn_obj(0x40, 4, sys_setup.ip); break;
      case 2: snmp_add_asn_integer(sys_setup.mask); break;
      case 3: snmp_add_asn_obj(0x40, 4, sys_setup.gate); break;
      case 4: snmp_add_asn_obj(0x04, 6, sys_setup.mac); break;
      }
    }
    break;
  default:
    return 0;
  }
  return 1;
}


unsigned char check_community(unsigned char *community) // community is pasc. string!
{
  unsigned char allow = 0;
  unsigned char buf[sizeof sys_setup.community_r + 12];
  int n;

  n = community[0] + 1;
  if(n > sizeof sys_setup.community_r) n = sizeof sys_setup.community_r;

  if(memcmp(community, sys_setup.community_r, n) == 0) allow |= 1; // allow read
  if(memcmp(community, sys_setup.community_w, n) == 0) allow |= 2; // allow write

  n = sizeof sys_setup.community_r;
  memset(buf, 0xff, n);
  if(memcmp(buf, sys_setup.community_r, n) == 0) allow |= 1; // empty flash => allow read
  if(memcmp(buf, sys_setup.community_w, n) == 0) allow |= 1 | 2 | 4; // empty flash => allow write & change

  // '0000007BSWITCH' for serial=123  (serial is const unsigned in BOOT_SERIAL segment)
  sprintf((char*)buf + 1, "%08X", *(unsigned*)0x0FFC /*serial*/);
  n = sys_setup.community_w[0];
  if(n >= sizeof sys_setup.community_w) n = sizeof sys_setup.community_w - 1;
  util_cpy(sys_setup.community_w + 1, buf + 9, n);
  n += 8;
  buf[0] = n;
  if(memcmp(community, buf, n) == 0) allow |= 1 | 2 | 4; // allow all + setup

  return allow;
}

#define snmp_err(errcode) do{ snmp_set_err(snmp_tx_error_pos, errcode, 0); goto err; }while(0)

void parse_snmp(void)
{
  unsigned char buf[600];
  unsigned char *p;
  int tmp;

  unsigned char *rx_varbind_start;
  unsigned rx_varbind_length;
  unsigned snmp_tx_error_pos = 0;
  unsigned varbind_seq_pos = 0;
  unsigned varbind_list_seq_pos = 0;
  unsigned pdu_seq_pos = 0;
  unsigned snmp_header_seq_pos = 0;
  unsigned saved;

  unsigned char *rx_community;
  unsigned short in_oid[MAX_OID_LEN];
  unsigned short out_oid[MAX_OID_LEN];
  unsigned in_oid_length;
  unsigned out_oid_length;
  unsigned short id;
  unsigned short vbind_index;
  unsigned short pdu_mode;

  //#warning "fixed SNMP port!"
  //if(*((unsigned short*)_UDP_RX_HEAD.dest_port)!= SNMP_PORT ) return;
  if(udp_rx_head.dest_port[0] != (sys_setup.snmp_port >> 8)       // 11.03.2011
  || udp_rx_head.dest_port[1] != (sys_setup.snmp_port & 0xff)) return;

  udp_rx_body_pointer = 0;

  // create responce packet
  snmp_create_responce_packet();
  if(snmp_ds == 0xff) return;
  udp_tx_body_pointer = 0;

  // requires patched NIC, safe to read beyond packet end !!!!!
  udp_get_rx_body(buf, sizeof buf);
  p = buf;
  // snmp header
  if(*p++ != 0x30) goto drop; // wrong format, drop it (as of RFC1157)
  p += asn_get_length(p, (unsigned*)&tmp);
  snmp_header_seq_pos = snmp_add_seq();
  // snmp version
  p += asn_get_integer(p, &tmp);
  if(tmp > 1) goto drop; // supports v1 (0) and v2c(1), if wrong version, just drop it (as of RFC1157)
  snmp_add_asn_integer(tmp); // LBS 7.08.2010, v2c tolerance
  // community
  if(*p++ != 0x04) snmp_err(SNMP_ERR_GEN_ERR);  // community, oct string type
  rx_community = p;
  if(*p>sizeof sys_setup.community_r+8/*SN+community*/) goto drop; // too long community
  community_check_result = check_community(p);
  p += *p + 1; // skip community
  udp_put_tx_body(snmp_ds, rx_community-1, *rx_community + 2); // copy community from rx to tx (implied 1-byte obj len)
  // PDU
  pdu_mode = *p++;   // implied sequence
  if(pdu_mode == SNMP_PDU_RESPONCE) goto drop; // ignore Responce PDU // 22.11.2012
  p += asn_get_length(p, (unsigned*)&tmp);
  snmp_add_raw_bytes(4, 0xA2 , 0x82, 0, 0); // GetResponce tx PDU
  pdu_seq_pos = udp_tx_body_pointer;
     // check access rights for this PDU type
  if(pdu_mode == 0xA3 /*set*/ )
  {
    if((community_check_result&6)==0) goto drop;
  }
  else
  {
    if((community_check_result&1)==0) goto drop;
  }
  // Request Id
  p += asn_get_integer(p, &tmp);   // possible bug, 31th bit (sign)
  snmp_add_asn_integer(tmp);
  // Error status
  p += asn_get_integer(p, &tmp);
  snmp_tx_error_pos = udp_tx_body_pointer;
  snmp_add_asn_integer(0);
  // Error index
  p += asn_get_integer(p, &tmp);
  snmp_add_asn_integer(0);
  // Varbind list
  if(*p++ != 0x30) goto drop;
  p += asn_get_length(p, &rx_varbind_length);
  rx_varbind_start = p;
  varbind_list_seq_pos = snmp_add_seq();
  for(vbind_index=1;;++vbind_index)
  {
    if(p - buf > sizeof buf - 10) snmp_err(SNMP_ERR_TOO_BIG); // too big PDU
    if(p - rx_varbind_start >= rx_varbind_length ) break;     // end of varbind list
    // Vardind pair
    if(*p++ != 0x30) snmp_err(SNMP_ERR_GEN_ERR);
    p += asn_get_length(p, /*&rx_varbind_len*/ (unsigned*)&tmp);
    varbind_seq_pos = snmp_add_seq();
    // oid
    p += asn_decode_oid(p, in_oid, &in_oid_length);
    id = check_fw_update_oid(pdu_mode, in_oid, in_oid_length);
    if(id != 0)
    {
      snmp_add_oid(in_oid_length, in_oid);
      saved = udp_tx_body_pointer;
      if(parse_fw_update_commands(pdu_mode, in_oid, in_oid_length, p, id) == -1) goto drop; // find device already found
      if(saved == udp_tx_body_pointer) snmp_add_raw_bytes(2, 0x05, 0); // add null if no data
    }
    else
    {
      tmp = find_oid(pdu_mode == 0xA1, in_oid, out_oid, &out_oid_length);
      if(tmp == -1)
      {
        snmp_add_oid(in_oid_length, in_oid);
        snmp_add_raw_bytes(2, 0x05, 0); // Null
        snmp_set_err(snmp_tx_error_pos, SNMP_ERR_NO_SUCH_NAME, vbind_index);
        goto err;
      }
      snmp_add_oid(out_oid_length, out_oid);
      // make snmp resource id for data processing...
      id = oid_tree[tmp].id;
      if(oid_tree[tmp].flags & OID_TREE_TABLE_LEGACY)
      {
        id |= out_oid[out_oid_length-1];
      }
      // ..pass snmp morsels via global struct
      snmp_data.id = id;
      if(oid_tree[tmp].flags & (OID_TREE_TABLE_LEGACY | OID_TREE_TABLE_INDEX))
        snmp_data.index = out_oid[out_oid_length-1];
      else
        snmp_data.index = 0;
      snmp_data.pdu = pdu_mode;
      snmp_data.rxdata = p;
      // value
      saved = udp_tx_body_pointer;

      int data_err = snmp_data_handler(pdu_mode, id, p); // some data passed via 'snmp_data' global struct 14.04.2010

      if(udp_tx_body_pointer==saved) // if no data added
      {
        snmp_add_raw_bytes(2, 0x05, 0); // add Null if no data was added by handler
        /*
        // DEBUGGG write id
        unsigned char idtxt[8];
        idtxt[0] = 0x04; idtxt[1] = 6;   // octet string, len 6
        sprintf((char*)idtxt+2, "0x%04x", id);
        udp_put_tx_body(snmp_ds, idtxt, 8);
        */
      }
      if(data_err) { snmp_set_err(snmp_tx_error_pos, data_err, vbind_index); goto err; }
    }
    // skip rx value
    *p++;  // type
    p += asn_get_length(p, (unsigned *)&tmp); // length
    p += tmp; // data
    // close this one varbind
    snmp_close_seq(varbind_seq_pos);
    varbind_seq_pos = 0;
  }
err:
  if(varbind_seq_pos)      snmp_close_seq(varbind_seq_pos);
  if(varbind_list_seq_pos) snmp_close_seq(varbind_list_seq_pos);
  if(pdu_seq_pos)          snmp_close_seq(pdu_seq_pos);
  if(snmp_header_seq_pos)  snmp_close_seq(snmp_header_seq_pos);
  udp_send_packet(snmp_ds, udp_tx_body_pointer);
  return;
drop:
  ip_free_packet(snmp_ds);
  return;
}

unsigned snmp_trap_body_seq_pos;
unsigned snmp_trap_pdu_seq_pos;
unsigned snmp_trap_varbind_list_seq_pos;

void snmp_create_trap(unsigned char *enterprise) // v2
{
  snmp_ds = udp_create_packet();
  if(snmp_ds == 0xff) return;

  ip_get_tx_header(snmp_ds);
  util_cpy(ip_head_rx.src_ip, ip_head_tx.dest_ip, 4);
  ip_put_tx_header(snmp_ds);
  *(unsigned short*)udp_tx_head.dest_port = SNMP_TRAP_PORT;
  *(unsigned short*)udp_tx_head.src_port  = SNMP_PORT;
  udp_put_tx_header(snmp_ds);
  udp_tx_body_pointer = 0;

  // Snmp header
  snmp_trap_body_seq_pos = snmp_add_seq();
  // Version
  snmp_add_asn_integer(0);
  // Community
  snmp_add_asn_obj(SNMP_TYPE_OCTET_STRING, sys_setup.community_r[0], sys_setup.community_r+1);
  // Trap PDU
  snmp_add_raw_bytes(4, 0xA4 , 0x82, 0, 0);
  snmp_trap_pdu_seq_pos = udp_tx_body_pointer;
  // Enterprise OID
  udp_put_tx_body(snmp_ds, enterprise, enterprise[1 /* obj len */]+2);
  // self IP
  snmp_add_asn_obj(0x40, 4, SNMP_AGENT_IP);
  // Generic-trap type
  snmp_add_asn_integer(6); // == application-specific
  // Specific-trap type
  snmp_add_asn_integer(1); // snmp v2 translation aware (v2 snmpTrapOID = enterprise.0.v1-specific-trap)
  // Timestamp
  snmp_add_asn_timestamp( ((unsigned)sys_clock()) / 10 ); // 1/100s ticks, 32b unsigned
  // Varbind list
  snmp_trap_varbind_list_seq_pos = snmp_add_seq();
}

  /// untested, not used !!!
void snmp_add_vbind_integer32(const unsigned char *enterprise, unsigned char last_oid_component, int value)
{
  unsigned oidlen = enterprise[1];
  if(oidlen > 30) return;
  unsigned seq_ptr = snmp_add_seq();
  unsigned char oid[32];
  util_cpy((void*)(enterprise+2), oid, oidlen);
  oid[oidlen] = last_oid_component & 0x7f;
  snmp_add_asn_obj(SNMP_OBJ_ID, oidlen+1, oid);
  snmp_add_asn_integer(value);
  snmp_close_seq(seq_ptr);
}

// .1.3.6.1.4.1.25728.90.1.0
const unsigned char notif_email_oid[] =
{0x2b,6,1,4,1,0x81,0xc9,0x00,90,1,0};

void add_vbind_email(void) // 20.08.2012
{
  unsigned seq_ptr = snmp_add_seq();
  snmp_add_asn_obj(SNMP_OBJ_ID, sizeof notif_email_oid, (void*)notif_email_oid);
  snmp_add_asn_obj(SNMP_TYPE_OCTET_STRING,
    sys_setup.notification_email[0], sys_setup.notification_email + 1);
  snmp_close_seq(seq_ptr);
}

void snmp_send_trap(unsigned char *ip) // v2
{
#ifndef NOTIFY_MODULE
  if(sys_setup.notification_email[0]) // 30.10.2013
    add_vbind_email();
#endif
  snmp_close_seq(snmp_trap_varbind_list_seq_pos);
  snmp_close_seq(snmp_trap_pdu_seq_pos);
  snmp_close_seq(snmp_trap_body_seq_pos);

  ip_get_tx_header(snmp_ds);
  util_cpy(ip, ip_head_tx.dest_ip, 4);
  ip_put_tx_header(snmp_ds);
  udp_send_packet(snmp_ds, udp_tx_body_pointer);
}

#endif
