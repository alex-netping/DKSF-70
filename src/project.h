

//////////////////////// Cortex M3 bit-banding ///////////////////////////////

// 2m region is AHB (peripheral sram, gpio and some other)
// 4m region is APB devices

#define bit2m(addr, bit_n) ((unsigned *)(0x22000000U + ((addr-0x200000000U)<<5 | bit_n<<2)))
#define bit4m(addr, bit_n) ((unsigned *)(0x42000000U + ((addr-0x400000000U)<<5 | bit_n<<2)))

///////////////////////////////////////////////////////////////////////////////

extern const char device_name[];

////////////////////////////// serial number /////////////////////////////////

extern unsigned serial;

///////////////////////////// CPU NVIC //////////////////////////////////////

void nvic_set_pri(unsigned irq_n, unsigned pri);
void nvic_enable_irq(unsigned irq_n);
void nvic_disable_irq(unsigned irq_n);

///////////////////////////// CPU WDT ////////////////////////////////////////

void wdt_on(void);
void wdt_reset(void);

///////////////////////////// reboot ////////////////////////////////////////

void reboot_proc(void);

///////////////////////////// clocks /////////////////////////////////////////

void proj_init_clocks(void);

/////////////////////////// interrupts ///////////////////////////////////////

unsigned proj_disable_interrupt(void);
void proj_restore_interrupt(unsigned primask);

void nvic_int_enable(unsigned exception_number);
void nvic_int_disable(unsigned exception_number);
void nvic_clr_pend(unsigned exception_number);
void nvic_int_pri(unsigned exception_number, unsigned priority_level);

////////////////////// GPIO driver LPC17xx ///////////////////////////////////

void pindir(int port, int pin, int dir);
void pinset(int port, int pin);
void pinclr(int port, int pin);
unsigned pinread(unsigned port, unsigned bit);
void pinwrite(int port, int pin, unsigned value);

///////////////////////////// reset button //////////////////////////////////

int reset_button(void);

//////////////////////////////// LEDs ///////////////////////////////////////

void led_pin_init(void);
void led_pin(enum leds_e ledn, int state);

///////////////////////// Ethernet  ////////////////////////////////////////

void emac_init_rmii_pins(void);
void emac_init_smi_pins(void);

void phy_reset_line_init(void);
void phy_reset_line_clear(void);
void phy_reset_line_set(void);

////////////////////// HW detect /////////////////////////////////////////

extern unsigned char proj_hardware_model;
extern unsigned short gsm_model;
unsigned proj_hardware_detect(void);

///////////////////////// relay //////////////////////////////////////////

void relay_pin_init(void);
void relay_pin(unsigned relay_n, int state);

//////////////////////////// hw i2c /////////////////////////////////////

void hw_i2c_init_pins(void);

///////////////////////// sw i2c /////////////////////////////////////////

void swi2c_init_pin(void);
void swi2c_scl(int ifnum, int state);
void swi2c_sda(int ifnum, int state);
int  swi2c_sda_in(int ifnum);

//////////////////////////////// 1W /////////////////////////////////////

void ow_init_pin(void);
void ow_out(int state);
int  ow_in(void);

//////////////////////////////// IO LINES ////////////////////////////////

void io_hardware_init(void);
int  io_read_level(unsigned ch);
void io_set_level(unsigned ch, unsigned level);
void io_set_dir(unsigned ch, unsigned dir);
void io_pin_mark_saved(void);

////////////////////////// RELAY /////////////////////////////////////////

void pwr_relay(unsigned ch, unsigned state);

////////////////////////// UART //////////////////////////////////////////

void uart_pins_init(void);
void rs485_tx(unsigned flag);

//////////////////////////// SMS IGT/EMEROFF driver //////////////////////

void gsm_power(int onoff);
void gsm_pins_init(void);
int  gsm_read_cts(void);
void gsm_igt(int level);
void gsm_emeroff(int level);
void gsm_led(int state);

//////////////////////////////////////////////////////////////////////////