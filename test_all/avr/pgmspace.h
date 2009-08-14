#include <string.h>


#define 	PROGMEM
#define 	PGM_VOID_P   const prog_void *


typedef void PROGMEM 	prog_void;


extern u16 pgm_read_word(u16 addr);
extern void* memcpy_P(void *, PGM_VOID_P, size_t);
