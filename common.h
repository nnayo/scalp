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

typedef enum {
	READY,
	WAIT_TAKE_OFF,
	WAIT_TAKE_OFF_CONF,	// minuterie only
	FLYING,
	WAIT_DOOR_OPEN,		// minuterie only
	RECOVERY,
	DOOR_OPENING,		// minuterie only
	DOOR_OPEN,			// minuterie only
	DOOR_CLOSING,		// minuterie only
} cmn_state_t;

typedef enum {
	NONE = 0x00,
	NOM_BUS_U0_OK = 0x80,
	NOM_BUS_U1_OK = 0x40,
	NOM_BUS_U2_OK = 0x20,
	NOM_BUS_UEXT_ON = 0x10,
	RED_BUS_U0_OK = 0x08,
	RED_BUS_U1_OK = 0x04,
	RED_BUS_U2_OK = 0x02,
	RED_BUS_UEXT_ON = 0x01,
} cmn_bus_state_t;

//----------------------------------------
// public types
//

//----------------------------------------
// public functions
//

// common module initialization
extern void CMN_init(void);

// common module run method
extern void CMN_run(void);


#endif	// __COMMON_H__
