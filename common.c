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

#include "common.h"

#include "dispatcher.h"

#include "utils/time.h"
#include "utils/pt.h"
#include "utils/fifo.h"

#include <avr/io.h>


//----------------------------------------
// private defines
//

#define OUT_SIZE	3
#define IN_SIZE		1

#define LED_PORT		PORTB
#define LED_DDR			DDRB
#define GREEN_LED		_BV(PB5)
#define ORANGE_LED		_BV(PB4)


//--------------------------------------------
// private structures
//

struct led {
	u8 lo;					// low level duration in 10 ms
	u8 hi;					// high level duration in 10 ms
	u8 cnt;					// current counter
};

//----------------------------------------
// private variables
//

static struct {
	struct scalp_dpt_interface interf;		// dispatcher interface

	pt_t in_pt;					// reception thread context
	pt_t out_pt;				// emission thread context
	pt_t blink_pt;				// leds blinking thread context

	struct led green_led, orange_led;	// led blinking condition

	u32 time;

	// outgoing fifo
	struct fifo out_fifo;
	struct scalp out_buf[OUT_SIZE];

	// incoming fifo
	struct fifo in_fifo;
	struct scalp in_buf[IN_SIZE];

	struct scalp fr;					// a buffer frame

	u8 state;			// board state
} cmn;


//----------------------------------------
// private functions
//

static PT_THREAD( scalp_cmn_out(pt_t* pt) )
{
	struct scalp fr;
	PT_BEGIN(pt);

	// dequeue a response if any
	PT_WAIT_UNTIL(pt, OK == fifo_get(&cmn.out_fifo, &fr));

	// make sure to send the response
        scalp_dpt_lock(&cmn.interf);
	if (KO == scalp_dpt_tx(&cmn.interf, &fr)) {
		// else requeue the frame
		fifo_unget(&cmn.out_fifo, &fr);
	}

	// loop back
	PT_RESTART(pt);

	PT_END(pt);
}


static PT_THREAD( scalp_cmn_in(pt_t* pt) )
{
	union {
		u8 part[4];
		u32 full;
	} time;

	PT_BEGIN(pt);

	// wait incoming frame
	PT_WAIT_UNTIL(pt, fifo_get(&cmn.in_fifo, &cmn.fr));

	// if frame is a response
	if (cmn.fr.resp) {
		// ignore it
		PT_RESTART(pt);
	}

	// update default response frame header
	cmn.fr.resp = 1;
	cmn.fr.error = 0;

	switch (cmn.fr.cmde) {
	case SCALP_STATE:
		switch (cmn.fr.argv[0]) {
		case SCALP_STAT_GET:	// get state
			// build the frame with the node state
			cmn.fr.argv[1] = cmn.state;
			break;

		case SCALP_STAT_SET:	// set state
			// save new node state
			cmn.state = cmn.fr.argv[1];
			break;

		default:
			cmn.fr.error = 1;
			break;
		}
		break;

	case SCALP_TIME:
		// get local time
		time.full = time_get();

		// build frame
		// (in AVR u32 representation is little endian)
		cmn.fr.argv[0] = time.part[3];
		cmn.fr.argv[1] = time.part[2];
		cmn.fr.argv[2] = time.part[1];
		cmn.fr.argv[3] = time.part[0];
		break;

	case SCALP_MUX:
		switch (cmn.fr.argv[0]) {
		case SCALP_MUX_RESET:
			// reset PCA9543
			// drive gate to 0
			PORTD &= ~_BV(PD5);
			break;

		case SCALP_MUX_UNRESET:
			// release PCA9543 reset pin
			// drive gate to 1
			PORTD |= _BV(PD5);
			break;

		default:
			// reject command
			cmn.fr.error = 1;
			break;
		}
		break;

	case SCALP_LED:
		switch (cmn.fr.argv[0]) {
		case SCALP_LED_ALIVE:	// green led
			switch (cmn.fr.argv[1]) {
			case SCALP_LED_SET:	// set
				cmn.green_led.lo = cmn.fr.argv[2];
				cmn.green_led.hi = cmn.fr.argv[3];
				break;

			case SCALP_LED_GET:	// get
				cmn.fr.argv[2] = cmn.green_led.lo;
				cmn.fr.argv[3] = cmn.green_led.hi;
				break;

			default:
				// reject command
				cmn.fr.error = 1;
				break;
			}
			break;

		case SCALP_LED_SIGNAL:	// orange led
			switch (cmn.fr.argv[1]) {
			case SCALP_LED_SET:	// set
				cmn.orange_led.lo = cmn.fr.argv[2];
				cmn.orange_led.hi = cmn.fr.argv[3];
				break;

			case SCALP_LED_GET:	// get
				cmn.fr.argv[2] = cmn.orange_led.lo;
				cmn.fr.argv[3] = cmn.orange_led.hi;
				break;

			default:
				// reject command
				cmn.fr.error = 1;
				break;
			}
			break;

		default:
			// reject command
			cmn.fr.error = 1;
			break;
		}
		break;

	default:
		// reject frame
		cmn.fr.error = 1;
		break;
	}

	// enqueue the response
	PT_WAIT_UNTIL(pt, OK == fifo_put(&cmn.out_fifo, &cmn.fr));

	PT_RESTART(pt);

	PT_END(pt);
}


static PT_THREAD( scalp_cmn_blink(pt_t* pt) )
{
	PT_BEGIN(pt);

	PT_WAIT_UNTIL(pt, time_get() >= cmn.time);

	// update time-out
	cmn.time += 10 * TIME_1_MSEC;

	// blink green led
	cmn.green_led.cnt++;

	// green led ON ?
	if ( LED_PORT & GREEN_LED ) {
		// led ON for its hi duration ?
		if ( cmn.green_led.cnt >= cmn.green_led.hi ) {
			// turn it off
			LED_PORT &= ~GREEN_LED;
                        cmn.green_led.cnt = 0;
		}
	}
	else {
		// led off for its lo duration ?
		if ( cmn.green_led.cnt >= cmn.green_led.lo ) {
			// turn it ON
			LED_PORT |= GREEN_LED;
                        cmn.green_led.cnt = 0;
		}
	}

//	// blink orange led
//	cmn.orange_led.cnt++;
//
//	// orange led ON ?
//	if ( LED_PORT & ORANGE_LED ) {
//		// led ON for its hi duration ?
//		if ( cmn.orange_led.cnt >= cmn.orange_led.hi ) {
//			// turn it off
//			LED_PORT &= ~ORANGE_LED;
//                        cmn.orange_led.cnt = 0;
//		}
//	}
//	else {
//		// led off for its lo duration ?
//		if ( cmn.orange_led.cnt >= cmn.orange_led.lo ) {
//			// turn it ON
//			LED_PORT |= ORANGE_LED;
//                        cmn.orange_led.cnt = 0;
//		}
//	}

	PT_RESTART(pt);

	PT_END(pt);
}


//----------------------------------------
// public functions
//

// common module initialization
void scalp_cmn_init(void)
{
	// fifo init
	fifo_init(&cmn.out_fifo, &cmn.out_buf, OUT_SIZE, sizeof(cmn.out_buf[0]));	
	fifo_init(&cmn.in_fifo, &cmn.in_buf, IN_SIZE, sizeof(cmn.in_buf[0]));	

	// thread context init
	PT_INIT(&cmn.out_pt);
	PT_INIT(&cmn.in_pt);
	PT_INIT(&cmn.blink_pt);

	// variables init
	cmn.state = SCALP_STAT_INIT;
	struct led zero = {0, 0, 0};
	cmn.green_led = zero;
	cmn.orange_led = zero;
	cmn.time = 0;

	// register own call-back for specific commands
	cmn.interf.channel = 3;
	cmn.interf.cmde_mask = _CM(SCALP_STATE) | _CM(SCALP_TIME) | _CM(SCALP_MUX) | _CM(SCALP_LED);
	cmn.interf.queue = &cmn.in_fifo;
	scalp_dpt_register(&cmn.interf);

	// set led port direction
	LED_DDR |= GREEN_LED | ORANGE_LED;

	// set I2C cmde direction and force the I2C bus low
	DDRD |= _BV(PD5);
	PORTD &= ~_BV(PD5);
}


// common module run method
void scalp_cmn_run(void)
{
	// handle command if any
	(void)PT_SCHEDULE(scalp_cmn_in(&cmn.in_pt));

	// send response if any
	(void)PT_SCHEDULE(scalp_cmn_out(&cmn.out_pt));

	// if all frames are handled
	if ( ( fifo_full(&cmn.out_fifo) == 0 ) && ( fifo_full(&cmn.in_fifo) == 0 ) ) {
		// unlock the dispatcher
		scalp_dpt_unlock(&cmn.interf);
	}

	// blink the leds
	(void)PT_SCHEDULE(scalp_cmn_blink(&cmn.blink_pt));
}
