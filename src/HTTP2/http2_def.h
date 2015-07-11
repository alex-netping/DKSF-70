
#define HTTP_MODUL		//Флажок включения модуля
//#define HTTP_DEBUG		//Флажок включения отладки в модуле

//---- Переопределение внешних связей модуля----------

// HTTP v2 (by LBS) -------------------------------

#define TCP_CONN_IS_CLOSED(socket) (tcp_socket[socket].tcp_state == TCP_LISTEN)

// положение http ресурсов в памяти (основное адр. пространство - cpu flash или eeprom)
#define HTML_RES_IS_IN_CPU_FLASH
/*#define HTML_RES_IS_IN_EEPROM*/
#define HTTP_RES_START 0x20000



#include "http2\http2.h"

