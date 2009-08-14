// GPL v3 : copyright Yann GOUY
//

#include "scalp/time_sync.h"

#include "scalp/dispatcher.h"
#include "scalp/dna.h"

#include "utils/time.h"
#include "utils/pt.h"


//----------------------------------------
// private defines
//


//----------------------------------------
// private variables
//

static struct {
	dpt_interface_t interf;		// dispatcher interface

	pt_t pt;			// thread context

	dpt_frame_t fr;			// a buffer frame
	volatile u8 rxed;		// flag set to OK when frame is rxed

	u32 time_out;
	s8 time_correction;
} TSN;


//----------------------------------------
// private functions
//

static void TSN_dpt_rx(dpt_frame_t* fr)
{
	// if not a time response
	if ( !(fr->cmde == FR_TIME_GET) && fr->resp ) {
		// throw it away
		return;
	}

	// if frame buffer is free
	if ( TSN.rxed == KO ) {
		// prevent overwrite
		TSN.rxed = OK;

		// save incoming frame
		TSN.fr = *fr;
	}
}


static PT_THREAD( TSN_pt(pt_t* pt) )
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
	TSN.fr.resp = 0;
	TSN.fr.error = 0;
	TSN.fr.nat = 0;
	TSN.fr.cmde = FR_TIME_GET;

	// send the time request
	DPT_lock(&TSN.interf);
	PT_WAIT_UNTIL(pt, OK == DPT_tx(&TSN.interf, &TSN.fr));
	DPT_unlock(&TSN.interf);

	// wait for the answer
	PT_WAIT_UNTIL(pt, OK == TSN.rxed);

	// permit to receive another frame
	TSN.rxed = KO;

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
	TSN.rxed = KO;
	TSN.time_correction = 0;
	TSN.time_out = TIME_1_SEC;
	TIME_set_incr(10 * TIME_1_MSEC);

	// register own call-back for specific commands
	TSN.interf.channel = 8;
	TSN.interf.cmde_mask = _CM(FR_TIME_GET);
	TSN.interf.rx = TSN_dpt_rx;
	DPT_register(&TSN.interf);
}


// Time Synchro module run method
void TSN_run(void)
{
	// send response if any
	(void)PT_SCHEDULE(TSN_pt(&TSN.pt));
}
