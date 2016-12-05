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
	struct scalp_dpt_interface interf;		// dispatcher interface

	pt_t rx_pt;					// rx context
	pt_t tx_pt;					// tx context

	struct scalp fr;					// a buffer scalp

	s8 volatile anti_bounce;	// anti-bounce counter
	u8 trigger;					// signal if the action has already happened

	u8 nb_mnt;					// number of other found minuteries
	u8 mnt_addr[2];				// addresses of the other minuteries
	u8 cur_mnt;					// current minuterie index

	u32 time_out;				// time-out for scan request sending

	struct scalp buf[QUEUE_SIZE];
	struct fifo in_fifo;				// reception queue
} alive;


//----------------------------------------
// private functions
//

static u8 scalp_alive_nodes_addresses(void)
{
	struct dna_list* list;
	u8 nb_is;
	u8 nb_bs;
	u8 i;
	u8 nb = 0;

	// retrieve nodes addresses
	list = scalp_dna_list(&nb_is, &nb_bs);

	// extract the other minuteries addresses
	for ( i = DNA_FIRST_IS_INDEX(nb_is); i <= DNA_LAST_IS_INDEX(nb_is); i++ ) {
		// same type but different address
		if ( (list[i].type == DNA_SELF_TYPE(list)) &&
			( list[i].i2c_addr != DNA_SELF_ADDR(list)) ) {
			// an other minuterie is found
			// save its address
			alive.mnt_addr[nb] = list[i].i2c_addr;

			// increment nb of found minuteries in this scan
			nb++;
		}
	}

	// finally save the number of found minuteries
	alive.nb_mnt = nb;

	// return the current node address
	return DNA_SELF_ADDR(list);
}


static u8 scalp_alive_next_address(void)
{
	u8 addr;

	// if no other minuterie is visible
	if ( alive.nb_mnt == 0 ) {
		// return an invalid address (it is in the range of the BS)
		return DPT_LAST_ADDR;
	}

	// so at least 1 other minuterie is declared
	// ensure the current MNT index is valid
	alive.cur_mnt %= alive.nb_mnt;

	// retrieve its address
	addr = alive.mnt_addr[alive.cur_mnt];

	// finally prepare the access to the next minuterie
	alive.cur_mnt++;

	// return the computed address
	return addr;
}


static PT_THREAD( scalp_alive_rx(pt_t* pt) )
{
	struct scalp fr;

	PT_BEGIN(pt);

	PT_WAIT_UNTIL(pt, fifo_get(&alive.in_fifo, &fr));

	// if not the response from current status request
	if ( !fr.resp || (fr.t_id != alive.fr.t_id) ) {
		// throw it away
		PT_RESTART(pt);
	}

	// if response is not in error
	if ( !fr.error ) {
		// increase number of successes
		alive.anti_bounce++;

		// prevent overflow
		if ( alive.anti_bounce > ALV_ANTI_BOUNCE_UPPER_LIMIT ) {
			alive.anti_bounce = ALV_ANTI_BOUNCE_UPPER_LIMIT;
		}
	}

	// loop back
	PT_RESTART(pt);

	PT_END(pt);
}


static PT_THREAD( scalp_alive_tx(pt_t* pt) )
{
	u8 self_addr;

	PT_BEGIN(pt);

	// every second
	PT_WAIT_UNTIL(pt, time_get() > alive.time_out);

	// update time-out
	alive.time_out += ALV_TIME_INTERVAL;

	// extract visible other minuteries addresses
	self_addr = scalp_alive_nodes_addresses();

	// if another minuterie is visible
	if ( alive.nb_mnt != 0 ) {
		// build the get state request
		alive.fr.orig = self_addr;
		alive.fr.dest = scalp_alive_next_address();
		alive.fr.resp = 0;
		alive.fr.error = 0;
		//alive.fr.nat = 0;
		alive.fr.cmde = SCALP_STATE;
		alive.fr.argv[0] = 0x00;

		// send the status request
		scalp_dpt_lock(&alive.interf);
		PT_WAIT_UNTIL(pt, OK == scalp_dpt_tx(&alive.interf, &alive.fr));
		scalp_dpt_unlock(&alive.interf);
	}
	else {
		// one more failure
		alive.anti_bounce--;

		// prevent underflow
		if ( alive.anti_bounce < ALV_ANTI_BOUNCE_LOWER_LIMIT ) {
			alive.anti_bounce = ALV_ANTI_BOUNCE_LOWER_LIMIT;
		}
	}

	// when there are too many failures
	if ( (alive.anti_bounce == ALV_ANTI_BOUNCE_LOWER_LIMIT) && !(alive.trigger & ALV_LOWER_TRIGGER) ) {
		// then set the mode to autonomous
		alive.fr.orig = DPT_SELF_ADDR;
		alive.fr.dest = DPT_SELF_ADDR;
		alive.fr.resp = 0;
		alive.fr.error = 0;
		//alive.fr.nat = 0;
		alive.fr.cmde = SCALP_RECONF;
		alive.fr.argv[0] = 0x00;
		alive.fr.argv[1] = 0x02;		// no bus active

		scalp_dpt_lock(&alive.interf);
		PT_WAIT_UNTIL(pt, OK == scalp_dpt_tx(&alive.interf, &alive.fr));
		scalp_dpt_unlock(&alive.interf);

		// prevent any further lower trigger action
		alive.trigger |= ALV_LOWER_TRIGGER;

		// but allow upper trigger action
		alive.trigger &= ~ALV_UPPER_TRIGGER;
	}

	if ( (alive.anti_bounce == ALV_ANTI_BOUNCE_UPPER_LIMIT) && !(alive.trigger & ALV_UPPER_TRIGGER) ) {
		// then set the mode to autonomous
		alive.fr.orig = DPT_SELF_ADDR;
		alive.fr.dest = DPT_SELF_ADDR;
		alive.fr.resp = 0;
		alive.fr.error = 0;
		//alive.fr.nat = 0;
		alive.fr.cmde = SCALP_RECONF;
		alive.fr.argv[0] = 0x00;
		alive.fr.argv[1] = 0x03;		// bus mode is automatic

		scalp_dpt_lock(&alive.interf);
		PT_WAIT_UNTIL(pt, OK == scalp_dpt_tx(&alive.interf, &alive.fr));
		scalp_dpt_unlock(&alive.interf);

		// prevent any further upper trigger action
		alive.trigger |= ALV_UPPER_TRIGGER;

		// but allow lower trigger action
		alive.trigger &= ~ALV_LOWER_TRIGGER;
	}

	// loop back
	PT_RESTART(pt);

	PT_END(pt);
}


//----------------------------------------
// public functions
//

// ALIVE module initialization
void scalp_alive_init(void)
{
	// threads context init
	PT_INIT(&alive.rx_pt);
	PT_INIT(&alive.tx_pt);

	// variables init
	alive.anti_bounce = 0;
	alive.trigger = 0x00;
	alive.nb_mnt = 0;
	alive.cur_mnt = 0;
	alive.time_out = ALV_TIME_INTERVAL;

	fifo_init(&alive.in_fifo, &alive.buf, QUEUE_SIZE, sizeof(alive.buf[0]));

	// register own call-back for specific commands
	alive.interf.channel = 9;
	alive.interf.cmde_mask = _CM(SCALP_STATE);
	alive.interf.queue = &alive.in_fifo;
	scalp_dpt_register(&alive.interf);
}


// ALIVE module run method
void scalp_alive_run(void)
{
	// handle response if any
	(void)PT_SCHEDULE(scalp_alive_rx(&alive.rx_pt));

	// send response if any
	(void)PT_SCHEDULE(scalp_alive_tx(&alive.tx_pt));
}
