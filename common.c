#include "scalp/common.h"

#include "scalp/dispatcher.h"

#include "utils/time.h"
#include "utils/pt.h"
#include "utils/fifo.h"

#include <avr/io.h>
#include <avr/eeprom.h>


//----------------------------------------
// private defines
//

#define OUT_SIZE	3

#define LED_GREEN_ON()		( PORTA |= _BV(2) )
#define LED_GREEN_OFF()		( PORTA &= ~_BV(2) )
#define LED_GREEN_ACTIVE()	( DDRA |= _BV(2) )
#define LED_GREEN_PASSIVE()	( DDRA &= ~_BV(2) )

#define LED_ORANGE_ON()		( PORTA |= _BV(1) )
#define LED_ORANGE_OFF()	( PORTA &= ~_BV(1) )
#define LED_ORANGE_ACTIVE()	( DDRA |= _BV(1) )
#define LED_ORANGE_PASSIVE()	( DDRA &= ~_BV(1) )

#define LED_RED_ON()		( PORTA |= _BV(0) )
#define LED_RED_OFF()		( PORTA &= ~_BV(0) )
#define LED_RED_ACTIVE()	( DDRA |= _BV(0) )
#define LED_RED_PASSIVE()	( DDRA &= ~_BV(0) )


//----------------------------------------
// private variables
//

static struct {
	dpt_interface_t interf;		// dispatcher interface

	pt_t pt_rx;			// reception thread context
	pt_t pt_tx;			// emission thread context

	u32 green_led_period;		// green led blinking period
	u32 orange_led_period;		// orange led blinking period
	u32 red_led_period;		// red led blinking period

	// outgoing fifo
	fifo_t out;
	dpt_frame_t out_buf[OUT_SIZE];
	dpt_frame_t fr_out;			// a buffer frame

	// response handling
	dpt_frame_t fr_rsp;			// a buffer frame

	// incoming handling variables
	dpt_frame_t fr_in;			// a buffer frame
	volatile u8 rxed;		// flag set to OK when frame is rxed

	cmn_state_t state;		// board state
	cmn_bus_state_t bus;		// bus state
} CMN;


//----------------------------------------
// private functions
//

static PT_THREAD( CMN_tx(pt_t* pt) )
{
	PT_BEGIN(pt);

	// dequeue a response if any
	PT_WAIT_UNTIL(pt, OK == FIFO_get(&CMN.out, &CMN.fr_out));

	// send the response
	DPT_lock(&CMN.interf);
	PT_WAIT_UNTIL(pt, OK == DPT_tx(&CMN.interf, &CMN.fr_out));
	DPT_unlock(&CMN.interf);

	// loop back
	PT_RESTART(pt);

	PT_END(pt);
}


static PT_THREAD( CMN_rx(pt_t* pt) )
{
	union {
		u8 part[4];
		u32 full;
	} time;

	PT_BEGIN(pt);

	// wait incoming frame
	PT_WAIT_UNTIL(pt, CMN.rxed == OK);

	// build default response frame header
	CMN.fr_rsp.dest = CMN.fr_in.orig;
	CMN.fr_rsp.orig = CMN.fr_in.dest;
	CMN.fr_rsp.resp = 1;
	CMN.fr_rsp.error = 0;
	CMN.fr_rsp.nat = CMN.fr_in.nat;
	CMN.fr_rsp.cmde = CMN.fr_in.cmde;
	CMN.fr_rsp.argv[0] = CMN.fr_in.argv[0];
	CMN.fr_rsp.argv[1] = CMN.fr_in.argv[1];
	CMN.fr_rsp.argv[2] = CMN.fr_in.argv[2];
	CMN.fr_rsp.argv[3] = CMN.fr_in.argv[3];

	switch (CMN.fr_rsp.cmde) {
		case FR_STATE:
			switch (CMN.fr_rsp.argv[0]) {
				case 0x00:	// get state
					// build the frame with the node state
					CMN.fr_rsp.argv[1] = CMN.state;

					// and the bus state
					CMN.fr_rsp.argv[2] = CMN.bus;
					break;

				case 0x7a:	// set state and bus
					// save new node state
					CMN.state = CMN.fr_rsp.argv[1];

					// save new bus state
					CMN.bus = CMN.fr_rsp.argv[2];
					break;

				case 0x8b:	// set state only
					// save new node state
					CMN.state = CMN.fr_rsp.argv[1];
					break;

				case 0x9c:	// set bus state only
					// save new bus state
					CMN.bus = CMN.fr_rsp.argv[2];
					break;

				default:
					CMN.fr_rsp.error = 1;
					break;
			}
			break;

		case FR_TIME_GET:
			// get local time
			time.full = TIME_get();

			// build frame
			// (in AVR u32 representation is little endian)
			CMN.fr_rsp.argv[0] = time.part[3];
			CMN.fr_rsp.argv[1] = time.part[2];
			CMN.fr_rsp.argv[2] = time.part[1];
			CMN.fr_rsp.argv[3] = time.part[0];
			break;

		case FR_MUX_POWER:
			switch (CMN.fr_rsp.argv[0]) {
				case 0xff:
					// switch ON
					// drive MOSFET P-channel gate to 0
					PORTA &= ~_BV(PA7);
					break;

				case 0x00:
					// switch off
					// drive MOSFET P-channel gate to 1
					PORTA |= _BV(PA7);
					break;

				default:
					// reject command
					CMN.fr_rsp.error = 1;
					break;
			}
			break;

		default:
			// reject frame
			CMN.fr_rsp.error = 1;
			break;
	}

	// enqueue the response
	PT_WAIT_UNTIL(pt, OK == FIFO_put(&CMN.out, &CMN.fr_rsp));

	// ready to receive another frame
	CMN.rxed = KO;

	PT_END(pt);
}


static void CMN_dpt_rx(dpt_frame_t* fr)
{
	// if frame is a response
	if (fr->resp) {
		// ignore it
		return;
	}

	// if frame buffer is free
	if ( CMN.rxed == KO ) {
		// prevent overwrite
		CMN.rxed = OK;

		// save incoming frame
		CMN.fr_in = *fr;
	}
}


//----------------------------------------
// public functions
//

// common module initialization
void CMN_init(void)
{
	// fifo init
	FIFO_init(&CMN.out, &CMN.out_buf, OUT_SIZE, sizeof(dpt_frame_t));	

	// thread context init
	PT_INIT(&CMN.pt_rx);
	PT_INIT(&CMN.pt_tx);

	// variables init
	CMN.rxed = KO;
	CMN.state = READY;
	CMN.bus = NONE;

	// register own call-back for specific commands
	CMN.interf.channel = 3;
	CMN.interf.cmde_mask = _CM(FR_STATE) | _CM(FR_TIME_GET) | _CM(FR_MUX_POWER);
	CMN.interf.rx = CMN_dpt_rx;
	DPT_register(&CMN.interf);

	// set led port direction
	LED_GREEN_ACTIVE();
	LED_ORANGE_ACTIVE();
	LED_RED_ACTIVE();

	// set I2C cmde direction and force the I2C bus low
	DDRA |= _BV(PA7);
	PORTA &= ~_BV(PA7);
}


// common module run method
void CMN_run(void)
{
	u32 time;

	// send response if any
	(void)PT_SCHEDULE(CMN_tx(&CMN.pt_tx));

	// handle command if any
	(void)PT_SCHEDULE(CMN_rx(&CMN.pt_rx));

	// get current time
	time = TIME_get();

	// by default, block every blinking
	CMN.green_led_period = TIME_MAX;
	CMN.orange_led_period = TIME_MAX;
	CMN.red_led_period = TIME_MAX;
	
	switch (CMN.state) {
		case READY:
			CMN.green_led_period = 2 * TIME_1_SEC;
			break;

		case WAIT_TAKE_OFF:
			CMN.green_led_period = 500 * TIME_1_MSEC;
			break;

		case WAIT_TAKE_OFF_CONF:
			CMN.orange_led_period = 500 * TIME_1_MSEC;
			break;

		case FLYING:
			CMN.orange_led_period = 1 * TIME_1_SEC;
			break;

		case WAIT_DOOR_OPEN:
			CMN.orange_led_period = 250 * TIME_1_MSEC;
			break;

		case RECOVERY:
			CMN.red_led_period = 2 * TIME_1_SEC;
			break;

		case BUZZER:
			CMN.red_led_period = 250 * TIME_1_MSEC;
			break;

		case DOOR_OPENING:
			break;

		case DOOR_OPEN:
			break;

		case DOOR_CLOSING:
			break;
	}

	// led is ON during second half of the period
	if ( (time % CMN.green_led_period) > (CMN.green_led_period / 2) ) {
		LED_GREEN_ON();
	}
	else {
		LED_GREEN_OFF();
	}

	// led is ON during second half of the period
	if ( (time % CMN.orange_led_period) > (CMN.orange_led_period / 2) ) {
		LED_ORANGE_ON();
	}
	else {
		LED_ORANGE_OFF();
	}

	// led is ON during second half of the period
	if ( (time % CMN.red_led_period) > (CMN.red_led_period / 2) ) {
		LED_RED_ON();
	}
	else {
		LED_RED_OFF();
	}
}
