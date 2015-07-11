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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "platform_setup.h"

#define HTTP_MAX_DATA_CHUNK 1024
#define ADD_HTTP_STRING(pkt, str) tcp_put_tx_body(pkt, (unsigned char*)str, sizeof(str)-1);

#pragma segment="HTML_HEADERS"

const char mime_html[]  = "text/html; charset=windows-1251";
const char mime_css[]   = "text/css";
const char mime_js[]    = "application/x-javascript; charset=windows-1251";
const char mime_png[]   = "image/png";
const char mime_sse[]   = "text/event-stream";

char sse_sock = 0xff;
unsigned sse_ping_timer;

// данные из заголовка Host:
char req_host[64];
char req_args[64];
char req_origin[64];

void http_start_get(void);

struct http_s http;

/*
#define ROOT_DATA \
"<html><head>"\
"<meta http-equiv=Content-Type content=\"text/html; charset=windows-1251\">"\
"<title>Веб-интерфейс</title>"\
"<script src=\"devname.js\"></script>"\
"</head>"\
"<body>"\
"test data 1<br>"\
"test data 2<br>"\
"<h2>--- <script>document.write(devname);</script> ---</h2>"\
"</body>"\
"</html>"

struct page_s root_page = {
  "/index.html",
  "text/html; charset=windows-1251",
  (void*)0,
  sizeof(ROOT_DATA) - 1,
  HTTP_GET_ENABLED,
  {0,0,0}
};
char test_html[] = ROOT_DATA;
*/

/*  DKSF 50.7 */
//struct page_s *http_first_page = (void*)(HTTP_RES_START + 4); /*&root_page;*/

/*
   цепочка страниц:
- службы, устанавливаемые at run time
- фикс. страницы в FW блоке
- первая страница ресурсов по фикс. адресу (0x2n000)
- последующие страницы ресурсов
*/

/* DKSF 50.7
// works in program flash and ram! can't use external flash or eeprom!
struct page_s* find_uri(char *uri)
{
  struct page_s *page = http_first_page;
  if(!page) return 0;
  for(;;)
  {
    if(strcmp(page->name, uri) == 0) return page;
    if(page->next == 0 || page->next == (void*)0xFFFFFFFF) break;
    page = page->next;
  }
  return 0;
}
*/

struct page_s* find_uri(char* href)
{
  char uri[32];
  int i;
  for(i=0; i<sizeof uri - 1 && href[i] && href[i]!='?'; ++i) uri[i] = href[i];  // 3.11.2010 by LBS (introduced in wtimer project)
  uri[i] = 0;
  struct page_s *page;
  for(page = __segment_begin("HTML_HEADERS"); (void*)page < __segment_end("HTML_HEADERS"); ++page)
  {
    if(strcmp(page->name, uri) == 0)
    {
      if((page->flags & HTML_FLG_CGI) == 0)
      {
#if defined( HTML_RES_IS_IN_CPU_FLASH )
        // unsigned char *page_sig = (void*)(HTML_RES_START + (unsigned)page->addr - 2);
        unsigned char *page_sig = (unsigned char*)(page->addr) - 2; // 30.10.2011
        if(page_sig[0] != 0x59 || page_sig[1] != 0x95) return 0;
#elif defined( HTML_RES_IS_IN_EEPROM )
#error "Data in ext. memory is not supported in this http2 version!"
        /*
        unsigned char page_sig[2];
        EEPROM_READ(html_base + (unsigned)page->addr - 2, page_sig, sizeof page_sig);
        if(page_sig[0] != 0x59 || page_sig[1] != 0x95) return 0;
        */
#else
#error "Define HTML_RES_IS_IN_xxxxx macro in http2_def.h!"
#endif
      }
      return page;
    }
  }
  return 0;
}


/* DKSF 50
// page must be in RAM!
void http_hook_page(struct page_s* page)
{
  // if http pages are not in place, drop pointer to it
  // 0xE3A6 is signeture for THIS VERSION page format
  if(*(unsigned short*)HTTP_RES_START != 0xE3A6 && (unsigned)http_first_page == (HTTP_RES_START+4)) http_first_page = 0;
  //
  if(find_uri(page->name)) return;
  page->next = http_first_page;
  http_first_page = page;
}

void http_hook_pages(struct page_s *page, unsigned sizeof_pages)
{
  struct page_s *p = page;
  struct page_s *end = (void*)((char*)page + sizeof_pages);
  if(page == end) return;
  for(;;)
  {
    http_hook_page(p++);
    if(p >= end) break;
  }
}
*/


////////  base64 decoder, LBS 06.2009 - 09.2009

unsigned char decode_minibyte(unsigned char c)
{
  if     (c>='A' && c<='Z') c = c - 'A' +  0;
  else if(c>='a' && c<='z') c = c - 'a' + 26;
  else if(c>='0' && c<='9') c = c - '0' + 52;
  else if(c=='+' || c=='-') c = 62;
  else if(c=='/' || c=='_') c = 63;
  else c = 0;
  return c;
}

// no CR LFs on input!
void base64_decode(char *in, char *out, int maxlen)
{
  unsigned long v;
  int n;
  if(maxlen > 0)
  {
    for(n=maxlen;;)
    {
      if(*in==0 || *in == '=') break;
      v  = decode_minibyte(*in++) << 18;
      if(*in==0 || *in == '=') break;
      v |= decode_minibyte(*in++) << 12;
      *out++ = (v >> 16) & 0xff;
      if(--n == 0) break;

      if(*in==0 || *in == '=') break;
      v |= decode_minibyte(*in++) <<  6;
      *out++ = (v >> 8) & 0xff;
      if(--n == 0) break;

      if(*in==0 || *in == '=') break;
      v |= decode_minibyte(*in++);
      *out++ = v & 0xff;
      if(--n == 0) break;
    }
  }
  *out = 0;
}

/*
AA==  0
AAA=  00
AAAA  000
*/

//////////

void http_responce_options(void) // 16.12.2014
{
  http.state = HTTP_SEND_HEADERS;
  unsigned pkt = tcp_create_packet(http.tcp_session);
  if(pkt == 0xff) return;
  char buf[512];
  char *p = buf;
  p += sprintf(p,
    "HTTP/1.1 200 Ok\r\n"
    "Access-Control-Allow-Origin: %s\r\n"
    "Access-Control-Allow-Credentials: true\r\n"
    "Access-Control-Allow-Methods: OPTIONS, %s\r\n"
    "Content-Type: %s\r\n"
    "Content-Length: 0\r\n"
    "\r\n",
      req_origin,
      (http.page->flags & HTML_FLG_POST) ? "POST" : "GET",
      http.page->mime
             );
  tcp_tx_body_pointer = 0;
  tcp_put_tx_body(pkt, (void*)buf, p - buf);
  tcp_send_packet(http.tcp_session, pkt, tcp_tx_body_pointer);
  tcp_close_session(http.tcp_session);
  http.state = HTTP_COMPLETE;
}

void http_responce_err(int code)
{
  char buf[256];
  char *s = buf;
  switch(code)
  {
  case 404:
    s += sprintf(s,"HTTP/1.1 404 Not Found\r\n");
    break;
  case 401:
    s += sprintf(s,
       "HTTP/1.1 401 Unauthorized\r\n"
       "WWW-Authenticate: Basic realm=\"%s\"\r\n",
           device_name);
    if(req_origin[0] != 0)
    {
      s += sprintf(s,
       "Access-Control-Allow-Origin: %s\r\n"
       "Access-Control-Allow-Credentials: true\r\n",
              req_origin);
    }
    break;
  default: // incl. 500
    s += sprintf(s,
      "HTTP/1.1 500 Internal Server Error\r\n"  );
    break;
  }
  s += sprintf(s, "\r\n");
  unsigned pkt = tcp_create_packet(http.tcp_session);
  if(pkt == 0xFF) return;
  unsigned len = (unsigned)(s - buf);
  tcp_tx_body_pointer = 0;
  tcp_put_tx_body(pkt, (unsigned char*)buf, len);
  tcp_send_packet(http.tcp_session, pkt, len);
  tcp_close_session(http.tcp_session);
  http.state = HTTP_COMPLETE;
}


static void send_redirect(char *location, unsigned location_len)
{
  if(location_len > 1024) return; // 16.12.2014 moved
  unsigned pkt = tcp_create_packet(http.tcp_session);
  if(pkt == 0xFF) return;
  tcp_tx_body_pointer = 0;
  tcp_put_tx_body(pkt, (unsigned char*)location, location_len);
  tcp_send_packet(http.tcp_session, pkt, location_len);
  tcp_close_session(http.tcp_session);
  http.state = HTTP_COMPLETE;
}

void http_redirect_to_addr(unsigned char *ip, unsigned short port, char *location)
{
  char buf[256];
  int n;
  n = sprintf(buf,
   "HTTP/1.1 303 See Other\r\n"
   "Location: http://%d.%d.%d.%d:%d%s\r\n"
   "Connection: Close\r\n\r\n",
   sys_setup.ip[0], sys_setup.ip[1], sys_setup.ip[2], sys_setup.ip[3],
   sys_setup.http_port,
   location
  );
  send_redirect(buf, n);
}

void http_redirect(char *location)
{
  char buf[192];
  int n = sprintf(buf,
   "HTTP/1.1 303 See Other\r\n"
     "Location: http://%s%s\r\n"
     "Connection: Close\r\n\r\n",
      req_host,
      location);
  send_redirect(buf, n);
}

// result code is unused now. always 200 OK
void http_reply(int code, char *data) // rewrite with tcp_ref() 16.12.2014
{
  unsigned pkt = tcp_create_packet(http.tcp_session);
  if(pkt != 0xFF)
  {
    unsigned dlen = strlen(data);
    if(dlen > 1024) dlen = 1024;
    char *p = tcp_ref(pkt, 0);
    char *s = p;
    p += sprintf(p,
      "HTTP/1.1 200 Ok\r\n"
      "Connection: Close\r\n"
      "Cache-Control: no-store, no-cache, must-revalidate\r\n"
      "Expires: Fri, 13 Oct 2000 00:00:00 GMT\r\n"
        );
    if(req_origin[0] != 0)
      p += sprintf(p,
      "Access-Control-Allow-Origin: %s\r\n"
      "Access-Control-Allow-Credentials: true\r\n",
        req_origin );
    p += sprintf(p,
      "Content-Type: %s\r\n"
      "Content-Length: %u\r\n"
      "\r\n",
         http.page->mime,
         dlen
         );
    memcpy(p, data, dlen); p += dlen;
    tcp_send_packet(http.tcp_session, pkt, (unsigned)(p - s));
  }
  tcp_close_session(http.tcp_session);
  http.state = HTTP_COMPLETE;
}

void http_get_pumping(unsigned pkt)
{
  struct page_s *page = http.page;

  if(tcp_socket[http.tcp_session].out_in_flight >= 2) return; // 17.05.2011
  if(tcp_socket[http.tcp_session].tcp_state != TCP_ESTABLISHED) return; // 9.05.2013

  if(pkt == 0xFF)
  {
    pkt = tcp_create_packet(http.tcp_session);
    tcp_tx_body_pointer=0;
  }
  if(pkt == 0xFF) return;

  if(http.page->flags & HTML_FLG_CGI) // call service (procedural data generation)
  {
    http.more_data = ((unsigned(*)(unsigned,unsigned))(http.page->addr))(pkt, http.more_data);
    if(http.state == HTTP_IDLE) return; // emergency cancel possibility
  }
  else // send generic http page
  {
#if defined(  HTML_RES_IS_IN_CPU_FLASH  )
    unsigned len = http.page->size - http.sent;
    // unsigned available = (1536 - 128) - tcp_tx_body_pointer;
    unsigned available = (1100 - 14 - 20 - 24)  - tcp_tx_body_pointer; // MTU 1100 // 28.07.2014
    if(len > available) len = available;
    tcp_put_tx_body(pkt, (unsigned char*)page->addr + http.sent, len);
    http.sent += len;
    http.more_data = http.sent < page->size;
#elif defined(  HTML_RES_IS_IN_EEPROM   )
# error "Data in ext. flash is unsupported in this version of http2.c!"
#else
# error "Define HTML_RES_IS_IN_xxxxx macro in http2_def.h!"
#endif
  }
  tcp_send_packet(http.tcp_session, pkt, tcp_tx_body_pointer);
  if(http.more_data == 0)
  {
    // push data, close tcp connection
    tcp_close_session(http.tcp_session);
    http.state = HTTP_COMPLETE;
  }
}

void http_start_get(void)
{
  http.state = HTTP_SEND_HEADERS;
  unsigned pkt = tcp_create_packet(http.tcp_session);
  if(pkt == 0xff) return;
  tcp_tx_body_pointer=0;
  ADD_HTTP_STRING(pkt,
    "HTTP/1.1 200 Ok\r\n"
    "Connection: Close\r\n");
  if(http.page->flags & HTML_FLG_NOCACHE)
  {
    ADD_HTTP_STRING(pkt,
    "Cache-Control: no-store, no-cache, must-revalidate\r\n"
    "Expires: Fri, 28 Apr 2000 00:00:00 GMT\r\n");
  }
  else
  {
    ADD_HTTP_STRING(pkt,
    "Cache-Control: max-age=10000, private\r\n");
  }
  if(http.page->flags & HTML_FLG_COMPRESSED)
  {
    ADD_HTTP_STRING(pkt,
    "Content-Encoding: gzip\r\n");
  }
  char *p = tcp_ref(pkt, tcp_tx_body_pointer);
  char *s = p;
  p += sprintf(p, "Content-Type: %s\r\n", http.page->mime);
  if(req_origin[0] != 0)
  {
    p += sprintf(p, "Access-Control-Allow-Origin: %s\r\n", req_origin);
    p += sprintf(p, "Access-Control-Allow-Credentials: true\r\n");
  }
  p += sprintf(p, "\r\n"); // end of headers
  tcp_tx_body_pointer += (unsigned)(p - s);
  if(http.page->flags & HTML_FLG_SS_EVENT)
  {
    if(sse_sock != 0xff)
    {
      // prepare old SSE socket to work as new http socket
      if(tcp_socket[sse_sock].tcp_state != TCP_RESERVED)
      {
        tcp_send_flags(TCP_MSK_ACK | TCP_MSK_RST | TCP_MSK_PSH, sse_sock);
        tcp_send_flags(TCP_MSK_ACK | TCP_MSK_RST | TCP_MSK_PSH, sse_sock);
        tcp_clear_connection(sse_sock);
      }
      // pass current http socket to SSE use
      unsigned swap_sock = sse_sock;
      sse_sock = http.tcp_session;
      http.tcp_session = swap_sock;
      // add initial data on SSE channel
      static char sse_retry_cmd[] = "retry: 2000\n\n";
      tcp_put_tx_body(pkt, (void*)sse_retry_cmd, strlen(sse_retry_cmd));
      ((unsigned(*)(unsigned, unsigned))(http.page->addr))(pkt, 0);
      // send 1st reply on SSE channel
      tcp_send_packet(sse_sock, pkt, tcp_tx_body_pointer);
      // start listening on alternative socket
      tcp_socket[http.tcp_session].tcp_state = TCP_LISTEN;
      http.state = HTTP_IDLE;
    }
  }
  else
  {
    http.state = HTTP_SEND_PAGE;
    http.sent = 0;
    http.more_data = 0;
    http_get_pumping(pkt);
  }
}


int cmp_char_caseless(char a, char b)
{
  if(a >= 'a' && a <= 'z') a -= 'a' - 'A';
  if(b >= 'a' && b <= 'z') b -= 'a' - 'A';
  if(a < b) return -1;
  if(a > b) return 1;
  return 0;
}

char* util_scan_caseless(char* s, char *sub, char* end, int skip_flag)
{
  int n;
  for(;;)
  {
    for(n=0;;++n)
    {
      if(sub[n] == 0) // matched
      {
        return skip_flag ? s+n : s ;
      }
      if(s+n > end) return 0; // вышли за конец s, результат отрицательный
      if(s[n] == 0) return 0; // то же самое
      if(cmp_char_caseless(s[n], sub[n]) != 0) break; // не совпало
    }
    ++s;
  }
}

char* util_skip_lws(char *s, char* end) //  LWS = [CRLF] 1*( SP | HT )
{
  if(s[0] == '\r' &&
     s[1] == '\n' &&
     (s[2] == ' ' || s[2] == '\t') &&
     s+2 <= end) s+=3;
  while((*s == ' ' || *s == '\t') && s <= end) ++s;
  return s;
}

char* util_skip_not_lws(char *s, char *end)
{
  for( ;s<=end; ++s)
  {
    if(*s == ' ' || *s == '\t' || *s == '\r') break;
  }
  return s;
}

// буфер http запроса
char req[HTTP_INPUT_DATA_SIZE];
// длина данных в буфере запроса
static int req_len = 0;
// указатель на 1й байт в req[] после http заголовков
char *hdr_end;

// добавить остаток принятого пакета к буферу запроса
void get_from_tcp(void)
{
  int len = sizeof req - 1 - req_len; // свободное место в буфере запроса // -1 17.12.2014, for zterm
  int received = tcp_rx_data_length - tcp_rx_body_pointer;
  if(received < len) len = received;
  if(len == 0) return;
  tcp_get_rx_body((unsigned char*)(req + req_len), len);
  req_len += len;
  req[req_len] = 0; // z-term // 17.12.2014
}

// удаляет http заголовки из буфера запроса, cдвигает данные к началу буфера
// char *req_end указывает на первый байт за CR LF CR LF
void drop_headers(void)
{
  if(!hdr_end) return;
  req_len = req + req_len - hdr_end;
  memmove(req, hdr_end, req_len);
  req[req_len] = 0; // z-term 17.12.2014
  hdr_end = 0;
}

int check_user_access(char *user, char *pass)
{
  unsigned n;
  n = sys_setup.uname[0];
  if(n == 0xFF) return 1;
  if(n > 16) n=16;
  if(memcmp((unsigned char*)sys_setup.uname+1, (void*)user, n) != 0) return 0;
  n = sys_setup.passwd[0];
  if(n == 0xFF) return 1;
  if(n > 16) n=16;
  if(memcmp((unsigned char*)sys_setup.passwd+1, (void*)pass, n) != 0) return 0;
  return 1;
}

int http_parse_headers()
{
  char *end;

  // разбор URI
  char *uri = util_scan_caseless(req, " ", req + 6 /* http op max len */, 1);
  if(uri == 0) return 500; // TODO code 501
  char *uri_end = util_scan_caseless(uri, " ", uri + 64 /* max uri len */, 0);
  if(uri_end == 0) return 500; // TODO generate error, too long URI
  *uri_end = 0; // z-term URI
  if(memcmp((unsigned char*)uri, "/", 2) == 0) uri = "/index.html";
  http.page = find_uri(uri);
  if(http.page == 0) return 404;
  *uri_end = ' '; // restore truncated rec data // LBS 27.01.2011

  // выделить аргументы - LBS 5.05.2012
  util_fill((void*)req_args, sizeof req_args, 0);
  char *arg = util_scan_caseless(req, "?", uri_end, 1);
  if(arg != 0)
  {
    unsigned arglen = uri_end - arg;
    if(arglen > sizeof req_args - 1) arglen = sizeof req_args - 1;
    util_cpy((void*)arg, (void*)req_args, arglen);
  }

  // найти начало http заголовков
  char *hdr = util_scan_caseless(uri_end+1, "\r\n", uri_end + 11 /* http version field approx. len + crlf */, 1);
  if(!hdr) return 500;

  // Host // LBS 27.01.2011
  req_host[0] = 0;
  char *host = util_scan_caseless(hdr, "Host:", hdr_end, 1);
  if(host)
  {
    end  = util_scan_caseless(host, "\r\n", hdr_end, 0);
    if(end)
    {
      host = util_skip_lws(host, end);
      unsigned hostlen = end - host;
      if(hostlen < sizeof req_host)
      {
        util_cpy((void*)host, (void*)req_host, hostlen);
        req_host[hostlen] = 0; // z-term
      }
    }
  }
  // CORS Origin // 16.12.2014
  req_origin[0] = 0;
  char *orig = util_scan_caseless(hdr, "Origin:", hdr_end, 1);
  if(orig)
  {
    orig = util_skip_lws(orig, end);
    strlccpy(req_origin, orig, '\r', sizeof req_origin);
  }
  // авторизация
  char *cre = util_scan_caseless(hdr, "Authorization:", hdr_end, 1);
  if(!cre)  return 401;
  end = util_scan_caseless(cre, "\r\n", hdr_end, 0);
  cre = util_scan_caseless(cre, "Basic", end, 1);
  if(!cre) return 401;
  cre = util_skip_lws(cre, end);
  end = util_skip_not_lws(cre, end); //  LWS = [CRLF] 1*( SP | HT )

  char userpass[36], *pass;
  base64_decode(cre, userpass, end - cre);

  for(pass=userpass; *pass!=':'; ++pass) // scan to passwd
    if(*pass == 0) return 401;
  *pass++ = 0; // replace ':' with 0, point to password
  if(check_user_access(userpass, pass) == 0)  return 401;

  // разбор типа операции
  if(memcmp((void*)req, "GET", 3) == 0 && (http.page->flags & HTML_FLG_GET))
  {
    return 290;
  }
  else if(memcmp(req, "OPTIONS", 7) == 0)
  {
    return 297;
  }
  else if(memcmp((void*)req, "POST", 4) == 0 && (http.page->flags & HTML_FLG_POST))
  {
    char *conlen = util_scan_caseless(hdr, "Content-Length:", hdr_end, 1);
    if(!conlen) return 500;
    conlen = util_skip_lws(conlen, hdr_end);
    end = util_scan_caseless(conlen, "\r\n", hdr_end, 0);
    *end = 0;
    //str_dec_to_data((void*)conlen, (void*)&http.post_content_length, sizeof http.post_content_length);
    http.post_content_length = atoi(conlen); // 13.03.2013 dksf48

    return 291;
  }
  return 500;
}

void process_post_data(void)
{
  http.state = HTTP_RCV_POST_DATA;
  if(http.post_content_length == 0)
  {
    http_responce_err(500);
  }
  if(req_len >= http.post_content_length) // все данные POST операции приняты в буфер запроса
  {
    ((void(*)(void))(http.page->addr))(); // обработать принятые данные
    http.state = HTTP_COMPLETE;
  }
}

// обработка входящих пакетов
void http_parsing(unsigned evdata_tcp_soc_n)
{
  if(evdata_tcp_soc_n != http.tcp_session) return; //25.04.2013
  //http.timeout = sys_clock() + HTTP_TIMEOUT;
  switch(http.state)
  {
  case HTTP_IDLE:
  case HTTP_COMPLETE:
    http.state = HTTP_RCV_HEADERS;
    req_len = 0;
    hdr_end = 0;
    http.post_content_length = 0;
    http.more_data = 0;
    // нет break, проваливаемся
  case HTTP_RCV_HEADERS:
    tcp_rx_body_pointer = 0;
    get_from_tcp();
    // определить принят ли конец заголовков
    hdr_end = util_scan_caseless(req, "\r\n\r\n", req + req_len, 1);
    if(!hdr_end)
    {
      if(req_len == sizeof req)
      {
        http_responce_err(500); // буфер заполнен, конец не найден, слишком длинные заголовки
      }
      return;
    }
    else
    {
      int code = http_parse_headers();
      switch(code)
      {
      case 290: // GET
        http.state = HTTP_SEND_HEADERS;
        http_start_get();
        break;
      case 291: // POST
        http.state = HTTP_RCV_POST_DATA;
        drop_headers();
        get_from_tcp(); // дочитать остаток из пакета, если он не влезал ранее
        process_post_data();
        break;
      case 297: // OPTIONS
        http_responce_options();
        break;
      default: // ошибка
        http_responce_err(code);
        break;
      }
    }
    break;
  case HTTP_RCV_POST_DATA:
    tcp_rx_body_pointer = 0;
    get_from_tcp();
    process_post_data();
    break;
  }
}

void http_exec(void)
{
  switch(http.state)
  {
  case HTTP_SEND_HEADERS:
    http_start_get();
    break;
  case HTTP_SEND_PAGE:
    http_get_pumping(0xff);
    break;
  case HTTP_COMPLETE:
    if(TCP_CONN_IS_CLOSED(http.tcp_session)) http.state = HTTP_IDLE;
    break;
  }
  // sse keep alive
  if(sys_clock_100ms > sse_ping_timer && http_can_send_sse())
  {
    unsigned pkt = tcp_create_packet_sized(sse_sock, 256);
    if(pkt != 0xff)
    {
      unsigned len = sprintf( (char*)tcp_ref(pkt, 0), "event: sse_ping\n""data: -\n\n" );
      tcp_send_packet(sse_sock, pkt, len);
      sse_ping_timer = sys_clock_100ms + 90; // 9s
    }
  }
}

void http_init(void)
{
  http.tcp_session = tcp_open(sys_setup.http_port);
  http.state = HTTP_IDLE;
  //http.timeout = 0;
  sse_sock = tcp_open(sys_setup.http_port);
  if(sse_sock != 0xff)
    tcp_socket[sse_sock].tcp_state = TCP_RESERVED;
}

unsigned char hex_to_byte(char *s)
{
  unsigned char b,c;
  c=s[0];
  if(c>='0' && c<='9') b = c - '0';
  else if(c>='a' && c<='f') b = c - 'a' + 10;
  else if(c>='A' && c<='F') b = c - 'A' + 10;
  else b = 0;
  b<<=4; c=s[1];
  if(c>='0' && c<='9') b |= c - '0';
  else if(c>='a' && c<='f') b |= c - 'a' + 10;
  else if(c>='A' && c<='F') b |= c - 'A' + 10;
  else ;
  return b;
}

unsigned http_post_data_part(char *src_text, void *data, unsigned len) // LBS 24.02.2011
{
  char *d = data;
  for(int n=0; n<len; ++n)
  {
    d[n] = hex_to_byte(&src_text[n<<1]);
  }
  return len<<1;
}

void http_post_data(unsigned char *data, unsigned len)
{
  if(http.post_content_length - HTTP_POST_HDR_SIZE != (len<<1) ) return;
  http_post_data_part(req + HTTP_POST_HDR_SIZE, data, len);
}

int pdata_pstring(char *dest, char *name, unsigned char *pstr)
{
  char *d = dest;
  unsigned char c;
  d += sprintf(d, "%s:", name);
  *d++='"';
  // escape quotes
  int len = *pstr;
#ifdef DNS_MODULE
  if(len > 62) len = 62; // LBS 3.06.2013, pstring up to 62 chars (dns hostnames)
#else
  if(len > 30) len = 30; // max labels are 30 chars
#endif
  for(int n=1;n<=len;++n)
  {
    c = pstr[n];
    if(c == '"') *d++ = '\\'; // escape quote char
    *d++ = c;
  }
  *d++ = '"';
  *d++ = ',';
  *d = 0;
  return d-dest;
}

int pdata_cstring(char *dest, char *name, char *cstr)
{
  char *d = dest;
  unsigned char c;
  d += sprintf(d, "%s:", name);
  *d++='"';
  // escape quotes
  int len = strlen(cstr);
  for(int n=0;n<len;++n)
  {
    c = cstr[n];
    if(c == '"') *d++ = '\\'; // escape quote char
    *d++ = c;
  }
  *d++ = '"';
  *d++ = ',';
  *d = 0;
  return d-dest;
}

int pdata_ip(char *dest, char *name, unsigned char *ip)
{
  return sprintf(dest, "%s:'%d.%d.%d.%d',", name, ip[0], ip[1], ip[2], ip[3] );
}

int http_can_send_sse(void)
{
  if(sse_sock == 0xff) return 0;
  struct tcp_socket_s *s = &tcp_socket[sse_sock];
  if( s->tcp_state == TCP_ESTABLISHED &&  s->out_in_flight < 2 )
    return 1;
  else
    return 0;
}

// dummy cgi for SSE channel
unsigned http_get_sse_cgi(unsigned pkt, unsigned more_data)
{
  return 0;
}

HOOK_CGI(sse,  (void*)http_get_sse_cgi, mime_sse,  HTML_FLG_GET | HTML_FLG_NOCACHE | HTML_FLG_SS_EVENT );


unsigned http_event(enum event_e event, unsigned evdata_tcp_soc_n)
{
  switch(event)
  {
  case E_EXEC:
    http_exec();
    break;
  case E_INIT:
    http_init();
    break;
  case E_PARSE_TCP:
    http_parsing(evdata_tcp_soc_n);
    break;
  }
  return 0;
}




#warning TODO tcp_close_session() may send 3rd in flight. Correct pumping, add HTTP_WAIT_CONFIRM state and wait (in flight <2) before closing.
