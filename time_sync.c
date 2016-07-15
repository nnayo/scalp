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
	dpt_interface_t interf;		// dispatcher interface

	pt_t pt;					// thread context

	frame_t fr;				// a buffer frame

	u32 time_out;
	s8 time_correction;

	fifo_t queue;				// reception queue
	frame_t buf[QUEUE_SIZE];
} TSN;


//----------------------------------------
// private functions
//

static PT_THREAD( TSN_tsn(pt_t* pt) )
{
	u32 local_time;
	union {
		u32 full;
		u8 part[4];
	} remote_time;
	dna_list_t* list;
	u8 nb_is;
	u8 nb_bs;

	PT_BEGIN(pt);

	// every second
	PT_WAIT_UNTIL(pt, TIME_get() > TSN.time_out);
	TSN.time_out += TIME_1_SEC;

	// retrieve self and BC node address
	list = DNA_list(&nb_is, &nb_bs);

	// build the time request
	TSN.fr.orig = DNA_SELF_ADDR(list);
	TSN.fr.dest = DNA_BC_ADDR(list);
	TSN.fr.cmde = FR_TIME_GET;
	TSN.fr.resp = 0;
	TSN.fr.error = 0;
	TSN.fr.eth = 0;
	TSN.fr.serial = 0;

	// send the time request
	dpt_lock(&TSN.interf);
	PT_WAIT_UNTIL(pt, OK == dpt_tx(&TSN.interf, &TSN.fr));
	dpt_unlock(&TSN.interf);

	// wait for the answer
	PT_WAIT_UNTIL(pt, FIFO_get(&TSN.queue, &TSN.fr) && TSN.fr.resp);

	// rebuild remote time (AVR is little endian)
	remote_time.part[0] = TSN.fr.argv[3];
	remote_time.part[1] = TSN.fr.argv[2];
	remote_time.part[2] = TSN.fr.argv[1];
	remote_time.part[3] = TSN.fr.argv[0];

	// read local time
	local_time = TIME_get();

	// check whether we are in the future
	if ( local_time > remote_time.full ) {
		// then the local time is running too fast
		// so slow it down
		TSN.time_correction--;
	}
	// check whether we are in the past
	if ( local_time < remote_time.full ) {
		// then the local time is running too slow
		// so speed it up
		TSN.time_correction++;
	}

	// update time increment
	TIME_set_incr(10 * TIME_1_MSEC + TSN.time_correction);

	// loop back
	PT_RESTART(pt);

	PT_END(pt);
}


//----------------------------------------
// public functions
//

// Time Synchro module initialization
void TSN_init(void)
{
	// thread context init
	PT_INIT(&TSN.pt);

	// variables init
	TSN.time_correction = 0;
	TSN.time_out = TIME_1_SEC;
	TIME_set_incr(10 * TIME_1_MSEC);
	FIFO_init(&TSN.queue, &TSN.buf, QUEUE_SIZE, sizeof(TSN.buf) / sizeof(TSN.buf[0]));

	// register to dispatcher
	TSN.interf.channel = 8;
	TSN.interf.cmde_mask = _CM(FR_TIME_GET);
	TSN.interf.queue = &TSN.queue;
	dpt_register(&TSN.interf);
}


// Time Synchro module run method
void TSN_run(void)
{
	// send response if any
	(void)PT_SCHEDULE(TSN_tsn(&TSN.pt));
}
