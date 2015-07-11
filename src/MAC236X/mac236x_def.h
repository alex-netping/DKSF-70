#define MAC236X_MODULE		//Флажок включения модуля
//#define MAC236X_DEBUG		//Флажок включения отладки в модуле

///Кол-во страниц для хранения принятых пакетов
#define MAC236X_MAC_RX_PAGE_NUM  18
///Кол-во страниц по 256 байт для хранения передаваемых пакетов
#define MAC236X_MAC_TX_PAGE_NUM  40
///Кол-во дескрипторов передаваемых пакетов
#define MAC236X_MAC_TX_DESCR_NUM 40

#define MAC236X_TX_PACKET_SIZE 256
#define MAC236X_RX_PACKET_SIZE 256

#include "mac236x\mac236x.h"
