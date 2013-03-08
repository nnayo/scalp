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

#include "nat.h"

#include "dispatcher.h"

#ifdef NAT_ENABLE_RS
#include "drivers/rs.h"
#endif

#ifdef NAT_ENABLE_ETH
#include "externals/w5100.h"
#endif

#include "utils/pt.h"
#include "utils/time.h"
#include "utils/fifo.h"

#include <string.h>

//----------------------------------------
// private defines
//

#define QUEUE_SIZE		1

#define NAT_ETH_CMDE_PORT	((u16)7777)
#define NAT_ETH_CMDE_IP		((u32)0xc0a80701)		// 192.168.7.1



//----------------------------------------
// private variables
//

static struct {
	pt_t twi_in_pt;				// twi in part
	fifo_t twi_in_fifo;
	frame_t twi_in_buf[QUEUE_SIZE];
	dpt_interface_t interf;		// dispatcher interface
	frame_t twi_in;	

	pt_t twi_out_pt;			// twi out part
	fifo_t twi_out_fifo;
	frame_t twi_out_buf[QUEUE_SIZE];
	frame_t twi_out;

#ifdef NAT_ENABLE_ETH
	pt_t eth_in_pt;				// eth in part
	fifo_t eth_in_fifo;
	frame_t eth_in_buf[QUEUE_SIZE];
	frame_t eth_in;

	pt_t eth_out_pt;			// eth out part
	fifo_t eth_out_fifo;
	frame_t eth_out_buf[QUEUE_SIZE];
	frame_t eth_out;
#endif

#ifdef NAT_ENABLE_RS
	pt_t rs_in_pt;				// rx in part
	u32 time_out;				// time-out in reception
	fifo_t rs_in_fifo;
	frame_t rs_in_buf[QUEUE_SIZE];
	frame_t rs_in;

	pt_t rs_out_pt;
	fifo_t rs_out_fifo;
	frame_t rs_out_buf[QUEUE_SIZE];
	frame_t rs_out;
#endif
} NAT;


//----------------------------------------
// private functions
//

//----------------------------------------
// eth part
//

static PT_THREAD( NAT_twi_in(pt_t* pt) )
{
	PT_BEGIN(pt);

	// wait for a frame
	PT_WAIT_UNTIL(pt, FIFO_get(&NAT.twi_in_fifo, &NAT.twi_in));
	DPT_unlock(&NAT.interf);

#ifdef NAT_ENABLE_ETH
	// if the frame is for eth link
	if ( NAT.twi_in.eth ) {
		// suppress the eth flag
		NAT.twi_in.eth = 0;

		// send it via this link
		PT_WAIT_UNTIL(pt, FIFO_put(&NAT.eth_out_fifo, &NAT.twi_in));
	}
#endif
#ifdef NAT_ENABLE_RS
	// if the frame is for the serial link
#ifndef NAT_FORCE_RS
	if ( NAT.twi_in.serial ) {
#endif
		// suppress the serial flag
		NAT.twi_in.serial = 0;

		// send it via this link
		PT_WAIT_UNTIL(pt, FIFO_put(&NAT.rs_out_fifo, &NAT.twi_in));
#ifndef NAT_FORCE_RS
	}
#endif
#endif

	// loop back for processing next frame
	PT_RESTART(pt);

	PT_END(pt);
}


static PT_THREAD( NAT_twi_out(pt_t* pt) )
{
	PT_BEGIN(pt);

	// wait for a frame
	PT_WAIT_UNTIL(pt, FIFO_get(&NAT.twi_out_fifo, &NAT.twi_out));

	// send it via the dispatcher
	DPT_lock(&NAT.interf);
	PT_WAIT_UNTIL(pt, DPT_tx(&NAT.interf, &NAT.twi_out));
	DPT_unlock(&NAT.interf);

	// loop back for processing next frame
	PT_RESTART(pt);

	PT_END(pt);
}


#ifdef NAT_ENABLE_ETH
//----------------------------------------
// eth part
//

static PT_THREAD( NAT_eth_in(pt_t* pt) )
{
	PT_BEGIN(pt);

	// check if an ethernet command frame has arrived
	PT_WAIT_UNTIL(pt, W5100_rx(NAT_ETH_CMDE_PORT, (u8*)&NAT.eth_in, sizeof(NAT.eth_in)));

	// force eth bit
	NAT.eth_in.eth = 1;

	// try to give it to twi part
	PT_WAIT_UNTIL(pt, FIFO_put(&NAT.twi_out_fifo, &NAT.eth_in));

	// loop back for processing next frame
	PT_RESTART(pt);

	PT_END(pt);
}


static PT_THREAD( NAT_eth_out(pt_t* pt) )
{
	PT_BEGIN(pt);

	// wait for a frame
	PT_WAIT_UNTIL(pt, FIFO_get(&NAT.eth_out_fifo, &NAT.eth_out));

	// send the frame via the eth link
	PT_WAIT_UNTIL(pt, W5100_tx(NAT_ETH_CMDE_IP, NAT_ETH_CMDE_PORT, (u8*)&NAT.eth_out, sizeof(NAT.eth_out)));

	// loop back for processing next frame
	PT_RESTART(pt);

	PT_END(pt);
}
#endif


#ifdef NAT_ENABLE_RS
//----------------------------------------
// rs part
//

static PT_THREAD( NAT_rs_in(pt_t* pt) )
{
	int c = EOF;

	PT_BEGIN(pt);

	// read tty command
	// first char (dest) can be awaited infinitively
	PT_WAIT_WHILE(pt, EOF == (c = getchar()) );
	NAT.rs_in.dest = (u8)(c & 0xff);

	// following char (orig) is subject of time-out
	NAT.time_out = TIME_get() + 5 * TIME_1_MSEC;
	PT_WAIT_WHILE(pt, (TIME_get() < NAT.time_out) && (EOF == (c = getchar())) );
	NAT.rs_in.orig = (u8)(c & 0xff);

	// next char (t_id) is also subject of time-out
	NAT.time_out += 5 * TIME_1_MSEC;
	PT_WAIT_WHILE(pt, (TIME_get() < NAT.time_out) && (EOF == (c = getchar())) );
	NAT.rs_in.t_id = (u8)(c & 0xff);

	// next char (cmde) is also subject of time-out
	NAT.time_out += 5 * TIME_1_MSEC;
	PT_WAIT_WHILE(pt, (TIME_get() < NAT.time_out) && (EOF == (c = getchar())) );
	NAT.rs_in.cmde = (u8)(c & 0xff);

	// next char (status) is also subject of time-out
	NAT.time_out += 5 * TIME_1_MSEC;
	PT_WAIT_WHILE(pt, (TIME_get() < NAT.time_out) && (EOF == (c = getchar())) );
	NAT.rs_in.resp = (c & 0x80) ? 1 : 0;
	NAT.rs_in.error = (c & 0x40) ? 1 : 0;
	NAT.rs_in.time_out = (c & 0x20) ? 1 : 0;
	NAT.rs_in.serial = 1;	// force serial bit
	NAT.rs_in.eth = 0;	// force serial bit

	// next char (argv #0) is also subject of time-out
	NAT.time_out += 5 * TIME_1_MSEC;
	PT_WAIT_WHILE(pt, (TIME_get() < NAT.time_out) && (EOF == (c = getchar())) );
	NAT.rs_in.argv[0] = (u8)(c & 0xff);

	// next char (argv #1) is also subject of time-out
	NAT.time_out += 5 * TIME_1_MSEC;
	PT_WAIT_WHILE(pt, (TIME_get() < NAT.time_out) && (EOF == (c = getchar())) );
	NAT.rs_in.argv[1] = (u8)(c & 0xff);

	// next char (argv #2) is also subject of time-out
	NAT.time_out += 5 * TIME_1_MSEC;
	PT_WAIT_WHILE(pt, (TIME_get() < NAT.time_out) && (EOF == (c = getchar())) );
	NAT.rs_in.argv[2] = (u8)(c & 0xff);

	// next char (argv #3) is also subject of time-out
	NAT.time_out += 5 * TIME_1_MSEC;
	PT_WAIT_WHILE(pt, (TIME_get() < NAT.time_out) && (EOF == (c = getchar())) );
	NAT.rs_in.argv[3] = (u8)(c & 0xff);

	// next char (argv #4) is also subject of time-out
	NAT.time_out += 5 * TIME_1_MSEC;
	PT_WAIT_WHILE(pt, (TIME_get() < NAT.time_out) && (EOF == (c = getchar())) );
	NAT.rs_in.argv[4] = (u8)(c & 0xff);

	// next char (argv #5) is also subject of time-out
	NAT.time_out += 5 * TIME_1_MSEC;
	PT_WAIT_WHILE(pt, (TIME_get() < NAT.time_out) && (EOF == (c = getchar())) );
	NAT.rs_in.argv[5] = (u8)(c & 0xff);

#if FRAME_NB_ARGS > 6
	// next char (argv #6) is also subject of time-out
	NAT.time_out += 5 * TIME_1_MSEC;
	PT_WAIT_WHILE(pt, (TIME_get() < NAT.time_out) && (EOF == (c = getchar())) );
	NAT.rs_in.argv[6] = (u8)(c & 0xff);
#endif

	// if a time-out happens
	if ( TIME_get() >= NAT.time_out ) {
		// loop back
		PT_RESTART(pt);
	}

	// enqueue the frame to send it via the twi link
	PT_WAIT_WHILE(pt, KO == FIFO_put(&NAT.twi_out_fifo, &NAT.rs_in));

	// loop back for processing next frame
	PT_RESTART(pt);

	PT_END(pt);
}


static PT_THREAD( NAT_rs_out(pt_t* pt) )
{
	PT_BEGIN(pt);

	PT_WAIT_UNTIL(pt, FIFO_get(&NAT.rs_out_fifo, &NAT.rs_out));

	// prepare received frame by suppressing serial bit
	NAT.rs_out.serial = 0;

	// enqueue the response
	putchar(NAT.rs_out.dest);
	putchar(NAT.rs_out.orig);
	putchar(NAT.rs_out.t_id);
	putchar( (NAT.rs_out.resp << 7) | (NAT.rs_out.error << 6) | (NAT.rs_out.time_out << 5) | (NAT.rs_out.eth << 4) | (NAT.rs_out.serial << 3) );
	putchar(NAT.rs_out.argv[0]);
	putchar(NAT.rs_out.argv[1]);
	putchar(NAT.rs_out.argv[2]);
	putchar(NAT.rs_out.argv[3]);
	putchar(NAT.rs_out.argv[4]);
	putchar(NAT.rs_out.argv[5]);
#if FRAME_NB_ARGS > 6
	putchar(NAT.rs_out.argv[6]);
#endif

	// loop back for processing next frame
	PT_RESTART(pt);

	PT_END(pt);
}
#endif


//----------------------------------------
// public functions
//

// NAT module initialization
void NAT_init(void)
{
	// init twi part
	PT_INIT(&NAT.twi_in_pt);
	FIFO_init(&NAT.twi_in_fifo, &NAT.twi_in_buf, QUEUE_SIZE, sizeof(NAT.twi_in_buf[0]));
	NAT.interf.channel = 5;
	NAT.interf.cmde_mask = 0xffffffff;	// accept all commands
	NAT.interf.queue = &NAT.twi_in_fifo;
	DPT_register(&NAT.interf);

	PT_INIT(&NAT.twi_out_pt);
	FIFO_init(&NAT.twi_out_fifo, &NAT.twi_out_buf, QUEUE_SIZE, sizeof(NAT.twi_out_buf[0]));

#ifdef NAT_ENABLE_ETH
	// init eth part
	PT_INIT(&NAT.eth_in_pt);
	FIFO_init(&NAT.eth_in_fifo, &NAT.eth_in_buf, QUEUE_SIZE, sizeof(NAT.eth_in_buf[0]));
	W5100_init();

	PT_INIT(&NAT.eth_out_pt);
	FIFO_init(&NAT.eth_out_fifo, &NAT.eth_out_buf, QUEUE_SIZE, sizeof(NAT.eth_out_buf[0]));
#endif

#ifdef NAT_ENABLE_RS
	// init serial part
	PT_INIT(&NAT.rs_in_pt);
	FIFO_init(&NAT.rs_in_fifo, &NAT.rs_in_buf, QUEUE_SIZE, sizeof(NAT.rs_in_buf[0]));
	NAT.time_out = 0;
	RS_init(B57600);

	PT_INIT(&NAT.rs_out_pt);
	FIFO_init(&NAT.rs_out_fifo, &NAT.rs_out_buf, QUEUE_SIZE, sizeof(NAT.rs_out_buf[0]));
#endif
}


// NAT run method
void NAT_run(void)
{
	// handle the twi part
	(void)PT_SCHEDULE(NAT_twi_out(&NAT.twi_out_pt));
	(void)PT_SCHEDULE(NAT_twi_in(&NAT.twi_in_pt));

#ifdef NAT_ENABLE_ETH
	// handle the eth part
	(void)PT_SCHEDULE(NAT_eth_in(&NAT.eth_in_pt));
	(void)PT_SCHEDULE(NAT_eth_out(&NAT.eth_out_pt));
#endif

#ifdef NAT_ENABLE_RS
	// handle the rs part
	(void)PT_SCHEDULE(NAT_rs_in(&NAT.rs_in_pt));
	(void)PT_SCHEDULE(NAT_rs_out(&NAT.rs_out_pt));
#endif
}
