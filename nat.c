#include "scalp/nat.h"

#include "scalp/dispatcher.h"

#include "drivers/rs.h"
#include "utils/pt.h"
#include "utils/time.h"

#include <string.h>

//----------------------------------------
// private defines
//


//----------------------------------------
// private variables
//

static struct {
	dpt_interface_t interf;		// dispatcher interface
	pt_t pt;					// context

	dpt_frame_t twi;			// a buffer frame on twi side
	dpt_frame_t tty;			// a buffer frame on tty side

	u32 time_out;				// time-out in reception

	u8 rxed;					// flag set on received frame
} NAT;


//----------------------------------------
// private functions
//

static PT_THREAD( NAT_tty_rx(pt_t* pt) )
{
	int c = EOF;

	PT_BEGIN(&NAT.pt);

	// read tty command
	// first char (dest) can be awaited infinitively
	PT_WAIT_WHILE(&NAT.pt, EOF == (c = getchar()) );
	NAT.twi.dest = (u8)(c & 0xff);

	// following char (orig) is subject of time-out
	NAT.time_out = TIME_get() + 5 * TIME_1_MSEC;
	PT_WAIT_WHILE(&NAT.pt, (TIME_get() < NAT.time_out) && (EOF == (c = getchar())) );

	NAT.twi.orig = (u8)(c & 0xff);

	// next char is also subject of time-out
	NAT.time_out += 5 * TIME_1_MSEC;
	PT_WAIT_WHILE(&NAT.pt, (TIME_get() < NAT.time_out) && (EOF == (c = getchar())) );
	NAT.twi.t_id = (u8)(c & 0xff);

	// next char is also subject of time-out
	NAT.time_out += 5 * TIME_1_MSEC;
	PT_WAIT_WHILE(&NAT.pt, (TIME_get() < NAT.time_out) && (EOF == (c = getchar())) );
	if (c & 0x80)
		NAT.twi.resp = 1;
	else
		NAT.twi.resp = 0;
	if (c & 0x40)
		NAT.twi.error = 1;
	else
		NAT.twi.error = 0;
	NAT.twi.nat = 1;	// force NAT bit
	NAT.twi.cmde = (u8)(c & 0x1f);

	// next char is also subject of time-out
	NAT.time_out += 5 * TIME_1_MSEC;
	PT_WAIT_WHILE(&NAT.pt, (TIME_get() < NAT.time_out) && (EOF == (c = getchar())) );
	NAT.twi.argv[0] = (u8)(c & 0xff);

	// next char is also subject of time-out
	NAT.time_out += 5 * TIME_1_MSEC;
	PT_WAIT_WHILE(&NAT.pt, (TIME_get() < NAT.time_out) && (EOF == (c = getchar())) );
	NAT.twi.argv[1] = (u8)(c & 0xff);

	// next char is also subject of time-out
	NAT.time_out += 5 * TIME_1_MSEC;
	PT_WAIT_WHILE(&NAT.pt, (TIME_get() < NAT.time_out) && (EOF == (c = getchar())) );
	NAT.twi.argv[2] = (u8)(c & 0xff);

	// next char is also subject of time-out
	NAT.time_out += 5 * TIME_1_MSEC;
	PT_WAIT_WHILE(&NAT.pt, (TIME_get() < NAT.time_out) && (EOF == (c = getchar())) );
	NAT.twi.argv[3] = (u8)(c & 0xff);

	// if a time-out happens
	if ( TIME_get() >= NAT.time_out ) {
		// loop back
		PT_RESTART(&NAT.pt);
	}

	// send the command
	DPT_lock(&NAT.interf);
	PT_WAIT_WHILE(&NAT.pt, KO == DPT_tx(&NAT.interf, &NAT.twi));
	DPT_unlock(&NAT.interf);

	// loop back for processing next frame
	PT_RESTART(&NAT.pt);

	PT_END(&NAT.pt);
}


static void NAT_tty_tx(void)
{
	// prepare received frame by suppressing NAT bit
	NAT.tty.nat = 0;

	// enqueue the response
	putchar(NAT.tty.dest);
	putchar(NAT.tty.orig);
	putchar(NAT.tty.t_id);
	putchar( (NAT.tty.resp << 7) | (NAT.tty.error << 6) | (NAT.tty.nat << 5) | NAT.tty.cmde);
	putchar(NAT.tty.argv[0]);
	putchar(NAT.tty.argv[1]);
	putchar(NAT.tty.argv[2]);
	putchar(NAT.tty.argv[3]);
}


static void NAT_twi_rx(dpt_frame_t* fr)
{
	// if NAT is not in reception state
	if ( NAT.rxed != KO ) {
		// ignore the frame
		return;
	}

	// if frame is not a NAT response
	if ( !(fr->nat && fr->resp) ) {
		// ignore it
		return;
	}

	// block any further reception
	NAT.rxed = OK;

	// save tty response
	NAT.tty = *fr;
}


//----------------------------------------
// public functions
//

// NAT module initialization
void NAT_init(void)
{
	// init context
	PT_INIT(&NAT.pt);

	// reset state
	NAT.time_out = 0;
	NAT.rxed = KO;

	// init serial link
	RS_init(B4800);

	// register own dispatcher call-back for specific commands
	NAT.interf.channel = 5;
	NAT.interf.cmde_mask = 0xffffffff;	// accept all commands
	NAT.interf.rx = NAT_twi_rx;
	DPT_register(&NAT.interf);
}


// NAT run method
void NAT_run(void)
{
	// handle the tty reception process
	(void)PT_SCHEDULE(NAT_tty_rx(&NAT.pt));

	// send back the frame
	if ( NAT.rxed ) {
#if 0
		// if the error frame flag is set
		if ( NAT.tty.error ) {
			// if transaction id is same in command and response
			if ( NAT.tty.t_id == NAT.twi.t_id ) {
				// send it
				NAT_tty_tx();

				// modify received transaction id to prevent further sending
				NAT.twi.t_id++;
			}
		}
		else {
			// if the received destination matches the source origin
			if ( NAT.tty.orig == NAT.twi.dest ) {
				// send it
				NAT_tty_tx();
			}
		}
#endif
		// if transaction id is same in command and response
		if ( NAT.tty.t_id == NAT.twi.t_id ) {
			// send it
			NAT_tty_tx();

			// modify received transaction id to prevent further sending
			//NAT.twi.t_id++;
		}

		// re-allow reception
		NAT.rxed = KO;
	}
}
