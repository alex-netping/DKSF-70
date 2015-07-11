/*
*rewrite by LBS
*v1.5
*17.02.2010
*v1.9-52
*24.03.2011
* cosmetic rewrite, some bugfix
*v2.0-52
*25.05.2011
*  major rewrite, retransmission, send queue (max 2 in flight)
v2.1-52
5.03.2012
  ip_free_packet() is called instead of nic_free_packet()
v2.2-50
14.11.2012
  adapted for ip3.c
v2.2-213
5.9.2012
  dksf213 support (sans http)
v2.2-60
24.02.2013
  tcp window for tcpcom flow ctrl
  handling of rx data by upper netwk layer is modified (tcp_active_session argument)
v2.3-60
9.05.2013
  removed FIN_WAIT_2 state, for browser tcp hang mitigation
v2.4-60
28.05.2013
  removed PSH flag in SYN packet
v2.5-52
14.08.2013
  tcp_create_packet_sized(), SSEvents support (TCP_RESERVED state)
v2.6-60
7.11.2014
  bugfixed wrong segment processing on rx, just ACK last received peer seq number (it will be dup ack or fast rexmit for peer)
*/


#include "platform_setup.h"
#ifndef  TCP_H
#define  TCP_H
///Версия модуля
#define  TCP_VER	2
///Сборка модуля
#define  TCP_BUILD	6


//---------------- Раздел, где будут определяться константы модуля -------------------------

///Тип протокола для TCP в заголовке принятого IP пакета
#define TCP_PROT  0x06

///Флаги заголовка TCP
#define TCP_MSK_URG 0x20
#define TCP_MSK_ACK 0x10
#define TCP_MSK_PSH 0x08
#define TCP_MSK_RST 0x04
#define TCP_MSK_SYN 0x02
#define TCP_MSK_FIN 0x01

// состояния TCP сессии
enum tcp_state_e {
  TCP_LISTEN,
  TCP_SYN_RCVD,
  TCP_ESTABLISHED,
  TCP_FIN_WAIT_1,
  TCP_FIN_WAIT_2,
  TCP_TIME_WAIT,
  TCP_CLOSE_WAIT,
  TCP_LAST_ACK,
  TCP_WAIT_ACK,
  TCP_RESERVED
};


//---------------- Раздел, где будут определяться структуры модуля -------------------------
/*! Заголовок TCP пакета
* port_src - порт отправителя (старший байт первый)
* port_dest - порт получателя (аналогично port_src)
* sequence_num - номер последовательности (-\\-)
* ack_num -номер подтверждения (-\\-)
* data_offs- смещение данных
* flags- флаги
* window -размер окна (старший байт первый)
* CRC -контрольная сумма
* p_urg - дополнительные данные
*/
struct tcp_header{
  unsigned char port_src[2];
  unsigned char port_dest[2];
  unsigned char sequence_num[4];
  unsigned char ack_num[4];
  unsigned char data_offs;
  unsigned char flags;
  unsigned char window[2];
  unsigned char CRC[2];
  unsigned char p_urg[2];
  unsigned char option[4];
};

/*! Структура описания TCP socket
* used -флаг использования сокета, если сокет открыт то флаг имеет значение 1
* listen_port -номер порта который слушает данный сокет (старший байт первый)
* socket_port -порт с которым установлено соединение (старший байт первый)
* socket_ip - IP с которым установлено соединение (старший байт первый)
* ack -номер потверждения
* seq- номер последовательности
* tcp_state - состояние соединения
* last_ack - последний полученный номер подтверждения
* resend_packet- номер последнего посланного пакета
* resend_retry - кол-во перепосылок
* tcp_timeout- счетчик таймаута
* tcp_rcv_len -кол-во принятых байт
*/
struct tcp_socket_s {
  uword used;
  unsigned char listen_port[2];
  unsigned char socket_port[2];
  unsigned char socket_ip[4];
  unsigned long ack;
  unsigned long seq;
  enum tcp_state_e tcp_state;
  unsigned long last_ack;
  uword resend_packet;
  uword resend_retry;
  uword tcp_timeout; // unused now
  unsigned short tcp_rcv_len;
  unsigned short tcp_tx_len;
  systime_t closing_time; // 25.03.2011
  systime_t timeout;      // 26.04.2011
  unsigned  char out_in_flight;
  unsigned char  send_q[16];
  unsigned window; // 24.04.2013
};

//---------------- Раздел, где будут определяться глобальные переменные модуля -------------
///Глобальная переменная возвращающая номер активной tcp сесии для внешнего обработчика
extern uword tcp_active_session;
///Таблица сокетов
extern struct tcp_socket_s tcp_socket[TCP_MAX_SOCKETS];
///Структуры для хранения заголовков TCP
extern struct tcp_header tcp_head_rx,tcp_head_tx;
extern upointer tcp_tx_body_pointer,tcp_rx_body_pointer;
//---------------- Раздел, где будут определяться функции модуля ---------------------------
/*!Процедура начальной инициализации модуля TCP
* Прим: Процедуру добавить структуру описания модуля.
* Процедура инициализирует таблицу сокетов след. образом:
* для каждого сокета поле used=0
*/
extern void tcp_init(void);

/*! Процедура создает TCP пакет
* Процедура выполняет следующие действия
 - создает IP пакет
 - Заполняет заголовок IP уровня
     - protocol=TCP_PROT
     - dest_ip= tcp_socket[tcp_socket_num].socket_ip
     - src_ip = TCP_MY_IP
 - Заполняет заголовок TCP уровня из полей сокета
      port_dest=tcp_socket[tcp_socket_num].socket_port
      port_src=tcp_socket[tcp_socket_num].listen_port
      sequence_num =tcp_socket[tcp_socket_num].seq
      ack_num= tcp_socket[tcp_socket_num].ack
      data_offs=0x50;
      window=TCP_WINDOW
      flags=TCP_ACK|TCP_PUSH
\param tcp_socket_num -номер tcp сокета
\return -идентификатор созданного пакета
*/
unsigned tcp_create_packet(unsigned tcp_socket_num);
unsigned tcp_create_packet_sized(unsigned tcp_socket_num, unsigned size);


void tcp_send_packet(unsigned sock_n, unsigned pkt, unsigned len); // 25.03.2011


/*! Процедура повторно отправляет пакет с дескриптором packet_id
\param packet_id - дескриптор пакета
*/
extern void tcp_resend_packet(uword packet_id);

/*! Процедура освобождает занятый пакет с дескриптором packet_id
\param packet_id - дескриптор пакета
*/
// DEBUGGG LBS rexmit
/* extern void tcp_remove_packet(uword packet_id); */
extern void tcp_remove_packet(unsigned socket, uword packet_id); // added socket arg
//

/*! Процедура загружает заголовок принятого TCP пакета в структуру tcp_head_rx
*/
extern void tcp_get_rx_head(void);
/*! Процедура загружает заголовок TCP пакета с дескриптором packet_id в структуру tcp_head_tx
\param packet_id - дескриптор пакета
*/
extern void tcp_get_tx_head(uword packet_id);

/*! Процедура загружает заголовок TCP пакета с дескриптором packet_id из структуры tcp_head_tx
\param packet_id - дескриптор пакета
*/
extern void tcp_put_tx_head(uword packet_id);

/*! Процедура загружает данные из тела TCP пакета с дескриптором packet_id в буфер buf, длинной len
\param packet_id - дескриптор пакета
\param buf- указатель на буфер
\param len- длинна данных
*/
extern void tcp_get_tx_body(uword packet_id,unsigned char *buf,uword len);

/*! Процедура загружает данные из тела принятого TCP пакета в буфер buf, длинной len
\param buf- указатель на буфер
\param len- длинна данных
*/
extern void tcp_get_rx_body(unsigned char *buf,uword len);

/*! Процедура загружает данные в тело TCP пакета с дескриптором packet_id из буфера buf, длинной len
\param packet_id - дескриптор пакета
\param buf- указатель на буфер
\param len- длинна данных
*/
extern void tcp_put_tx_body(uword packet_id,unsigned char *buf,uword len);


/*! Макрос для задания позиции в теле принятого пакета
\pointer- новая позиция
*/
#define tcp_set_rx_body_pos(pointer) tcp_rx_body_pointer=pointer


/*! Макрос для задания позиции в теле отправляемого пакета
\pointer- новая позиция
*/
#define tcp_set_tx_body_pos(pointer) tcp_tx_body_pointer=pointer

/*!Процедура открытия порта TCP
Процедура ищет в таблице сокетов свободный, и резервирует его для заданного TCP порта
при этом состояние TCP сессии для этого сокета устанавливается в TCP_LISTEN. Процедура возвращает номер
занятого сокета,если свободный сокет найти не удалось то возращается значение 0xFF
\param listen_port - номер TCP порта который будет слушать сокет
*/
extern uword tcp_open(unsigned short listen_port);

/*!Процедура обработки принятого TCP пакет
* Процедура разбирает принятый TCP пакет след. образом:
*  1. Загружаем заголовок TCP в структуру tcp_head_rx
*  2. Ищем в таблице сокетов, сокет с полем used==1 и полем listen_port==tcp_head_rx.port_dest
*   2.1. Если сокет имеет в поле tcp_state значение отличное от  TCP_LISTEN,
*        проверим сооствествие полей сокета socket_port и socket_ip, полям tcp_head_rx.port_src и _IP_HEAD_RX.src_ip.
*        Если поля соотвественно равны запомним номер сокета в таблице в переменную tcp_active_session, переходим к п5.
*   2.2. Если сокет последний в таблице, переходим к п3., иначе переходим к п.2
*  3. Ищем в таблице сокетов, сокет с полем used==1,
*     полем listen_port==tcp_head_rx.port_dest, и полем tcp_state==TCP_LISTEN,
*     если сокет не найден выходим из процедуры.
*  4. Производим действия:
*      - tcp_active_session = номер найденного сокета в состоянии tcp_state==TCP_LISTEN.
*      - tcp_socket[tcp_active_session].socket_port = tcp_head_rx.port_src.
*      - tcp_socket[tcp_active_session].socket_ip = _IP_HEAD_RX.src_ip.
*      - tcp_socket[tcp_active_session].ack =  tcp_head_rx.ack ( необходимо преобразовать tcp_head_rx.ack )
*
*  5. Анализируем состояние tcp сессии (поле tcp_state для активного сокета).
*     5.1. tcp_state== TCP_LISTEN
*       5.1.1. Анализируем поле заголовка tcp_head_rx.flags. Проверяем установлен ли бит  TCP_MSK_SYN,
*              если бит не установлен выходим из процедуры
*       5.1.2. Производим действия:
*               - tcp_socket[tcp_active_session].seq = 0
*               - tcp_socket[tcp_active_session].ack ++
*               - tcp_socket[tcp_active_session].tcp_state =TCP_SYN_RCVD;
*               - tcp_socket[tcp_active_session].tcp_timeout =TCP_TIMEOUT;
*       5.1.3. Посылаем tcp пакет с установленными флагами TCP_MSK_SYN и TCP_MSK_ACK (поля sequence_num, ack_num заполняются
*               из полей сокета seq и ack соответсвенно)
*       5.1.4. Завершаем работу.
*     5.2. tcp_state== TCP_SYN_RCVD
*       5.2.1. Анализируем поле заголовка tcp_head_rx.flags. Проверяем установлен ли бит  TCP_MSK_ACK,
*              если бит не установлен переходим к 5.2.5.
*       5.2.2. Производим действия:
*                - tcp_socket[tcp_active_session].tcp_state=TCP_ESTABLISHED
*                - tcp_socket[tcp_active_session].seq++
*       5.2.3. Вызываем внешний обработчик TCP_OPEN_HANDLER
*       5.2.4. Завершаем работу
*       5.2.5. Анализируем поле заголовка tcp_head_rx.flags. Проверяем установлен ли бит  TCP_MSK_RST.
*       5.2.6. Производим действия:
*                - tcp_socket[tcp_active_session].tcp_state=TCP_LISTEN
*       5.2.7. Завершаем работу.
*     5.3. tcp_state== TCP_ESTABLISHED или   tcp_state==TCP_WAIT_ACK
*       5.3.1. Если стоит флаг TCP_MSK_ACK в tcp_head_rx.flags
*         5.3.1.1. Запоминаем tcp_head_rx.ack_num в поле сокета tcp_socket[tcp_active_session].last_ack
*         5.3.1.2. Проверяем если tcp_socket[tcp_active_session].resend_packet!=0xFF и
*                   tcp_socket[tcp_active_session].ack<=tcp_socket[tcp_active_session].last_ack, то:
*                   - удаляем посланный пакет с дескриптором tcp_socket[tcp_active_session].resend_packet
*                   - tcp_socket[tcp_active_session].resend_packet=0xFF;
*                   - tcp_socket[tcp_active_session].tcp_state=TCP_ESTABLISHED
*       5.3.2. Увеличиваем tcp_socket[tcp_active_session].ack на длинну данных TCP пришедших в пакете.
*       5.3.3. Если стоит флаг TCP_MSK_FIN в tcp_head_rx.flags
*         5.3.3.1. Производим след. действия:
*                  - tcp_socket[tcp_active_session].ack++;
*                  - tcp_socket[tcp_active_session].tcp_timeout=TCP_TIME_OUT
*                  - tcp_socket[tcp_active_session].tcp_state=TCP_LAST_ACK
*                  - Посылаем tcp пакет с установленными флагами TCP_MSK_FIN и TCP_MSK_ACK (поля sequence_num, ack_num заполняются
*                    из полей сокета seq и ack соответсвенно)
*                  - завершаем работу.
*       5.3.4. Если длинна данных в TCP пакете больше 0
*         5.3.4.1. Производим след.действия:
*                  - в tcp_socket[tcp_active_session].tcp_rcv_len помещаем длинну принятых данных.
*                  - Посылаем tcp пакет с установленным флагом TCP_MSK_ACK (поля sequence_num, ack_num заполняются
*                    из полей сокета seq и ack соответсвенно)
*                  - вызываем обработчик TCP_HANDLER_DATA
*                  - завершаем работу
*     5.4. tcp_state== TCP_FIN_WAIT_1
*         5.4.1. Проверяем tcp_head_rx.flags если одновременно установлены флаги TCP_MSK_FIN и TCP_MSK_ACK, то:
*                   - tcp_socket[tcp_active_session].tcp_state=TCP_FIN_WAIT_2
*                   - завершаем работу
*                 иначе если установлен флаг TCP_MSK_FIN:
*                   - Посылаем tcp пакет с установленным флагом TCP_MSK_ACK (поля sequence_num, ack_num заполняются
*                     из полей сокета seq и ack соответсвенно)
*                   - tcp_socket[tcp_active_session].tcp_state=TCP_LISTEN
*                   - вызываем обработчик TCP_HANDLER_CLOSE
*                   - завершаем работу.
*     5.5. tcp_state== TCP_FIN_WAIT_2
*        5.5.1. Проверяем tcp_head_rx.flags если установлен флаг TCP_MSK_FIN , то:
*                   - tcp_socket[tcp_active_session].ack++;
*                   - tcp_socket[tcp_active_session].seq++;
*                   - tcp_socket[tcp_active_session].tcp_state=TCP_LISTEN;
*                   - Посылаем tcp пакет с установленными флагами TCP_MSK_ACK и TCP_MSK_FIN
*                     (поля sequence_num, ack_num заполняются tcp_send_flags(tcp,FIN|ACK); //Отвечаем ACK
*     5.6. tcp_state== TCP_LAST_ACK
*       5.6.1. Проверяем tcp_head_rx.flags если установлен флаг TCP_MSK_ACK , то:
*                   - если  tcp_socket[tcp_active_session].resend_packet!=0xFF  удаляем не пакет из NIC
*                    - tcp_socket[tcp_active_session].tcp_state=TCP_LISTEN
*                    - вызываем обработчик TCP_HANDLER_CLOSE
*                    - завершаем работу
*/
extern void tcp_handler(void);
/*! Процедура закрывает TCP сессию socket_num
\param socket_num- номер сессии TCP
*/
extern void tcp_close_session(uword session_num);

/* Обработчик тайммаутов TCP сессий
* Прим: Процедуру добавить структуру описания модуля.
* Процедура производит следующие действия над каждым элементом таблицы TCP сокетов
* 1. Анализируем поле used, если оно равно 0 переходим к след. элементу, возвращаемся на п1.
* 2. Проверяем поле tcp_timeout, если оно не равно 0 переходим к след. элементу, возвращаемся на п1.
* 3. Анализируем состояние сессии (поле tcp_state)
*   3.1. Если поле равно TCP_WAIT_ACK, то:
*        3.1.1. Если поле resend_retry не равно нулю повторно посылаем пакет с дескриптором
*                resend_packet и декрементируем поле resend_retry, иначе освобождаем память NIC выделенную
*                под пакет и переводим сессию в TCP_LISTEN.
*   3.2. Если поле равно TCP_FIN_WAIT_1 или TCP_FIN_WAIT_2 или TCP_LAST_ACK или TCP_SYN_RCVD, то
*         если resend_packet!=0xff, освобождаем пакет и переводим сессию в TCP_LISTEN.
*  4. Переходим к след.элементу и возвращаемся на п1.
*
*/
extern void tcp_exec(void);

/*! Процедура декрементирует счетчики таймаута для TCP сокетов
*  Процедура делает проход по таблице сокетов и декрементирует поле tcp_timeout
*/
extern void tcp_timer(void);


#define _TCP_ACTIVE_SESSION tcp_active_session
#define _TCP_SOCKET tcp_socket
#define _TCP_CREATE_PACKET(tcp_socket_num) tcp_create_packet(tcp_socket_num)
#define _TCP_SEND_PACKET(tcp_socket_num,packet_id,len) tcp_send_packet(tcp_socket_num,packet_id,len)
#define _TCP_RESEND_PACKET(packet_id) tcp_resend_packet(packet_id)
#define _TCP_REVOME_PACKET(packet_id) tcp_remove_packet(packet_id)
#define _TCP_GET_RX_HEAD tcp_get_rx_head()
#define _TCP_PUT_RX_HEAD(packet_id) tcp_put_tx_head(packet_id)
#define _TCP_GET_TX_BODY(packet_id,buf,len) tcp_get_tx_body(packet_id,buf,len)
#define _TCP_GET_RX_BODY(buf,len) tcp_get_rx_body(buf,len)
#define _TCP_PUT_TX_BODY(packet_id,buf,len) tcp_put_tx_body(packet_id,buf,len)
#define _TCP_SET_RX_BODY_POS(pointer) tcp_rx_body_pointer=pointer
#define _TCP_SET_TX_BODY_POS(pointer) tcp_tx_body_pointer=pointer
#define _TCP_OPEN(listen_port) tcp_open(listen_port)
#define _TCP_CLOSE_SESSION(session_num) tcp_close_session(session_num)

extern unsigned tcp_rx_data_length;
void tcp_send_flags(uword flags, uword socket);
void tcp_clear_connection(int sock_n);

unsigned tcp_event(enum event_e event);

#endif
//}@
