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

extern unsigned char dns_max_idx;

void dns_add(unsigned char *hname, unsigned char *ip); // hname is p.string ref
void dns_delete_cache_item(unsigned idx, unsigned len_items); // ATTN! don't use it without extreme care!!!!
void dns_write_new_cache_item(unsigned idx, unsigned char *hname, unsigned char *ip); // ATTN! don't use it without extreme care!!!!
void dns_resolve(unsigned char *eeprom_hostname, unsigned char *hostname); // args is p.string refs, call it before saving new setup to eeprom
void dns_invalidate(unsigned char *ip_or_hostname);

void dns_parsing(void);
void dns_init(void);
void dns_event(enum event_e event);
