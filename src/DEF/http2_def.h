
#define HTTP_MODULE		//Флажок включения модуля
//#define HTTP_DEBUG		//Флажок включения отладки в модуле

//---- Переопределение внешних связей модуля----------

// HTTP v2 (by LBS) -------------------------------

#define TCP_CONN_IS_CLOSED(socket) (tcp_socket[socket].tcp_state == TCP_LISTEN)

#define HTTP_INPUT_DATA_SIZE 3072

// положение http ресурсов в памяти (основное адр. пространство - cpu flash или eeprom)
#define HTML_RES_IS_IN_CPU_FLASH
/*#define HTML_RES_IS_IN_EEPROM*/
//#define HTML_RES_START 0x40000



#include "http2\http2.h"

