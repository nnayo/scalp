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

#include "time_sync.h"

#include "dispatcher.h"
#include "dna.h"

#include "utils/time.h"
#include "utils/pt.h"


//----------------------------------------
// private defines
//

#define QUEUE_SIZE		1


//----------------------------------------
// private variables
//

static struct {
	struct scalp_dpt_interface interf;		// dispatcher interface

	pt_t pt;					// thread context

	struct scalp fr;				// a buffer frame

	u32 time_out;
	s8 time_correction;

	struct fifo queue;				// reception queue
	struct scalp buf[QUEUE_SIZE];
} tsn;


//----------------------------------------
// private functions
//

static PT_THREAD( scalp_tsn_tsn(pt_t* pt) )
{
	u32 local_time;
	union {
		u32 full;
		u8 part[4];
	} remote_time;
	struct dna_list* list;
	u8 nb_is;
	u8 nb_bs;

	PT_BEGIN(pt);

	// every second
	PT_WAIT_UNTIL(pt, time_get() > tsn.time_out);
	tsn.time_out += TIME_1_SEC;

	// retrieve self and BC node address
	list = scalp_dna_list(&nb_is, &nb_bs);

	// build the time request
	tsn.fr.orig = DNA_SELF_ADDR(list);
	tsn.fr.dest = DNA_BC_ADDR(list);
	tsn.fr.cmde = SCALP_TIME;
	tsn.fr.resp = 0;
	tsn.fr.error = 0;

	// send the time request
	scalp_dpt_lock(&tsn.interf);
	PT_WAIT_UNTIL(pt, OK == scalp_dpt_tx(&tsn.interf, &tsn.fr));
	scalp_dpt_unlock(&tsn.interf);

	// wait for the answer
	PT_WAIT_UNTIL(pt, fifo_get(&tsn.queue, &tsn.fr) && tsn.fr.resp);

	// rebuild remote time (AVR is little endian)
	remote_time.part[0] = tsn.fr.argv[3];
	remote_time.part[1] = tsn.fr.argv[2];
	remote_time.part[2] = tsn.fr.argv[1];
	remote_time.part[3] = tsn.fr.argv[0];

	// read local time
	local_time = time_get();

	// check whether we are in the future
	if ( local_time > remote_time.full ) {
		// then the local time is running too fast
		// so slow it down
		tsn.time_correction--;
	}
	// check whether we are in the past
	if ( local_time < remote_time.full ) {
		// then the local time is running too slow
		// so speed it up
		tsn.time_correction++;
	}

	// update time increment
	time_incr_set(10 * TIME_1_MSEC + tsn.time_correction);

	// loop back
	PT_RESTART(pt);

	PT_END(pt);
}


//----------------------------------------
// public functions
//

// time synchro module initialization
void scalp_tsn_init(void)
{
	// thread context init
	PT_INIT(&tsn.pt);

	// variables init
	tsn.time_correction = 0;
	tsn.time_out = TIME_1_SEC;
	time_incr_set(10 * TIME_1_MSEC);
	fifo_init(&tsn.queue, &tsn.buf, QUEUE_SIZE, sizeof(tsn.buf) / sizeof(tsn.buf[0]));

	// register to dispatcher
	tsn.interf.channel = 8;
	tsn.interf.cmde_mask = _CM(SCALP_TIME);
	tsn.interf.queue = &tsn.queue;
	scalp_dpt_register(&tsn.interf);
}


// time synchro module run method
void scalp_tsn_run(void)
{
	// send response if any
	(void)PT_SCHEDULE(scalp_tsn_tsn(&tsn.pt));
}
