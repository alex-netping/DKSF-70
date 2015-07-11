#define LOG_MODULE		//Флажок включения модуля

// physical EEPROM addresses for LOG circular message buffer!
/*
#define  LOG_SIZE    8192
#define  LOG_END    32767   // last byte of circular log
#define  LOG_START  (LOG_END-LOG_SIZE+1) // first byte of circular log
*/

// eeprom_log[]  is defined in in eeprom_map.h
#define  LOG_SIZE   (sizeof eeprom_log)
#define  LOG_END    ((unsigned)eeprom_log + sizeof eeprom_log - 1)  // last byte of circular log
#define  LOG_START  ((unsigned)eeprom_log) // first byte of circular log
//

#define  LOG_BLOCK_SIZE   256 //  in DKSF160, it's AT45DB021 block
#define  LOG_READ(addr, buf, size)   log_read(addr, buf, size)
#define  LOG_WRITE(addr, buf, size)  log_write(addr, buf, size)


#include "LOG\LOG.h"
