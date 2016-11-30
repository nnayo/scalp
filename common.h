// GPL v3 : copyright Yann GOUY
//
//
// COMMON : goal and description
//
// this package provides common services :
//
//	- node status : status values depend on the node
//	- time : retrieve node local time in 10 us
//	- mux power : drive the bus multiplexer power line
//


#ifndef __COMMON_H__
# define __COMMON_H__

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

// common module initialization
extern void scalp_cmn_init(void);

// common module run method
extern void scalp_cmn_run(void);


#endif	// __COMMON_H__
