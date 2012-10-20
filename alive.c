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

#include "alive.h"

#include "dispatcher.h"
#include "dna.h"

#include "utils/time.h"
#include "utils/pt.h"
#include "utils/fifo.h"


//----------------------------------------
// private defines
//

#define ALV_ANTI_BOUNCE_UPPER_LIMIT		7
#define ALV_UPPER_TRIGGER				0x01
#define ALV_ANTI_BOUNCE_LOWER_LIMIT		-7
#define ALV_LOWER_TRIGGER				0x02

#define ALV_TIME_INTERVAL				TIME_1_SEC

#define QUEUE_SIZE						3


//----------------------------------------
// private variables
//

static struct {
	dpt_interface_t interf;		// dispatcher interface

	pt_t rx_pt;					// rx context
	pt_t tx_pt;					// tx context

	frame_t fr;					// a buffer frame

	s8 volatile anti_bounce;	// anti-bounce counter
	u8 trigger;					// signal if the action has already happened

	u8 nb_mnt;					// number of other found minuteries
	u8 mnt_addr[2];				// addresses of the other minuteries
	u8 cur_mnt;					// current minuterie index

	u32 time_out;				// time-out for scan request sending

	frame_t buf[QUEUE_SIZE];
	fifo_t in_fifo;				// reception queue
} ALV;


//----------------------------------------
// private functions
//

static u8 ALV_nodes_addresses(void)
{
	dna_list_t* list;
	u8 nb_is;
	u8 nb_bs;
	u8 i;
	u8 nb = 0;

	// retrieve nodes addresses
	list = DNA_list(&nb_is, &nb_bs);

	// extract the other minuteries addresses
	for ( i = DNA_FIRST_IS_INDEX(nb_is); i <= DNA_LAST_IS_INDEX(nb_is); i++ ) {
		// same type but different address
		if ( (list[i].type == DNA_SELF_TYPE(list)) &&
			( list[i].i2c_addr != DNA_SELF_ADDR(list)) ) {
			// an other minuterie is found
			// save its address
			ALV.mnt_addr[nb] = list[i].i2c_addr;

			// increment nb of found minuteries in this scan
			nb++;
		}
	}

	// finally save the number of found minuteries
	ALV.nb_mnt = nb;

	// return the current node address
	return DNA_SELF_ADDR(list);
}


static u8 ALV_next_address(void)
{
	u8 addr;

	// if no other minuterie is visible
	if ( ALV.nb_mnt == 0 ) {
		// return an invalid address (it is in the range of the BS)
		return DPT_LAST_ADDR;
	}

	// so at least 1 other minuterie is declared
	// ensure the current MNT index is valid
	ALV.cur_mnt %= ALV.nb_mnt;

	// retrieve its address
	addr = ALV.mnt_addr[ALV.cur_mnt];

	// finally prepare the access to the next minuterie
	ALV.cur_mnt++;

	// return the computed address
	return addr;
}


static PT_THREAD( ALV_rx(pt_t* pt) )
{
	frame_t fr;

	PT_BEGIN(pt);

	PT_WAIT_UNTIL(pt, FIFO_get(&ALV.in_fifo, &fr));

	// if not the response from current status request
	if ( !fr.resp || (fr.t_id != ALV.fr.t_id) ) {
		// throw it away
		PT_RESTART(pt);
	}

	// if response is not in error
	if ( !fr.error ) {
		// increase number of successes
		ALV.anti_bounce++;

		// prevent overflow
		if ( ALV.anti_bounce > ALV_ANTI_BOUNCE_UPPER_LIMIT ) {
			ALV.anti_bounce = ALV_ANTI_BOUNCE_UPPER_LIMIT;
		}
	}

	// loop back
	PT_RESTART(pt);

	PT_END(pt);
}


static PT_THREAD( ALV_tx(pt_t* pt) )
{
	u8 self_addr;

	PT_BEGIN(pt);

	// every second
	PT_WAIT_UNTIL(pt, TIME_get() > ALV.time_out);

	// update time-out
	ALV.time_out += ALV_TIME_INTERVAL;

	// extract visible other minuteries addresses
	self_addr = ALV_nodes_addresses();

	// if another minuterie is visible
	if ( ALV.nb_mnt != 0 ) {
		// build the get state request
		ALV.fr.orig = self_addr;
		ALV.fr.dest = ALV_next_address();
		ALV.fr.resp = 0;
		ALV.fr.error = 0;
		//ALV.fr.nat = 0;
		ALV.fr.cmde = FR_STATE;
		ALV.fr.argv[0] = 0x00;

		// send the status request
		DPT_lock(&ALV.interf);
		PT_WAIT_UNTIL(pt, OK == DPT_tx(&ALV.interf, &ALV.fr));
		DPT_unlock(&ALV.interf);
	}
	else {
		// one more failure
		ALV.anti_bounce--;

		// prevent underflow
		if ( ALV.anti_bounce < ALV_ANTI_BOUNCE_LOWER_LIMIT ) {
			ALV.anti_bounce = ALV_ANTI_BOUNCE_LOWER_LIMIT;
		}
	}

	// when there are too many failures
	if ( (ALV.anti_bounce == ALV_ANTI_BOUNCE_LOWER_LIMIT) && !(ALV.trigger & ALV_LOWER_TRIGGER) ) {
		// then set the mode to autonomous
		ALV.fr.orig = DPT_SELF_ADDR;
		ALV.fr.dest = DPT_SELF_ADDR;
		ALV.fr.resp = 0;
		ALV.fr.error = 0;
		//ALV.fr.nat = 0;
		ALV.fr.cmde = FR_RECONF_MODE;
		ALV.fr.argv[0] = 0x00;
		ALV.fr.argv[1] = 0x02;		// no bus active

		DPT_lock(&ALV.interf);
		PT_WAIT_UNTIL(pt, OK == DPT_tx(&ALV.interf, &ALV.fr));
		DPT_unlock(&ALV.interf);

		// prevent any further lower trigger action
		ALV.trigger |= ALV_LOWER_TRIGGER;

		// but allow upper trigger action
		ALV.trigger &= ~ALV_UPPER_TRIGGER;
	}

	if ( (ALV.anti_bounce == ALV_ANTI_BOUNCE_UPPER_LIMIT) && !(ALV.trigger & ALV_UPPER_TRIGGER) ) {
		// then set the mode to autonomous
		ALV.fr.orig = DPT_SELF_ADDR;
		ALV.fr.dest = DPT_SELF_ADDR;
		ALV.fr.resp = 0;
		ALV.fr.error = 0;
		//ALV.fr.nat = 0;
		ALV.fr.cmde = FR_RECONF_MODE;
		ALV.fr.argv[0] = 0x00;
		ALV.fr.argv[1] = 0x03;		// bus mode is automatic

		DPT_lock(&ALV.interf);
		PT_WAIT_UNTIL(pt, OK == DPT_tx(&ALV.interf, &ALV.fr));
		DPT_unlock(&ALV.interf);

		// prevent any further upper trigger action
		ALV.trigger |= ALV_UPPER_TRIGGER;

		// but allow lower trigger action
		ALV.trigger &= ~ALV_LOWER_TRIGGER;
	}

	// loop back
	PT_RESTART(pt);

	PT_END(pt);
}


//----------------------------------------
// public functions
//

// ALIVE module initialization
void ALV_init(void)
{
	// threads context init
	PT_INIT(&ALV.rx_pt);
	PT_INIT(&ALV.tx_pt);

	// variables init
	ALV.anti_bounce = 0;
	ALV.trigger = 0x00;
	ALV.nb_mnt = 0;
	ALV.cur_mnt = 0;
	ALV.time_out = ALV_TIME_INTERVAL;

	FIFO_init(&ALV.in_fifo, &ALV.buf, QUEUE_SIZE, sizeof(ALV.buf[0]));

	// register own call-back for specific commands
	ALV.interf.channel = 9;
	ALV.interf.cmde_mask = _CM(FR_STATE);
	ALV.interf.queue = &ALV.in_fifo;
	DPT_register(&ALV.interf);
}


// ALIVE module run method
void ALV_run(void)
{
	// handle response if any
	(void)PT_SCHEDULE(ALV_rx(&ALV.rx_pt));

	// send response if any
	(void)PT_SCHEDULE(ALV_tx(&ALV.tx_pt));
}
