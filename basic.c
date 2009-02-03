#include "scalp/basic.h"

#include "scalp/dispatcher.h"

#include <avr/eeprom.h>
#include <avr/pgmspace.h>
#include <string.h>

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
	dpt_interface_t interf;			// dispatcher interface
	u32	time_out;

	// for incoming frames
	pt_t	pt_in;				// proto-thread context
	fifo_t	fifo_in;			// fifo
	dpt_frame_t buf_in[NB_IN_FRAMES];	// buffer

	// for response frames
	pt_t	pt_out;				// context
	fifo_t	fifo_out;			// fifo
	dpt_frame_t buf_out[NB_OUT_FRAMES];	// buffer
	dpt_frame_t out;
} BSC;


//----------------------------------------
// private functions
//

// frame handling
static void BSC_frame_handling(dpt_frame_t* fr)
{
	u8 i;
	dpt_frame_t resp;	// response frame
	dpt_frame_t cont_fr;	// contained frame
	volatile u16* addr;
	u16 data = 0;
	u32 time, delay;

	// build response frame header
	resp.dest = fr->orig;
	resp.orig = fr->dest;
	resp.t_id = fr->t_id;
	resp.resp = 1;
	resp.error = fr->error;
	resp.nat = fr->nat;
	resp.cmde = fr->cmde;

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
			eeprom_read_block(&data, (const void *)addr, sizeof(u16));
			break;

		case FR_EEP_WRITE:
			// extract data
			data = (fr->argv[2] << 8) + fr->argv[3];

			// write data
			eeprom_write_block(&data, (void*)addr, sizeof(u16));

			// read back data
			eeprom_read_block(&data, (const void *)addr, sizeof(u16));
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

			// for each frame in the container
			for ( i = 0; i < fr->argv[2]; i++) {
				// extract the frame from EEPROM
				eeprom_read_block(&cont_fr, (const void *)((u8*)addr + i * sizeof(dpt_frame_t)), sizeof(dpt_frame_t));

				// enqueue the contained frame
				FIFO_put(&BSC.fifo_out, &cont_fr);
			}

			break;

		default:
			// should never happen
			// but in case, sent back an error response frame
			resp.error = 1;
			break;
	}

	// set response frame arguments
	// in most frames, it's a read u16
	resp.argv[2] = (data & 0xff00) >> 8;
	resp.argv[3] = (data & 0x00ff) >> 0;

	// enqueue response
	FIFO_put(&BSC.fifo_out, &resp);
}


// basic receive function
static void BSC_rx(dpt_frame_t* fr)
{
	// if receiving a response or an error frame
	if ( fr->resp | fr->error ) {
		// ignore it
		return;
	}

	// lock the dispatcher
	DPT_lock(&BSC.interf);

	// enqueue received frame
	FIFO_put(&BSC.fifo_in, fr);
}


PT_THREAD(BSC_in(pt_t* pt))
{
	dpt_frame_t fr;

	PT_BEGIN(pt);

	// dequeue the incomed frame if any
	PT_WAIT_WHILE(pt, KO == FIFO_get(&BSC.fifo_in, &fr));

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
	if ( FIFO_full(&BSC.fifo_out) == 0 ) {
		// unlock the dispatcher
		DPT_unlock(&BSC.interf);
	}

	// dequeue the response frame if any
	PT_WAIT_WHILE(pt, KO == FIFO_get(&BSC.fifo_out, &BSC.out));

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
	FIFO_init(&BSC.fifo_in, BSC.buf_in, NB_IN_FRAMES, sizeof(dpt_frame_t));
	FIFO_init(&BSC.fifo_out, BSC.buf_out, NB_OUT_FRAMES, sizeof(dpt_frame_t));

	// thread init
	PT_INIT(&BSC.pt_in);
	PT_INIT(&BSC.pt_out);

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
	BSC.interf.rx = BSC_rx;
	DPT_register(&BSC.interf);

	// read reset frame
	eeprom_read_block(&fr, (const void *)0x00, sizeof(dpt_frame_t));

	// enqueue the reset frame
	BSC_rx(&fr);
}


void BSC_run(void)
{
	// incoming frames handling
	(void)PT_SCHEDULE(BSC_in(&BSC.pt_in));

	// reponse frames handling
	(void)PT_SCHEDULE(BSC_out(&BSC.pt_out));
}
