#ifndef WTIMER_H
#define WTIMER_H


struct wtimer_setup_s {
  // ���� ���������
  unsigned char flags;
  // ���� 1..6 = 1 ������ ����������� ���������� � ����. ��� ������
  unsigned char same_as_prev_day;
  // ������ ������ - ���� ������ + �������� ����������
  // ������ ������ - ����� ��� (������) ���� (�������� �������), ������ � ������ �����
  unsigned short schedule[10][8];
  // ������ �����, ������������ ��������� (����������) ��� �������������� ������
  unsigned cycle_time; // unused
  unsigned active_time; // unused
  // �������� �����, ������ NTP >> 32
  unsigned starting_point; // unused
  unsigned signature;
  unsigned char reserved[28];
};

struct wtimer_status_s {
  signed char row;   // currently active schedule point row number
  signed char col;   // currently active schedule point column number
  signed char hol_idx; // currently active index in wtimer_hilidays[]
};

struct wtimer_holidays_s {
  // ������� ����������
  unsigned char hol_day[24];
  unsigned char hol_month[24];
  unsigned char hol_replacement[24];
};

// "������" �������� ��� onoff

#define WTIM_UNUSED_TIME  0xffff

// flags bits

#define WTIM_PERIODIC           1
#define WTIM_USE_WATCHDOG       2
#define WTIM_ACTIVE_STATE_ON    4
#define WTIM_WEEKLY             8
#define WTIM_IGNORE_WRONG_TIME 16 // 18.08.2014

extern unsigned char wtimer_schedule_output[WTIMER_MAX_CHANNEL];

void wtimer_init();
void wtimer_event(enum event_e event);

#endif
