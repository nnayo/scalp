//---------------------
//  Copyright (C) 2000-2008  <Yann GOUY>
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

#include "scalp/dna.h"

#include "scalp/dispatcher.h"

#include "utils/pt.h"		// PT_*
#include "utils/pt_sem.h"	// PT_SEM_*
#include "utils/time.h"		// TIME_*

#include <string.h>

//--------------------------------------
// private defines
//

#define PCA9540B_ADDR	0x70	// I2C mux


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
	dpt_interface_t interf;		// dispatcher interface

	pt_t pt;			// dna thread
	pt_t pt2;			// secondary thread (scan_free, scan_bs, is_reg)
	pt_t pt_list;			// list thread

	dna_list_t list[DNA_LIST_SIZE];	// list of the I2C connected components (BC, self then other IS)
	u8 nb_is;			// number of registered IS
	u8 nb_bs;			// number of discovered BS
	u8 index;			// index in current sending of the list

	dpt_frame_t in;			// incoming frame
	volatile u8 rxed;		// flag set to OK while the incoming frame is not treated
	dpt_frame_t out;		// out going frame

	u8 tmp;				// all purpose temporary buffer

	u32 time;			// temporary variable for saving retry registering time
} DNA;


//--------------------------------------
// private functions
//

// this protothread purpose is to scan the I2C bus
// to find a free address
// DNA.tmp is used to store the scanned I2C address
static PT_THREAD( dna_scan_free(pt_t* pt) )
{
	PT_BEGIN(pt);

	// set scanning start address
	DNA.tmp = DNA_I2C_ADDR_MIN;

	// scan all the IS address range
	while (1) {
		// send a frame to test if the I2C address is free
		DNA.out.orig = 0;
		DNA.out.dest = DNA.tmp;
		DNA.out.resp = 0;
		DNA.out.error = 0;
		DNA.out.nat = 0;
		DNA.out.cmde = FR_I2C_READ;

		// then send frame
		PT_WAIT_UNTIL(pt, DPT_tx(&DNA.interf, &DNA.out) == OK);

		// reset rx flag
		DNA.rxed = KO;

		// wait for the response
		PT_WAIT_UNTIL(pt, DNA.rxed == OK);

#if 0
		switch (DNA.in.cmde) {
			case FR_I2C_READ|FR_RESP|FR_ERROR:
				// a free address is found
				DNA_SELF_ADDR(DNA.list) = DNA.tmp;

				// the initial phase is finished
				// bring the node to partial capability
				DPT_set_sl_addr(DNA.tmp);
				PT_EXIT(pt);
				break;

			case FR_I2C_READ|FR_RESP:
				// the address is in use
				// try the next one
				DNA.tmp++;

				// if end of range is reached without a free slot
				if ( DNA.tmp > DNA_I2C_ADDR_MAX ) {
					// block there for ever
					PT_YIELD(pt);
				}
				break;

			default:
				// ignore any other frame
				break;
		}
#endif
		if ( (DNA.in.cmde == FR_I2C_READ) && DNA.in.resp && DNA.in.error ) {
			// a free address is found
			DNA_SELF_ADDR(DNA.list) = DNA.tmp;

			// the initial phase is finished
			// bring the node to partial capability
			DPT_set_sl_addr(DNA.tmp);
			PT_EXIT(pt);
		}

		if ( (DNA.in.cmde == FR_I2C_READ) && DNA.in.resp ) {
			// the address is in use
			// try the next one
			DNA.tmp++;

			// if end of range is reached without a free slot
			if ( DNA.tmp > DNA_I2C_ADDR_MAX ) {
				// block there for ever
				PT_YIELD(pt);
			}
		}
	}

	PT_END(pt);
}


// this protothread purpose is to scan the I2C bus
// to find every BS
// DNA.tmp is used to stored the scanned BS address
static PT_THREAD( dna_scan_bs(pt_t* pt) )
{
	PT_BEGIN(pt);

	// start the scanning range after the reserved address for local node
	DNA.tmp = DPT_FIRST_ADDR;

	// scan the whole I2C bus addresses range
	while (1) {
		// send a frame to test if the I2C address is free
		DNA.out.orig = 0;
		DNA.out.dest = DNA.tmp;
		DNA.out.resp = 0;
		DNA.out.error = 0;
		DNA.out.nat = 0;
		DNA.out.cmde = FR_I2C_READ;

		// reset rx flag
		DNA.rxed = KO;

		// then send frame
		PT_WAIT_UNTIL(pt, DPT_tx(&DNA.interf, &DNA.out) == OK);

		// wait for the response
		PT_WAIT_UNTIL(pt, DNA.rxed == OK);

		if ( (DNA.in.cmde == FR_I2C_READ) && DNA.in.resp && !DNA.in.error ) {
			// a new BS is found
			DNA.nb_bs++;

			// if there is still some place left
			if ( DNA.nb_is + DNA.nb_bs < DNA_LIST_SIZE - DNA_BC ) {
				// add it to the list from the end
				DNA.list[DNA_LIST_SIZE - DNA.nb_bs].i2c_addr = DNA.tmp;
				DNA.list[DNA_LIST_SIZE - DNA.nb_bs].type = DNA_BS;
			}
		}

		// try the next address
		DNA.tmp++;

		// if the address is in the IS range
		if ( DNA.tmp == DNA_I2C_ADDR_MIN) {
			// skip it
			DNA.tmp = DNA_I2C_ADDR_MAX + 1;
		}

		// if the address is the I2C mux (PCA9540B) one
		if ( DNA.tmp == PCA9540B_ADDR) {
			// skip it
			DNA.tmp++;
		}

		// if the end of I2C address range is reached
		if ( DNA.tmp > DPT_LAST_ADDR ) {
			// stop scanning the bus
			PT_EXIT(pt);
		}
	}

	PT_END(pt);
}


// this thread is ran only if the reg node list is to be updated
// it is restarted each time the BC signals a new node in the list
static PT_THREAD( dna_list(pt_t* pt) )
{
	PT_BEGIN(pt);

	// when the thread is call, DNA.index == DNA_BC
	// increment the index in the reg nodes list
	DNA.index++;

	// prebuild cmde header
	DNA.out.dest = DPT_BROADCAST_ADDR;
	DNA.out.orig = DNA_SELF_ADDR(DNA.list);
	DNA.out.resp = 0;
	DNA.out.error = 0;
	DNA.out.nat = 0;
	DNA.out.argv[3] = 0x00;
	
	if ( DNA.index == DNA_LIST_SIZE) {
		// once the update is finished
		// compose a FR_LIST command
		DNA.out.cmde = FR_LIST;
		DNA.out.argv[0] = DNA.nb_is;
		DNA.out.argv[1] = DNA.nb_bs;
		DNA.out.argv[2] = 0x00;

		// send the frame
		PT_WAIT_UNTIL(pt, DPT_tx(&DNA.interf, &DNA.out) == OK);

		// update is finished, so finish the thread
		PT_EXIT(pt);
	}
	else {
		// compose a FR_LINE command
		DNA.out.cmde = FR_LINE;
		DNA.out.argv[0] = DNA.index;
		DNA.out.argv[1] = DNA.list[DNA.index].type;
		DNA.out.argv[2] = DNA.list[DNA.index].i2c_addr;

		// send the frame
		PT_WAIT_UNTIL(pt, DPT_tx(&DNA.interf, &DNA.out) == OK);

		// some more lines have to be sent
		// loop back to begin
		PT_RESTART(pt);
	}

	PT_END(pt);
}


static PT_THREAD( dna_bc(pt_t* pt) )
{
	u8 addr;

	PT_BEGIN(pt);

	// wait incoming command
	PT_WAIT_UNTIL(pt, DNA.rxed == OK);

	// prebuild response header (code factorizing)
	DNA.out.dest = DNA.in.orig;
	DNA.out.orig = DNA_SELF_ADDR(DNA.list);
	DNA.out.resp = 1;
	DNA.out.error = 0;
	DNA.out.nat = DNA.in.nat;
	DNA.out.cmde = DNA.in.cmde;
	DNA.out.argv[0] = 0x00;
	DNA.out.argv[1] = 0x00;
	DNA.out.argv[2] = 0x00;
	DNA.out.argv[3] = 0x00;

	// lock the channel
	DPT_lock(&DNA.interf);

	switch (DNA.in.cmde) {
		case FR_REGISTER:
			// an IS is registering
			addr = DNA.in.orig;

			// check if there is some place left in the list
			if ( DNA.nb_is + 1 + DNA.nb_bs < DNA_LIST_SIZE - DNA_BC ) {
				// update the number of ISs
				DNA.nb_is++;

				// fill the registered node list
				DNA.list[DNA_BC + DNA.nb_is].i2c_addr = DNA.in.argv[0];
				DNA.list[DNA_BC + DNA.nb_is].type = DNA.in.argv[1];

				// and send back the REGISTER response
				PT_WAIT_UNTIL(pt, DPT_tx(&DNA.interf, &DNA.out) == OK);

				// prepare to send the new list to every node
				// list sending will start by the first line after the BC line
				DNA.index = DNA_BC;
				PT_SPAWN(pt, &DNA.pt_list, dna_list(&DNA.pt_list));
			}

			break;

		case FR_LIST:
			// an IS is asking for the list size
			// build the response
			DNA.out.argv[0] = DNA.nb_is;
			DNA.out.argv[1] = DNA.nb_bs;

			// then send it
			PT_WAIT_UNTIL(pt, DPT_tx(&DNA.interf, &DNA.out) == OK);
			break;

		case FR_LINE:
			// an IS is asking for a specific line
			// build the response
			DNA.out.argv[0] = DNA.in.argv[0];
			DNA.out.argv[1] = DNA.list[DNA.in.argv[0]].type;
			DNA.out.argv[2] = DNA.list[DNA.in.argv[0]].i2c_addr;

			// then send it
			PT_WAIT_UNTIL(pt, DPT_tx(&DNA.interf, &DNA.out) == OK);
			break;

		default:
			// ignore frame
			break;
	}

	// finally, unlock the channel
	DPT_unlock(&DNA.interf);

	// reset flag to be able to receive other messages
	DNA.rxed = KO;

	// loop back
	PT_RESTART(pt);

	PT_END(pt);
}


static PT_THREAD( dna_is_reg(pt_t* pt) )
{
	// for the IS, it is needed to register
	PT_BEGIN(pt);

	// reset rx flag
	DNA.rxed = KO;

	// compute the time-out time
	DNA.time = TIME_get() + TIME_1_MSEC * 10;

	// send the REGISTER cmde
	DNA.out.dest = DPT_BROADCAST_ADDR;
	DNA.out.orig = DNA_SELF_ADDR(DNA.list);
	DNA.out.cmde = FR_REGISTER;
	DNA.out.resp = 0;
	DNA.out.error = 0;
	DNA.out.nat = 0;
	DNA.out.argv[0] = DNA_SELF_ADDR(DNA.list);
	DNA.out.argv[1] = DNA_SELF_TYPE(DNA.list);
	PT_WAIT_UNTIL(pt, DPT_tx(&DNA.interf, &DNA.out) == OK );

	// wait for the time-out or the reception of the response
	PT_WAIT_UNTIL(pt, (TIME_get() >= DNA.time)||(DNA.rxed == OK) );

	// on time-out
	if ( DNA.rxed == KO ) {
		// one more try
		DNA.tmp++;

		// check if retries limit is reached
		if (DNA.tmp >= 5) {
			// send reconf bus force mode to NONE
			DNA.out.orig = DPT_SELF_ADDR;
			DNA.out.dest = DPT_SELF_ADDR;
			DNA.out.cmde = FR_RECONF_FORCE_MODE;
			DNA.out.resp = 0;
			DNA.out.error = 0;
			DNA.out.nat = 0;
			DNA.out.argv[0] = 0x00;	// set
			DNA.out.argv[1] = 0x02;	// NONE
			PT_WAIT_UNTIL(pt, DPT_tx(&DNA.interf, &DNA.out) == OK );

			// give up registering
			PT_EXIT(pt);
		}
		else {
			// else loop back to send the register command
			PT_RESTART(pt);
		}
	}

	// check whether the REGISTER resp is received
	if ( (DNA.in.cmde == FR_REGISTER) && DNA.in.resp ) {
		// registering is done
		// update reg nodes list
		DNA.list[DNA_BC].type = DNA_BC;
		DNA.list[DNA_BC].i2c_addr = DNA.in.orig;

		// allow general calls
		DPT_gen_call(OK);

		// init is finished
		PT_EXIT(pt);
	}

	PT_END(pt);
}


static PT_THREAD( dna_is(pt_t* pt) )
{
	PT_BEGIN(pt);

	// wait incoming commands
	PT_WAIT_UNTIL(pt, DNA.rxed == OK);

	switch (DNA.in.cmde) {
		case FR_LIST:
			// BC is signaling modification in the header of registered nodes list
			// update own list
			DNA.nb_is = DNA.in.argv[0];
			DNA.nb_bs = DNA.in.argv[1];

			break;

		case FR_LINE:
			// BC is signaling modification of registered nodes list
			// update own list
			DNA.list[DNA.in.argv[0]].type = DNA.in.argv[1];
			DNA.list[DNA.in.argv[0]].i2c_addr = DNA.in.argv[2];

			break;

		default:
			// ignore frame
			break;
	}

	// reset flag to allow reception of following frames
	DNA.rxed = KO;

	// loop back to wait for the next frames
	PT_RESTART(pt);

	PT_END(pt);
}


static void DNA_rx(dpt_frame_t* fr)
{
	// save received frame if none is in treatment
	if ( DNA.rxed != OK ) {
		DNA.in = *fr;
		DNA.rxed = OK;
	}
}


//--------------------------------------
// public variables
//




//--------------------------------------
// public functions
//

void DNA_init(dna_t mode)
{
	// reset the whole structure
	memset(&DNA, 0, sizeof(DNA));

	// init protothread
	PT_INIT(&DNA.pt);

	// save self config
	DNA_SELF_TYPE(DNA.list) = mode;

	// register to the dispatcher
	DNA.interf.channel = 2;
	DNA.interf.cmde_mask = _CM(FR_REGISTER) | _CM(FR_LIST) | _CM(FR_LINE) | _CM(FR_I2C_WRITE) | _CM(FR_I2C_READ);
	DNA.interf.rx = DNA_rx;
	DPT_register(&DNA.interf);

	// the channel will be locked
	// until the end of the scannings
	// or
	// until the IS is registered
	DPT_lock(&DNA.interf);

	// set rxed to prevent unexpected frame to be handled
	DNA.rxed = OK;
}


dna_list_t* DNA_list(u8* nb_is, u8* nb_bs)
{
	// update the number of ISs and BSs
	*nb_is = DNA.nb_is;
	*nb_bs = DNA.nb_bs;

	// return the whole list
	return DNA.list;
}


u8 DNA_run(void)
{
	PT_BEGIN(&DNA.pt);

	// scan for a free I2C address
	DNA.tmp = DNA_I2C_ADDR_MIN;
	PT_SPAWN(&DNA.pt, &DNA.pt2, dna_scan_free(&DNA.pt2));

	// when acting as a BC
	if ( DNA_SELF_TYPE(DNA.list) == DNA_BC ) {
		// find every BS
		PT_SPAWN(&DNA.pt, &DNA.pt2, dna_scan_bs(&DNA.pt2));

		// scannings are finished, so release the channel
		DPT_unlock(&DNA.interf);

		// save BC info in registered node list
		DNA_BC_ADDR(DNA.list) = DNA_SELF_ADDR(DNA.list);
		DNA.list[DNA_BC].type = DNA_BC;

		// accept general calls to allow the IS to register
		DPT_gen_call(OK);

		// BC behaviour handling
		PT_SPAWN(&DNA.pt, &DNA.pt2, dna_bc(&DNA.pt2));
	}
	// else acting as an IS
	else {
		// reset retries counter
		DNA.tmp = 0;

		// for the IS, it is needed to register
		PT_SPAWN(&DNA.pt, &DNA.pt2, dna_is_reg(&DNA.pt2));

		// whenever IS is registered or not, release the channel
		DPT_unlock(&DNA.interf);

		// IS behaviour handling
		PT_SPAWN(&DNA.pt, &DNA.pt2, dna_is(&DNA.pt2));
	}

	PT_END(&DNA.pt);
}
