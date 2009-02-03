#ifndef __AVR_IO_H__
# define __AVR_IO_H__

unsigned char SREG; 

#define PD4	4
#define PD5	5

#define _BV(n)	(1 << n)

extern u8 PORTD;

#endif  // __AVR_IO_H__
