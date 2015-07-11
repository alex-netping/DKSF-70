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

struct irc_tr_version_s {
  unsigned char hw_ver;
  unsigned char hw_subver;
  unsigned char bldr_ver;
  unsigned char bldr_subver;
  unsigned char fw_ver;
  unsigned char fw_subver;
};

enum ir_commands_e {
  CMD_RESET          =  0,  // command to reset device
  CMD_GET_VERSION    =  1,
  CMD_CAPTURE        =  2,  // command to begin capture IRcommand from IR remote control
  CMD_PLAY           =  3,  // command to send saved IRcommand number N to airconditioner
  CMD_SAVE           =  4,  // command to save captured IRcommand to ext.Flash with number N and label
  CMD_MOVE           =  5,  // command to move saved IR record in ext.Flash from number to number
  CMD_GET_LBL        =  6,  // command to send label of saved IR record to Master
  CMD_SAVE_LBL       =  7,  // command to write(update) label of saved IR record
  CMD_READ_REC       =  8,  // command to send saved IR record to Master
  CMD_WRITE_REC      =  9,  // command to save getted from Master IR record to ext.Flash with number N
  CMD_WRITE_FW_UPDATE = 10,  // command to save one block of finmware in ext.Flash at specified address
  CMD_APPLY_FW_UPDATE = 11,  // command to start firmware update from ext.flash to internal flash of mcu
  CMD_STOP           = 12, // command to stop(cancel) sending or capture IRcommand
  CMD_GET_ALL_LBLS   = 13,
  CMD_GET_ALL_LBLS_CONT = 14,

  /* Add new commands opcode here !!! */
  MAX_OPCODE            // number of operation codes (commands)
};

enum ir_status_e {
  STATUS_DONE        = 0,
  STATUS_UNINTERRUPT = 1,
  STATUS_IN_PROGRESS = 2,
  STATUS_ERR_UNKNOWN    = 16,
  STATUS_ERR_BAD_NUMBER = 17,
  STATUS_ERR_NO_DATA    = 18,
  STATUS_ERR_FLASH_CHIP = 19,
  STATUS_ERR_TIMEOUT    = 20,
  STATUS_ERR_EXT_I2C_BUS_BUSY = 21
};


struct ir_setup_s
{
  unsigned char useIO;
  unsigned char reserved[3];
};

extern struct ir_setup_s ir_setup;

/*
struct old_ir_record_s {
  unsigned char label[32];
  unsigned char record[448];
  unsigned char used_ver;
  unsigned char reserved;
  unsigned char reserved2[30];
};
*/

int ir_snmp_get(unsigned id, unsigned char *data);
int ir_snmp_set(unsigned id, unsigned char *data);

void ir_play_record(unsigned record_num);

void ir_init(void);
unsigned ir_event(enum event_e event);


