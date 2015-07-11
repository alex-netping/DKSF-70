#define MODULE0  &pwr_struct

#define MODULE0_INITS  PWR_INITS
#define MODULE0_EXECS  PWR_EXECS
#define MODULE0_TIMERS PWR_TIMERS
#define PWR_MODUL		//������ ��������� ������
//#define PWR_DEBUG		//������ ��������� ������� � ������
///����������� ����������� INIT/EXEC
#define PWR_INIT1_PRI	15
#define PWR_EXEC1_PRI	5
#define PWR_TIMER1_PRI	5
//---- ��������������� ������� ������ ������----------
///���-��  ������� ���������� ��������
#define PWR_MAX_CHANNEL 1
#define PWR_MAX_TARGETS 3
/*! ������� ���������� ����
\param ch   - ����� ������ ������� 
\param relay_st - ��������� ���� 1-���, 0-����
*/
#define PWR_RELAY(ch,relay_st)

/*! ������� ������ ������ ������ ����
\param ch   - ����� ������ ������� 
\param mode - ����� ������ ���� 
*/
#define PWR_GET_RELAY_MODE(ch,mode)
/*! ������� ������ ������� ������ ���� ��� ��������� ������
\param ch   - ����� ������ ������� 
\param delay - ����� ������
*/
#define PWR_GET_RESET_DELAY(ch,delay)
/*! ������� ������ ��������� ����� ICMP ���������
\param ch   - ����� ������ ������� 
\param delay - ��������
*/
#define PWR_GET_PING_INTERVAL(ch,delay)
/*! ������� ������ ������� �������� ������ �� ICMP ������
\param ch   - ����� ������ ������� 
\param delay - ������� �������� ������
*/
#define PWR_GET_PING_TIMEOUT(ch,delay)
/*! ������� ������ ������� �������������� ���������� ����� ������ �������
\param ch   - ����� ������ ������� 
\param delay - ����� ��������������
*/
#define PWR_GET_RECOVERY_TIME(ch,delay)

/*! ������� ��������� IP ������ � ������� target ��� ������ ch
\param ch   - ����� ������ ������� 
\param target - ����� IP ������
\param ip -(unsigned char*) ��������� �� ����� ���� ���� ��������� ip
*/
#define PWR_GET_TARGET(ch,target,ip)

/*! ������� ��������� ������ ������ ��� ������ ch
\param ch   - ����� ������ ������� 
\param logic - ������ ������
*/
#define PWR_GET_LOGIC(ch,logic)

/*! ������� ���������� �������� ��������� ������� 
\param ch - ����� ������
*/
#define PWR_INC_FAIL(ch)

/*! ������� ������ �������� ����� �������������� ������� �������
\param delay - ��������
*/
#define PWR_GET_SWITCH_INTERVAL(delay)

#define PWR_GET_MAX_FAIL(ch,num)

#define PWR_GET_DOUBLE(ch,num)

#include "pwr\pwr.h"
