// GPL v3 : copyright Yann GOUY
//
//
// ALV (Alive) : goal and description
//
// this package checks whether the current bus is functionnal (alive)
//
// 5 times per second, the status of a node time is retrieved
// if it is not possible, an anti-bounce counter is incremented
// else it is reset.
//
// if the counter reaches the value of 5, the node is set as
// autonomous.
//
//


#ifndef __ALIVE_H__
# define __ALIVE_H__

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

// ALIVE module initialization
extern void scalp_alive_init(void);

// ALIVE module run method
extern void scalp_alive_run(void);


#endif	// __ALIVE_H__
