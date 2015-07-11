/*@{
defgroup STR
* Модуль STR предоставляет утилиты для работы со строками
* модуль позволяет:
*\autor
*version 1.0
*\date 17.09.2007
*
*
12.2009
- новая str_data_to_dec() с проверкой ошибок
активируется включением #include <stdio.h> в начале .c файла:
- модификация str_ip_to_str для сборок использующих stdio.h
- модификация str_mask_to_str для сборок использующих stdio.h
v2.2
7.12.2012
  str_pzt_cpy()
*/

#include "platform_setup.h"
#ifndef  STR_H
#define  STR_H
///Версия модуля
#define  STR_VER	2
///Сборка модуля
#define  STR_BUILD	2

#include <string.h>


extern uword str_error; //Флаг ошибки

//---------------- Раздел, где будут определяться функции модуля ---------------------------

/*!Процедура преобразует входные данные любого типа (байт/слово/двойное слово и т.д) в шестнадцатиричную строку символов (пример: "A67E")
* Прим: строка символов должна оканчиваться нулем.
* Прим: Впереди идущие нули в выходной строке отсекаются
\param data -указатель на входные данные
\param str -указатель на результирующую строку
\param len -длинна данных
*/
extern void str_data_to_hex(unsigned char * data, unsigned char *str,uword len);
/*!Процедура преобразует входные данные любого типа (байт/слово/двойное слово и т.д) в десятичную строку символов (пример: "68")
* Прим: строка символов должна оканчиваться нулем.
* Прим: Впереди идущие нули в выходной строке отсекаются
\param data -указатель на входные данные
\param str -указатель на результирующую строку
\param len -длинна данных
*/
extern void str_data_to_dec(unsigned char * data, unsigned char *str,uword len);
/*!Процедура преобразует входные данные любого типа (байт/слово/двойное слово и т.д) в бинарную строку символов (пример: "101")
* Прим: строка символов должна оканчиваться нулем.
* Прим: Впереди идущие нули в выходной строке отсекаются
\param data -указатель на входные данные
\param str -указатель на результирующую строку
\param len -длинна данных
*/
extern void str_data_to_bin(unsigned char * data, unsigned char *str,uword len);
/*! Процедура преобразовывает входные данные в представление Boolean, т.е. :
* Если (data&1)==1 то поместить в выходную строку "ENABLE", иначе поместить в выходную строку "DISABLE"
\param data -входные данные
\param str -указатель на результирующую строку
*/
extern void str_data_to_bool(uword data, unsigned char *str);
/*! Процедура преобразует входную строку в формате HEX в данные.
* Например: входная строка "0AbC3\0" будет преобразована в данные ABC3
* Прим: строка символов должна оканчиваться нулем.
* Прим: Процедура не чуствительна к регистру вводимых символов
* Прим: В случае если преобразование не возможно, обнулить данные
\param str -указатель на входную строку
\param data- указатеть на выодные данные
\param len- длинна данных
*/
extern void str_hex_to_data( unsigned char *str,unsigned char * data,uword len);
/*! Процедура преобразует входную строку в формате DEC в данные.
* Например: входная строка "0123\0" будет преобразована в данные 123
* Прим: строка символов должна оканчиваться нулем.
* Прим: В случае если преобразование не возможно, обнулить данные
\param str -указатель на входную строку
\param data- указатеть на выодные данные
\param len- длинна данных
*/
extern void str_dec_to_data(unsigned char *str,unsigned char * data, uword len);
/*! Процедура преобразует входную строку в формате BIN в данные.
* Например: входная строка "0110\0" будет преобразована в данные 5
* Прим: строка символов должна оканчиваться нулем.
* Прим: В случае если преобразование не возможно, обнулить данные
\param str -указатель на входную строку
\param data- указатеть на выодные данные
\param len- длинна данных
*/
extern void str_bin_to_data(unsigned char *str, unsigned char * data, uword len);
/*!
! Процедура преобразует входную строку в формате BOOLEAN (ENABLE/DISABE) в данные.
* Например: входная строка EnAble после преобразования в данных 1. входная строка DiSAble после преобразования в данных 1.
* Прим: строка символов должна оканчиваться нулем.
* Прим: В случае если преобразование не возможно, обнулить данные

\param str -указатель на входную строку
\param data -выходные данные
*/
extern uword str_bool_to_data(unsigned char *str);
/*! Процедура преобразует маску подсети в строку маской подсети:
* Пример: маска равна 24, строка на выходе  "255.255.255.0"
* Прим: строка символов должна оканчиваться нулем.
\param data -маска
\param str -выходная строка
*/
extern void str_mask_to_str(uword data,unsigned char *str);
/*! Процедура преобразует строку в маску:
* Пример: маска равна 24, строка на выходе  "255.255.255.0"
* Прим: строка символов должна оканчиваться нулем.
* Прим: В случае если преобразование не возможно, обнулить данные
\param str -входная строка
\param data -маска
*/
extern void str_str_to_mask(unsigned char *str,unsigned char *data);

/*! Процедура преобразует IP адрес в строку символов в формате xx.yy.zz.ff  (напимер 192.168.0.100) :
* Прим: строка символов должна оканчиваться нулем.
\param data -маска
\param str -выходная строка
*/
extern void str_ip_to_str(unsigned char *data,unsigned char *str);
/*! Процедура преобразует строку IP адреса в формате xx.yy.zz.ff в IP адрес.
* Прим: строка символов должна оканчиваться нулем.
* Прим: В случае если преобразование не возможно, обнулить данные
\param str -входная строка
\param data -маска
*/
extern void str_str_to_ip(unsigned char *str,unsigned char *data);

/*! Процедура преобразует MAC адрес в строку символов в формате xx.yy.zz.ff  (напимер 192.168.0.100) :
* Прим: строка символов должна оканчиваться нулем.
\param data -маска
\param str -выходная строка
*/
extern void str_mac_to_str(unsigned char *data,unsigned char *str);

/*! Процедура преобразует строку MAC адреса в формате xx:yy:zz:ff:gg:hh в MAC адрес
* Прим: строка символов должна оканчиваться нулем.
* Прим: В случае если преобразование не возможно, обнулить данные
\param str -входная строка
\param data -маска
*/
extern void str_str_to_mac(unsigned char *str,unsigned char *data);


/*! Процедура возвращает длинну строки
* Прим: строка символов должна оканчиваться нулем.
\param str - входная строка
*/
extern uword str_lenght(unsigned char *str);

/*! Копирует "паскальную" строку в zero-terminated строку.
    target_size - размер буфера zeroterm_str (включая место под завершающий ноль)
    Если исходная строка велика, она обрезается до target_size-1 символов.
    LBS 09.2009
*/
int str_pasc_to_zeroterm(unsigned char *pasc_str, unsigned char *zeroterm_str, int target_size);

/*! Копирует строку в pascal + zero-terminated строку.
    pasc_str_size - размер pasc_str (включая место под длину и завершающий ноль)
    Если исходная строка велика, она обрезается до pasc_str_size-2 символов.
    LBS 12.2012
*/
void str_pzt_cpy(unsigned char *pasc_str, char *src, size_t pasc_str_size);



/*! Процедура преобразования unsigned long в HEX строку
\param data- данные
\param str - указатель на выходную строку
*/
#define STR_LONG_TO_HEX(data,str) str_data_to_hex((unsigned char*)&data,str,4);
/*! Процедура преобразования unsigned short в HEX строку
\param data- данные
\param str - указатель на выходную строку
*/
#define STR_SHORT_TO_HEX(data,str) str_data_to_hex((unsigned char*)&data,str,2);
/*! Процедура преобразования unsigned char в HEX строку
\param data- данные
\param str - указатель на выходную строку
*/
#define STR_BYTE_TO_HEX(data,str) str_data_to_hex((unsigned char*)&data,str,1);

/*! Процедура преобразования unsigned long в DEC строку
\param data- данные
\param str - указатель на выходную строку
*/
#define STR_LONG_TO_DEC(data,str) str_data_to_dec((unsigned char*)&data,str,4);

/*! Процедура преобразования unsigned short в DEC строку
\param data- данные
\param str - указатель на выходную строку
*/
#define STR_SHORT_TO_DEC(data,str) str_data_to_dec((unsigned char*)&data,str,2);

/*! Процедура преобразования unsigned char в DEC строку
\param data- данные
\param str - указатель на выходную строку
*/
#define STR_BYTE_TO_DEC(data,str) str_data_to_dec((unsigned char*)&data,str,1);

/*! Процедура преобразования unsigned long в BIN строку
\param data- данные
\param str - указатель на выходную строку
*/
#define STR_LONG_TO_BIN(data,str) str_data_to_bin((unsigned char*)&data,str,4);

/*! Процедура преобразования unsigned short в BIN строку
\param data- данные
\param str - указатель на выходную строку
*/
#define STR_SHORT_TO_BIN(data,str) str_data_to_bin((unsigned char*)&data,str,2);

/*! Процедура преобразования unsigned char в BIN строку
\param data- данные
\param str - указатель на выходную строку
*/
#define STR_BYTE_TO_BIN(data,str) str_data_to_bin((unsigned char*)&data,str,1);

/*! Процедура преобразования строки HEX в unsigned long
\param data- данные
\param str - указатель на входную строку
*/
#define STR_HEX_TO_LONG(str,data) str_hex_to_data(str,(unsigned char*)&data,4);

/*! Процедура преобразования строки HEX в unsigned short
\param data- данные
\param str - указатель на входную строку
*/
#define STR_HEX_TO_SHORT(str,data) str_hex_to_data_to_hex(str,(unsigned char*)&data,2);

/*! Процедура преобразования строки HEX в unsigned char
\param data- данные
\param str - указатель на входную строку
*/
#define STR_HEX_TO_BYTE(str,data) str_hex_to_data(str,(unsigned char*)&data,1);

/*! Процедура преобразования строки DEC в unsigned long
\param data- данные
\param str - указатель на входную строку
*/
#define STR_DEC_TO_LONG(str,data) str_dec_to_data(str,(unsigned char*)&data,4);
/*! Процедура преобразования строки DEC в unsigned short
\param data- данные
\param str - указатель на входную строку
*/
#define STR_DEC_TO_SHORT(str,data) str_dec_to_data_to_hex(str,(unsigned char*)&data,2);
/*! Процедура преобразования строки DEC в unsigned char
\param data- данные
\param str - указатель на входную строку
*/
#define STR_DEC_TO_BYTE(str,data) str_dec_to_data((str,unsigned char*)&data,1);
/*! Процедура преобразования строки BIN в unsigned long
\param data- данные
\param str - указатель на входную строку
*/
#define STR_BIN_TO_LONG(str,data) str_bin_to_data(str,(unsigned char*)&data,4);
/*! Процедура преобразования строки BIN в unsigned short
\param data- данные
\param str - указатель на входную строку
*/
#define STR_BIN_TO_SHORT(str,data) str_bin_to_data_to_hex(str,(unsigned char*)&data,2);
/*! Процедура преобразования строки BIN в unsigned char
\param data- данные
\param str - указатель на входную строку
*/
#define STR_BIN_TO_BYTE(str,data) str_bin_to_data((str,unsigned char*)&data,1);

#define STR_BOOL_TO_LONG(str) 1
#define STR_LONG_TO_BOOL(str,data)

#define STR_BOOL_TO_DATA(str,data) str_bool_to_data(str,data)
#define STR_MASK_TO_STR(data,str) str_mask_to_str(data,str)
#define STR_STR_TO_MASK(str,data) str_str_to_mask(str,data)
#define STR_IP_TO_STR(data,str) str_ip_to_str(data,str)
#define STR_STR_TO_IP(str,data) str_str_to_ip(str,data)
#define STR_MAC_TO_STR(data,str) str_mac_to_str(data,str)
#define STR_STR_TO_MAC(str,data) str_str_to_mac(str,data)
#define STR_LENGHT(str) str_lenght(str)


#endif
//}@
