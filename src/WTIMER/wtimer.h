#ifndef WTIMER_H
#define WTIMER_H


struct wtimer_setup_s {
  // биты настройки
  unsigned char flags;
  // биты 1..6 = 1 значит скопировать расписание с пред. дня недели
  unsigned char same_as_prev_day;
  // первый индекс - день недели + запасные расписания
  // второй индекс - время вкл (четные) выкл (нечётные индексы), минуты с начала суток
  unsigned short schedule[10][8];
  // период цикла, длительность включения (выключения) для периодического режима
  unsigned cycle_time; // unused
  unsigned active_time; // unused
  // привязка цикла, формат NTP >> 32
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
  // подмена праздников
  unsigned char hol_day[24];
  unsigned char hol_month[24];
  unsigned char hol_replacement[24];
};

// "пустое" значение для onoff

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
