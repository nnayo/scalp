//---------------------
//  Copyright (C) 2000-2009  <Yann GOUY>
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 3 of the License, or
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

// RCF goal is to detect a bus change
//
// this triggers a switch from current bus
// to the other one
//
// if the 2 buses are available, priority is given
// to the nominal bus
//
// it remains possible to force the use of a specified bus
// or to prevent the use of the numeric part of both buses
//


#include "scalp/dispatcher.h"

#include "utils/pt.h"
#include "utils/fifo.h"

#include <avr/io.h>


//--------------------------------------
// private defines
//

#define QUEUE_SIZE		1


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
	pt_t in_pt;					// in thread
	frame_t in;
	fifo_t in_fifo;
	frame_t in_buf[QUEUE_SIZE];
	dpt_interface_t interf;

	pt_t out_pt;				// out thread
	frame_t out;
	fifo_t out_fifo;
	frame_t out_buf[QUEUE_SIZE];

	pt_t scan_pt;				// scan thread
	frame_t scan_bus;		// scan bus frame
	bus_t bus_force_mode;		// force bus mode
	bus_t bus_curr_stat;		// current bus status
	bus_t bus_prev_stat;		// previous bus status
} RCF;


//--------------------------------------
// private functions
//

// bus scanning function
// return KO if bus state is unchanged
// else return OK and fill the scan_bus frame
//
// bus nominal state is read on PD4
// bus redundant state is read on PD5
//
// nominal bus is dominant on redundant
static u8 RCF_scan_bus(void)
{
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
	if ( bus == RCF.bus_curr_stat ) {
		// nothing to do, so quit
		return KO;
	}

	// else
	// save previous state and update current
	RCF.bus_prev_stat = RCF.bus_curr_stat;
	RCF.bus_curr_stat = bus;

	// use event frame depending on current new state
	// build a container frame to read the state frame from eeprom
	RCF.scan_bus.orig = DPT_SELF_ADDR;
	RCF.scan_bus.dest = DPT_SELF_ADDR;
	RCF.scan_bus.cmde = FR_CONTAINER;
	RCF.scan_bus.argv[0] = 0;
	RCF.scan_bus.argv[1] = 0;
	RCF.scan_bus.argv[2] = 0;
	RCF.scan_bus.argv[3] = bus + 1;

	// return the pointer to reconf frame
	return OK;
}


static PT_THREAD( RCF_scan(pt_t* pt) )
{
	PT_BEGIN(pt);

	// if the bus state has changed
	if ( RCF_scan_bus() ) {
		// send the reconf frame
		PT_WAIT_UNTIL(pt, FIFO_put(&RCF.out_fifo, &RCF.scan_bus));
	}

	PT_RESTART(pt);

	PT_END(pt);
}


static PT_THREAD( RCF_out(pt_t* pt) )
{
	PT_BEGIN(pt);

	PT_WAIT_UNTIL(pt, FIFO_get(&RCF.out_fifo, &RCF.out));

	// send the reconf frame
	DPT_lock(&RCF.interf);
	if ( !DPT_tx(&RCF.interf, &RCF.out) ) {
		FIFO_unget(&RCF.out_fifo, &RCF.out);
	}

	PT_RESTART(pt);

	PT_END(pt);
}


// reconf receive function
static PT_THREAD( RCF_in(pt_t* pt) )
{
	u8 swap;

	PT_BEGIN(pt);

	PT_WAIT_UNTIL(pt, FIFO_get(&RCF.in_fifo, &RCF.in));

	// when receiving a take-off negative response
	// if the bus state is NONE
	// it the responsability of the reconf module
	// to send a 2nd positive command
	// to simulate the correct take-off detection
	// because the minuterie is in stand-alone mode
	if ( (RCF.in.cmde == FR_MINUT_TAKE_OFF) && RCF.in.resp && RCF.in.error  && (RCF.bus_curr_stat == NONE) ) {
		RCF.in.orig = DPT_SELF_ADDR;
		RCF.in.dest = DPT_SELF_ADDR;
		RCF.in.cmde = FR_MINUT_TAKE_OFF;
		RCF.in.resp = 0;
		RCF.in.error = 0;

		// once the frame can be sent
		PT_WAIT_UNTIL(pt, FIFO_put(&RCF.out_fifo, &RCF.in));
	}

	// force bus mode handling
	else if ( (RCF.in.cmde == FR_RECONF_MODE) && !(RCF.in.resp || RCF.in.error || RCF.in.time_out) ) {
		swap = RCF.in.orig;
		RCF.in.orig = RCF.in.dest;
		RCF.in.dest = swap;

		switch (RCF.in.argv[0]) {
			case FR_RECONF_MODE_SET:	// set force mode
				RCF.bus_force_mode = RCF.in.argv[1];
				break;

			case FR_RECONF_MODE_GET:	// get force mode
				RCF.in.argv[1] = RCF.bus_force_mode;
				RCF.in.argv[2] = RCF.bus_curr_stat;
				break;

			default:
				RCF.in.error = 1;
				break;
		}
		RCF.in.resp = 1;

		// once the frame can be sent
		PT_WAIT_UNTIL(pt, FIFO_put(&RCF.out_fifo, &RCF.in));
	}

	// it would so great if every sent command
	// is checked with the received response
	// may be later or if really necessary
	PT_RESTART(pt);

	PT_END(pt);
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

	// init in thread
	PT_INIT(&RCF.in_pt);
	FIFO_init(&RCF.in_fifo, &RCF.in_buf, QUEUE_SIZE, sizeof(RCF.in_buf[0]));
	RCF.interf.channel = 1;
	RCF.interf.cmde_mask = _CM(FR_MINUT_TAKE_OFF) | _CM(FR_RECONF_MODE);
	RCF.interf.queue = &RCF.in_fifo;
	DPT_register(&RCF.interf);

	// init out thread
	PT_INIT(&RCF.out_pt);
	FIFO_init(&RCF.out_fifo, &RCF.out_buf, QUEUE_SIZE, sizeof(RCF.out_buf[0]));
}


void RCF_run(void)
{
	(void)PT_SCHEDULE(RCF_scan(&RCF.scan_pt));
	(void)PT_SCHEDULE(RCF_out(&RCF.out_pt));
	(void)PT_SCHEDULE(RCF_in(&RCF.in_pt));

	// if fifoes are empty
	if ( ( FIFO_full(&RCF.out_fifo) == 0 ) && ( FIFO_full(&RCF.out_fifo) == 0 ) ) {
		// unlock the dispatcher
		DPT_unlock(&RCF.interf);
	}
}
