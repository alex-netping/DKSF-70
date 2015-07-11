

#define BOOTLDR_SIGNATURE_START_UPDATE 955211253

struct bootldr_data_s {
  unsigned signature;
  unsigned length;
  unsigned short crc16;
  unsigned short reserved1;
  unsigned reserved2;
};

extern __no_init struct bootldr_data_s bootldr_data @ 0x10007F40; // in RAM 64kb - 192, for LPC1778
