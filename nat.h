// GPL v3 : copyright Yann GOUY
//
//
// NAT : goal and description
//
// this package is a Node Address Translation.
//
// it is to be used as a serial link gateway
// to access every node from a single one
// connected throught a serial link.
//
//                                    I2C
//                                    bus
//                                     | <--> twi [Node W]
// [PC] tty <--> tty [Node X] twi <--> | 
//                                     | <--> twi [Node Y]
//                                     | <--> twi [Node Z]


#ifndef __NAT_H__
# define __NAT_H__

# include "type_def.h"


//----------------------------------------
// public defines
//


//----------------------------------------
// public types
//

//----------------------------------------
// public functions
//

// NAT module initialization
extern void NAT_init(void);


// NAT run method
extern u8 NAT_run(void);

#endif	// __NAT_H__
