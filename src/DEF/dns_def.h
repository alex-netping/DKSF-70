#define DNS_MODULE

#define DNS_MAX_SLOTS (8 + 1 + 1 + 2 + 3)  // (system+1spare) + sms pinger + logic pingers + 1 ch wdog

#define DNS_MAX_NAME 64
#define DNS_MAX_ATTEMPTS 3


#include "dns/dns.h"
