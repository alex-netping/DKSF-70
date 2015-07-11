/*
v1.0-60
16.05.2013
  initial version
v1.1-52
13.08.2013
  removed valid_dns, dns_ip[4] (only single DNS suppored)
v1.2-707
27.12.2013
  protection from old replies by dns.id and dns.state
  dns_delete_cache_item() special api for movable dns cache entries
  check for simple text ip instead of FQDN
v1.3-707
28.02.2014
  dns_write_new_cache_item() special api for movable dns cache entries
  bugfix of simple text ip treated as fqdn in dns_invalidate()
v1.4-70
29.05.2014
  sys_setup.smtp_hostname added
v1.5-48
16.10.2014
  critical bugfix in dns_add(), support for [empty hname, valid ip] input
  critical bugfix in dns_exec(), fixed dropped dns server responces for refresh resolution after ttl
  stop operations on item in dns_resolve() if empty hname
*/

#include "platform_setup.h"
#include "eeprom_map.h"
#include <string.h>

#if HTTP_INPUT_DATA_SIZE < 2048
#error "too small HTTP_INPUT_DATA_SIZE to handle hostnames!"
#endif

#define DNS_PORT       53
#define DNS_SRC_PORT   53

enum dns_state_e {
  DNS_OK,
  DNS_UNRESOLVED
};

struct dns_s {
  unsigned char *hostname; // ref to pasc string
  unsigned char *ip_ref;   // ref to char[4]
  unsigned char ip[4];
  enum dns_state_e state;
  unsigned char attempt;
  unsigned time;
  unsigned id;
  unsigned ttl;
} dns[DNS_MAX_SLOTS];


unsigned char dns_max_idx;
//unsigned char dns_ip[4];
//unsigned char valid_dns;
unsigned char dns_fails;
struct dns_s *dnsp = dns;
unsigned short dns_id = 0x1110;

unsigned dns_encode_name(char *qname, char *hname)
{
  int lenpos = 0;
  int cnt = 0;
  int i = 0;
  char c;

  for(;;)
  {
    c = hname[i];
    if(c == '.' || c == 0)
    {
      qname[lenpos] = cnt;
      lenpos = i + 1;
      cnt = 0;
    }
    else
    {
      qname[i + 1] = c;
      ++cnt;
    }
    if(c == 0) break;
    ++i;
  }
  qname[i + 1] = 0;
  return i + 2;
}

unsigned dns_skip_qname(char *p)
{
  unsigned len = 0;
  unsigned n;
  for(;;)
  {
    if(*p == 0) return len + 1; // + trailing zero
    if(*p & 0xc0) return len + 2; // compressed tail
    n = *p + 1;
    len += n;
    p += n;
    if(n > 64 || len > 128) break; // scan protection
  }
  return len; // emergency, too long, result is garbage
}

unsigned dns_send_request(void)
{
  unsigned pkt = udp_create_packet_sized(256);
  if(pkt == 0xff) return 0xff;
  char *base = udp_ref(pkt, 0);
  char *p = base;
  dnsp->id = ++dns_id;
  p += htonps(p, dns_id); // id
  p += htonps(p, 0x0100); // flags, RD=1
  p += htonps(p, 1); // qiestions=1
  p += htonps(p, 0); // remaining 0s
  p += htonps(p, 0);
  p += htonps(p, 0);
  ///ZTERM(dnsp->hostname); // can't be applied, hostname is ref, not array
  p += dns_encode_name(p, (char*)dnsp->hostname + 1);
  p += htonps(p, 1); // QTYPE
  p += htonps(p, 1); // QCLASS
  udp_send_packet_to(pkt, sys_setup.dns_ip1, DNS_PORT, DNS_SRC_PORT, p - base);
  return 0;
}

void dns_parsing(void)
{
#warning TODO parsing protection from malformed rx
  unsigned dest_port = udp_rx_head.dest_port[0]<<8 | udp_rx_head.dest_port[1];
  if(dest_port != DNS_SRC_PORT) return;
  if(memcmp(ip_head_rx.src_ip, sys_setup.dns_ip1, 4) != 0) return;
  unsigned rxlen = pntohs(udp_rx_head.len) - sizeof udp_rx_head;
  if(rxlen < 12) return; // too small rx udp, less than dns header
  char *p = udp_ref(NIC_RX_PACKET, 0);
  char *end = p + rxlen;
  unsigned id =    pntohs(p); p += 2;
  unsigned flags = pntohs(p); p += 2;
  unsigned nquestions = pntohs(p); p += 2;
  if(nquestions != 1) return;
  unsigned nanswers =   pntohs(p); p += 2;
  p = p + 2 + 2; // skip remaining dns header
  if((flags & 0x8100) != 0x8100)
    return; // not answer, RD is not copied from request or recursion not available
  int n;
  struct dns_s *dp = dns;
  for(n=0; n<dns_max_idx; ++n, ++dp) // ATTN! don't encode cache elem. position into id! cache elements are movable in dksf 707!
    if(dp->id == id)
      break;
  if(n == dns_max_idx)
    return; // id not found
  if(dp->state != DNS_UNRESOLVED)
    return; // protection from doubled replies
  if(dp->hostname[0] == 0)
    return; // protection from deleted cache item
  if( (flags & 0x000f) != 0
  ||  (flags & 0x0080) != 0x0080
  ||   nanswers == 0 )
  { // dns server reports error or recursion is not available or no ip for this domain
    dp->attempt = 0;
    dp->time = sys_clock_100ms + 120 * 10 + n; // 2 min t/o
    return;
  };
  // check question name
  char qname[DNS_MAX_NAME + 2];
  ///ZTERM(dp->hostname);
  char len = dns_encode_name(qname, (char*)dp->hostname + 1);
  if(memcmp(p, qname, len) != 0) return; // name mismatch
  p = p + len + 2 + 2; // skip question
  unsigned short atype, acls, attl, alen;
  for(n=0; n<nanswers; ++n)
  {
    if(p >= end) return; // malformed rx
    p += dns_skip_qname(p);
    atype = pntohs(p); p += 2;
    acls  = pntohs(p); p += 2;
    attl  = pntohl(p); p += 4;
    alen  = pntohs(p); p += 2;
    if(atype == 1 && acls == 1 && alen == 4)
    {
      struct dns_s *dpp = dns;
      for(int i=0; i<dns_max_idx; ++i, ++dpp)
      { // update all dns[] entries with this hostname
        if(dpp == dp || memcmp(dp->hostname, dpp->hostname, dp->hostname[0] + 1) == 0)
        {
          dpp->ttl = attl;
          dpp->attempt = 0;
          dpp->time = sys_clock_100ms + attl * 10 + i * 20; // spread life time
          memcpy(dpp->ip, p, 4);
          memcpy(dpp->ip_ref, p, 4);
          dpp->state = DNS_OK;
        }
      }
      return;
    }
    p += alen; // skip this answer
  }
}

void dns_exec(void)
{
//  if(valid_dns == 0) return; // no dns servers (valid dns ips)
  if(!valid_ip(sys_setup.dns_ip1)) return;
  if(++dnsp >= &dns[dns_max_idx]) dnsp = dns;
  if(dnsp->hostname[0] == 0) return; // hostname is unused, ip in use
  if(sys_clock_100ms < dnsp->time) return; // fresh resolution or waiting responce
  if(++dnsp->attempt >= DNS_MAX_ATTEMPTS)
  {
    dnsp->attempt = 0;
    dnsp->time = sys_clock_100ms + 300 + ((unsigned)sys_clock() & 31);
  }
  else
  {
    dnsp->time = sys_clock_100ms + (1 << (dnsp->attempt - 1)) * 10;
    dnsp->state = DNS_UNRESOLVED; // 16.10.2014 CRITICAL fresh resolution start after ttl
    dns_send_request();
  }
}

void invalidate_item(unsigned idx, unsigned wait_before_dns_req_100ms_ticks)
{
  if(idx >= dns_max_idx) return;
  struct dns_s *dp = &dns[idx];
  unsigned char ip[4];
  int ip_conv_result = str_str_to_ip((char*)dp->hostname + 1, ip);
  if(ip_conv_result < 0)
  { // not text ip, treat as FQDN, ask DNS
    dp->state = DNS_UNRESOLVED;
    memset(dp->ip, 0, 4);
    memset(dp->ip_ref, 0, 4);
    dp->attempt = 0;
    dp->id = 0; // protection from replies to obsolete dns request
    dp->time = sys_clock_100ms + wait_before_dns_req_100ms_ticks;
  }
  else
  { // hostname is text ip
    memcpy(dp->ip,     ip, 4);
    memcpy(dp->ip_ref, ip, 4);
    dp->state = DNS_OK;
    dp->ttl = ~0U;
    dp->time = ~0U;
  }
}

// hack-level opening of internal API for snmp_bridge.c! extreme care is required!
// aware of writing beyond dns_max_idx!!!
void dns_write_new_cache_item(unsigned idx, unsigned char *hname, unsigned char *ip)
{
  if(idx >= DNS_MAX_SLOTS) return;
  struct dns_s *dp = &dns[idx];
  dp->hostname = hname;
  dp->ip_ref = ip;
  dp->ttl = 60;
#warning TODO check saved ip behaviour and use of the feature if no reply on dns request
  memcpy(dp->ip, dp->ip_ref, 4); // save referenced ip
}

// hack-level API for snmp_bridge.c! extreme care is required!
// don't use this for dns items with fixed positions!
// don't use movable items by reference!!!  extreme care is required!
//// deletes first of len_items starting from idx, last item of len_items filled by 0s
void dns_delete_cache_item(unsigned idx, unsigned len_items)
{
  if(len_items == 0) return;
  if(idx + len_items > dns_max_idx) return;
  memmove(&dns[idx], &dns[idx + 1], (len_items - 1) * sizeof dns[0]);
  memset(&dns[idx + len_items - 1], 0, sizeof dns[0]);
}

void dns_add(unsigned char *hname, unsigned char *ip)
{
  if(dns_max_idx == DNS_MAX_SLOTS)
  {
    log_printf("DNS cache overflow!");
    return;
  }
  if(*hname == 0 && valid_ip(ip)) // 16.10.2014 - CRITICAL!
  { // copy numeric ip to textual hostname
    str_ip_to_str(ip, (char*)hname + 1);
    hname[0] = strlen((char*)hname + 1);
  }
  unsigned idx = dns_max_idx++; // increase max_idx *before* futher operations
  dns_write_new_cache_item(idx, hname, ip);
  invalidate_item(idx, dns_max_idx + 10); // if time low, startup problems! arp?

}

// call it before saving new setup to eeprom!
void dns_resolve(unsigned char *eeprom_hostname, unsigned char *hostname)
{
  if(hostname[0] == 0)
  { // empty hostname, no need to resolve or restore POSTed ip
    // just stop item operations
    struct dns_s *p = dns;
    for(int i=0; i<dns_max_idx; ++i, ++p)
    {
       if(p->hostname == hostname)
       {
         p->state = DNS_OK;
         p->id = 0;
         p->ttl = ~0U;
         p->time = ~0U;
       }
    }
    return;
  }
  // read old hostname from eeprom
  unsigned char old[64];
  EEPROM_READ(eeprom_hostname, old, sizeof old);
  unsigned n = old[0];  if(n > sizeof old - 1) n = sizeof old - 1;
  int changed = memcmp(hostname, old, n + 1);
  // find name refs, invalidate records
  struct dns_s *p = dns;
  for(int i=0; i<dns_max_idx; ++i, ++p)
  {
    if(p->hostname == hostname)
    {
      // check if name was changed
      if(changed == 0)
      {
        // restore ip_ref->[4] cleared by POST
        memcpy(p->ip_ref, p->ip, 4);
      }
      else
      { // changed
#warning TODO сомнительно использовать задержки по индексу в таблице ДНС при больших таблицах
        invalidate_item(i, i * 1);
      }
    } // if
  } // for
}

void dns_invalidate(unsigned char *ip_or_hostname)
{
  // find name refs, invalidate records
  unsigned spread_time = 0;
  struct dns_s *p = dns;
  for(int i=0; i<dns_max_idx; ++i, ++p)
  {
    if(p->ip == ip_or_hostname || p->hostname == ip_or_hostname)    ////// && ip_or_hostname[0] != 0) ) - ??? wrong for ip 0.xxx.xxx.xxx
      invalidate_item(i, ++spread_time); // 28.02.2014
  }
}

void dns_init(void)
{
  dns_add(sys_setup.trap_hostname1, sys_setup.trap_ip1);
  dns_add(sys_setup.trap_hostname2, sys_setup.trap_ip2);
  dns_add(sys_setup.ntp_hostname1, sys_setup.ntp_ip1);
  dns_add(sys_setup.ntp_hostname2, sys_setup.ntp_ip2);
  dns_add(sys_setup.syslog_hostname1, sys_setup.syslog_ip1);
}

void dns_event(enum event_e event)
{
  switch(event)
  {
  case E_EXEC:
    dns_exec();
    break;
  }
}

#if HTTP_BUILD < 14
#error DATA_PASC_STR required to put 62 chars!
#endif

