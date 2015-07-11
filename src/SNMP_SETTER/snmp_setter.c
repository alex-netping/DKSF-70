/*
snmp setter ('logic' add-on for remote control of IO, relay, eth switch ports etc.)
v1.1 by LBS
21.11.2012
  initial version
v1.2-48
18.10.2013
  DNS compatible
v1.3-201
24.03.2014
  immediate error in sentter_send() if empty host
v1.4-70
11.06.2014
  flip command support
v1.5-70
12.08.2014
  setter_send() bugfix on empty host
*/

#include "platform_setup.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "plink.h"
#include "eeprom_map.h"

//const unsigned setter_signature = 1645636837;
const unsigned setter_signature = 1645636871; // DNS enchanced, 18.10.2013

struct setter_setup_s setter_setup[SETTER_MAX_CH];
struct setter_state_s setter_state[SETTER_MAX_CH];

unsigned setter_request_id;

void setter_send(unsigned ch, unsigned onoff);

void setter_add_oid(unsigned char *poid) // oid = pasc-z str!
{
  char buf[128];
  unsigned subid, b;
  char *p = (char*)poid + 1 + 4; // skip p-string len and .1.3 prefix
  char *q = buf;
  int i;
  *q++ = 0x2b; // .1.3 prefix
  for(;;)
  {
    if(*p++ != '.') break;
    subid = atoi(p); // get next subid
    while(*p >= '0' && *p <= '9') ++p; // skip number
    // xform to asn.1 subid encoding
    for(i = 28; i>=0; i-=7) // skip leading zero bits
      if((subid >> i) & 0x7f) break;
    for(; i>=0; i-=7) // encode
    {
      b = (subid >> i) & 0x7f;
      if(i > 0) b |= 0x80;
      *q++ = b;
    }
  }
  snmp_add_asn_obj(SNMP_OBJ_ID, q - buf, (void*)buf);
}


void setter_send_request(unsigned ch)
{
  struct setter_state_s *state = &setter_state[ch];
  struct setter_setup_s *setup = &setter_setup[ch];
  if(!valid_ip(setup->ip)) return;
  snmp_ds = udp_create_packet_sized(256);
  if(snmp_ds != 0xff)
  {
    memcpy(ip_head_tx.dest_ip, setup->ip, 4);
    ip_put_tx_header(snmp_ds);
    udp_tx_head.dest_port[0] = setup->port >> 8;
    udp_tx_head.dest_port[1] = setup->port >> 0;
    udp_tx_head.src_port[0] = sys_setup.snmp_port >> 8;
    udp_tx_head.src_port[1] = sys_setup.snmp_port >> 0;
    udp_put_tx_header(snmp_ds);
    udp_tx_body_pointer = 0;
    unsigned snmp_seq = snmp_add_seq();
    // version
    snmp_add_asn_integer(0);
    // community
    snmp_add_asn_obj(SNMP_TYPE_OCTET_STRING, setup->community[0], setup->community + 1);
    // Set Request PDU
    snmp_add_raw_bytes(4, SNMP_PDU_SET, 0x82, 0, 0);
    unsigned pdu_seq = udp_tx_body_pointer;
    // Request Id
    state->request_id = ((++setter_request_id << 4) & 0x7ffffff0) | ch;
    snmp_add_asn_integer(state->request_id);
    // Error Status
    snmp_add_asn_integer(0);
    // Error Index
    snmp_add_asn_integer(0);
    // Varbind List
    unsigned varbind_seq = snmp_add_seq();
    // Variable to be set
    unsigned variable_seq = snmp_add_seq();
    // OID
    setter_add_oid(setup->oid);
    // Value
    snmp_add_asn_integer(state->value_idx ? setup->value_on : setup->value_off);
    // close sequences
    snmp_close_seq(variable_seq);
    snmp_close_seq(varbind_seq);
    snmp_close_seq(pdu_seq);
    snmp_close_seq(snmp_seq);
    // send
    udp_send_packet(snmp_ds, udp_tx_body_pointer);
  } // if pkt != 0xff
}

void setter_parsing(void)
{
  unsigned dest_port = udp_rx_head.dest_port[0]<<8 | udp_rx_head.dest_port[1]<<0;
  if(dest_port != sys_setup.snmp_port) return;

  int tmp;
  unsigned char buf[600];
  udp_rx_body_pointer = 0;
  // requires patched NIC, safe to read beyond packet end !!!!!
  udp_get_rx_body(buf, sizeof buf);
  unsigned char *p = buf;
  // snmp header
  if(*p++ != 0x30) return; // sequence
  p += asn_get_length(p, (unsigned*)&tmp); // seq length
  p += asn_get_integer(p, &tmp); // version
  if(tmp > 1) return; // accepts snmp v1 (0) and v2c (1)
  // skip community
  if(*p++ != 0x04) return; // type == oct string
  tmp = *p++; // length
  p += tmp; // skip oct string
  // PDU
  unsigned pdu_type = *p++;   // implied sequence
  if(pdu_type != SNMP_PDU_RESPONCE) return;
  p += asn_get_length(p, (unsigned*)&tmp); // skip seq length
  // Request Id
  p += asn_get_integer(p, &tmp);   // possible bug, 31th bit (sign)
  unsigned ch = tmp & 15;
  if(ch >= SETTER_MAX_CH) return;
  struct setter_state_s *state = &setter_state[ch];
  if(tmp != state->request_id) return;
  // Respocnce received
  // Error status
  p += asn_get_integer(p, &tmp);
  if(tmp > 5) tmp = SNMP_ERR_GEN_ERR;
  state->err_status = tmp;
  state->state = SETTER_STATE_IDLE;
  state->timeout = ~0ULL;
}

unsigned setter_http_get(unsigned pkt, unsigned more_data)
{
  char buf[768];
  char *dest = buf;
  if(more_data == 0)
  {
    dest+=sprintf(dest,"var setter_packfmt={");
    PLINK(dest, setter_setup[0], name);
    PLINK(dest, setter_setup[0], oid);
    PLINK(dest, setter_setup[0], ip);
    PLINK(dest, setter_setup[0], hostname);
    PLINK(dest, setter_setup[0], community);
    PLINK(dest, setter_setup[0], port);
    PLINK(dest, setter_setup[0], value_on);
    PLINK(dest, setter_setup[0], value_off);
    PSIZE(dest, sizeof setter_setup[0]); // must be the last // alignment!
    dest+=sprintf(dest, "};\nvar setter_data=[");
  }
  struct setter_setup_s *setup = &setter_setup[more_data];
  unsigned n = more_data;
  for(;; ++n, ++setup)
  {
    *dest++ = '{';
    PDATA_PASC_STR(dest, (*setup), name);
#   ifndef DNS_MODULE
#   error in pre-DNS assembly, length is limited to 30 chars!
#   endif
    PDATA_PASC_STR(dest, (*setup), oid);
    PDATA_IP      (dest, (*setup), ip);
    PDATA_PASC_STR(dest, (*setup), hostname);
    PDATA_PASC_STR(dest, (*setup), community);
    PDATA         (dest, (*setup), port);
    PDATA_SIGNED  (dest, (*setup), value_on);
    PDATA_SIGNED  (dest, (*setup), value_off);
    --dest; // remove last comma
    *dest++ = '}';
    *dest++ =',';
    if(n == SETTER_MAX_CH - 1 || dest > buf + sizeof buf - 280) break;
  }
  --dest; // remove last comma
  *dest++ = ']'; *dest++ = ';';
  tcp_put_tx_body(pkt, (unsigned char*)buf, dest - buf);
  return n == SETTER_MAX_CH - 1 ? 0 : n;
}

int setter_http_set(void)
{
  http_post_data((void*)setter_setup, sizeof setter_setup);
  for(int n=0; n<SETTER_MAX_CH; ++n)
  {
    setter_state[n].state = SETTER_STATE_IDLE;
    setter_state[n].err_status = 0xff;
    dns_resolve(eeprom_setter_setup[n].hostname, setter_setup[n].hostname); // call it before saving new setup to eeprom
  }
  EEPROM_WRITE(&eeprom_setter_setup, &setter_setup, sizeof eeprom_setter_setup);
  http_redirect("/logic.html");
  return 0;
}

unsigned setter_http_test_get(unsigned pkt, unsigned more_data)
{
  if(req_args[0] != 'c' || req_args[1] != 'h' || req_args[3] != '=') return 0;
  unsigned ch = req_args[2] - '1';
  if(ch >= SETTER_MAX_CH) return 0;
  unsigned flag = req_args[4] - '0';
  if(flag > 1) return 0;
  setter_send(ch, flag);
  return 0;
}

HOOK_CGI(setter_get,  setter_http_get,        mime_js, HTML_FLG_GET | HTML_FLG_NOCACHE );
HOOK_CGI(setter_set,  setter_http_set,        mime_js, HTML_FLG_POST );
HOOK_CGI(setter_test, setter_http_test_get,   mime_js, HTML_FLG_GET | HTML_FLG_NOCACHE );

void setter_reset_params(void)
{
  memset(&setter_setup, 0, sizeof setter_setup);
  struct setter_setup_s *setup = setter_setup;
  for(int n=0; n<SETTER_MAX_CH; ++n, ++setup)
  {
    setup->port = 161; // default snmp port
    strncpy((char*)(setup->oid + 1), ".1.3.6.1.4.1.25728.5800.3.1.3.1", sizeof setup->oid - 2); // pasc-zterm string
    setup->oid[0] = strlen((char*)(setup->oid + 1));
    setup->value_on = 1;
  }
  EEPROM_WRITE(eeprom_setter_setup, setter_setup, sizeof eeprom_setter_setup);
  EEPROM_WRITE(&eeprom_setter_signature, &setter_signature, sizeof eeprom_setter_signature);
}

void setter_init(void)
{
  unsigned sign;
  EEPROM_READ(&eeprom_setter_signature, &sign, sizeof sign);
  if(sign != setter_signature) setter_reset_params();
  EEPROM_READ(eeprom_setter_setup, setter_setup, sizeof setter_setup);

  setter_request_id = copy4(sys_setup.mac + 2);
  struct setter_setup_s *setup = setter_setup;
  struct setter_state_s *state = setter_state;
  for(int n=0; n<SETTER_MAX_CH; ++n, ++setup, ++state)
  {
    dns_add(setup->hostname, setup->ip);
    state->state = SETTER_STATE_IDLE;
    state->err_status = 0xff;
    state->timeout = ~0ULL;
  }
}

void setter_send(unsigned ch, unsigned onoff)
{
  if(ch >= SETTER_MAX_CH || onoff > 1) return;
  struct setter_state_s *state = &setter_state[ch];
  if(!valid_ip(setter_setup[ch].ip))
  {
    if(setter_setup[ch].hostname[0] == 0)
      state->err_status = SNMP_ERR_NO_HOST; // defined in snmp.h
    else
      state->err_status = SNMP_ERR_DNS_IP; // defined in snmp.h
    state->timeout = ~0UL;
    state->state = SETTER_STATE_IDLE;
    return; // 12.08.2014
  }
  if(onoff == 2)
    state->value_idx = state->value_idx ? 0 : 1 ; // flip
  else
    state->value_idx = onoff ? 1 : 0 ; // normal value // used in setter_send_request()
  setter_send_request(ch);
  state->state = SETTER_STATE_ATTEMPT_1;
  state->timeout = sys_clock() + SETTER_TIMEOUT;
  state->err_status = SNMP_ERR_WAITING_RESPONCE;
}

void setter_exec(void)
{
  static int skip = 0;
  if(++skip < 73) return;
  skip = 0;
  systime_t time = sys_clock();
  unsigned ch;
  struct setter_state_s *state = setter_state;
  for(ch=0; ch<SETTER_MAX_CH; ++ch, ++state)
  {
    if(time > state->timeout)
    {
      state->timeout = ~0ULL;
      if(state->state == SETTER_STATE_ATTEMPT_1
      || state->state == SETTER_STATE_ATTEMPT_2 )
      {
        ++state->state;
        state->timeout = time + SETTER_TIMEOUT;
        setter_send_request(ch);
      }
      else if(state->state == SETTER_STATE_ATTEMPT_3)
      {
        if(valid_ip(setter_setup[ch].ip) == 0)
          state->err_status = SNMP_ERR_DNS_IP;
        else
          state->err_status = SNMP_ERR_TIMEOUT;
        return;
      }
    }
  } //for
}

void setter_event(enum event_e event)
{
  switch(event)
  {
  case E_EXEC:
    setter_exec();
    break;
  case E_RESET_PARAMS:
    setter_reset_params();
    break;
  }
}

#warning "TODO on web, implement fast result of test"
