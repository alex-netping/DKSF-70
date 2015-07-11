#ifndef WTIMER_DEF_H
#define WTIMER_DEF_H

#define WTIMER_MODULE


// используемое (открытое в интерфейсе) количество моментов вкл-выкл в течение суток
#define WTIM_ONOFF_POINTS_N  8 // <=8!
// используемое (открытое в интерфейсе) количество доп. суточных расписаний
#define WTIM_AUX_SCHEDULE_N  3 // <=3!

//количество розеток
#define WTIMER_MAX_CHANNEL  1


#include "wtimer\wtimer.h"

#endif
