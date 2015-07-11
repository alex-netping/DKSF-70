#define HW_I2C_MODULE		//Флажок включения модуля

//#define HW_I2C_DEBUG		//Флажок включения отладки в модуле

// edit carefully! double check!
#define I2C_TIMER_TCR  T2TCR
#define I2C_TIMER_PR   T2PR
#define I2C_TIMER_TC   T2TC
#ifdef                 T2_USED
#error "Timer usage conflict!"
#endif
#define                T2_USED

// edit carefully! double check!
////#define HW_I2C0_USED
////#define HW_I2C1_USED
#define HW_I2C2_USED

#include "hw_i2c\hw_i2c.h"

