#define MODULE26  &io_struct

#define MODULE26_INITS  IO_INITS
#define MODULE26_EXECS  IO_EXECS
#define MODULE26_TIMERS IO_TIMERS
#define IO_MODUL		//Флажок включения модуля

//#define IO_DEBUG		//Флажок включения отладки в модуле

///Определения приоритетов INIT/EXEC
#define IO_INIT1_PRI	240
#define IO_EXEC1_PRI	240
#define IO_TIMER1_PRI	240
//---- Переопределение внешних связей модуля----------

///Кол-во  линий ввода-вывода
#define IO_MAX_CHANNEL 2

#include "io\io.h"
