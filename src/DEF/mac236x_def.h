#define MAC236X_MODULE		//Флажок включения модуля
//#define MAC236X_DEBUG		//Флажок включения отладки в модуле

#define PHY_MIIM_ADDR  1

///Кол-во страниц для хранения принятых пакетов
#define MAC236X_MAC_RX_PAGE_NUM  24
///Кол-во страниц по 256 байт для хранения передаваемых пакетов
#define MAC236X_MAC_TX_PAGE_NUM  44
///Кол-во дескрипторов передаваемых пакетов
#define MAC236X_MAC_TX_DESCR_NUM 44

#define MAC236X_TX_PACKET_SIZE 256
#define MAC236X_RX_PACKET_SIZE 256

#include "mac236x\mac236x.h"
