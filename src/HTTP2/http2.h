/*
*HTTP2
*\autor P.V.Lyubasov
*\version 21.3
*\date 17.02.2010
*\version 21.4
*\date 18.03.2010
*\version 21.5 (http_reply())
*\date 1.04.2010
* v21.6-50
* 27.01.2011
*   correct 303 redirect, NAT-aware
*v21.7-200
*24.02.2011
*  skip url-encoded parameters after ? in html page search (introduced in wtimer proj)
*  corrected bug in hex_to_byte() - wrong uppercase text conversion
*  http_post_data_part() is modified - void* args, returns converted text length
*v21.8-200
*3.03.2011
*  pdata_pstring() now writes up to 30 chars
*v21.8-50
*9.03.2011
* PNG mime type
*v21.9-52
*30.03.2011
*  uri length in http_parse_headers() has extended to 64 chars
*  uri len in html headers leaved intact
*v21.10-52
*17.05.2011
*  skip data pumping if 2 unconfirmed tcp packets already are in flight
*v21.11-253
*30.10.2011
*  modified 'html data in code segment' feature, for fw update via web
*  direct links in page headers instead of offsets - UNCOMPATIBLE with prev. version!
*  UNCOMPATIBLE with html pages in ext. flash!
*v21.11-52
*5.05.2012
*  cgi args extraction
*v21.12-48
*3.03.2013
  http_get_pumping() optimized, less pumping cycles
  removed util_cmp(), str_dec_to_data() references, stdlib.h used
v21.12-60
25.04.2013
  http_parsing(soc_n) modification, for multiple tcp users
v21.13-60
9.05.2013
  don't pump if tcp state is not ESTABLISHED
v21.14-60
3.06.2013
  pdata_pstring() now puts 62 chars
v21.15-52
14.08.2013
  Server-sent events support
v21.16-48
23.10.2013
  moved SSE retry: to this module from 'clients'
  http_sse_can_send()
v21.17-707
10.12.2013
  pdata_cstring() added
v21.18-70
28.07.2014
  max MTU while pumping html is 1100 byte
v21.19-54
24.10.2014
  SSE ping
*/

#include "platform_setup.h"

#ifndef HTTP_H
#define HTTP_H

#define HTTP_VER     21
#define HTTP_BUILD   19


//---------------- Раздел, где будут определяться константы модуля -------------------------

///Сигнатура наличия таблицы ресурсов http
#define HTTP_ID_SIGN  0x55AA
///Сигнатура наличия таблицы страниц http
#define HTTP_PAGE_SIGN 0x5AA5


enum ERR_ENUM
{
    HTTP_ERR_OK=200,
    HTTP_ERR_BAD_REQUEST=400,
    HTTP_ERR_AUTORISATION=401,
    HTTP_ERR_NOT_FOUND=404
};

extern unsigned char http_init_failed;

// mime-типы, сюда ссылается поле mime структуры page_s
extern const char mime_html[];
extern const char mime_css[];
extern const char mime_js[];
extern const char mime_png[];
extern const char mime_sse[];

// буфер http запроса
extern char req[HTTP_INPUT_DATA_SIZE];

// структура заголовка страницы, автокомпонуются в сегмент HTML_HEADERS скриптом упаковки страниц и макросом HOOK_CGI
struct page_s {
  const char*    name;
  const char*    mime;
  const void*    addr;
  unsigned short size;
  unsigned short flags;
};

// биты поля flags стуктуры page_s
#define HTML_FLG_GET         0x01
#define HTML_FLG_POST        0x02
#define HTML_FLG_COMPRESSED  0x04
#define HTML_FLG_NOCACHE     0x08
#define HTML_FLG_CGI         0x40
#define HTML_FLG_SS_EVENT    0x80

// макрос для генерации заголовков CGI
// usage:
//     HOOK_CGI(ioset, (void*)io_http_set, mine_js, HTML_FLG_POST);  // ioset.cgi
//     HOOK_CGI(ioget, (void*)io_http_get, mime_js, HTML_FLG_GET | HTML_FLG_NOCACHE);  // ioget.cgi

#define HOOK_CGI(name, proc, mime, flags)    \
    __root const struct page_s name##_cgi_hdr @ "HTML_HEADERS" =  \
                { "/" #name ".cgi", mime, (void*)proc, 0, (flags)|HTML_FLG_CGI }

typedef enum {
  HTTP_IDLE = 0,
  HTTP_RCV_HEADERS,
  HTTP_RCV_POST_DATA,
  HTTP_SEND_HEADERS,
  HTTP_SEND_PAGE,
  HTTP_COMPLETE
} http_state_t;

// состояние http соединения
struct http_s {
  struct  page_s  *page;
  int             tcp_session;
  http_state_t    state;
  int             post_content_length;
  int             sent;
  unsigned        more_data;
};
extern struct http_s http;
extern char sse_sock;

// part of url after '?', z-terminated, empty if no ? in requested url
extern char req_args[64];

// перенаправляет броузер. location - ссылка на страницу ("/index.html" etc.)
void http_redirect(char *location);

void http_redirect_to_addr(unsigned char *ip, unsigned short port, char *location);

// отвечает с кодом code (по факту, всегда 200 - не реализовано)
//   с данными data (zero-term, up to 1024 byte)
void http_reply(int code, char *data);

// size of 'data=' text in data, POSTed by webinterface page
#define HTTP_POST_HDR_SIZE (sizeof "data=" - 1) // compiler dependant?

// Помещает hex данные, посланные через POST, по адресу data длиной len
void http_post_data(unsigned char *data, unsigned len);

// Помещает hex данные, на которые указывает src_text, по адресу data длиной len
// возвращает длину потреблённого текста (len*2)
unsigned http_post_data_part(char *src_text, void *data, unsigned len);

// для PLINK системы, добавляет в буфер dest паскальную строку и глушит нулём
int addpstring(char *dest, unsigned char *pstr);

// для PLINK системы, добавляет в буфер dest 'имя:"паскальная строка",' и глушит нулём
int pdata_pstring(char *dest, char *name, unsigned char *pstr);

// для PLINK системы, добавляет в буфер dest 'имя:"C строка",' и глушит нулём
int pdata_cstring(char *dest, char *name, char *cstr);

// для PLINK системы, добавляет в буфер dest 'имя:"255.255.255.255",' и глушит нулём
int pdata_ip(char *dest, char *name, unsigned char *ip);

// переводит 2 символа (hex цифры) в байт
unsigned char hex_to_byte(char *s);

// true if SSE session ready to send data to page // 22.10.2013
int http_can_send_sse(void);

void http_init(void);
unsigned http_event(enum event_e event, unsigned evdata_tcp_soc_n);

#endif
