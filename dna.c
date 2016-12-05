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

#include "dna.h"

#include "dispatcher.h"

#include "utils/pt.h"		// PT_*
#include "utils/pt_sem.h"	// PT_SEM_*
#include "utils/time.h"		// time_*
#include "utils/fifo.h"		// fifo_*

#include <string.h>
#include <stdlib.h>

//--------------------------------------
// private defines
//

#define PCA9540B_ADDR	0x70	// I2C mux

#define NB_IN			3		// incoming frames buffer size

//--------------------------------------
// private enums
//


//--------------------------------------
// private types
//


//--------------------------------------
// private structures
//

static struct {
	struct scalp_dpt_interface interf;		// dispatcher interface

	pt_t pt;					// dna thread
	pt_t pt2;					// secondary thread (scan_free, scan_bs, is_reg)
	pt_t pt3;					// thirdary thread (list_updater, is_reg_wait)

	struct dna_list list[DNA_LIST_SIZE];	// list of the I2C connected components (BC, self then other IS)
	u8 nb_is;					// number of registered IS
	u8 nb_bs;					// number of discovered BS
	u8 index;					// index in current sending of the list

	struct fifo in_fifo;				// incoming frames fifo
	struct scalp in_buf[NB_IN];		// incoming frames buffer

	struct scalp out;				// out going frame

	u8 tmp;						// all purpose temporary buffer

	u32 time;					// temporary variable for saving retry registering time
} dna;


//--------------------------------------
// private functions
//

// this protothread purpose is to scan the I2C bus
// to find a free address
// dna.tmp is used to store the scanned I2C address
static PT_THREAD( scalp_dna_scan_free(pt_t* pt, u8 is_bc) )
{
	struct scalp fr;

	PT_BEGIN(pt);

	// set scanning start address
	dna.tmp = DNA_I2C_ADDR_MIN;

	// scan all the IS address range
	while (1) {
		// send a frame to test if the I2C address is free
		dna.out.orig = 0;
		dna.out.dest = dna.tmp;
		dna.out.status = 0;
		dna.out.cmde = SCALP_TWIREAD;

		// if not BC
		if (!is_bc) {
			// wait a random time to let BC start and avoid collision between IS
			dna.time = (rand() % 100) * TIME_1_MSEC + time_get();

			PT_WAIT_UNTIL(pt, time_get() > dna.time);
		}

		// then send frame
		PT_WAIT_UNTIL(pt, scalp_dpt_tx(&dna.interf, &dna.out) == OK);

		// wait for the matching response evicting the others
		PT_WAIT_UNTIL(pt, (OK == fifo_get(&dna.in_fifo, &fr)) && (fr.t_id == dna.out.t_id) );

		if ( (fr.cmde == SCALP_TWIREAD) && fr.resp && fr.error ) {
			// a free address is found
			DNA_SELF_ADDR(dna.list) = dna.tmp;

			// the initial phase is finished
			// bring the node to partial capability
			scalp_dpt_sl_addr_set(dna.tmp);
			PT_EXIT(pt);
		}

		if ( (fr.cmde == SCALP_TWIREAD) && fr.resp ) {
			// the address is in use
			// try the next one
			dna.tmp++;

			// if end of range is reached without a free slot
			if ( dna.tmp > DNA_I2C_ADDR_MAX ) {
				// block there for ever
				PT_YIELD(pt);
			}
		}
	}

	PT_END(pt);
}


// this protothread purpose is to scan the I2C bus
// to find every BS
// dna.tmp is used to stored the scanned BS address
static PT_THREAD( scalp_dna_scan_bs(pt_t* pt) )
{
	struct scalp fr;

	PT_BEGIN(pt);

	// start the scanning range after the reserved address for local node
	dna.tmp = DPT_FIRST_ADDR;

	// scan the whole I2C bus addresses range
	while (1) {
		// send a frame to test if the I2C address is free
		dna.out.orig = 0;
		dna.out.dest = dna.tmp;
		dna.out.status = 0;
		dna.out.cmde = SCALP_TWIREAD;

		// then send frame
		PT_WAIT_UNTIL(pt, scalp_dpt_tx(&dna.interf, &dna.out) == OK);

		// wait for the matching response evicting the others
		PT_WAIT_UNTIL(pt, (OK == fifo_get(&dna.in_fifo, &fr)) && (fr.t_id == dna.out.t_id) );

		if ( (fr.cmde == SCALP_TWIREAD) && fr.resp && !fr.error ) {
			// a new BS is found
			dna.nb_bs++;

			// if there is still some place left
			if ( dna.nb_is + dna.nb_bs < DNA_LIST_SIZE - DNA_BC ) {
				// add it to the list from the end
				dna.list[DNA_LIST_SIZE - dna.nb_bs].i2c_addr = dna.tmp;
				dna.list[DNA_LIST_SIZE - dna.nb_bs].type = DNA_BS;
			}
		}

		// try the next address
		dna.tmp++;

		// if the address is in the IS range
		if ( dna.tmp == DNA_I2C_ADDR_MIN) {
			// skip it
			dna.tmp = DNA_I2C_ADDR_MAX + 1;
		}

		// if the address is the I2C mux (PCA9540B) one
		if ( dna.tmp == PCA9540B_ADDR) {
			// skip it
			dna.tmp++;
		}

		// if the end of I2C address range is reached
		if ( dna.tmp > DPT_LAST_ADDR ) {
			// stop scanning the bus
			PT_EXIT(pt);
		}
	}

	PT_END(pt);
}


// this thread runs when the reg node list is to be updated
// it is restarted each time the BC signals a new node in the list
// this is done by setting the index to DNA_BC
static PT_THREAD( scalp_dna_list_updater(pt_t* pt) )
{
	PT_BEGIN(pt);

	// wait as long as the updated list doesn't need to be sent
	PT_WAIT_WHILE(pt, dna.index >= DNA_LIST_SIZE);

	// when list updating starts
	// it is useless to lock the channel
	// as it is done by scalp_dna_bc_frame

	// increment the index in the reg nodes list
	dna.index++;

	// prebuild cmde header
	dna.out.dest = DPT_BROADCAST_ADDR;
	dna.out.orig = DNA_SELF_ADDR(dna.list);
	dna.out.status = 0;
	dna.out.argv[3] = 0x00;
	
	if ( dna.index < DNA_LIST_SIZE ) {
		// compose a line command
		dna.out.cmde = SCALP_DNALINE;
		dna.out.argv[0] = dna.index;
		dna.out.argv[1] = dna.list[dna.index].type;
		dna.out.argv[2] = dna.list[dna.index].i2c_addr;

		// send the frame
		PT_WAIT_UNTIL(pt, scalp_dpt_tx(&dna.interf, &dna.out) == OK);
	}

	if ( dna.index == DNA_LIST_SIZE) {
		// once the update is finished
		// compose a list command
		dna.out.cmde = SCALP_DNALIST;
		dna.out.argv[0] = dna.nb_is;
		dna.out.argv[1] = dna.nb_bs;
		dna.out.argv[2] = 0x00;

		// send the frame
		PT_WAIT_UNTIL(pt, scalp_dpt_tx(&dna.interf, &dna.out) == OK);

		// then unlock the channel
		scalp_dpt_unlock(&dna.interf);
	}

	// loop back
	PT_RESTART(pt);

	PT_END(pt);
}


static PT_THREAD( scalp_dna_bc_frame(pt_t* pt) )
{
	struct scalp fr;

	PT_BEGIN(pt);

	// wait incoming command
	PT_WAIT_UNTIL(pt, OK == fifo_get(&dna.in_fifo, &fr));

	// if frame is a response
	if ( fr.resp ) {
		// unlock the channel
		scalp_dpt_unlock(&dna.interf);

		// ignore it and wait next frame
		PT_RESTART(pt);
	}
	
	// prebuild response header (code factorizing)
	dna.out.dest = fr.orig;
	dna.out.orig = DNA_SELF_ADDR(dna.list);
	dna.out.t_id = fr.t_id;
	dna.out.cmde = fr.cmde;
	dna.out.error = 0;
	dna.out.resp = 1;
	dna.out.time_out = 0;
	dna.out.argv[0] = 0x00;
	dna.out.argv[1] = 0x00;
	dna.out.argv[2] = 0x00;
	dna.out.argv[3] = 0x00;

	switch (fr.cmde) {
		case SCALP_DNAREGISTER:
			// an IS is registering

			// check if there is some place left in the list
			if ( dna.nb_is + 1 + dna.nb_bs < DNA_LIST_SIZE - DNA_BC ) {
				// update the number of ISs
				dna.nb_is++;

				// fill the registered node list
				dna.list[DNA_BC + dna.nb_is].i2c_addr = fr.argv[0];
				dna.list[DNA_BC + dna.nb_is].type = fr.argv[1];

				// and send back the REGISTER response
				PT_WAIT_UNTIL(pt, scalp_dpt_tx(&dna.interf, &dna.out) == OK);

				// prepare to send the new list to every node
				// list sending will start by the BC line
				dna.index = 0;
			}

			break;

		case SCALP_DNALIST:
			// an IS is asking for the list size
			// build the response
			dna.out.argv[0] = dna.nb_is;
			dna.out.argv[1] = dna.nb_bs;

			// then send it
			PT_WAIT_UNTIL(pt, scalp_dpt_tx(&dna.interf, &dna.out) == OK);
			break;

		case SCALP_DNALINE:
			// an IS is asking for a specific line
			// build the response
			dna.out.argv[0] = fr.argv[0];
			dna.out.argv[1] = dna.list[fr.argv[0]].type;
			dna.out.argv[2] = dna.list[fr.argv[0]].i2c_addr;

			// then send it
			PT_WAIT_UNTIL(pt, scalp_dpt_tx(&dna.interf, &dna.out) == OK);
			break;

		default:
			// ignore frame
			break;
	}

	// if list updating isn't running
	if ( dna.index >= DNA_LIST_SIZE ) {
		// finally, unlock the channel
		scalp_dpt_unlock(&dna.interf);
	}

	// loop back
	PT_RESTART(pt);

	PT_END(pt);
}


static u8 scalp_dna_bc(void)
{
	// incoming frames handling
	(void)PT_SCHEDULE(scalp_dna_bc_frame(&dna.pt2));

	// DNA list updating handling
	(void)PT_SCHEDULE(scalp_dna_list_updater(&dna.pt3));

	return OK;
}


static PT_THREAD( scalp_dna_is_reg_wait(pt_t* pt, u8* ret) )
{
	struct scalp fr;
	u8 time_out;

	PT_BEGIN(pt);

	// wait for the time-out or the reception of the response
	PT_WAIT_UNTIL(pt, (time_out = (time_get() >= dna.time)) || (OK == (*ret = fifo_get(&dna.in_fifo, &fr))) );

	// if time-out occured
	if (time_out) {
		// wait is finished
		PT_EXIT(pt);
	}

	// check whether the REGISTER resp is received
	if ( (fr.cmde == SCALP_DNAREGISTER) && fr.resp && !(fr.error || fr.time_out) ) {
		// registering is done
		// update reg nodes list
		dna.list[DNA_BC].type = DNA_BC;
		dna.list[DNA_BC].i2c_addr = fr.orig;

		// allow general calls
		scalp_dpt_gen_call(OK);

		// init is finished
		PT_EXIT(pt);
	}

	// probably a false trigger on frame reception
	// so restart waiting for the answer or the time-out
	PT_RESTART(pt);

	PT_END(pt);
}


static PT_THREAD( scalp_dna_is_reg(pt_t* pt) )
{
	u8 ret = KO;

	// for the IS, it is needed to register
	PT_BEGIN(pt);

	// compute the time-out time
	dna.time = time_get() + TIME_1_MSEC * 500;

	// send the REGISTER cmde
	dna.out.dest = DPT_BROADCAST_ADDR;
	dna.out.orig = DNA_SELF_ADDR(dna.list);
	dna.out.status = 0;
	dna.out.cmde = SCALP_DNAREGISTER;
	dna.out.argv[0] = DNA_SELF_ADDR(dna.list);
	dna.out.argv[1] = DNA_SELF_TYPE(dna.list);
	PT_WAIT_UNTIL(pt, scalp_dpt_tx(&dna.interf, &dna.out) == OK );

	// wait for the time-out or the reception of the response
	PT_SPAWN(pt, &dna.pt3, scalp_dna_is_reg_wait(&dna.pt3, &ret));

	// on time-out
	if ( ret == KO ) {
		// one more try
		dna.tmp++;

		// check if retries limit is reached
		if (dna.tmp >= 5) {
			// send reconf bus force mode to NONE
			dna.out.orig = DPT_SELF_ADDR;
			dna.out.dest = DPT_SELF_ADDR;
			dna.out.cmde = SCALP_RECONF;
			dna.out.status = 0;
			dna.out.argv[0] = 0x00;	// set
			dna.out.argv[1] = 0x02;	// NONE
			PT_WAIT_UNTIL(pt, scalp_dpt_tx(&dna.interf, &dna.out) == OK );

			// give up registering
			PT_EXIT(pt);
		}
		else {
			// else loop back to send the register command
			PT_RESTART(pt);
		}
	}

	PT_END(pt);
}


static PT_THREAD( scalp_dna_is(pt_t* pt) )
{
	struct scalp fr;

	PT_BEGIN(pt);

	// wait incoming commands
	PT_WAIT_UNTIL(pt, OK == fifo_get(&dna.in_fifo, &fr));

	// immediatly release the channel
	scalp_dpt_unlock(&dna.interf);

	// if frame is a response
	if ( fr.resp ) {
		// ignore it
		PT_RESTART(pt);
	}

	switch (fr.cmde) {
		case SCALP_DNALIST:
			// BC is signaling modification in the header of registered nodes list
			// update own list
			dna.nb_is = fr.argv[0];
			dna.nb_bs = fr.argv[1];

			break;

		case SCALP_DNALINE:
			// BC is signaling modification of registered nodes list
			// update own list
			dna.list[fr.argv[0]].type = fr.argv[1];
			dna.list[fr.argv[0]].i2c_addr = fr.argv[2];

			break;

		default:
			// ignore frame
			break;
	}

	// loop back to wait for the next frames
	PT_RESTART(pt);

	PT_END(pt);
}


//--------------------------------------
// public variables
//




//--------------------------------------
// public functions
//

void scalp_dna_init(enum dna mode)
{
	// reset the whole structure
	memset(&dna, 0, sizeof(dna));

	// init protothread
	PT_INIT(&dna.pt);

	// save self config
	DNA_SELF_TYPE(dna.list) = mode;

	// set fifoes
	fifo_init(&dna.in_fifo, &dna.in_buf, NB_IN, sizeof(dna.in_buf[0]));

	// register to the dispatcher
	dna.interf.channel = 2;
	dna.interf.cmde_mask = _CM(SCALP_DNAREGISTER)
                                | _CM(SCALP_DNALIST)
                                | _CM(SCALP_DNALINE)
                                | _CM(SCALP_TWIWRITE)
                                | _CM(SCALP_TWIREAD);
	dna.interf.queue = &dna.in_fifo;
	scalp_dpt_register(&dna.interf);

	// the channel will be locked
	// until the end of the scannings
	// or
	// until the IS is registered
	scalp_dpt_lock(&dna.interf);
}


struct dna_list* scalp_dna_list(u8* nb_is, u8* nb_bs)
{
	// update the number of ISs and BSs
	*nb_is = dna.nb_is;
	*nb_bs = dna.nb_bs;

	// return the whole list
	return dna.list;
}


u8 scalp_dna_run(void)
{
	PT_BEGIN(&dna.pt);

	// when acting as a BC
	if ( DNA_SELF_TYPE(dna.list) == DNA_BC ) {
		// scan for a free I2C address
		PT_SPAWN(&dna.pt, &dna.pt2, scalp_dna_scan_free(&dna.pt2, 1));

		// find every BS
		PT_SPAWN(&dna.pt, &dna.pt2, scalp_dna_scan_bs(&dna.pt2));

		// scannings are finished, so release the channel
		scalp_dpt_unlock(&dna.interf);

		// save BC info in registered node list
		DNA_BC_ADDR(dna.list) = DNA_SELF_ADDR(dna.list);
		dna.list[DNA_BC].type = DNA_BC;

		// accept general calls to allow the IS to register
		scalp_dpt_gen_call(OK);

		// BC behaviour handling
		PT_INIT(&dna.pt2);
		PT_INIT(&dna.pt3);
		dna.index = DNA_LIST_SIZE;
		PT_WAIT_WHILE(&dna.pt, scalp_dna_bc() == OK);
	}
	// else acting as an IS
	else {
		// scan for a free I2C address
		PT_SPAWN(&dna.pt, &dna.pt2, scalp_dna_scan_free(&dna.pt2, 0));

		// reset retries counter
		dna.tmp = 0;

		// for the IS, it is needed to register
		PT_SPAWN(&dna.pt, &dna.pt2, scalp_dna_is_reg(&dna.pt2));

		// whenever IS is registered or not, release the channel
		scalp_dpt_unlock(&dna.interf);

		// IS behaviour handling
		PT_SPAWN(&dna.pt, &dna.pt2, scalp_dna_is(&dna.pt2));
	}

	PT_END(&dna.pt);
}
