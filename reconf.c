//---------------------
//  Copyright (C) 2000-2007  <Yann GOUY>
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; see the file COPYING.  If not, write to
//  the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
//  Boston, MA 02111-1307, USA.
//
//  you can write to me at <yann_gouy@yahoo.fr>
//

#include "scalp/dispatcher.h"

#include "utils/pt.h"

#include <avr/io.h>
#include <avr/eeprom.h>


//--------------------------------------
// private defines
//


//--------------------------------------
// private enums
//

// bus status
typedef enum {
	NOM,	// nominal bus active
	RED,	// redundant bus active
	NONE,	// no active bus
	AUTO,	// bus automatic selection (only for force bus mode)
} bus_t;


//--------------------------------------
// private types
//


//--------------------------------------
// private structures
//

static struct {
	bus_t bus_force_mode;		// force bus mode

	bus_t bus_curr_stat;		// current bus status
	bus_t bus_prev_stat;		// previous bus status

	dpt_interface_t interf;		// dispatcher interface

	dpt_frame_t out;		// outgoing buffer frame
	dpt_frame_t in;			// incoming buffer frame

	pt_t pt;			// protothread context
} RCF;


//--------------------------------------
// private functions
//

// bus scanning function
// return a pointer to the event frame to be executed
// or NULL if bus state is unchanged
//
// bus nominal state is read on PD4
// bus redundant state is read on PD5
//
// nominal bus is dominant on redundant
dpt_frame_t* RCF_scan_bus(void)
{
#if 0
	void* addr;
#else
	u16 addr;
#endif

	// by default, no bus is active
	bus_t bus = NONE;

	// redundant bus is active
	if (PIND & _BV(PD5)) {
		// save current state
		bus = RED;
	}

	// nominal bus is active
	if (PIND & _BV(PD4)) {
		// save current state
		bus = NOM;
	}

	// handle bus force mode
	if (RCF.bus_force_mode != AUTO) {
		bus = RCF.bus_force_mode;
	}

	// check if active bus has not changed
	if ( bus == RCF.bus_curr_stat )
		// nothing to do, so quit
		return NULL;

	// else
	// save previous state and update current
	RCF.bus_prev_stat = RCF.bus_curr_stat;
	RCF.bus_curr_stat = bus;

	// use event frame depending on current new state
	// compute frame address (first event frame is reset then we have NOM, RED, NONE)
#if 0
	addr = (void*)( sizeof(dpt_frame_t) + bus * sizeof(dpt_frame_t) );
#else
	addr = sizeof(dpt_frame_t) + bus * sizeof(dpt_frame_t);
#endif

#if 0
	// copy event frame from EEPROM
	eeprom_read_block(&RCF.out, (const void *)addr, sizeof(dpt_frame_t));
#else
	// force a container frame with only 1 frame
	RCF.out.orig = DPT_SELF_ADDR;
	RCF.out.dest = DPT_SELF_ADDR;
	RCF.out.cmde = FR_CONTAINER;
	RCF.out.argv[0] = (addr & 0xff00) >> 8;
	RCF.out.argv[1] = (addr & 0x00ff) >> 0;
	RCF.out.argv[2] = 1;
#endif

	// return the pointer to reconf frame
	return &RCF.out;
}


// reconf receive function
static void RCF_rx(dpt_frame_t* fr)
{
	// when receiving a take-off negative response
	// if the bus state is NONE
	// it the responsability of the reconf module
	// to send a 2nd positive command
	// to simulate the correct take-off detection
	// because the minuterie is in stand-alone mode
	if ( (fr->cmde == FR_MINUT_TAKE_OFF) && fr->resp && fr->error && (RCF.bus_curr_stat == NONE) ) {
		// store the incoming frame
		RCF.in = *fr;
	}

	// handle the set/get bus force mode frame
	if ( fr->cmde == FR_RECONF_FORCE_MODE ) {
		// store the incoming frame
		RCF.in = *fr;
	}

	// it would so great if every sent command
	// is checked with the received response
	// may be later or if really necessary
}


//--------------------------------------
// public variables
//


//--------------------------------------
// public functions
//

void RCF_init(void)
{
	// set direction port as INPUT
	DDRD &= ~(_BV(PD4) | _BV(PD5));

	// reset bus force mode
	RCF.bus_force_mode = AUTO;

	// reset bus status
	RCF.bus_curr_stat = NONE;
	RCF.bus_prev_stat = NONE;

	// reset in frame to prevent take-off handling
	RCF.in.cmde = FR_NO_CMDE;
	RCF.in.resp = 0;
	RCF.in.error = 0;
	RCF.in.nat = 0;

	// register own call-back for specific commands
	RCF.interf.channel = 1;
	RCF.interf.cmde_mask = _CM(FR_MINUT_TAKE_OFF) | _CM(FR_RECONF_FORCE_MODE);
	RCF.interf.rx = RCF_rx;
	DPT_register(&RCF.interf);

	// reset context
	PT_INIT(&RCF.pt);
}


u8 RCF_run(void)
{
	u8 swap;
	dpt_frame_t* fr;

	PT_BEGIN(&RCF.pt);

	fr = RCF_scan_bus();

	// if state has changed
	if (fr != NULL ) {
		// store frame
		RCF.out = *fr;

		// lock the dispatcher
		DPT_lock(&RCF.interf);

		// be sure the frame is sent
		PT_WAIT_UNTIL(&RCF.pt, DPT_tx(&RCF.interf, &RCF.out) == OK);

		// unlock the dispatcher
		DPT_unlock(&RCF.interf);
	}

	// take-off handling
	if ( (RCF.in.cmde == FR_MINUT_TAKE_OFF) && RCF.in.resp && RCF.in.error ) {
		RCF.in.orig = DPT_SELF_ADDR;
		RCF.in.dest = DPT_SELF_ADDR;
		RCF.in.resp = 0;
		RCF.in.error = 0;
		RCF.in.nat = 0;
		RCF.in.cmde = FR_MINUT_TAKE_OFF;

		// lock the dispatcher
		DPT_lock(&RCF.interf);

		// once the frame can be sent
		PT_WAIT_UNTIL(&RCF.pt, DPT_tx(&RCF.interf, &RCF.in) == OK);

		// it is no use sending it more times
		RCF.in.cmde = FR_NO_CMDE;

		// unlock the dispatcher
		DPT_unlock(&RCF.interf);
	}

	// force bus mode handling
	if ( RCF.in.cmde == FR_RECONF_FORCE_MODE ) {
		swap = RCF.in.orig;
		RCF.in.orig = RCF.in.dest;
		RCF.in.dest = swap;

		switch (RCF.in.argv[0]) {
			case 0x00:	// set force mode
				RCF.bus_force_mode = RCF.in.argv[1];
				break;

			case 0xff:	// get force mode
				RCF.in.argv[1] = RCF.bus_force_mode;
				RCF.in.argv[2] = RCF.bus_curr_stat;
				break;

			default:
				RCF.in.error = 1;
				break;
		}
		RCF.in.resp = 1;

		// lock the dispatcher
		DPT_lock(&RCF.interf);

		// once the frame can be sent
		PT_WAIT_UNTIL(&RCF.pt, DPT_tx(&RCF.interf, &RCF.in) == OK);

		// it is no use sending it more times
		RCF.in.cmde = FR_NO_CMDE;

		// unlock the dispatcher
		DPT_unlock(&RCF.interf);
	}
	PT_RESTART(&RCF.pt);

	PT_END(&RCF.pt);
}
