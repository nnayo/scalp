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

#include "scalp.h"

#include "dispatcher.h"

#include <avr/pgmspace.h>
#include <string.h>

#include "drivers/eeprom.h"
#include "drivers/spi.h"
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
	struct scalp_dpt_interface interf;				// dispatcher interface
	u32	time_out;

	// for incoming frames
	pt_t	in_pt;						// context
	struct nnk_fifo	in_fifo;					// fifo
	struct scalp in_buf[NB_IN_FRAMES];		// buffer
	struct scalp in;

	// for response frames
	pt_t	out_pt;						// context
	struct nnk_fifo	out_fifo;					// fifo
	struct scalp out_buf[NB_OUT_FRAMES];		// buffer
	struct scalp out;

	// for frame handling
	pt_t	handling_pt;				// context
	struct scalp resp;						// response frame
	struct scalp cont;						// contained frame
	u8 i;								// container index
	u16* addr;							// address for read and write
	u16 data;							// read or written data
	u8 is_running:1;					// TRUE while processing a frame
} bsc;


//----------------------------------------
// private functions
//

// frame handling
//static
PT_THREAD(bsc_frame_handling(pt_t* pt))
{
	u32 time, delay;

	PT_BEGIN(pt);

	bsc.data = 0;
	bsc.is_running = TRUE;

	// if receiving a response or an error frame
	if ( bsc.in.resp || bsc.in.error ) {
		// ignore it
		bsc.is_running = FALSE;
		PT_EXIT(pt);
	}

	// build response frame
	bsc.resp.dest = bsc.in.orig;
	bsc.resp.orig = bsc.in.dest;
	bsc.resp.t_id = bsc.in.t_id;
	bsc.resp.resp = 1;
	bsc.resp.error = bsc.in.error;
	bsc.resp.cmde = bsc.in.cmde;
	bsc.resp.argv[0] = bsc.in.argv[0];
	bsc.resp.argv[1] = bsc.in.argv[1];
	bsc.resp.argv[2] = bsc.in.argv[2];
	bsc.resp.argv[3] = bsc.in.argv[3];
	bsc.resp.argv[4] = bsc.in.argv[4];
	bsc.resp.argv[5] = bsc.in.argv[5];

	// extract address (in most frames, the 2 first argv are an u16)
	bsc.addr = (u16*)( (u16)(bsc.in.argv[0] << 8) + bsc.in.argv[1] );

	switch (bsc.in.cmde) {
	case SCALP_NULL:
		// nothing to do
		break;

	case SCALP_RAMREAD:
		// read data
		bsc.data = *bsc.addr;
		bsc.resp.argv[2] = (bsc.data & 0xff00) >> 8;
		bsc.resp.argv[3] = (bsc.data & 0x00ff) >> 0;

		break;

	case SCALP_RAMWRITE:
		// extract data
		bsc.data = (bsc.in.argv[2] << 8) + bsc.in.argv[3];

		// write data
		*bsc.addr = bsc.data;

		// read back data
		bsc.data = *bsc.addr;
		bsc.resp.argv[2] = (bsc.data & 0xff00) >> 8;
		bsc.resp.argv[3] = (bsc.data & 0x00ff) >> 0;
		break;

	case SCALP_EEPREAD:
		// read data
		nnk_eep_read((u16)bsc.addr, (u8*)&bsc.data, sizeof(u16));

		// wait until reading is done
		PT_WAIT_UNTIL(pt, nnk_eep_is_fini());

		bsc.resp.argv[2] = (bsc.data & 0xff00) >> 8;
		bsc.resp.argv[3] = (bsc.data & 0x00ff) >> 0;
		break;

	case SCALP_EEPWRITE:
		// extract data
		bsc.data = (bsc.in.argv[2] << 8) + bsc.in.argv[3];

		// write data
		nnk_eep_write((u16)bsc.addr, (u8*)&bsc.data, sizeof(u16));

		// wait until writing is done
		PT_WAIT_UNTIL(pt, nnk_eep_is_fini());

		// read back data
		nnk_eep_read((u16)bsc.addr, (u8*)&bsc.data, sizeof(u16));

		// wait until reading is done
		PT_WAIT_UNTIL(pt, nnk_eep_is_fini());

		bsc.resp.argv[2] = (bsc.data & 0xff00) >> 8;
		bsc.resp.argv[3] = (bsc.data & 0x00ff) >> 0;
		break;

	case SCALP_FLHREAD:
		// read data
		bsc.data = pgm_read_word((u16)bsc.addr);
		bsc.resp.argv[2] = (bsc.data & 0xff00) >> 8;
		bsc.resp.argv[3] = (bsc.data & 0x00ff) >> 0;
		break;

	case SCALP_FLHWRITE:
		// TODO : difficult to handle correctly
		// right now, skip it by answering an error
		bsc.resp.error = 1;

		// extract address
		// write data
		// read back data
		// set response frame arguments
		break;

//	case SCALP_SPI_READ:
//		// only read data
//		SPI_master(NULL, 0, bsc.resp.argv, bsc.in.len);
//
//		// wait until reading is done
//		PT_WAIT_UNTIL(pt, SPI_is_fini());
//
//		// check the transfer
//		bsc.resp.error = SPI_is_ok();
//		break;
//
//	case SCALP_SPI_WRITE:
//		// only write data
//		SPI_master(bsc.in.argv, bsc.in.len, NULL, 0);
//
//		// wait until writing is done
//		PT_WAIT_UNTIL(pt, SPI_is_fini());
//
//		// check the transfer
//		bsc.resp.error = SPI_is_ok();
//		break;

	case SCALP_WAIT:
		// get current time
		time = nnk_time_get();

		// compute time at end of delay
		delay = (u16)bsc.addr;
		delay *= TIME_1_MSEC;
		delay += time;
		bsc.time_out = delay;

		break;

	case SCALP_CONTAINER:
		// container frames can hold other containers
		// even if this seems useless
		// except perhaps for eeprom size optimization

		// upon the memory storage zone
		switch (bsc.in.argv[3]) {
		case SCALP_CONT_EEPROM_STORAGE:
			// for each frame in the container
			for ( bsc.i = 0; bsc.i < bsc.in.argv[2]; bsc.i++) {
				// extract the frames from EEPROM
				nnk_eep_read((u16)((u8*)bsc.addr + bsc.i * sizeof(struct scalp)), (u8*)&bsc.cont, sizeof(struct scalp));

				// wait until reading is done
				PT_WAIT_UNTIL(pt, nnk_eep_is_fini());

				// enqueue the contained frame
				PT_WAIT_UNTIL(pt, nnk_fifo_put(&bsc.out_fifo, &bsc.cont));
			}
			break;

		case SCALP_CONT_RAM_STORAGE:
			// for each frame in the container
			for ( bsc.i = 0; bsc.i < bsc.in.argv[2]; bsc.i++) {
				// read the frame from RAM
				bsc.cont = *((struct scalp *)((u8*)bsc.addr + bsc.i * sizeof(struct scalp)));

				// enqueue the contained frame
				PT_WAIT_UNTIL(pt, nnk_fifo_put(&bsc.out_fifo, &bsc.cont));
			}
			break;

		case SCALP_CONT_FLASH_STORAGE:
			// for each frame in the container
			for ( bsc.i = 0; bsc.i < bsc.in.argv[2]; bsc.i++) {
				// extract the frame from FLASH
				memcpy_P(&bsc.cont, (const void *)((u8*)bsc.addr + bsc.i * sizeof(struct scalp)), sizeof(struct scalp));

				// enqueue the contained frame
				PT_WAIT_UNTIL(pt, nnk_fifo_put(&bsc.out_fifo, &bsc.cont));
			}
			break;

		case SCALP_CONT_PRE_0_STORAGE:
		case SCALP_CONT_PRE_1_STORAGE:
		case SCALP_CONT_PRE_2_STORAGE:
		case SCALP_CONT_PRE_3_STORAGE:
		case SCALP_CONT_PRE_4_STORAGE:
		case SCALP_CONT_PRE_5_STORAGE:
		case SCALP_CONT_PRE_6_STORAGE:
		case SCALP_CONT_PRE_7_STORAGE:
		case SCALP_CONT_PRE_8_STORAGE:
		case SCALP_CONT_PRE_9_STORAGE:
			// extract the frame from EEPROM
                        bsc.addr = (u16*)(bsc.in.argv[3] * sizeof(struct scalp));
			nnk_eep_read((u16)((u8*)bsc.addr), (u8*)&bsc.cont, sizeof(struct scalp));

			// wait until reading is done
			PT_WAIT_UNTIL(pt, nnk_eep_is_fini());

			// enqueue the contained frame
			PT_WAIT_UNTIL(pt, nnk_fifo_put(&bsc.out_fifo, &bsc.cont));
			break;

		default:
			// frame format is invalid
			bsc.resp.error = 1;
			break;
		}

		break;

	default:
		// should never happen
		// and in this case, ignore frame
		bsc.is_running = FALSE;
		//PT_RESTART(pt);
		PT_EXIT(pt);
		break;
	}

	// enqueue response
	PT_WAIT_UNTIL(pt, nnk_fifo_put(&bsc.out_fifo, &bsc.resp));

	// let's process the next frame
	bsc.is_running = FALSE;
	PT_EXIT(pt);

	PT_END(pt);
}


PT_THREAD(bsc_in(pt_t* pt))
{
	PT_BEGIN(pt);

	// dequeue the incomed frame if any
	PT_WAIT_WHILE(pt, KO == nnk_fifo_get(&bsc.in_fifo, &bsc.in));

	// frame interpretation
	PT_SPAWN(pt, &bsc.handling_pt, bsc_frame_handling(&bsc.handling_pt));

	// if the last handled frame was a wait one
	// no other one will be treated before the time-out elapses
	PT_WAIT_WHILE(pt, (0 != bsc.time_out) && (nnk_time_get() <= bsc.time_out));

	// reset time-out was elapsed
	// this will unblock response sending
	bsc.time_out = 0;

	// let's process the next frame
	PT_RESTART(pt);

	PT_END(pt);
}


PT_THREAD(bsc_out(pt_t* pt))
{
	PT_BEGIN(pt);

	// if the last handled frame was a wait one
	// no other one will be sent before the time-out elapses
	PT_WAIT_WHILE(pt, 0 != bsc.time_out);

	// dequeue the response frame if any
	PT_WAIT_WHILE(pt, KO == nnk_fifo_get(&bsc.out_fifo, &bsc.out));

	// be sure the response is sent
        scalp_dpt_lock(&bsc.interf);
	if ( KO == scalp_dpt_tx(&bsc.interf, &bsc.out) ) {
		// else requeue the frame
		nnk_fifo_unget(&bsc.out_fifo, &bsc.out);
	}

	// let's wait the next response
	PT_RESTART(pt);

	PT_END(pt);
}


//----------------------------------------
// public functions
//

// basic module initialization
void scalp_bsc_init(void)
{
	struct scalp fr;

	// fifoes init
	nnk_fifo_init(&bsc.in_fifo, &bsc.in_buf, NB_IN_FRAMES, sizeof(struct scalp));
	nnk_fifo_init(&bsc.out_fifo, &bsc.out_buf, NB_OUT_FRAMES, sizeof(struct scalp));

	// thread init
	PT_INIT(&bsc.in_pt);
	PT_INIT(&bsc.out_pt);

	// reset time-out
	bsc.time_out = 0;
	bsc.is_running = FALSE;

	// register own call-back for specific commands
	bsc.interf.channel = 0;
    bsc.interf.cmde_mask = 
        _CM(SCALP_NULL)
        | _CM(SCALP_RAMREAD)
        | _CM(SCALP_RAMWRITE)
        | _CM(SCALP_EEPREAD)
        | _CM(SCALP_EEPWRITE)
        | _CM(SCALP_FLHREAD)
        | _CM(SCALP_FLHWRITE)
        //				| _CM(SCALP_SPI_READ)
        //				| _CM(SCALP_SPI_WRITE)
        | _CM(SCALP_WAIT)
        | _CM(SCALP_CONTAINER);

	bsc.interf.queue = &bsc.in_fifo;
	scalp_dpt_register(&bsc.interf);

	// drivers init
	nnk_slp_init();
	nnk_eep_init();
	//SPI_init(SPI_MASTER, SPI_THREE, SPI_MSB, SPI_DIV_16);

	// read reset frame
	nnk_eep_read(0x00, (u8*)&fr, sizeof(struct scalp));
	while ( ! nnk_eep_is_fini() )
		;

	// check if the frame is valid
	if ( fr.dest == 0xff || fr.orig == 0xff || fr.cmde == 0xff || fr.status == 0xff ) {
		return;
	}

	// enqueue the reset frame
	nnk_fifo_put(&bsc.out_fifo, &fr);

	// lock the dispatcher to be able to treat the frame
	scalp_dpt_lock(&bsc.interf);
}


void scalp_bsc_run(void)
{
	// incoming frames handling
	(void)PT_SCHEDULE(bsc_in(&bsc.in_pt));

	// reponse frames handling
	(void)PT_SCHEDULE(bsc_out(&bsc.out_pt));

	// if all frames are handled
	if ( !bsc.is_running && ( nnk_fifo_full(&bsc.out_fifo) == 0 ) && ( nnk_fifo_full(&bsc.in_fifo) == 0 ) ) {
		// unlock the dispatcher
		scalp_dpt_unlock(&bsc.interf);
	}
}
