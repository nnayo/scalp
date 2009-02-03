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
	pt_t pt;			// context

	dpt_frame_t twi;		// a buffer frame on twi side
	dpt_frame_t tty;		// a buffer frame on tty side

	u32 time_out;			// time-out in reception
} NAT;


//----------------------------------------
// private functions
//

static void NAT_twi_rx(dpt_frame_t* fr)
{
	// if frame is a NAT response
	//if ( (fr->cmde & (FR_NAT|FR_RESP)) == (FR_NAT|FR_RESP) ) {
	if ( fr->nat && fr->resp ) {
		// build tty response
		NAT.tty = *fr;
		//NAT.tty.cmde &= ~FR_NAT;	// suppress NAT bit
		NAT.tty.nat = 0;	// suppress NAT bit
		//printf("%7s", (char*)&NAT.tty);
		putchar(NAT.tty.dest);
		putchar(NAT.tty.orig);
		putchar( (NAT.tty.resp << 7) | (NAT.tty.error << 6) | (NAT.tty.nat << 5) | NAT.tty.cmde);
		putchar(NAT.tty.argv[0]);
		putchar(NAT.tty.argv[1]);
		putchar(NAT.tty.argv[2]);
		putchar(NAT.tty.argv[3]);
	}
}


//----------------------------------------
// public functions
//

// NAT module initialization
void NAT_init(void)
{
	// init context
	PT_INIT(&NAT.pt);

	// reset time-out
	NAT.time_out = 0;

	// init serial link
	RS_init(B4800);

	// register own dispatcher call-back for specific commands
	NAT.interf.channel = 5;
	NAT.interf.cmde_mask = 0xffffffff;	// accept all commands
	NAT.interf.rx = NAT_twi_rx;
	DPT_register(&NAT.interf);
}


// NAT run method
u8 NAT_run(void)
{
	int c = EOF;

	PT_BEGIN(&NAT.pt);

	// read tty command
	PT_WAIT_WHILE(&NAT.pt, EOF == (c = getchar()) );
	NAT.twi.dest = (u8)(c & 0xff);

	NAT.time_out = TIME_get() + 5 * TIME_1_MSEC;
	PT_WAIT_WHILE(&NAT.pt, (TIME_get() < NAT.time_out) && (EOF == (c = getchar())) );
	if ( TIME_get() >= NAT.time_out )
		PT_RESTART(&NAT.pt);

	NAT.twi.orig = (u8)(c & 0xff);

	NAT.time_out = TIME_get() + 5 * TIME_1_MSEC;
	PT_WAIT_WHILE(&NAT.pt, (TIME_get() < NAT.time_out) && (EOF == (c = getchar())) );
	if (c & 0x80)
		NAT.twi.resp = 1;
	else
		NAT.twi.resp = 0;
	if (c & 0x40)
		NAT.twi.error = 1;
	else
		NAT.twi.error = 0;
	/*if (c & 0x20)
		NAT.twi.nat = 1;
	else
		NAT.twi.nat = 0;*/
	NAT.twi.nat = 1;	// add NAT bit
	NAT.twi.cmde = (u8)(c & 0x1f);

	NAT.time_out = TIME_get() + 5 * TIME_1_MSEC;
	PT_WAIT_WHILE(&NAT.pt, (TIME_get() < NAT.time_out) && (EOF == (c = getchar())) );
	NAT.twi.argv[0] = (u8)(c & 0xff);

	NAT.time_out = TIME_get() + 5 * TIME_1_MSEC;
	PT_WAIT_WHILE(&NAT.pt, (TIME_get() < NAT.time_out) && (EOF == (c = getchar())) );
	NAT.twi.argv[1] = (u8)(c & 0xff);

	NAT.time_out = TIME_get() + 5 * TIME_1_MSEC;
	PT_WAIT_WHILE(&NAT.pt, (TIME_get() < NAT.time_out) && (EOF == (c = getchar())) );
	NAT.twi.argv[2] = (u8)(c & 0xff);

	NAT.time_out = TIME_get() + 5 * TIME_1_MSEC;
	PT_WAIT_WHILE(&NAT.pt, (TIME_get() < NAT.time_out) && (EOF == (c = getchar())) );
	NAT.twi.argv[3] = (u8)(c & 0xff);

	// send the command
	DPT_lock(&NAT.interf);
	PT_WAIT_WHILE(&NAT.pt, KO == DPT_tx(&NAT.interf, &NAT.twi));
	DPT_unlock(&NAT.interf);

	// loop back for processing next frame
	PT_RESTART(&NAT.pt);

	PT_END(&NAT.pt);
}
