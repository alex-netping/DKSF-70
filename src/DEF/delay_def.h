#define DELAY_MODULE		//������ ��������� ������
//#define DELAY_DEBUG		//������ ��������� ������� � ������

#ifdef  T1_USED
#error "Timer usage conflict!"
#endif
#define T1_USED

#include "delay\delay.h"
