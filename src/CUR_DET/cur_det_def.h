#define MODULE0  &cur_det_struct
#include "cur_det\cur_det.h"
#define MODULE0_INITS  CUR_DET_INITS
#define MODULE0_EXECS  CUR_DET_EXECS
#define MODULE0_TIMERS CUR_DET_TIMERS
#define CUR_DET_MODUL		//Флажок включения модуля
//#define CUR_DET_DEBUG		//Флажок включения отладки в модуле
///Определения приоритетов INIT/EXEC
#define CUR_DET_INIT1_PRI	        15
#define CUR_DET_EXEC1_PRI	        5
#define CUR_DET_TIMER1_PRI	        3
//---- Переопределение внешних связей модуля---------
