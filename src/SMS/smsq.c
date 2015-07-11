/*
#include "platform_setup.h"
#include "string.h"

#define SMS_Q_LEN 8
#define SMS_Q_MSG_LEN 160

#ifdef SMS_MODULE

char qsms[SMS_Q_LEN][SMS_Q_MSG_LEN];

void sms_q_text(char *txt)
{
  int i, n;
  for(i=0; i<SMS_Q_LEN; ++i)
  {
    if(qsms[i][0] == 0)
    { // protected copy
      char *p = &qsms[i][0];
      for(n=0; n<SMS_Q_MSG_LEN-1 && txt[n]; ++n) *p++ = *txt++;
      *p = 0;
    }
  }
}

void sms_q_advance(void)
{
  if(qsms[0][0] == 0) return;
  memcpy(qsms[0], qsms[1], sizeof(qsms) - sizeof(qsms[0]) );
  memset(qsms[SMS_Q_LEN - 1], sizeof(qsms[0]), 0);
}



void sms_exec(void)
{
  if(sms_state == SMS_IDLE)
  {
    sms_printf("at+cmgs=
  }
}


#endif // SMS_MODULE
*/
