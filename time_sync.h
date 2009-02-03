// GPL v3 : copyright Yann GOUY
//
//
// TSN (Time SyNchronisation) : goal and description
//
// this package provides time synchronisation from the BC node :
//
// every second, the BC node time is retrieved
// and compare to the local time.
//
// if the local time is higher (lower) than the remote one,
// the time increment is decreased (increased) to ensure
// a smouth correction.
//


#ifndef __TIME_SYNC_H__
# define __TIME_SYNC_H__

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

// Time Synchro module initialization
extern void TSN_init(void);

// Time Synchro module run method
extern void TSN_run(void);


#endif	// __TIME_SYNC_H__
