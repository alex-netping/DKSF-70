/*
* ping.c
*v1.1
*23.01.08 by Kovlyagin V.N.
*25.03.2010 modified by LBS
*v1.3
*2.06.2010 compatible rewrite by LBS
*v2.4-48
*  rewrite, internal retries, removed unused functionality
*v2.5-52
*  non-valid ip => channel fail (use with DNS)
*/


#include "platform_setup.h"
#include <string.h>

#ifdef PING_MODULE

#if ICMP_VER*100+ICMP_BUILD < 108
#error "Update icmp.c to 1.8+!"
// uncompatible arguments of icmp_send_packet()!
#endif
	
struct ping_state_s ping_state[PING_MAX_CHANNELS];
unsigned ping_reset;
unsigned ping_start;
unsigned ping_completed;

unsigned long ping_sequence = 0x10043119;

// must be less 200 bytes for 1-segment packet !!!
const unsigned char ping_test_data[] = "PING PING PING PING";

#define PING_TABLE_END  &ping_state[PING_MAX_CHANNELS]

void ping_init(void)
{
  struct ping_state_s *ping;
  for(ping = ping_state; ping < PING_TABLE_END; ++ping)
  {
    ping->state = PING_RESET;
    ping->result = 0xff; // undef
  }
  ping_start = 0;
  ping_completed = 0;
}

unsigned ping_send(struct ping_state_s *ping)
{
  unsigned pkt = icmp_create_packet_sized(256);
  if(pkt == 0xff) return 0xff;

  ping_sequence += 0x00010001;
  ping->icmp_seq = ping_sequence;

  ip_get_tx_header(pkt);
  util_cpy(ping->ip, ip_head_tx.dest_ip, 4);
  ip_put_tx_header(pkt);

  icmp_tx_header.type = 8;
  icmp_tx_header.opcode = 0;
  memcpy(icmp_tx_header.id, &ping_sequence, 2+2);
  icmp_put_tx_header(pkt);

  icmp_tx_body_pointer = 0;
  icmp_put_tx_body(pkt, (void*)ping_test_data, sizeof ping_test_data);

  icmp_send_packet(pkt, icmp_tx_body_pointer);
  return 0;
}

void ping_exec(void)
{
  systime_t time = sys_clock();
  struct ping_state_s *ping;
  unsigned mask = 1;
  for(ping = ping_state; ping < PING_TABLE_END; ++ping, mask<<=1)
  {
    if(ping_start & mask)
    {
      ping->state = PING_START;
      ping_start &=~ mask;
    }
    if(ping_reset & mask)
    {
      ping->state = PING_RESET;
      ping_reset &=~ mask;
    }
    switch(ping->state)
    {
    case PING_START:
      ping->count = 0;
      if(!valid_ip(ping->ip))
      {
        ping->result = 0;
        ping->state = PING_COMPLETED;
        ping_completed |= mask;
        break;
      }
      // no break, pass to first retry
    case PING_RETRY:
      if(ping_send(ping) == 0xff) break;
      ++ ping->count;
      ping->wait_end_time = time + ping->timeout;
      ping->state = PING_WAIT_ANSWER;
      ping_completed &=~ mask;
      break;
    case PING_WAIT_ANSWER:
      if(time > ping->wait_end_time)
      {
        if(ping->count < ping->max_retry)
        {
          ping->state = PING_RETRY;
        }
        else
        {
          ping->result = 0;
          ping->state = PING_COMPLETED;
          ping_completed |= mask;
        }
      }
      break;
    } // switch state
  } // for ch
}


extern void ping_parsing(void)
{
  struct ping_state_s *ping;
  unsigned mask = 1;
  for(ping = ping_state; ping < PING_TABLE_END; ++ping, mask<<=1)
  {
    if(ping->state != PING_WAIT_ANSWER) continue;
    if(memcmp((void*)&ping->icmp_seq, icmp_rx_header.id, 2+2) != 0) continue;
    if(memcmp(ping->ip, ip_head_rx.src_ip, 4) != 0) continue;
    ping->result = 1;
    ping->state = PING_COMPLETED;
    ping_completed |= mask;
  }
}

void ping_event(enum event_e event)
{
  switch(event)
  {
  case E_EXEC:
    ping_exec();
    break;
  case E_INIT:
    ping_init();
    break;
  }
}
#endif // End PING_MODULE

