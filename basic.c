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

#include "scalp/basic.h"

#include "scalp/dispatcher.h"

#include <avr/pgmspace.h>
#include <string.h>

#include "drivers/eeprom.h"

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
	pt_t	in_pt;						// proto-thread context
	fifo_t	in_fifo;					// fifo
	dpt_frame_t in_buf[NB_IN_FRAMES];	// buffer

	// for response frames
	pt_t	out_pt;						// context
	fifo_t	out_fifo;					// fifo
	dpt_frame_t out_buf[NB_OUT_FRAMES];	// buffer
	dpt_frame_t out;
} BSC;


//----------------------------------------
// private functions
//

// frame handling
//static void BSC_frame_handling(dpt_frame_t* fr)
void BSC_frame_handling(dpt_frame_t* fr)
{
	u8 i;
	dpt_frame_t resp;		// response frame
	dpt_frame_t cont_fr;	// contained frame
	u16* addr;
	u16 data = 0;
	u32 time, delay;

	// if receiving a response or an error frame
	if ( fr->resp | fr->error ) {
		// ignore it
		return;
	}

	// build response frame header
	resp.dest = fr->orig;
	resp.orig = fr->dest;
	resp.t_id = fr->t_id;
	resp.resp = 1;
	resp.error = fr->error;
	resp.cmde = fr->cmde;
	resp.eth = fr->eth;
	resp.serial = fr->serial;

	// and copy the 2 first argv values (in most cases, it's a time win)
	resp.argv[0] = fr->argv[0];
	resp.argv[1] = fr->argv[1];

	// extract address (in most frames, the 2 first argv are an u16)
	addr = (u16*)( (u16)(fr->argv[0] << 8) + fr->argv[1] );

	switch (fr->cmde) {
		case FR_NO_CMDE:
			// nothing to do
			break;

		case FR_RAM_READ:
			// read data
			data = *addr;

			break;

		case FR_RAM_WRITE:
			// extract data
			data = (fr->argv[2] << 8) + fr->argv[3];

			// write data
			*addr = data;

			// read back data
			data = *addr;
			break;

		case FR_EEP_READ:
			// read data
			EEP_read((u16)addr, (u8*)&data, sizeof(u16));
			break;

		case FR_EEP_WRITE:
			// extract data
			data = (fr->argv[2] << 8) + fr->argv[3];

			// write data
			EEP_write((u16)addr, (u8*)&data, sizeof(u16));

			// check if writing is done
			while ( OK != EEP_is_fini() )
				;

			// read back data
			EEP_read((u16)addr, (u8*)&data, sizeof(u16));
			break;

		case FR_FLH_READ:
			// read data
			data = pgm_read_word((u16)addr);
			break;

		case FR_FLH_WRITE:
			// TODO : difficult to handle correctly
			// right now, skip it by answering an error
			resp.error = 1;

			// extract address
			// write data
			// read back data
			// set response frame arguments
			break;

		case FR_WAIT:
			// get current time
			time = TIME_get();

			// compute time at end of delay
			delay = (u16)addr;
			delay *= TIME_1_MSEC;
			delay += time;
			BSC.time_out = delay;

			break;

		case FR_CONTAINER:
			// container frames can hold other containers
			// even if this seems useless
			// except perhaps for eeprom size optimization

			// upon the memory storage zone
			switch (fr->argv[3]) {
				case EEPROM_STORAGE:
					// for each frame in the container
					for ( i = 0; i < fr->argv[2]; i++) {
						// extract the frames from EEPROM
						EEP_read((u16)((u8*)addr + i * sizeof(dpt_frame_t)), (u8*)&cont_fr, sizeof(dpt_frame_t));

						// enqueue the contained frame
						FIFO_put(&BSC.out_fifo, &cont_fr);
					}
					break;

				case RAM_STORAGE:
					// for each frame in the container
					for ( i = 0; i < fr->argv[2]; i++) {
						// read the frame from RAM
						cont_fr = *((dpt_frame_t *)((u8*)addr + i * sizeof(dpt_frame_t)));

						// enqueue the contained frame
						FIFO_put(&BSC.out_fifo, &cont_fr);
					}
					break;

				case FLASH_STORAGE:
					// for each frame in the container
					for ( i = 0; i < fr->argv[2]; i++) {
						// extract the frame from FLASH
						memcpy_P(&cont_fr, (const void *)((u8*)addr + i * sizeof(dpt_frame_t)), sizeof(dpt_frame_t));

						// enqueue the contained frame
						FIFO_put(&BSC.out_fifo, &cont_fr);
					}
					break;

				case 0x00:
				case 0x01:
				case 0x02:
				case 0x03:
				case 0x04:
				case 0x05:
					// extract the frame from EEPROM
					EEP_read((u16)((u8*)addr + fr->argv[3] * sizeof(dpt_frame_t)), (u8*)&cont_fr, sizeof(dpt_frame_t));

					// enqueue the contained frame
					FIFO_put(&BSC.out_fifo, &cont_fr);
					break;

				default:
					// frame format is invalid
					resp.error = 1;
					break;
				}

			break;

		default:
			// should never happen
			// and in this case, ignore frame
			return;
			break;
	}

	// set response frame arguments
	// in most frames, it's a read u16
	resp.argv[2] = (data & 0xff00) >> 8;
	resp.argv[3] = (data & 0x00ff) >> 0;

	// enqueue response
	FIFO_put(&BSC.out_fifo, &resp);
}


PT_THREAD(BSC_in(pt_t* pt))
{
	dpt_frame_t fr;

	PT_BEGIN(pt);

	// dequeue the incomed frame if any
	PT_WAIT_WHILE(pt, KO == FIFO_get(&BSC.in_fifo, &fr));

	// frame interpretation
	BSC_frame_handling(&fr);

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

	// if no more response is available
	if ( FIFO_full(&BSC.out_fifo) == 0 ) {
		// unlock the dispatcher
		DPT_unlock(&BSC.interf);
	}

	// dequeue the response frame if any
	PT_WAIT_WHILE(pt, KO == FIFO_get(&BSC.out_fifo, &BSC.out));

	// if the last handled frame was a wait one
	// no other one will be sent before the time-out elapses
	PT_WAIT_WHILE(pt, 0 != BSC.time_out);

	// be sure the response is sent
	PT_WAIT_WHILE(pt, KO == DPT_tx(&BSC.interf, &BSC.out));

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
	dpt_frame_t fr;

	// fifoes init
	FIFO_init(&BSC.in_fifo, &BSC.in_buf, NB_IN_FRAMES, sizeof(dpt_frame_t));
	FIFO_init(&BSC.out_fifo, &BSC.out_buf, NB_OUT_FRAMES, sizeof(dpt_frame_t));

	// thread init
	PT_INIT(&BSC.in_pt);
	PT_INIT(&BSC.out_pt);

	// reset time-out
	BSC.time_out = 0;

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

	EEP_init();

	// read reset frame
	EEP_read(0x00, (u8*)&fr, sizeof(dpt_frame_t));

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
}
