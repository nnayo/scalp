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

#include "basic.h"

#include "dispatcher.h"

#include <avr/pgmspace.h>
#include <string.h>

#include "drivers/eeprom.h"
#include "drivers/sleep.h"

#include "utils/time.h"
#include "utils/pt.h"
#include "utils/fifo.h"


//----------------------------------------
// private defines
//

#define NB_IN_FRAMES	3	// number of incoming frames

#define NB_OUT_FRAMES	7	// number of response frames


//----------------------------------------
// private variables
//

static struct {
	dpt_interface_t interf;				// dispatcher interface
	u32	time_out;

	// for incoming frames
	pt_t	in_pt;						// context
	fifo_t	in_fifo;					// fifo
	frame_t in_buf[NB_IN_FRAMES];		// buffer
	frame_t in;

	// for response frames
	pt_t	out_pt;						// context
	fifo_t	out_fifo;					// fifo
	frame_t out_buf[NB_OUT_FRAMES];		// buffer
	frame_t out;

	// for frame handling
	pt_t	handling_pt;				// context
	frame_t resp;						// response frame
	frame_t cont;						// contained frame
	u8 i;								// container index
	u16* addr;							// address for read and write
	u16 data;							// read or written data
	u8 is_running:1;					// TRUE while processing a frame
} BSC;


//----------------------------------------
// private functions
//

// frame handling
//static
PT_THREAD(BSC_frame_handling(pt_t* pt))
{
	u32 time, delay;

	PT_BEGIN(pt);

	BSC.data = 0;
	BSC.is_running = TRUE;

	// if receiving a response or an error frame
	if ( BSC.in.resp || BSC.in.error ) {
		// ignore it
		BSC.is_running = FALSE;
		PT_EXIT(pt);
	}

	// build response frame
	BSC.resp.dest = BSC.in.orig;
	BSC.resp.orig = BSC.in.dest;
	BSC.resp.t_id = BSC.in.t_id;
	BSC.resp.resp = 1;
	BSC.resp.error = BSC.in.error;
	BSC.resp.cmde = BSC.in.cmde;
	BSC.resp.eth = BSC.in.eth;
	BSC.resp.serial = BSC.in.serial;
	BSC.resp.argv[0] = BSC.in.argv[0];
	BSC.resp.argv[1] = BSC.in.argv[1];
	BSC.resp.argv[2] = BSC.in.argv[2];
	BSC.resp.argv[3] = BSC.in.argv[3];
	BSC.resp.argv[4] = BSC.in.argv[4];
	BSC.resp.argv[5] = BSC.in.argv[5];

	// extract address (in most frames, the 2 first argv are an u16)
	BSC.addr = (u16*)( (u16)(BSC.in.argv[0] << 8) + BSC.in.argv[1] );

	switch (BSC.in.cmde) {
		case FR_NO_CMDE:
			// nothing to do
			break;

		case FR_RAM_READ:
			// read data
			BSC.data = *BSC.addr;
			BSC.resp.argv[2] = (BSC.data & 0xff00) >> 8;
			BSC.resp.argv[3] = (BSC.data & 0x00ff) >> 0;

			break;

		case FR_RAM_WRITE:
			// extract data
			BSC.data = (BSC.in.argv[2] << 8) + BSC.in.argv[3];

			// write data
			*BSC.addr = BSC.data;

			// read back data
			BSC.data = *BSC.addr;
			BSC.resp.argv[2] = (BSC.data & 0xff00) >> 8;
			BSC.resp.argv[3] = (BSC.data & 0x00ff) >> 0;
			break;

		case FR_EEP_READ:
			// read data
			EEP_read((u16)BSC.addr, (u8*)&BSC.data, sizeof(u16));

			// wait until reading is done
			PT_WAIT_UNTIL(pt, EEP_is_fini());

			BSC.resp.argv[2] = (BSC.data & 0xff00) >> 8;
			BSC.resp.argv[3] = (BSC.data & 0x00ff) >> 0;
			break;

		case FR_EEP_WRITE:
			// extract data
			BSC.data = (BSC.in.argv[2] << 8) + BSC.in.argv[3];

			// write data
			EEP_write((u16)BSC.addr, (u8*)&BSC.data, sizeof(u16));

			// wait until writing is done
			PT_WAIT_UNTIL(pt, EEP_is_fini());

			// read back data
			EEP_read((u16)BSC.addr, (u8*)&BSC.data, sizeof(u16));

			// wait until reading is done
			PT_WAIT_UNTIL(pt, EEP_is_fini());

			BSC.resp.argv[2] = (BSC.data & 0xff00) >> 8;
			BSC.resp.argv[3] = (BSC.data & 0x00ff) >> 0;
			break;

		case FR_FLH_READ:
			// read data
			BSC.data = pgm_read_word((u16)BSC.addr);
			BSC.resp.argv[2] = (BSC.data & 0xff00) >> 8;
			BSC.resp.argv[3] = (BSC.data & 0x00ff) >> 0;
			break;

		case FR_FLH_WRITE:
			// TODO : difficult to handle correctly
			// right now, skip it by answering an error
			BSC.resp.error = 1;

			// extract address
			// write data
			// read back data
			// set response frame arguments
			break;

		case FR_WAIT:
			// get current time
			time = TIME_get();

			// compute time at end of delay
			delay = (u16)BSC.addr;
			delay *= TIME_1_MSEC;
			delay += time;
			BSC.time_out = delay;

			break;

		case FR_CONTAINER:
			// container frames can hold other containers
			// even if this seems useless
			// except perhaps for eeprom size optimization

			// upon the memory storage zone
			switch (BSC.in.argv[3]) {
				case EEPROM_STORAGE:
					// for each frame in the container
					for ( BSC.i = 0; BSC.i < BSC.in.argv[2]; BSC.i++) {
						// extract the frames from EEPROM
						EEP_read((u16)((u8*)BSC.addr + BSC.i * sizeof(frame_t)), (u8*)&BSC.cont, sizeof(frame_t));

						// wait until reading is done
						PT_WAIT_UNTIL(pt, EEP_is_fini());

						// enqueue the contained frame
						PT_WAIT_UNTIL(pt, FIFO_put(&BSC.out_fifo, &BSC.cont));
					}
					break;

				case RAM_STORAGE:
					// for each frame in the container
					for ( BSC.i = 0; BSC.i < BSC.in.argv[2]; BSC.i++) {
						// read the frame from RAM
						BSC.cont = *((frame_t *)((u8*)BSC.addr + BSC.i * sizeof(frame_t)));

						// enqueue the contained frame
						PT_WAIT_UNTIL(pt, FIFO_put(&BSC.out_fifo, &BSC.cont));
					}
					break;

				case FLASH_STORAGE:
					// for each frame in the container
					for ( BSC.i = 0; BSC.i < BSC.in.argv[2]; BSC.i++) {
						// extract the frame from FLASH
						memcpy_P(&BSC.cont, (const void *)((u8*)BSC.addr + BSC.i * sizeof(frame_t)), sizeof(frame_t));

						// enqueue the contained frame
						PT_WAIT_UNTIL(pt, FIFO_put(&BSC.out_fifo, &BSC.cont));
					}
					break;

				case PRE_0_STORAGE:
				case PRE_1_STORAGE:
				case PRE_2_STORAGE:
				case PRE_3_STORAGE:
				case PRE_4_STORAGE:
				case PRE_5_STORAGE:
					// extract the frame from EEPROM
					EEP_read((u16)((u8*)BSC.addr + BSC.in.argv[3] * sizeof(frame_t)), (u8*)&BSC.cont, sizeof(frame_t));

					// wait until reading is done
					PT_WAIT_UNTIL(pt, EEP_is_fini());

					// enqueue the contained frame
					PT_WAIT_UNTIL(pt, FIFO_put(&BSC.out_fifo, &BSC.cont));
					break;

				default:
					// frame format is invalid
					BSC.resp.error = 1;
					break;
				}

			break;

		default:
			// should never happen
			// and in this case, ignore frame
			BSC.is_running = FALSE;
			//PT_RESTART(pt);
			PT_EXIT(pt);
			break;
	}

	// enqueue response
	PT_WAIT_UNTIL(pt, FIFO_put(&BSC.out_fifo, &BSC.resp));

	// let's process the next frame
	BSC.is_running = FALSE;
	PT_EXIT(pt);

	PT_END(pt);
}


PT_THREAD(BSC_in(pt_t* pt))
{
	PT_BEGIN(pt);

	// dequeue the incomed frame if any
	PT_WAIT_WHILE(pt, KO == FIFO_get(&BSC.in_fifo, &BSC.in));

	// frame interpretation
	PT_SPAWN(pt, &BSC.handling_pt, BSC_frame_handling(&BSC.handling_pt));

	// if the last handled frame was a wait one
	// no other one will be treated before the time-out elapses
	PT_WAIT_WHILE(pt, (0 != BSC.time_out) && (TIME_get() <= BSC.time_out));

	// reset time-out was elapsed
	// this will unblock response sending
	BSC.time_out = 0;

	// let's process the next frame
	PT_RESTART(pt);

	PT_END(pt);
}


PT_THREAD(BSC_out(pt_t* pt))
{
	PT_BEGIN(pt);

	// if the last handled frame was a wait one
	// no other one will be sent before the time-out elapses
	PT_WAIT_WHILE(pt, 0 != BSC.time_out);

	// dequeue the response frame if any
	PT_WAIT_WHILE(pt, KO == FIFO_get(&BSC.out_fifo, &BSC.out));

	// be sure the response is sent
	if ( KO == DPT_tx(&BSC.interf, &BSC.out) ) {
		// else requeue the frame
		FIFO_unget(&BSC.out_fifo, &BSC.out);
	}

	// let's wait the next response
	PT_RESTART(pt);

	PT_END(pt);
}


//----------------------------------------
// public functions
//

// basic module initialization
void BSC_init(void)
{
	frame_t fr;

	// fifoes init
	FIFO_init(&BSC.in_fifo, &BSC.in_buf, NB_IN_FRAMES, sizeof(frame_t));
	FIFO_init(&BSC.out_fifo, &BSC.out_buf, NB_OUT_FRAMES, sizeof(frame_t));

	// thread init
	PT_INIT(&BSC.in_pt);
	PT_INIT(&BSC.out_pt);

	// reset time-out
	BSC.time_out = 0;
	BSC.is_running = FALSE;

	// register own call-back for specific commands
	BSC.interf.channel = 0;
	BSC.interf.cmde_mask = _CM(FR_NO_CMDE)
				| _CM(FR_RAM_READ)
				| _CM(FR_RAM_WRITE)
				| _CM(FR_EEP_READ)
				| _CM(FR_EEP_WRITE)
				| _CM(FR_FLH_READ)
				| _CM(FR_FLH_WRITE)
				| _CM(FR_WAIT)
				| _CM(FR_CONTAINER);
	BSC.interf.queue = &BSC.in_fifo;
	DPT_register(&BSC.interf);

	// drivers init
	SLP_init();
	EEP_init();

	// read reset frame
	EEP_read(0x00, (u8*)&fr, sizeof(frame_t));
	while ( ! EEP_is_fini() )
		;

	// check if the frame is valid
	if ( fr.dest == 0xff || fr.orig == 0xff || fr.cmde == 0xff || fr.status == 0xff ) {
		return;
	}

	// enqueue the reset frame
	FIFO_put(&BSC.out_fifo, &fr);

	// lock the dispatcher to be able to treat the frame
	DPT_lock(&BSC.interf);
}


void BSC_run(void)
{
	// incoming frames handling
	(void)PT_SCHEDULE(BSC_in(&BSC.in_pt));

	// reponse frames handling
	(void)PT_SCHEDULE(BSC_out(&BSC.out_pt));

	// if all frames are handled
	if ( !BSC.is_running && ( FIFO_full(&BSC.out_fifo) == 0 ) && ( FIFO_full(&BSC.in_fifo) == 0 ) ) {
		// unlock the dispatcher
		DPT_unlock(&BSC.interf);
	}
}
