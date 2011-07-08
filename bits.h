enum {
  A  = 0,
  B  = 1,
  C  = 2,
  Ci = 3,
  D  = 4
};

enum BOOLEAN {
  FALSE = 0,
  TRUE  = 1
};

#define _5BIT  0x000001F
#define _10BIT 0x00003FF
#define _16BIT 0x000FFFF
#define _26BIT 0x3FFFFFF
#define _28BIT 0xFFFFFFF

int          rcvd[5] = {FALSE}, erbloks=0;
unsigned int grp_data[4] = {0}, insync = FALSE, expofs = 0;
unsigned int BlkPointer = 0, errblock[50] = {0};
char         *blockname[5] = {"A","B","C","C'","D"};

/* Function declarations */

short int    get_bit();
unsigned int syndrome(unsigned int);
void         blockerror();
