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
//   |A|    |B|    |C|        |Y|    |Z|
//   \ /    \ /    \ / ...... \ /    \ /
//    |      |      |          |      |
// ---+------+------+---+------+------+---------- soft
//           appli fifo |  in fifo ^
//                    |   |        |
//                    |   |
//             out fifo |
//            ----------+---------- twi
//

#include "dispatcher.h"

#include "routing_tables.h"

#include "drivers/twi.h"

#include "utils/time.h"
#include "utils/fifo.h"
#include "utils/pt.h"

#include <avr/interrupt.h>	// cli()

#include <string.h>	// memset


//----------------------------------------
// private defines
//

#define NB_IN_FRAMES			(MAX_ROUTES / 2)
#define NB_OUT_FRAMES			(MAX_ROUTES / 2)
#define NB_APPLI_FRAMES			1


//----------------------------------------
// private variables
//

static struct {
        struct scalp_dpt_interface* channels[DPT_CHAN_NB];	// available channels
        u64 lock;				// lock bitfield

        pt_t appli_pt;				// appli thread
        struct nnk_fifo appli_fifo;
        struct scalp appli_buf[NB_APPLI_FRAMES];
        struct scalp appli;

        pt_t in_pt;				// in thread
        struct nnk_fifo in_fifo;
        struct scalp in_buf[NB_IN_FRAMES];
        struct scalp in;

        pt_t out_pt;				// out thread
        struct nnk_fifo out_fifo;
        struct scalp out_buf[NB_OUT_FRAMES];
        struct scalp out;
        struct scalp hard;
        volatile u8 hard_fini;

        u8 sl_addr;				// own I2C slave address
        u32 time_out;				// tx time-out time
        u8 t_id;				// current transaction id value
} dpt;


//----------------------------------------
// private functions
//

// dispatch the frame to each registered listener
static void scalp_dpt_dispatch(struct scalp* fr)
{
        u8 i;
        enum scalp_cmde cmde = fr->cmde;

        // for each registered commands ranges
        for (i = 0; i < DPT_CHAN_NB; i++) {
                // if channel is not registered
                if ( dpt.channels[i] == NULL )
                        // skip to following
                        continue;

                // if command is in a range
                // and if a queue is available
                if (dpt.channels[i]->cmde_mask & _CM(cmde) && dpt.channels[i]->queue)
                        // enqueue it
                        nnk_fifo_put(dpt.channels[i]->queue, fr);
                //		// if command is in a range
                //		if (dpt.channels[i]->cmde_mask & _CM(cmde))
                //			// enqueue it if a queue is available
                //			if (dpt.channels[i]->queue && (OK == nnk_fifo_put(dpt.channels[i]->queue, fr)))
                //				// if a success, lock the channel
                //				dpt.lock |= 1 << i;
        }
}


static PT_THREAD( scalp_dpt_appli(pt_t* pt) )
{
        struct scalp fr;
        u8 routes[MAX_ROUTES];
        u8 nb_routes;
        u8 i;

        PT_BEGIN(pt);

        // if any awaiting incoming frames
        PT_WAIT_UNTIL(pt, nnk_fifo_get(&dpt.appli_fifo, &fr));

        // route the frame
        ROUT_route(fr.dest, routes, &nb_routes);

        // no route
        if (nb_routes == 0) {
                // so we will send it unmodify
                // but tweecking the resulting route
                nb_routes = 1;
                routes[0] = fr.dest;
        }

        for (i = 0; i < nb_routes; i++) {
                fr.dest = routes[i];
                // if the frame destination is only local
                if ((fr.dest == DPT_SELF_ADDR) || (fr.dest == dpt.sl_addr)) {
                        nnk_fifo_put(&dpt.in_fifo, &fr);

                        // short cut the handling to speed up
                        break;
                }

                // if broadcasting
                if (fr.dest == DPT_BROADCAST_ADDR) {
                        // also goes to local node
                        nnk_fifo_put(&dpt.in_fifo, &fr);
                }

                // and finally goes to distant node
                fr.orig = dpt.sl_addr;
                nnk_fifo_put(&dpt.out_fifo, &fr);
        }

        // so loop back for the next frame
        PT_RESTART(pt);

        PT_END(pt);
}


static PT_THREAD( scalp_dpt_in(pt_t* pt) )
{
        struct scalp fr;

        PT_BEGIN(pt);

        // if any awaiting incoming frames
        PT_WAIT_UNTIL(pt, nnk_fifo_get(&dpt.in_fifo, &fr));

        // dispatch the frame
        scalp_dpt_dispatch(&fr);

        // the frame has been sent to its destination
        // so loop back for the next frame
        PT_RESTART(pt);

        PT_END(pt);
}


static PT_THREAD( scalp_dpt_out(pt_t* pt) )
{
        u8 twi_res;

        PT_BEGIN(pt);

        // if no twi transfer running
        if (dpt.hard_fini == OK) {
                // read any available frame
                PT_WAIT_UNTIL(pt, nnk_fifo_get(&dpt.out_fifo, &dpt.hard));

                // compute and save time-out limit
                // byte transmission is typically 100 us
                dpt.time_out = nnk_time_get() + TIME_1_MSEC * sizeof(struct scalp);

                // now a twi transfer shall begin
                dpt.hard_fini = KO;
        }

        // read from and write to an I2C component are handled specificly
        // the frame characteristics to correctly complete the fields of the response
        // in case of I2C read or write are taken from the dpt.hard frame
        switch (dpt.hard.cmde) {
        case SCALP_TWIREAD:
                twi_res = nnk_twi_ms_rx(dpt.hard.dest, dpt.hard.len, (u8*)&dpt.hard + SCALP_ARGV_OFFSET);
                break;

        case SCALP_TWIWRITE:
                twi_res = nnk_twi_ms_tx(dpt.hard.dest, dpt.hard.len, (u8*)&dpt.hard + SCALP_ARGV_OFFSET);
                break;

        default:
                twi_res = nnk_twi_ms_tx(dpt.hard.dest, sizeof(struct scalp) - SCALP_ORIG_OFFSET, (u8*)&dpt.hard + SCALP_ORIG_OFFSET);
                break;
        }

        // if the TWI is not able to sent the frame
        if (twi_res == KO) {
                // prevent time-out signalling
                dpt.time_out = TIME_MAX;

                // retry sending the frame
                PT_RESTART(pt);
        }

        // wait until the twi transfer is done
        PT_WAIT_UNTIL(pt, dpt.hard_fini != OK);

        // and loop back for another transfer
        PT_RESTART(pt);

        PT_END(pt);
}


// I2C reception call-back
static void scalp_dpt_twi_call_back(enum nnk_twi_state state, u8 nb_data, void* misc)
{
        (void)misc;

        // reset tx time-out because the driver is signalling an event
        dpt.time_out = TIME_MAX;

        // upon the state
        switch (state) {
        case TWI_NO_SL:
                // if the slave doesn't respond
                // whether the I2C address is free, so take it
                // or the slave has crached
                // whatever the problem, put a failed resp in rx frame
                // as the comm was locally initiated
                // all the fields are those of dpt.hard frame
                dpt.hard.orig = dpt.hard.dest;
                dpt.hard.dest = dpt.sl_addr;
                dpt.hard.resp = 1;
                dpt.hard.error = 1;

                // enqueue the response
                nnk_fifo_put(&dpt.in_fifo, &dpt.hard);

                // and stop the com
                nnk_twi_stop();
                dpt.hard_fini = OK;

                break;

        case TWI_MS_RX_END:
                // reading data ends
        case TWI_MS_TX_END:
                // writing data ends

                // simple I2C actions are directly handled
                // communications with other nodes will received a response later
                if ((dpt.hard.cmde == SCALP_TWIREAD) || (dpt.hard.cmde == SCALP_TWIWRITE)) {
                        // update header
                        dpt.hard.orig = dpt.hard.dest;
                        dpt.hard.dest = DPT_SELF_ADDR;
                        dpt.hard.resp = 1;
                        dpt.hard.error = 0;

                        // enqueue the response
                        nnk_fifo_put(&dpt.in_fifo, &dpt.hard);
                }

                // and stop the com
                nnk_twi_stop();
                dpt.hard_fini = OK;

                break;

        case TWI_SL_RX_BEGIN:
                // just provide a buffer to store the incoming frame
                // only the origin, the cmde/resp and the arguments are received
                dpt.hard_fini = KO;
                dpt.hard.dest = dpt.sl_addr;
                nnk_twi_sl_rx(sizeof(struct scalp) - SCALP_ORIG_OFFSET, (u8*)&dpt.hard + SCALP_ORIG_OFFSET);

                break;

        case TWI_SL_RX_END:
                // if the msg len is correct
                if (nb_data == (sizeof(struct scalp) - SCALP_ORIG_OFFSET))
                        // enqueue the response
                        nnk_fifo_put(&dpt.in_fifo, &dpt.hard);
                // else it is ignored

                // release the bus
                nnk_twi_stop();
                dpt.hard_fini = OK;

                break;

        case TWI_SL_TX_BEGIN:
                // don't want to send a single byte
                nnk_twi_sl_tx(0, NULL);

                break;

        case TWI_SL_TX_END:
                // the bus will be released by hardware
                break;

        case TWI_GENCALL_BEGIN:
                // just provide a buffer to store the incoming frame
                // only the origin, the cmde/resp and the arguments are received
                dpt.hard_fini = KO;
                dpt.hard.dest = dpt.sl_addr;
                nnk_twi_sl_rx(sizeof(struct scalp) - SCALP_ORIG_OFFSET, (u8*)&dpt.hard + SCALP_ORIG_OFFSET);

                break;

        case TWI_GENCALL_END:
                // if the msg len is correct
                if (nb_data == (sizeof(struct scalp) - SCALP_ORIG_OFFSET))
                        // enqueue the incoming frame
                        nnk_fifo_put(&dpt.in_fifo, &dpt.hard);
                // else it is ignored

                // release the bus
                nnk_twi_stop();
                dpt.hard_fini = OK;

                break;

        default:
                // error or time-out state
                dpt.hard.dest = dpt.sl_addr;
                dpt.hard.resp = 1;
                dpt.hard.time_out = 1;

                // enqueue the response
                nnk_fifo_put(&dpt.in_fifo, &dpt.hard);

                // and then release the bus
                nnk_twi_stop();
                dpt.hard_fini = OK;

                break;
        }
}


//----------------------------------------
// public functions
//

// dispatcher initialization
void scalp_dpt_init(void)
{
        u8 i;

        // channels and lock reset
        for (i = 0; i < DPT_CHAN_NB; i++)
                dpt.channels[i] = NULL;
        dpt.lock = 0;

        // appli thread init
        nnk_fifo_init(&dpt.appli_fifo, &dpt.appli_buf, NB_APPLI_FRAMES, sizeof(struct scalp));
        PT_INIT(&dpt.appli_pt);

        // in thread init
        nnk_fifo_init(&dpt.in_fifo, &dpt.in_buf, NB_IN_FRAMES, sizeof(struct scalp));
        PT_INIT(&dpt.in_pt);

        // out thread init
        dpt.sl_addr = DPT_SELF_ADDR;
        dpt.time_out = TIME_MAX;
        nnk_fifo_init(&dpt.out_fifo, &dpt.out_buf, NB_OUT_FRAMES, sizeof(struct scalp));
        PT_INIT(&dpt.out_pt);
        dpt.hard_fini = OK;

        // start TWI layer
        nnk_twi_init(scalp_dpt_twi_call_back, NULL);
}


// dispatcher time-out handling
void scalp_dpt_run(void)
{
        // if current time is above the computed time-out
        if ((nnk_time_get() > dpt.time_out) && (dpt.hard_fini != OK)) {
                cli();
                dpt.hard.time_out = 1;
                // fake an interrupt with twi layer error
                scalp_dpt_twi_call_back(TWI_ERROR, 0, NULL);
                sei();
        }

        (void)PT_SCHEDULE(scalp_dpt_out(&dpt.out_pt));
        (void)PT_SCHEDULE(scalp_dpt_in(&dpt.in_pt));
        (void)PT_SCHEDULE(scalp_dpt_appli(&dpt.appli_pt));
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
void scalp_dpt_register(struct scalp_dpt_interface* interf)
{
        u8 i;

        // check if interface is invalid
        if (interf == NULL) {
                // then quit immediatly
                return;
        }

        // check if channel is invalid
        if (interf->channel >= DPT_CHAN_NB) {
                // then quit immediatly with invalid channel
                interf->channel = 0xff;
                return;
        }

        // check if requested channel is free
        // else find and use the next free
        for (i = interf->channel; i < DPT_CHAN_NB; i++) {
                if (dpt.channels[i] == NULL) {
                        break;
                }
        }
        // if none free, return error (0xff)
        if (i == DPT_CHAN_NB ){
                interf->channel = 0xff;
                return;
        }

        // store interface for used channel
        dpt.channels[i] = interf;

        // set the available channel
        interf->channel = i;
}


void scalp_dpt_lock(struct scalp_dpt_interface* interf)
{
        // set the lock bit associated to the channel
        dpt.lock |= 1 << interf->channel;
}


void scalp_dpt_unlock(struct scalp_dpt_interface* interf)
{
        // reset the lock bit associated to the channel
        dpt.lock &= ~(1 << interf->channel);
}


u8 scalp_dpt_tx(struct scalp_dpt_interface* interf, struct scalp* fr)
{
        u8 i;

        // if the tx is locked by a channel of higher priority
        for (i = 0; (i < DPT_CHAN_NB) && (i < interf->channel); i++) {
                if ( dpt.lock & (1 << i) ) {
                        // the sender shall retry
                        // so return KO
                        return KO;
                }
        }

        // if the sender didn't lock the channel
        if (!(dpt.lock & (1 << interf->channel))) {
                // it can't send the frame
                return KO;
        }

        // if the frame is not a response
        if (!fr->resp) {
                // increment transaction id
                dpt.t_id++;

                // and set it in the current frame
                fr->t_id = dpt.t_id;
        }

        // try to enqueue the frame
        return nnk_fifo_put(&dpt.appli_fifo, fr);
}


void scalp_dpt_sl_addr_set(u8 addr)
{
        // save slave address
        dpt.sl_addr = addr;

        // set slave address at TWI level
        nnk_twi_sl_addr_set(addr);
}


void scalp_dpt_gen_call(u8 flag)
{
        // just set the general call recognition mode
        nnk_twi_gen_call(flag);
}
