#define MODULE0  &pwr_struct

#define MODULE0_INITS  PWR_INITS
#define MODULE0_EXECS  PWR_EXECS
#define MODULE0_TIMERS PWR_TIMERS
#define PWR_MODUL		//Флажок включения модуля
//#define PWR_DEBUG		//Флажок включения отладки в модуле
///Определения приоритетов INIT/EXEC
#define PWR_INIT1_PRI	15
#define PWR_EXEC1_PRI	5
#define PWR_TIMER1_PRI	5
//---- Переопределение внешних связей модуля----------
///Кол-во  каналов управления питанием
#define PWR_MAX_CHANNEL 1
#define PWR_MAX_TARGETS 3
/*! Функция управления реле
\param ch   - номер канала питания 
\param relay_st - состояние реле 1-вкл, 0-выкл
*/
#define PWR_RELAY(ch,relay_st)

/*! Функция чтения режима работы реле
\param ch   - номер канала питания 
\param mode - режим работы реле 
*/
#define PWR_GET_RELAY_MODE(ch,mode)
/*! Функция чтения времени сброса реле для заданного канала
\param ch   - номер канала питания 
\param delay - время сброса
*/
#define PWR_GET_RESET_DELAY(ch,delay)
/*! Функция чтения интервала между ICMP запросами
\param ch   - номер канала питания 
\param delay - интервал
*/
#define PWR_GET_PING_INTERVAL(ch,delay)
/*! Функция чтения времени ожидания ответа на ICMP запрос
\param ch   - номер канала питания 
\param delay - таймаут ожидания ответа
*/
#define PWR_GET_PING_TIMEOUT(ch,delay)
/*! Функция чтения времени восстановления устройства после сброса питания
\param ch   - номер канала питания 
\param delay - время восстановления
*/
#define PWR_GET_RECOVERY_TIME(ch,delay)

/*! Функция получения IP адреса с номером target для канала ch
\param ch   - номер канала питания 
\param target - номер IP адреса
\param ip -(unsigned char*) указатель на буфер куда надо поместить ip
*/
#define PWR_GET_TARGET(ch,target,ip)

/*! Функция получения логики работы для канала ch
\param ch   - номер канала питания 
\param logic - логика работы
*/
#define PWR_GET_LOGIC(ch,logic)

/*! Функция увеличения счетчика неудачных попыток 
\param ch - номер канала
*/
#define PWR_INC_FAIL(ch)

/*! Функция чтения задержки между переключениями каналов питания
\param delay - задержка
*/
#define PWR_GET_SWITCH_INTERVAL(delay)

#define PWR_GET_MAX_FAIL(ch,num)

#define PWR_GET_DOUBLE(ch,num)

#include "pwr\pwr.h"
