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

#define LED_STEP		(50 * TIME_1_MSEC)
#define GREEN_BOTTOM	0
#define GREEN_TOP		LED_STEP
#define ORANGE_BOTTOM	GREEN_TOP
#define ORANGE_TOP		(ORANGE_BOTTOM + LED_STEP)


//--------------------------------------------
// private structures
//

typedef struct {
	u8 led;
	u32 pseudo_period;
	u32 top;
	u32 bottom;
} blink_t;



//----------------------------------------
// private variables
//

static struct {
	dpt_interface_t interf;		// dispatcher interface

	pt_t in_pt;					// reception thread context
	pt_t out_pt;				// emission thread context
	pt_t blink_pt;				// leds blinking thread context

	u32 green_led_period;		// green led blinking period
	u32 orange_led_period;		// orange led blinking period

	u32 time;

	// outgoing fifo
	fifo_t out_fifo;
	frame_t out_buf[OUT_SIZE];

	// incoming fifo
	fifo_t in_fifo;
	frame_t in_buf[IN_SIZE];

	frame_t fr;					// a buffer frame

	cmn_state_t state;			// board state
	cmn_bus_state_t bus;		// bus state
} CMN;


//----------------------------------------
// private functions
//

void blink_led(blink_t* const b)
{
	u32 modulo = CMN.time % b->pseudo_period;

	// led is ON between BOTTOM and TOP
	if ( (modulo >= b->bottom) && (modulo < b->top) ) {
		// led ON
		LED_PORT |= b->led;
	}
	else {
		// led off
		LED_PORT &= ~b->led;
	}
}


static PT_THREAD( CMN_out(pt_t* pt) )
{
	frame_t fr;
	PT_BEGIN(pt);

	// dequeue a response if any
	PT_WAIT_UNTIL(pt, OK == FIFO_get(&CMN.out_fifo, &fr));

	// make sure to send the response
	if ( KO == DPT_tx(&CMN.interf, &fr)) {
		// else requeue the frame
		FIFO_unget(&CMN.out_fifo, &fr);
	}

	// loop back
	PT_RESTART(pt);

	PT_END(pt);
}


static PT_THREAD( CMN_in(pt_t* pt) )
{
	union {
		u8 part[4];
		u32 full;
	} time;

	PT_BEGIN(pt);

	// wait incoming frame
	PT_WAIT_UNTIL(pt, FIFO_get(&CMN.in_fifo, &CMN.fr));

	// if frame is a response
	if (CMN.fr.resp) {
		// ignore it
		PT_RESTART(pt);
	}

	// update default response frame header
	CMN.fr.resp = 1;
	CMN.fr.error = 0;

	switch (CMN.fr.cmde) {
	case FR_STATE:
		switch (CMN.fr.argv[0]) {
		case FR_STATE_GET:	// get state
			// build the frame with the node state
			CMN.fr.argv[1] = CMN.state;

			// and the bus state
			CMN.fr.argv[2] = CMN.bus;
			break;

		case FR_STATE_SET_BOTH:	// set state and bus
			// save new node state
			CMN.state = CMN.fr.argv[1];

			// save new bus state
			CMN.bus = CMN.fr.argv[2];
			break;

		case FR_STATE_SET_STATE:	// set state only
			// save new node state
			CMN.state = CMN.fr.argv[1];
			break;

		case FR_STATE_SET_BUS:	// set bus state only
			// save new bus state
			CMN.bus = CMN.fr.argv[2];
			break;

		default:
			CMN.fr.error = 1;
			break;
		}
		break;

	case FR_TIME_GET:
		// get local time
		time.full = TIME_get();

		// build frame
		// (in AVR u32 representation is little endian)
		CMN.fr.argv[0] = time.part[3];
		CMN.fr.argv[1] = time.part[2];
		CMN.fr.argv[2] = time.part[1];
		CMN.fr.argv[3] = time.part[0];
		break;

	case FR_MUX_RESET:
		switch (CMN.fr.argv[0]) {
		case FR_MUX_RESET_RESET:
			// reset PCA9543
			// drive gate to 0
			PORTD &= ~_BV(PD5);
			break;

		case FR_MUX_RESET_UNRESET:
			// release PCA9543 reset pin
			// drive gate to 1
			PORTD |= _BV(PD5);
			break;

		default:
			// reject command
			CMN.fr.error = 1;
			break;
		}
		break;

	case FR_LED_CMD:
		switch (CMN.fr.argv[0]) {
		case 0xa1:	// green led
			switch (CMN.fr.argv[1]) {
			case 0x00:	// set
				CMN.green_led_period = CMN.fr.argv[2] * 10 * TIME_1_MSEC;
				break;

			case 0xff:	// get
				CMN.fr.argv[2] = CMN.green_led_period / (10 * TIME_1_MSEC);
				break;

			default:
				// reject command
				CMN.fr.error = 1;
				break;
			}
			break;

		case 0x09:	// green led
			switch (CMN.fr.argv[1]) {
			case 0x00:	// set
				CMN.orange_led_period = CMN.fr.argv[2] * 10 * TIME_1_MSEC;
				break;

			case 0xff:	// get
				CMN.fr.argv[2] = CMN.orange_led_period / (10 * TIME_1_MSEC);
				break;

			default:
				// reject command
				CMN.fr.error = 1;
				break;
			}
			break;

		default:
			// reject command
			CMN.fr.error = 1;
			break;
		}
		break;

	default:
		// reject frame
		CMN.fr.error = 1;
		break;
	}

	// enqueue the response
	PT_WAIT_UNTIL(pt, OK == FIFO_put(&CMN.out_fifo, &CMN.fr));

	PT_RESTART(pt);

	PT_END(pt);
}


static PT_THREAD( CMN_blink(pt_t* pt) )
{
	blink_t b;

	PT_BEGIN(pt);

	PT_WAIT_UNTIL(pt, TIME_get() >= CMN.time);

	// update time-out
	CMN.time += 10 * TIME_1_MSEC;

	// blink green led
	b.led = GREEN_LED;
	b.pseudo_period = CMN.green_led_period;
	b.top = GREEN_TOP;
	b.bottom = GREEN_BOTTOM;
	blink_led(&b);
	PT_YIELD(pt);

	// blink orange led
	b.led = ORANGE_LED;
	b.pseudo_period = CMN.orange_led_period;
	b.top = ORANGE_TOP;
	b.bottom = ORANGE_BOTTOM;
	blink_led(&b);
	PT_YIELD(pt);

	PT_RESTART(pt);

	PT_END(pt);
}


//----------------------------------------
// public functions
//

// common module initialization
void CMN_init(void)
{
	// fifo init
	FIFO_init(&CMN.out_fifo, &CMN.out_buf, OUT_SIZE, sizeof(CMN.out_buf[0]));	
	FIFO_init(&CMN.in_fifo, &CMN.in_buf, IN_SIZE, sizeof(CMN.in_buf[0]));	

	// thread context init
	PT_INIT(&CMN.out_pt);
	PT_INIT(&CMN.in_pt);
	PT_INIT(&CMN.blink_pt);

	// variables init
	CMN.state = READY;
	CMN.bus = NONE;
	CMN.green_led_period = TIME_MAX;
	CMN.orange_led_period = TIME_MAX;
	CMN.time = 0;

	// register own call-back for specific commands
	CMN.interf.channel = 3;
	CMN.interf.cmde_mask = _CM(FR_STATE) | _CM(FR_TIME_GET) | _CM(FR_MUX_RESET) | _CM(FR_LED_CMD);
	CMN.interf.queue = &CMN.in_fifo;
	DPT_register(&CMN.interf);

	// set led port direction
	LED_DDR |= GREEN_LED | ORANGE_LED;

	// set I2C cmde direction and force the I2C bus low
	DDRD |= _BV(PD5);
	PORTD &= ~_BV(PD5);
}


// common module run method
void CMN_run(void)
{
	// handle command if any
	(void)PT_SCHEDULE(CMN_in(&CMN.in_pt));

	// send response if any
	(void)PT_SCHEDULE(CMN_out(&CMN.out_pt));

	// if all frames are handled
	if ( ( FIFO_full(&CMN.out_fifo) == 0 ) && ( FIFO_full(&CMN.in_fifo) == 0 ) ) {
		// unlock the dispatcher
		DPT_unlock(&CMN.interf);
	}

	// blink the leds
	(void)PT_SCHEDULE(CMN_blink(&CMN.blink_pt));
}
