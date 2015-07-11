
#if PROJECT_MODEL == 51 || PROJECT_MODEL == 52
void io_hardware_init(void) // DKST51.1 only!!!!
{
  PINSEL3 &=~ (0xFF << 4); // select P1.18 - P1.21 pin finction as GPIO
  PINMODE3 |=  0xFF << 4;  // pulldown P1.18 - P1.21
  IO1DIR &=~  ( 3 << 18);  // P1.18,P1.19 are inputs
  IO1DIR |=     3 << 20;   // P1.20,P1.21 are outputs
}

void io_set_level(unsigned ch, unsigned level) // DKST51.1 only!!!!
{
  unsigned mask;
  level = !level; // output is inverse!
  if(ch > 1) return;
  if(ch==0) mask = 1<<21; // IO1
  if(ch==1) mask = 1<<20; // IO2
  // P1.20, P1.21 = ch 1,0 output
  IO1DIR |= mask;
  if (level) IO1SET = mask;
  else IO1CLR = mask;
}

int io_read_level(unsigned ch) // DKST51.1 only!!!!
{
  unsigned mask;
  if(ch > 1) return 0;
  if(ch==0) mask = 1<<19; // IO1
  if(ch==1) mask = 1<<18; // IO2
  // P1.18, P1.19 = ch 1,0 input
  return IO1PIN & mask ? 1 : 0; // NOT inverse input!
}

void io_set_dir(unsigned ch, unsigned dir)
{
  if(dir == 0) io_set_level(ch, 1); // на DKST51 лог.1 == Z (открытый коллектор)
}
#endif

#if PROJECT_MODEL == 53

// IOxDIR requires __monitor (disabled interrupt) for safe manipulation
// if IOxDIR changed in ISR, result is unpredictable
// because of |=, &=~ isn't atomic operations!

__monitor void  io_hardware_init(void) // DKST53 only!
{
  IO0DIR &=~ ((1<<4) | (1<<5) | (1<<6));
  IO0CLR  =  ((1<<4) | (1<<5) | (1<<6));
}

__monitor void io_set_level(unsigned ch, unsigned level) // DKST53 only!
{
  if(ch>2) return;
  unsigned mask = (1<<4) << ch;
  if(level) { IO0SET = mask; IO0DIR &=~ mask; }  // passive pull-up to 5V via 10k+1k
  else      { IO0CLR = mask; IO0DIR |= mask; }   // active 0
}

__monitor int io_read_level(unsigned ch) // DKST53 only!
{
  if(ch>2) return 0;
  unsigned mask = (1<<4) << ch;
  return IO0PIN & mask ? 1 : 0 ;
}

__monitor void io_set_dir(unsigned ch, unsigned dir) // DKST53 only!
{
  if(ch>2) return;
  unsigned mask = (1<<4) << ch;
  if(dir == 0) IO0DIR &=~ mask;
  else {} // DIR is controlled by io_set_level(), passive pull-up
}

#endif // io lines driver


