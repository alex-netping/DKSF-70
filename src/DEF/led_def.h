
#define LED_MODULE		//Флажок включения модуля

enum leds_e {
  CPU_LED,
  GSM_LED, // not wired on DKST70
  LEDS_NUMBER
};

#define MAX_LED ((unsigned)LEDS_NUMBER)

#define TX_LED CPU_LED
#define TRAFFIC_LED_TIME 100

#define CPU_LED_ON()    led(CPU_LED,1)
#define CPU_LED_OFF()   led(CPU_LED,0)

#include "led\led.h"
