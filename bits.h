enum {
  A  = 0,
  B  = 1,
  C  = 2,
  Ci = 3,
  D  = 4
};

#define _5BIT  0x000001F
#define _10BIT 0x00003FF
#define _16BIT 0x000FFFF
#define _26BIT 0x3FFFFFF
#define _28BIT 0xFFFFFFF

unsigned char  expofs = 0, erbloks=0;
unsigned int   BlkPointer = 0;
char          *blockname[5] = {"A","B","C","C'","D"};
bool           rcvd[5] = {false}, insync = false, errblock[50] = {false};
unsigned short grp_data[4] = {0};

/* Function declarations */

bool           get_bit();
unsigned short syndrome(unsigned int);
void           blockerror();
