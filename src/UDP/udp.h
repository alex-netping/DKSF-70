
#ifndef UDP_H
#define UDP_H

#define UDP_VER     2
#define UDP_BUILD   0

//---------------- Раздел, где будут определяться константы модуля -------------------------
///Значение поля protocol, IP заголовка, для протокола UDP
#define UDP_PROTO  0x11
//---------------- Раздел, где будут определяться структуры модуля -------------------------
///Заголовок UDP
struct udp_header {
  unsigned char src_port[2];  //Порт источника
  unsigned char dest_port[2]; //Порт приемника
  unsigned char len[2];       //Общая длина пакета
  unsigned char CRC[2];       //Контрольная сумма
};

struct udp_hdr_s { // v2
  unsigned short src_port;
  unsigned short dst_port;
  unsigned short total_len;
  unsigned short checksum;
};

//---------------- Раздел, где будут определяться глобальные переменные модуля -------------

extern struct udp_header udp_rx_head;
extern struct udp_header udp_tx_head;
extern unsigned udp_rx_body_pointer;
extern unsigned udp_tx_body_pointer;

//---------------- Раздел, где будут определяться функции модуля ---------------------------

unsigned udp_create_packet(void);
unsigned udp_create_packet_sized(unsigned size);

void udp_send_packet(unsigned packet_id, unsigned payload_len);
void udp_send_packet_to(unsigned pkt, unsigned char *ip, unsigned dst_port, unsigned src_port, unsigned payload_len);

/*! Процедура копирует заголовок принятого UDP пакета в структуру udp_rx_head */
void udp_get_rx_header(void);

/*! Процедура копирует заголовок UDP пакета c дескриптором id в структуру udp_tx_head */
void udp_get_tx_header(unsigned packet_id);

/*! Процедура копирует структуру udp_tx_head в заголовок UDP пакета c дескриптором packet_id */
void udp_put_tx_header(unsigned packet_id);


/*! Процедура копирует данные из тела принятого UDP пакета в буфер buf длинной len
    Смещение данных в теле UDP пакета задаётся глобальной переменной udp_rx_body_pointer,
    которая инкрементируется на len */
void udp_get_rx_body(void *buf, unsigned len);


/*! Процедура копирует данные из тела UDP пакета c дескриптором packet_id в буфер buf длинной len
    Смещение данных в теле UDP пакета задаётся глобальной переменной udp_tx_body_pointer,
    которая инкрементируется на len  */
void udp_get_tx_body(unsigned packet_id, void *buf, unsigned len);

/*! Процедура копирует буфер buf длинной len в тело UDP пакета c дескриптором packet_id
    Смещение данных в теле UDP пакета задаётся глобальной переменной udp_tx_body_pointer,
    которая инкрементируется на len */
void udp_put_tx_body(unsigned packet_id, void *buf, unsigned len);

void udp_init(void);
void udp_parsing(void);
void udp_event(enum event_e);

#endif
//}@

