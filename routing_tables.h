//
// routing tables
//
//
// principle :
//
// it relies on virtual addresses.
// hidden behide a virtual address, there can be :
//  - a physical address
//  - a virtual address
//  - a list of addresses (physical and virtual can be mixed)
//
// a physical address is the real address of a component on the bus.
// a virtual address is just an address to be used for translation.
//
//
// implementation :
//
// the routing table is a very simple list with
// an virtual address and a routing address.
//
// the whole table is scanned,
//
// if no match is found between the given address and a table input,
// the given address is considered as a physical address and is left untranslated.
//
// each time a match occures,
// the given address is translated to the routing address.
//
//

#ifndef __ROUT_H__
# define __ROUT_H__

# include "type_def.h"


//------------------------------------------
// configuration
//

// maximum number of routes
#define MAX_ROUTES	10


//------------------------------------------
// pthread interface
//

extern void ROUT_init(void);
extern void ROUT_run(void);


//------------------------------------------
// public functions
//

// retrieve the routed addresses from the specified address
extern void ROUT_route(const u8 addr, u8 list[MAX_ROUTES], u8* list_len);

#endif	// __ROUT_H__
