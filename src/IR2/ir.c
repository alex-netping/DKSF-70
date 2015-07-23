/*
v2.0-52
 initial release
v2.4-52
4.04.2011
  saved record offset bugfix is backported from v2.3-50
  v2.3-50 lcp233x support and bus sharing **was not** backported
  Unused slot playback protection (IR_RECORD_VER)
v2.6-52
14.04.2011
  IO1, IO2 as TRC-IR bus
v3.1-52
21.03.2012
  moved to DKST35 hardware
v3.2-50
4.05.2012
  cancel Rel.Humidity sensor activity on shared SDA line before operation
  generalised i2c access via macrosses
v3.3-50
12.05.2012
  i2c ack as macros, bus ack bug fixed
  checking for all-zero on bus (shorted or pulled down bus) to save polling time
  labels reading is skipped if no device on bus
v3.4-52
31.05.2012
  ir.cgi?play=13
v3.5-201
29.08.2012
  bugfix in ir.cgi, wrong use of http API
v3.6-70
13.08.2014
  json-p CGI API (since 70.3.4 release!)
*/

#include "platform_setup.h"

#ifdef IR_MODULE

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "plink.h"
#include "eeprom_map.h"


/*
const unsigned ir_signature = 510720894;
*/
struct ir_setup_s ir_setup; // legacy, for compatibility with shared IO lines


unsigned char magic_n = 78;
unsigned short ir_proto_timeout;


/*
enum old_ir_status_e {
  IR_OK              = 0x00,
  IR_BUSY_CAPTURE_T1 = 0x01,
  IR_BUSY_CAPTURE_T2 = 0x02,
  IR_BUSY_PLAY       = 0x03,
  IR_ERROR           = 0xff
};
*/
enum ir_status_e ir_status = STATUS_DONE;


void ir_receive_record(void)
{
}

void ir_place_crc(unsigned char *buf, unsigned len) // len is full, incl. crc
{
  unsigned short crc;
  crc16_reset(&crc);
  crc16_incremental_calc(&crc, buf, len - 2);
  buf[len - 2] = crc & 0xff;
  buf[len - 1] = crc >> 8;
}

enum ir_status_e ir_execute_cmd(unsigned char *cmd, unsigned cmd_len, unsigned char *answ, unsigned answ_len)
{
  enum ir_status_e status;
  unsigned short crc;
  int i, t, tr;

  /*
  log_printf("debug: ir cmd %u n=%u", cmd[1], cmd[3]);
  */
#if PROJECT_MODEL == 50
  relhum_cancel(); // force shared SDA line to be free, IR has priority as interactive application
#endif
  if(i2c_accquire('R') == 0) return STATUS_ERR_EXT_I2C_BUS_BUSY;
  for(t=0; t<4; ++t)
  {
    cmd[0] = IR_IIC_ADDR | 0; // addr + write
    // cmd[1] - opcode filled by caller
    cmd[2] = ++magic_n;
    // cmd[3..cmd_len-2] - data filled by caller
    ir_place_crc(cmd, cmd_len);
    delay_us(100);
    IR_IIC_WRITE(IR_IIC_ADDR, (void*)&cmd[1], cmd_len - 1);
    if(IR_IIC_ACK == 0) continue; // no ack, repeat sending
    for(tr=0; tr<32; ++tr)
    {
      delay(10);
      util_fill(answ, answ_len, 0xbb);
      IR_IIC_READ(IR_IIC_ADDR, (void*)&answ[1], answ_len - 1);
      if(IR_IIC_ACK == 0) break; // repeat command
      // check bus for all zeros
      for(i=1; i < answ_len; ++i)
      {
        if(answ[i] != 0) break;
      }
      if(i == answ_len)
      {
        break; // all zeros on bus, don't wait answer
      }
      answ[0] = IR_IIC_ADDR | 0x01;
      crc16_reset(&crc);
      crc16_incremental_calc(&crc, (void*)answ, 6);
      if(crc != 0) continue; // wrong 1st crc, repeat read
      if(answ[1] != cmd[1] // opcode mismatch, repeat command
      || answ[2] != cmd[2]) // magic number mismatch, repeat command
      {
        /*
         #warning "debuggg"
        log_printf("debug: ir op/magic mismatch, %u->%u, %02xh->%02xh",
                   cmd[1], answ[1], cmd[2], answ[2] );
        */
        break;
      }
      status = (enum ir_status_e)(answ[3]);
      if(status == STATUS_DONE
      || status == STATUS_IN_PROGRESS
      || (unsigned)status >= 16 )
      {
        if(answ_len > 6)
        { // data expected in answer
          crc16_reset(&crc);
          crc16_incremental_calc(&crc, (void*)&answ[6], answ_len - 6);
          if(crc != 0) continue; // bad crc2 (data crc), repeat read
        }
        /*
        #warning "debuggg"
        if(t>0) log_printf("debug: ir cmd repeated %u", t+1);
        */
        i2c_release('R');
        ir_status = status;
        return status;
      } // if cmd completed
      /* repeat answer read */
    } // for ( answer )
  } // for( command )
  /*
  #warning "debuggg"
  log_printf("debug: ir cmd repeated %u, timeout", t+1);
  */
  i2c_release('R');
  ir_status = STATUS_ERR_TIMEOUT;
  return STATUS_ERR_TIMEOUT;
}

/*
#include "IRControl_hex.c" // hexdata[] is here
void debug_test_update(void)
{
  unsigned a;
  unsigned short crc;
  unsigned char cmd[5 + 4 + 256]; // +addr+fwdata
  unsigned char ans[6];
  crc16_reset(&crc);
  crc16_incremental_calc(&crc, (void*)(hexdata + 4096), 32768 - 4096);
  for(a=4096; a<32768; a+=256)
  {
    cmd[1] = CMD_WRITE_FW_UPDATE;
    util_cpy((void*)&a, &cmd[3], 4);
    util_cpy((void*)(hexdata + a), &cmd[3+4], 256);
    ir_execute_cmd(cmd, sizeof cmd, ans, sizeof ans);
  }
  cmd[1] = CMD_APPLY_FW_UPDATE;
  cmd[3] = crc >> 0;
  cmd[4] = crc >> 8;
  ir_execute_cmd(cmd, 7, ans, sizeof ans);
}
*/

void ir_play_record(unsigned record_num)
{
  unsigned char cmd[5+1]; // rec number
  unsigned char ans[6]; // status
  cmd[1] = CMD_PLAY; // i.e. play
  cmd[3] = record_num & 0xff;
  ir_execute_cmd(cmd, sizeof cmd, ans, sizeof ans);
}


void ir_start_capture(void)
{
  unsigned char cmd[5]; // no data
  unsigned char ans[6 + 8 + 2]; // status, reserved, crc2
  enum ir_status_e status;
  cmd[1] = CMD_CAPTURE;
  status = ir_execute_cmd(cmd, sizeof cmd, ans, sizeof ans);
  status = STATUS_DONE; // dummy, for debug
}

/*
#warning "fw update/boot debugggg"
void ir_debug(void)
{
  unsigned char cmd[5 + 2]; // no data
  unsigned char ans[6]; // status, versions, crc2
  unsigned status;
  cmd[1] = CMD_APPLY_FW_UPDATE;
  cmd[3] = 0x3c; // copy exact calculated value!!!
  cmd[4] = 0xe7;
  status = ir_execute_cmd(cmd, sizeof cmd, ans, sizeof ans);
  ans[0] = cmd[0]; // dummy, for debug
  status = 0; // dummy, for debug
}
*/

void ir_save_record(unsigned num, char *label)
{
  unsigned char cmd[5 + 1 + 32]; // rec number, label
  unsigned char ans[6]; // status
  cmd[1] = CMD_SAVE;
  cmd[3] = num & 0xff;
  util_cpy((void*)label, &cmd[4], 32);
  cmd[4 + 32 - 1] = 0; // z-term protection of label field
  ir_execute_cmd(cmd, sizeof cmd, ans, sizeof ans);
}

void ir_get_label(unsigned num, char *buf) // buf must be 32 byte
{
  unsigned char cmd[5 + 1]; // rec number
  unsigned char ans[6 + 32 + 8 + 2]; // status, label, reserved, crc2
  enum ir_status_e status;

  cmd[1] = CMD_GET_LBL;
  cmd[3] = num & 0xff;
  status = ir_execute_cmd(cmd, sizeof cmd, ans, sizeof ans);
  if(status == STATUS_DONE)
  {
    ans[6 + 32 - 1] = 0; // zterm protection
    strcpy(buf, (char const *)ans + 6);
  }
  else if(status == STATUS_ERR_NO_DATA)
  {
    buf[0] = '-'; buf[1] = 0;
  }
  else
  {
    sprintf(buf, "error %u", (unsigned) status);
  }
}

int ir_get_irc_tr_version(struct irc_tr_version_s *ver)
{
  unsigned char cmd[5]; // no data
  unsigned char ans[6 + 6 + 2]; // status, version, crc2
  enum ir_status_e status;

  cmd[1] = CMD_GET_VERSION;
  status = ir_execute_cmd(cmd, sizeof cmd, ans, sizeof ans);
  if(status != STATUS_DONE)
  {
    util_fill((void*)ver, sizeof *ver, 0);
    return 0;
  }
  else
  {
    unsigned char *p = &ans[6];
    ver->hw_ver    = *p++;
    ver->hw_subver = *p++;
    ver->bldr_ver  = *p++;
    ver->bldr_subver = *p++;
    ver->fw_ver    = *p++;
    ver->fw_subver = *p++;
    return 1;
  }
}

void ir_exec(void)
{
  /*
  /////// DEBUGGGGGGGGGG
  static int done = 0;
  if((!done) && sys_clock() > 3000)
  {
    done = 1;
    if(!i2c_accquire('R')) return;

    unsigned char cmd[64];
    cmd[0] = IR_IIC_ADDR | 0; // addr + write
    cmd[1] = CMD_PLAY;
    cmd[2] = ++magic_n;
    cmd[3] = 2; // N
    util_fill(cmd + 4, 32, 0);
    util_cpy("1234567890", cmd + 4, 10);
    ir_place_crc(cmd, 6);
    hw_i2c_write(IR_IIC_CH, IR_IIC_ADDR, cmd + 1, 6);
    debug_ans[126]=i2c_ack;
    delay(2000);

    hw_i2c_read(IR_IIC_CH, IR_IIC_ADDR, debug_ans, 16);
    debug_ans[127] = i2c_ack;
    delay(2000);
    hw_i2c_read(IR_IIC_CH, IR_IIC_ADDR, debug_ans, 16);
    debug_ans[127] = i2c_ack;
    i2c_release('R');
  }
  */
} // ir_exec()


unsigned ir_http_get_data(unsigned pkt, unsigned more_data)
{
  unsigned n = more_data;
  char data[1024];
  char *dest = data;
  char buf[32];
  static int irc_found; // keep value between reading cycles!
  struct irc_tr_version_s ver;
  if(n==0)
  {
    irc_found = ir_get_irc_tr_version(&ver);
    dest += sprintf(dest,
      "data={ir_status:%u,"
      "ir_timeout:%u,"
      "ir_proto_state:%u,"
      //"useIO:%u,"
      "irc_tr_ver:[%u,%u,%u,%u,%u,%u],"
      "labels:[",
      ir_status, ir_proto_timeout, 0,
      //ir_setup.useIO=='Y' ? 1 : 0,
      ver.hw_ver, ver.hw_subver, ver.bldr_ver, ver.bldr_subver, ver.fw_ver, ver.fw_subver
      );
  }
  for(;n<IR_COMMANDS_N;)
  {
    if(irc_found || n == 0) // if no irc on bus, skip reading of labels except cmd 0
    {
      ir_get_label(n, buf);
      buf[sizeof buf - 1] = 0;
    }
    else
    {
      buf[0] = '-';
      buf[1] = 0;
    }
    dest += sprintf(dest, "\n\"%s\",", buf);
    ++n;
    if(dest-data > sizeof data-128) break; // data buffer capacity used up to 80%
  }
  if(n == IR_COMMANDS_N)
  {
    n = 0;
    --dest; // remove last comma
    dest += sprintf(dest, "]};");
  }
  tcp_put_tx_body(pkt, (unsigned char*)data, dest - data);
  return n; // more data info
}

int ir_http_set_data(void)
{
  // format: data=CCNNPPLLLLLL..LL
  //   CC - 1 byte command, 0x01 = start rec, 0x02 = play rec, 0x03 - save record
  //   NN - 1 byte ir command (record) number, 0xff = play captured from buffer (for Save and Play commands)
  //   PP - 1 byte lablel length in bytes (for Save command)
  //   LL - win1251 chars of command label (for Save command)
  struct {
    unsigned char opcode;
    unsigned char rec_n;
    unsigned char label_len;
  } cmd;
  char label[32];
  unsigned postlen;
  if(http.post_content_length - HTTP_POST_HDR_SIZE < (3<<1) ) return 0; // at least 3 hex byte must be posted
  http_post_data_part(req + HTTP_POST_HDR_SIZE, (void*)&cmd, sizeof cmd);
  switch(cmd.opcode)
  {
  case 0x01:
    ir_start_capture();
    http_reply(200,"ok");
    break;
  case 0x02:
    if(cmd.rec_n != 0xff && cmd.rec_n >= IR_COMMANDS_N) break; // validate cmd number
    ir_play_record(cmd.rec_n);
    http_reply(200,"ok");
    break;
  case 0x03:
    if(cmd.rec_n >= IR_COMMANDS_N) break; // validate cmd number
    postlen = (sizeof cmd + cmd.label_len)<<1;
    if(http.post_content_length != HTTP_POST_HDR_SIZE + postlen) break; // check POST data length
    util_fill((void*)label, sizeof label, 0); // LBS 27.02.2012 - it was wrong function args sequence !!!
    if(cmd.label_len > 31) cmd.label_len = 31; // limit label length
    http_post_data_part(req + HTTP_POST_HDR_SIZE + sizeof cmd * 2, label, cmd.label_len); // read label
    ir_save_record(cmd.rec_n, label);
    http_redirect("/ir.html");
    break;
  default:
    http_redirect("/ir.html");
    break;
  }
  return 0;
}

unsigned ir_http_get_cgi(unsigned pkt, unsigned more_data) // url-encoded iface
{
  int cmd;
  char *p = req_args;
  char *result = "ir_result('error');";
  if(strncmp(p, "play=", 5) != 0) goto end;
  p += 5;
  cmd = atoi(p);
  if(cmd < 1 || cmd > IR_COMMANDS_N) goto end;
  ir_play_record(cmd - 1);
  if(ir_status >= STATUS_ERR_UNKNOWN) goto end;
  result = "ir_result('ok');";
end:
  tcp_put_tx_body(pkt, (void*)result, strlen(result));
  return 0;
}

HOOK_CGI(ir_get,    (void*)ir_http_get_data,  mime_js,  HTML_FLG_GET | HTML_FLG_NOCACHE );
HOOK_CGI(ir_set,    (void*)ir_http_set_data,  mime_js,  HTML_FLG_POST );
HOOK_CGI(ir,        (void*)ir_http_get_cgi,   mime_js,  HTML_FLG_GET | HTML_FLG_NOCACHE );

int ir_snmp_set(unsigned id, unsigned char *data)
{
  int val = 0;
  if(*data != 0x02) return SNMP_ERR_BAD_VALUE; // not INTEGER
  data += asn_get_integer(data, &val);
  snmp_add_asn_integer(val);
  switch(id)
  {
  case 0x7901:
    if(val < 1 || val > IR_COMMANDS_N) return SNMP_ERR_BAD_VALUE;
    ir_play_record(val-1);
    break;
  case 0x7902:
    if(val==1) { /*ir_start_flush();*/ }
    else return SNMP_ERR_BAD_VALUE;
    break;
  default:
    return SNMP_ERR_READ_ONLY;
  }
  return 0;
}


int ir_snmp_get(unsigned id, unsigned char *data)
{
  int val = 0;
  switch(id)
  {
  case 0x7901:
  case 0x7902:
    val = 0;
    break;
  case 0x7903:
    val = ir_status;
    break;
  }
  snmp_add_asn_integer(val);
  return 0;
}

void ir_reset_params(void)
{
 // no params for IRC-TR v2
}

void ir_init(void)
{
  ir_status = STATUS_DONE;
}

/*
void ir_timer_10ms(void)
{
  if(ir_proto_timeout) --ir_proto_timeout;
}
*/

unsigned ir_event(enum event_e event)
{
  switch(event)
  {
  case E_EXEC:
    ir_exec();
    break;
    /*
  case E_TIMER_10ms:
    ir_timer_10ms();
    break;
    */
  case E_RESET_PARAMS:
    ir_reset_params();
    break;
  }
  return 0;
}

#endif // IR_MODULE
