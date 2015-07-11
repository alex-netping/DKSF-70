/*
* IP module
* author P.V.Lyubasov
*v2.2
* 22.10.2010 - ???
* 3.06.2010 - Gratious ARP
*v2.3-50
*5.03.2012
* void ip_free_packet(uword packet_id) added and used outside instead of nic_free_packet()
v3.0-50
13.11.2012
 major revrite
v3.1-50
7.06.2013
  accept misused ARP responces with wrong target ip
  (responce to web camera incident)
v3.1-60
8.05.2013
  ip_send_packet_to() [byref]
  ip_creatce_packet_sized(size, protocol)
v3.1-48
8.07.2013
  removed __no_init from ip_head_rx etc. declarations
v3.1-52
30.09.2013
  'late use of rx packet ip-mac pair for making reply packet' bugfix
  bugfix og logic if no arp rec is found in ip_send_packet()
*/

#include "platform_setup.h"

#ifdef IP_MODULE

#include "string.h"


///////////////// v3 stuff //////////////////////

#define ARP_CACHE_SIZE 16

volatile unsigned ip_clock; // 0.1s tick
char resolved_flag; // hint after receiving mac

enum arp_state_e {
  ARP_STATE_FREE,      // rec not used, also ip32 == 0
  ARP_STATE_RESOLVING, // new rec, mac is unknown
  ARP_STATE_RESOLVED,  // mac just received and cached
  ARP_STATE_AGING      // arp waiting packets sent
};

struct arp_cache_s {
  unsigned ip32;
  unsigned char mac[6];
  enum arp_state_e state;
  unsigned timestamp;
} arp_cache[ARP_CACHE_SIZE];

struct arp_waiting_s {
  unsigned dest_ip32; // if not 0, packet is waiting for this ip resolution. It may be def.gateway ip, not packet dest ip!
  unsigned short ip_payload_len;
  unsigned short remove_flag; // if not 0, free packet after sending
} arp_waiting[NIC_RAM_SIZE]; // the same size as possible packet ID

unsigned self_ip32;
unsigned self_mask32;
unsigned self_gate32;
unsigned self_subnet_bcast32;
unsigned filter_ip32;
unsigned filter_mask32;
char     ip_saved_arp_flag; // =1 if ip packet payload is parsed now, =0 after ip packet payload parsing is over

int ip_allow_packet(unsigned src_ip32);
void ip_send_to_mac(unsigned pkt, unsigned ip_payload_len, int remove_flag, unsigned char *dest_mac);
void ip_send_arp_request(unsigned dest_ip32);
unsigned copy4(unsigned char *ip);

////////////////////////////////////////////////

struct arp_header_s arp_head_tx;
struct arp_header_s arp_head_rx;

struct ip_header_s  ip_head_tx;
struct ip_header_s  ip_head_rx;

unsigned ip_rx_body_pointer;
unsigned ip_tx_body_pointer;

unsigned ip_rx_body_length;

unsigned char self_ip_not_defined = 0; // flag
unsigned char broadcast[6] = {255,255,255,255,255,255};  // const string for broadcast mac (ip) or empty flash
unsigned char zero_ip[4] = {0,0,0,0}; // const string for comparison
unsigned char arp_template[8] = {0,1,8,0,6,4,0,2}; // ARP pkt header

//v3
void ip_init(void)
{
  ip_reload();
}

//v3
void ip_reload(void)
{
  memset(&ip_head_rx, 0, sizeof ip_head_rx);
  memset(&arp_head_rx, 0, sizeof arp_head_rx);
  memset(arp_cache, 0, sizeof arp_cache);

  self_ip32   = copy4(sys_setup.ip);
  self_mask32 = ~(0xffffffff >> sys_setup.mask);
  self_gate32 = copy4(sys_setup.gate);
  self_subnet_bcast32 = self_ip32 | ~self_mask32;
  filter_ip32 = copy4(sys_setup.filt_ip1);
  filter_mask32 = ~(0xffffffff >> sys_setup.filt_mask1);

  self_ip_not_defined = self_ip32 == 0 || self_ip32 == ~0UL;

  garp_init();
}

//v3 - reformat ip[4] to ip32
unsigned copy4(unsigned char *ip)
{
  return ip[0]<<24 | ip[1]<<16 | ip[2]<<8 | ip[3]<<0;
}

//v3
void copy32(unsigned char *ip, unsigned ip32)
{
  ip[0] = ip32 >> 24;
  ip[1] = ip32 >> 16;
  ip[2] = ip32 >> 8;
  ip[3] = ip32 >> 0;
}

//v3
struct arp_cache_s *find_arp_rec(unsigned ip32)
{
  struct arp_cache_s *rec;
  for(rec = arp_cache; rec < &arp_cache[ARP_CACHE_SIZE]; ++rec)
  {
    if(rec->ip32 == ip32) return rec;
  }
  return 0;
}

//v3
struct arp_cache_s *new_arp_rec(unsigned ip32)
{
  struct arp_cache_s *rec;
  for(rec = arp_cache; rec < &arp_cache[ARP_CACHE_SIZE]; ++rec)
  {
    if(rec->ip32 == 0)
    {
      rec->ip32 = ip32;
      memset(rec->mac, 0, 6);
      rec->timestamp = ip_clock;
      rec->state = ARP_STATE_RESOLVING;
      return rec;
    }
  }
  return 0;
}

//v3
void scan_arp_waiting_packets(struct arp_cache_s *arp) // also purges arp waiting pool
{
  unsigned dest_ip32 = arp->ip32;
  enum arp_state_e arp_state = arp->state;
  struct arp_waiting_s *sp = arp_waiting;
  unsigned pkt;

  for(pkt = 0; pkt<NIC_RAM_SIZE; ++pkt, ++sp)
  {
    if(sp->dest_ip32 == dest_ip32)
    {
      if(arp_state == ARP_STATE_RESOLVED)
        ip_send_to_mac(pkt, sp->ip_payload_len, sp->remove_flag, arp->mac); // send
      else
        if(sp->remove_flag) nic_free_packet(pkt); // dump
      sp->dest_ip32 = 0; // clear arp waiting status
    }
  }

}

//v3
void ip_exec(void)
{
  struct arp_cache_s *rec;
  static unsigned old_ip_clock;

  if(resolved_flag) // send arp-waiting by hint
  {
    for(rec = arp_cache; rec < &arp_cache[ARP_CACHE_SIZE]; ++rec)
    {
      if(rec->state == ARP_STATE_RESOLVED)
      {
        scan_arp_waiting_packets(rec); // send pkts waiting for this rec resolution
        rec->state = ARP_STATE_AGING; // change state after sending
      }
    }
    resolved_flag = 0;
  }

  if(old_ip_clock != ip_clock) // age out once per tick
  {
    old_ip_clock = ip_clock;
    for(rec = arp_cache; rec < &arp_cache[ARP_CACHE_SIZE]; ++rec)
    {
      if(rec->ip32 == 0) continue;
      unsigned age = ip_clock - rec->timestamp;
      if(rec->state == ARP_STATE_RESOLVING)
      {
        if(age > IP_ARP_TIMEOUT)
        {
          scan_arp_waiting_packets(rec); // drop waiting packets
          rec->state = ARP_STATE_FREE;
          rec->ip32 = 0;
        }
      }
      else if(rec->state == ARP_STATE_AGING)
      {
        if(age > IP_ARP_LIVE_TIME)
        {
          rec->state = ARP_STATE_FREE;
          rec->ip32 = 0;
        }
      }
    } // for
  } // if tick

}


//v3
void update_arp_rec(struct arp_cache_s *rec)
{
  memcpy(rec->mac, mac_head_rx.src_mac, 6); // update mac, timestamp
  rec->timestamp = ip_clock;
  if(rec->state == ARP_STATE_RESOLVING)
  { // now mac is known, start sending waiting packets
    rec->state = ARP_STATE_RESOLVED;
    resolved_flag = 1; // hint
  }
}

//v3
void add_to_arp_cache(unsigned src_ip32)
{
  struct arp_cache_s *arp = find_arp_rec(src_ip32);
  if(!arp) arp = new_arp_rec(src_ip32);
  if(!arp) return;
  update_arp_rec(arp);
}

//v3
void ip_parsing(void)
{
  unsigned pkt;
  unsigned ethertype = mac_head_rx.prot_type[0]<<8 | mac_head_rx.prot_type[1]<<0;
  if(ethertype == IP_PROT_TYPE_ARP)
  {
    //// TO DO защиту от дурака, посылка от себя
    nic_set_rx_body_addr(0);
    nic_get_rx_body((void*)&arp_head_rx, sizeof arp_head_rx);

    /// if(memcmp(arp_head_rx.target_ip, sys_setup.ip, 4) != 0) return;
    if( memcmp(arp_head_rx.target_ip,  sys_setup.ip,  4) != 0  // 7.06.2013 - accept misused ARP (web camera incident)
    &&  memcmp(arp_head_rx.target_mac, sys_setup.mac, 6) != 0 )
      return;

    unsigned arp_op = arp_head_rx.opcode[0]<<8 | arp_head_rx.opcode[1]<<0;
    if(arp_op == 1) // ARP Request, report self MAC
    {
      pkt = nic_create_packet_sized(256);
      if(pkt == 0xff) return;
      mac_head_tx.prot_type[0]=0x08;
      mac_head_tx.prot_type[1]=0x06;
      memcpy(mac_head_tx.src_mac, sys_setup.mac, 6);
      memcpy(mac_head_tx.dest_mac, arp_head_rx.sender_mac, 6);
      nic_put_tx_head(pkt);
      memcpy(arp_head_tx.target_mac, arp_head_rx.sender_mac, 6);
      memcpy(arp_head_tx.target_ip,  arp_head_rx.sender_ip,  4);
      memcpy(arp_head_tx.sender_mac, sys_setup.mac,          6);
      memcpy(arp_head_tx.sender_ip,  sys_setup.ip,           4);
      memcpy(&arp_head_tx,           arp_template,           8);
      nic_set_tx_body_addr(0);
      nic_put_tx_body(pkt, (void*)&arp_head_tx, sizeof arp_head_tx);
      nic_send_packet(pkt, sizeof mac_head_tx + sizeof arp_head_tx, 1);
      return;
    }
    else if(arp_op == 2) // ARP Reply
    {
      unsigned source_ip32 = copy4(arp_head_rx.sender_ip);
      if(!ip_allow_packet(source_ip32)) return; // access filter by source ip
      struct arp_cache_s *arp = find_arp_rec(source_ip32);
      if(!arp) return; // it was no request for this ip
      update_arp_rec(arp);
    }
  }
  else if(ethertype == IP_PROT_TYPE_IP)
  {
    ip_get_rx_header();

    // drop any kind of fragmented ip
    if(ip_head_rx.fragment_offset[0] & (0x20 | 0x1f)) return;
    if(ip_head_rx.fragment_offset[1]) return;

    unsigned ip32 = copy4(ip_head_rx.dest_ip);

    if( ip32 == self_ip32 // unicast
    ||  ip32 == ~0U       // broadcast 255.255.255.255
    ||  ip32 == self_subnet_bcast32 // broadcast in subnet
    ||  (self_ip_not_defined && memcmp(mac_head_rx.dest_mac, sys_setup.mac, 6) == 0) )  // compare mac instead of ip, used in search
    { // process packet
      unsigned source_ip32 = copy4(ip_head_rx.src_ip);
      if(!ip_allow_packet(source_ip32)) return; // access filter by source ip
      struct arp_cache_s *arp;
      arp = find_arp_rec(source_ip32);
      if(arp) update_arp_rec(arp);
      ip_saved_arp_flag = 1;
      ip_rx_body_length = ip_head_rx.total_len[0]<<8 | ip_head_rx.total_len[1]<<0;
      ip_rx_body_length -= (ip_head_rx.ver_ihl & 15) << 2; // ip header len (bytes)
      IP_PARSING;
      ip_saved_arp_flag = 0;
    }
  }
}

//v3
void ip_timer_10ms(void)
{
  static int scale;
  if(++scale < 10) return;
  scale = 0;
  ++ip_clock;
}

//v3
unsigned ip_create_packet_sized(unsigned size, unsigned protocol) // modif. 8.05.2013
{
  unsigned pkt = nic_create_packet_sized(size);
  if(pkt == 0xff) return 0xff;

  memcpy(mac_head_tx.src_mac, sys_setup.mac, 6);
  memset(mac_head_tx.dest_mac, 0x00, 6);
  mac_head_tx.prot_type[0] = 0x08;
  mac_head_tx.prot_type[1] = 0x00;
  nic_put_tx_head(pkt);

  memset(&ip_head_tx, 0x00, sizeof ip_head_tx);
  ip_head_tx.ver_ihl = 0x45;
  ip_head_tx.ttl = 128;
  ip_head_tx.fragment_offset[0] = 0x40;
  ip_head_tx.protocol = protocol;
  memcpy(ip_head_tx.src_ip, sys_setup.ip, 4);
  ip_put_tx_header(pkt);

  return pkt;
}

//v3
unsigned ip_create_packet(void)
{
  return ip_create_packet_sized(6*256, 0);
}

//v3
void ip_free_packet(unsigned pkt)
{
  struct arp_waiting_s *pq;
  if(pkt >= NIC_RAM_SIZE) return;
  pq = &arp_waiting[pkt];
  if(pq->dest_ip32) pq->remove_flag = 1;
  else nic_free_packet(pkt);
}

//v3
void ip_send_to_mac(unsigned pkt, unsigned ip_payload_len, int remove_flag, unsigned char *dest_mac)
{
  nic_get_tx_head(pkt);
  memcpy(mac_head_tx.dest_mac, dest_mac, 6);
  nic_put_tx_head(pkt);

  ip_get_tx_header(pkt);
  unsigned total_len = sizeof ip_head_tx + ip_payload_len;
  ip_head_tx.total_len[0] = total_len >> 8;
  ip_head_tx.total_len[1] = total_len & 0xff;
  calc_and_place_checksum(&ip_head_tx, sizeof ip_head_tx, ip_head_tx.checksum);
  ip_put_tx_header(pkt);

  nic_send_packet(pkt, sizeof mac_head_tx + total_len, remove_flag);
}

//v3
void ip_send_packet(unsigned pkt, unsigned ip_payload_len, int remove_flag)
{
  struct arp_cache_s *arp = arp_cache;

  ip_get_tx_header(pkt);

  unsigned dest_ip32 = copy4(ip_head_tx.dest_ip);
  unsigned nic_len = sizeof mac_head_tx + sizeof ip_head_tx + ip_payload_len;

  if(dest_ip32 == ~0UL || dest_ip32 == self_subnet_bcast32) // is broadcast
  {
    ip_head_tx.ttl = 1; // tx 255.255.255.255 to local subnet only
    ip_send_to_mac(pkt, ip_payload_len, remove_flag, broadcast);
    return;
  }

  if((dest_ip32 ^ self_ip32) & self_mask32) // if foreign subnet
  {
    if(ip_saved_arp_flag && dest_ip32 == copy4(ip_head_rx.src_ip))
    { // ip is known from request, return to request's mac (it may be gateway ip or some weirdo direct mac)
      ip_send_to_mac(pkt, ip_payload_len, remove_flag, mac_head_rx.src_mac);
      goto end;
    }
    else
    { // unknown foreign ip, send to def.gateway
      if(self_gate32 == 0 || self_gate32 == ~0UL) goto end;
      dest_ip32 = self_gate32;
    }
  }

  arp = find_arp_rec(dest_ip32);
  if(!arp)
  {
    arp = new_arp_rec(dest_ip32);
    if(!arp) goto end;
    if(ip_saved_arp_flag && dest_ip32 == copy4(ip_head_rx.src_ip))
      update_arp_rec(arp); // 'late' update of arp cache on reply only, not with src_ip,src_mac in any incoming packets!
    else
      ip_send_arp_request(dest_ip32); // state or arp rec will be ARP_STATE_RESOLVING
  }
  if(arp->state == ARP_STATE_RESOLVING)
  {
    struct arp_waiting_s *p = &arp_waiting[pkt];
    p->dest_ip32 = dest_ip32; // it may be def.gateway instead of packet dest ip!
    p->ip_payload_len = ip_payload_len;
    p->remove_flag = remove_flag;
    nic_resize_packet(pkt, nic_len);
    return; ///// THE BUG!!!! It was no return! 30.09.2013
  }
  else
    ip_send_to_mac(pkt, ip_payload_len, remove_flag, arp->mac);

end:
  if(remove_flag) nic_free_packet(pkt);
  else nic_resize_packet(pkt, nic_len);
}

void ip_send_packet_to(unsigned pkt, unsigned char *ip, unsigned ip_payload_len, int remove_flag) // 8.05.2013
{
  struct ip_header_s *ih = mac_ref(pkt, 0);
  if(ih == 0) return;
  memcpy(ih->dest_ip, ip, 4);
  ip_send_packet(pkt, ip_payload_len, remove_flag);
}

//v3
void ip_send_arp_request(unsigned dest_ip32)
{
  unsigned pkt = nic_create_packet_sized(256);
  if(pkt == 0xff) return;

  memcpy(mac_head_tx.src_mac, sys_setup.mac, 6);
  memset(mac_head_tx.dest_mac, 0xff, 6);
  mac_head_tx.prot_type[0]=0x08;
  mac_head_tx.prot_type[1]=0x06;
  nic_put_tx_head(pkt);

  arp_head_tx.opcode[1] = 1; // request
  memcpy(&arp_head_tx, arp_template, 7); // sans low byte of opcode
  memcpy(arp_head_tx.sender_mac, sys_setup.mac, 6);
  memcpy(arp_head_tx.sender_ip,  sys_setup.ip,  4);
  memset(arp_head_tx.target_mac, 0x00,  6);
  copy32(arp_head_tx.target_ip,  dest_ip32);
  nic_set_tx_body_addr(0);
  nic_put_tx_body(pkt, (void*)&arp_head_tx, sizeof arp_head_tx);

  nic_send_packet(pkt, sizeof mac_head_tx + sizeof arp_head_tx, 1);
}

void ip_get_rx_header(void)
{
  nic_rx_body_pointer = 0;
  nic_get_rx_body((void*)&ip_head_rx, sizeof ip_head_rx);
}

void ip_get_tx_header(unsigned pkt)
{
  nic_tx_body_pointer = 0;
  nic_get_tx_body(pkt, (void*)&ip_head_tx, sizeof ip_head_tx);
}

void ip_put_tx_header(unsigned pkt)
{
  nic_tx_body_pointer = 0;
  nic_put_tx_body(pkt, (void*)&ip_head_tx, sizeof ip_head_tx);
}

void ip_set_rx_body_addr(unsigned addr)
{
  ip_rx_body_pointer = addr;
}

void ip_set_tx_body_addr(unsigned addr)
{
  ip_tx_body_pointer = addr;
}

void ip_get_rx_body(void *buf, unsigned len)
{
  nic_rx_body_pointer = ((ip_head_rx.ver_ihl&15)<<2) + ip_rx_body_pointer;
  nic_get_rx_body(buf, len);
  ip_rx_body_pointer += len;
}

void ip_get_tx_body(unsigned pkt, void *buf, unsigned len)
{
  nic_tx_body_pointer = sizeof ip_head_tx + ip_tx_body_pointer;
  nic_get_tx_body(pkt, buf, len);
  ip_tx_body_pointer += len;
}

void ip_put_tx_body(unsigned pkt, void *buf, unsigned len)
{
  nic_tx_body_pointer = sizeof ip_head_tx + ip_tx_body_pointer;
  nic_put_tx_body(pkt, buf, len);
  ip_tx_body_pointer += len;
}

//v3
int ip_allow_packet(unsigned src_ip32)
{
  return ((src_ip32 ^ filter_ip32) & filter_mask32) == 0;
}


/* ------------------- Gratious ARP ---------------- */

// 3.06.2010 phy_link_status (in phy_xxxx.c) instead of mac_link_status

unsigned garp_next_time;
unsigned char garps_sent;

void garp_exec(void) // add it to main exec loop
{
  if(phy_link_status == 0)
  {
    garps_sent = 0;
    return;
  }
  if(garps_sent == 2) return;
  if(ip_clock < garp_next_time) return;
  if(self_ip_not_defined) return;
  ip_send_arp_request(self_ip32);
  ++garps_sent;
  garp_next_time = ip_clock + 10; // 100ms ticks
}

void garp_init(void) // call it from the end of ip_reload()
{
  if(self_ip_not_defined) return;
  ip_send_arp_request(self_ip32);
  garps_sent = 1;
  garp_next_time = ip_clock + 10; // 100ms ticks
}

/* ------------ end of Gratious ARP ---------------- */


int valid_ip(unsigned char *ip)
{
  unsigned ip32;
  util_cpy(ip, (void*)&ip32, 4); // ip may be not aligned!
  return ip32 != 0  &&  (~ip32) != 0;
}


unsigned ip_event(enum event_e event)
{
  switch(event)
  {
  case E_TIMER_10ms:
    ip_timer_10ms();
    break;
  case E_EXEC:
    ip_exec();
    garp_exec();
    break;
  case E_INIT:
    ip_init();
    break;
  }
  return 0;
}

#endif

