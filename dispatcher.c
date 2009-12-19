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

// design
//
// the software emission fifo is one frame deep only
// and is filled by the applications upon locking their channel.
// a thread is in charge of routing the frame placed in this fifo.
// if enough place is available in the emission fifoes,
// the frame is routed and place in these fifoes,
// else it is re-queued.
//
// on reception, the frame is enqueued in a reception fifo.
// this fifo is proceeded by a thread.
//
//
// nice-to-have
//
// routing tables to completely abstract the node destination
// from the application point of view.
// node disconnection tolerance by dynamic re-routing
//
//   /A\    /B\    /C\        /Y\    /Z\
//   \ /    \ /    \ / ...... \ /    \ /
//    |      |      |          |      |
// ---+------+------+---+------+------+---------- soft
//           appli fifo |  in fifo ^
//                    /   \        |
//                    \   /
//             out fifo |
//            ----------+---------- twi
//

#include "scalp/dispatcher.h"

#include "scalp/routing_tables.h"

#include "drivers/twi.h"

#include "utils/time.h"
#include "utils/fifo.h"
#include "utils/pt.h"

#include <avr/interrupt.h>	// cli()

#include <string.h>	// memset


//----------------------------------------
// private defines
//

#define DPT_FRAME_DEST_OFFSET	0
#define DPT_FRAME_ORIG_OFFSET	1
#define DPT_FRAME_T_ID_OFFSET	2
#define DPT_FRAME_CMDE_OFFSET	3
#define DPT_FRAME_ARGV_OFFSET	4

#define NB_IN_FRAMES			MAX_ROUTES
#define NB_OUT_FRAMES			MAX_ROUTES
#define NB_APPLI_FRAMES			1


//----------------------------------------
// private variables
//

static struct {
	dpt_interface_t* channels[DPT_CHAN_NB];	// available channels
	u64 lock;								// lock bitfield

	pt_t appli_pt;							// appli thread
	fifo_t appli_fifo;
	dpt_frame_t appli_buf[NB_APPLI_FRAMES];
	dpt_frame_t appli;

	pt_t in_pt;								// in thread
	fifo_t in_fifo;
	dpt_frame_t in_buf[NB_IN_FRAMES];
	dpt_frame_t in;	

	pt_t out_pt;							// out thread
	fifo_t out_fifo;
	dpt_frame_t out_buf[NB_OUT_FRAMES];
	dpt_frame_t out;
	dpt_frame_t hard;
	volatile u8 hard_fini;

	u8 sl_addr;								// own I2C slave address
	u32 time_out;							// tx time-out time
	u8 t_id;								// current transaction id value
} DPT;


//----------------------------------------
// private functions
//

// dispatch the frame to each registered listener
static void DPT_dispatch(dpt_frame_t* fr)
{
	u8 i;
	fr_cmdes_t cmde = fr->cmde;
	union {
		struct {
			u32 hi;
			u32 lo;
		};
		u64 raw;
	} mask;	// temporary variable to work-around a avr-gcc bug with 64bit

	// for each registered commands ranges
	for (i = 0; i < DPT_CHAN_NB; i++) {
		// if channel is not registered
		if ( DPT.channels[i] == NULL )
			// skip to following
			continue;

		// if command is in a range
		//if ( DPT.channels[i]->cmde_mask & _CM(cmde) ) {
		mask.raw = DPT.channels[i]->cmde_mask;
		mask.raw = mask.raw & _CM(cmde);
		if ( mask.hi || mask.lo ) {
			// enqueue it
			if ( OK == FIFO_put(DPT.channels[i]->queue, fr) ) {
				// if a success, lock the channel
				DPT.lock |= 1 << i;
			}
		}
	}
}


static PT_THREAD( DPT_appli(pt_t* pt) )
{
	dpt_frame_t fr;
	u8 routes[MAX_ROUTES];
	u8 nb_routes;
	u8 i;

	PT_BEGIN(pt);

	// if any awaiting incoming frames
	PT_WAIT_UNTIL(pt, FIFO_get(&DPT.appli_fifo, &fr));

	// route the frame
	ROUT_route(fr.dest, routes, &nb_routes);

	// no route
	if ( nb_routes == 0 ) {
		// so we will send it unmodify
		// but twicking the resulting route
		nb_routes = 1;
		routes[0] = fr.dest;
	}

	for ( i = 0; i < nb_routes; i++ ) {
		fr.dest = routes[i];
		// if the frame destination is only local
		if ( (fr.dest == DPT_SELF_ADDR) || (fr.dest == DPT.sl_addr) ) {
			FIFO_put(&DPT.in_fifo, &fr);

			// short cut the handling to speed up
			break;
		}

		// if broadcasting
		if (fr.dest == DPT_BROADCAST_ADDR) {
			// also goes to local node
			FIFO_put(&DPT.in_fifo, &fr);
		}

		// and finally goes to distant node
		FIFO_put(&DPT.out_fifo, &fr);
	}

	// so loop back for the next frame
	PT_RESTART(pt);

	PT_END(pt);
}


static PT_THREAD( DPT_in(pt_t* pt) )
{
	dpt_frame_t fr;

	PT_BEGIN(pt);

	// if any awaiting incoming frames
	PT_WAIT_UNTIL(pt, FIFO_get(&DPT.in_fifo, &fr));

	// dispatch the frame
	DPT_dispatch(&fr);

	// the frame has been sent to its destination
	// so loop back for the next frame
	PT_RESTART(pt);

	PT_END(pt);
}


static PT_THREAD( DPT_out(pt_t* pt) )
{
	u8 twi_res;

	PT_BEGIN(pt);

	// if no twi transfer running
	if ( DPT.hard_fini == OK ) {
		// read any available frame
		PT_WAIT_UNTIL(pt, FIFO_get(&DPT.out_fifo, &DPT.hard));

		// compute and save time-out limit
		// byte transmission is typically 100 us
		DPT.time_out = TIME_get() + TIME_1_MSEC * sizeof(dpt_frame_t);

		// now a twi transfer shall begin
		DPT.hard_fini = KO;
	}

	// read from and write to an I2C component are handled specificly
	// the frame characteristics to correctly complete the fields of the response
	// in case of I2C read or write are taken from the DPT.hard frame
	switch ( DPT.hard.cmde ) {
		case FR_I2C_READ:
			twi_res = TWI_ms_rx(DPT.hard.dest, DPT.hard.argv[0], (u8*)&DPT.hard + DPT_FRAME_ARGV_OFFSET + 1);
			break;

		case FR_I2C_WRITE:
			twi_res = TWI_ms_tx(DPT.hard.dest, DPT.hard.argv[0], (u8*)&DPT.hard + DPT_FRAME_ARGV_OFFSET + 1);
			break;

		default:
			twi_res = TWI_ms_tx(DPT.hard.dest, sizeof(dpt_frame_t) - DPT_FRAME_ORIG_OFFSET, (u8*)&DPT.hard + DPT_FRAME_ORIG_OFFSET);
			break;
	}

	// if the TWI is not able to sent the frame
	if ( twi_res == KO ) {
		// prevent time-out signalling
		DPT.time_out = TIME_MAX;

		// retry sending the frame
		PT_RESTART(pt);
	}

	// wait until the twi transfer is done
	PT_WAIT_UNTIL(pt, DPT.hard_fini != OK);

	// and loop back for another transfer
	PT_RESTART(pt);

	PT_END(pt);
}


// I2C reception call-back
static void DPT_I2C_call_back(twi_state_t state, u8 nb_data, void* misc)
{
	(void)misc;

	// reset tx time-out because the driver is signalling an event
	DPT.time_out = TIME_MAX;

	// upon the state
	switch ( state ) {
		case TWI_NO_SL:
			// if the slave doesn't respond
			// whether the I2C address is free, so take it
			// or the slave has crached
			// whatever the problem, put a failed resp in rx frame
			// as the comm was locally initiated
			// all the fields are those of DPT.hard frame
			DPT.hard.dest = DPT.sl_addr;
			DPT.hard.resp = 1;
			DPT.hard.error = 1;

			// enqueue the response
			FIFO_put(&DPT.in_fifo, &DPT.hard);

			// and stop the com
			TWI_stop();
			DPT.hard_fini = OK;

			break;

		case TWI_MS_RX_END:
			// reading data ends
		case TWI_MS_TX_END:
			// writing data ends

			// simple I2C actions are directly handled
			// communications with other nodes will received a response later
			if ( (DPT.hard.cmde == FR_I2C_READ) || (DPT.hard.cmde == FR_I2C_WRITE) ) {
				// update header
				DPT.hard.orig = DPT.hard.dest;
				DPT.hard.dest = DPT_SELF_ADDR;
				DPT.hard.resp = 1;
				DPT.hard.error = 0;

				// enqueue the response
				FIFO_put(&DPT.in_fifo, &DPT.hard);
			}

			// and stop the com
			TWI_stop();
			DPT.hard_fini = OK;

			break;

		case TWI_SL_RX_BEGIN:
			// just provide a buffer to store the incoming frame
			// only the origin, the cmde/resp and the arguments are received
			DPT.hard_fini = KO;
			DPT.hard.dest = DPT.sl_addr;
			TWI_sl_rx(sizeof(dpt_frame_t) - DPT_FRAME_ORIG_OFFSET, (u8*)&DPT.hard + DPT_FRAME_ORIG_OFFSET);

			break;

		case TWI_SL_RX_END:
			// if the msg len is correct
			if ( nb_data == (sizeof(dpt_frame_t) - DPT_FRAME_ORIG_OFFSET)) {
				// enqueue the response
				FIFO_put(&DPT.in_fifo, &DPT.hard);
			}
			// else it is ignored

			// release the bus
			TWI_stop();
			DPT.hard_fini = OK;

			break;

		case TWI_SL_TX_BEGIN:
			// don't want to send a single byte
			TWI_sl_tx(0, NULL);

			break;

		case TWI_SL_TX_END:
			// release the bus
			TWI_stop();

			break;

		case TWI_GENCALL_BEGIN:
			// just provide a buffer to store the incoming frame
			// only the origin, the cmde/resp and the arguments are received
			DPT.hard_fini = KO;
			DPT.hard.dest = DPT.sl_addr;
			TWI_sl_rx(sizeof(dpt_frame_t) - DPT_FRAME_ORIG_OFFSET, (u8*)&DPT.hard + DPT_FRAME_ORIG_OFFSET);

			break;

		case TWI_GENCALL_END:
			// if the msg len is correct
			if ( nb_data == (sizeof(dpt_frame_t) - DPT_FRAME_ORIG_OFFSET)) {
				// enqueue the incoming frame
				FIFO_put(&DPT.in_fifo, &DPT.hard);
			}
			// else it is ignored

			// release the bus
			TWI_stop();
			DPT.hard_fini = OK;

			break;

		default:
			// error or time-out state
			DPT.hard.dest = DPT.sl_addr;
			DPT.hard.resp = 1;
			DPT.hard.time_out = 1;

			// enqueue the response
			FIFO_put(&DPT.in_fifo, &DPT.hard);

			// and then release the bus
			TWI_stop();
			DPT.hard_fini = OK;

			break;
	}
}


//----------------------------------------
// public functions
//

// dispatcher initialization
void DPT_init(void)
{
	// appli thread init
	FIFO_init(&DPT.appli_fifo, &DPT.appli_buf, NB_APPLI_FRAMES, sizeof(dpt_frame_t));
	PT_INIT(&DPT.appli_pt);

	// in thread init
	FIFO_init(&DPT.in_fifo, &DPT.in_buf, NB_IN_FRAMES, sizeof(dpt_frame_t));
	PT_INIT(&DPT.in_pt);

	// out thread init
	DPT.sl_addr = DPT_SELF_ADDR;
	DPT.time_out = TIME_MAX;
	FIFO_init(&DPT.out_fifo, &DPT.out_buf, NB_OUT_FRAMES, sizeof(dpt_frame_t));
	PT_INIT(&DPT.out_pt);
	DPT.hard_fini = OK;

	// start TWI layer
	TWI_init(DPT_I2C_call_back, NULL);
}


// dispatcher time-out handling
void DPT_run(void)
{
	// if current time is above the computed time-out
	if ( (TIME_get() > DPT.time_out) && (DPT.hard_fini != OK) ) {
		cli();
		DPT.hard.time_out = 1;
		// fake an interrupt with twi layer error
		DPT_I2C_call_back(TWI_ERROR, 0, NULL);
		sei();
	}

	(void)PT_SCHEDULE(DPT_out(&DPT.in_pt));
	(void)PT_SCHEDULE(DPT_in(&DPT.out_pt));
	(void)PT_SCHEDULE(DPT_appli(&DPT.appli_pt));
}


// dispatcher registering function
// the application needs to register itself to the dispatcher
// in order to be able to send and receive frames
//
// the parameter is an interface containing :
//  - the requested channel
//  - the command range that is used to transmit the received frame to the application : the low and high values are inclusive
//  - the write function is called by the dispatcher when a frame is received
//  - the status function is called to give the transmission status
//
// the available channel is directly set in the structure
// if it is 0xff, it means no more channel are available
void DPT_register(dpt_interface_t* interf)
{
	u8 i;

	// check if interface is invalid
	if ( interf == NULL ) {
		// then quit immediatly
		return;
	}

	// check if channel is invalid
	if ( interf->channel >= DPT_CHAN_NB ) {
		// then quit immediatly with invalid channel
		interf->channel = 0xff;
		return;
	}

	// check if requested channel is free
	// else find and use the next free
	for ( i = interf->channel; i < DPT_CHAN_NB; i++ ) {
		if ( DPT.channels[i] == NULL ) {
			break;
		}
	}
	// if none free, return error (0xff)
	if ( i == DPT_CHAN_NB ) {
		interf->channel = 0xff;
		return;
	}

	// store interface for used channel
	DPT.channels[i] = interf;

	// set the available channel
	interf->channel = i;
}


void DPT_lock(dpt_interface_t* interf)
{
	// set the lock bit associated to the channel
	DPT.lock |= 1 << interf->channel;
}


void DPT_unlock(dpt_interface_t* interf)
{
	// reset the lock bit associated to the channel
	DPT.lock &= ~(1 << interf->channel);
}


u8 DPT_tx(dpt_interface_t* interf, dpt_frame_t* fr)
{
	u8 i;

	// if the tx is locked by a channel of higher priority
	for ( i = 0; (i < DPT_CHAN_NB) && (i < interf->channel); i++ ) {
		if ( DPT.lock & (1 << i) ) {
			// the sender shall retry
			// so return KO
			return KO;
		}
	}

	// if the sender didn't lock the channel
	if ( !(DPT.lock & (1 << interf->channel)) ) {
		// it can't send the frame
		return KO;
	}

	// if the frame is not a response
	if ( !fr->resp ) {
		// increment transaction id
		DPT.t_id++;

		// and set it in the current frame
		fr->t_id = DPT.t_id;
	}

	// try to enqueue the frame
	return FIFO_put(&DPT.appli_fifo, fr);
}


void DPT_set_sl_addr(u8 addr)
{
	// save slave address
	DPT.sl_addr = addr;

	// set slave address at TWI level
	TWI_set_sl_addr(addr);
}


void DPT_gen_call(u8 flag)
{
	// just set the general call recognition mode
	TWI_gen_call(flag);
}
