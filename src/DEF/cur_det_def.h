
#define CUR_DET_MODUL		//Флажок включения модуля
#define CUR_DET_MODULE

#define CURDET_ADC_I_CHANNEL   4
#define CURDET_ADC_V_CHANNEL   5
#define CURDET_FILT_DEPTH     16  // 16*8=128 samples

#if CPU_FAMILY == LPC17xx
#define CURDET_ADC_DIV 4   // 24 MHz PCLK -> 6 MHz ADC clock, max for LPC17xx is 12.6 MHz
#else
#error "Define ADC divider for this CPU!"
// 3.4 MHz was used on LPC2366
#endif

// gpio pins definition

//#define LIMIT_RESET 2,6
#define ENA_12V_CURSENS     3,12 // DKST70

#include "cur_det\cur_det.h"
