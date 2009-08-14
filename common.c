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

#define LED_PORT		PORTA
#define LED_DDR			DDRA
#define GREEN_LED		_BV(PA2)
#define ORANGE_LED		_BV(PA1)
#define RED_LED			_BV(PA0)

#define LED_STEP		(200 * TIME_1_MSEC)
#define GREEN_BOTTOM	0
#define GREEN_TOP		LED_STEP
#define ORANGE_BOTTOM	GREEN_TOP
#define ORANGE_TOP		(ORANGE_BOTTOM + LED_STEP)
#define RED_BOTTOM		ORANGE_TOP
#define RED_TOP			(RED_BOTTOM + LED_STEP)


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

	pt_t pt_rx;					// reception thread context
	pt_t pt_tx;					// emission thread context
	pt_t pt_blink;				// leds blinking thread context

	u32 green_led_period;		// green led blinking period
	u32 orange_led_period;		// orange led blinking period
	u32 red_led_period;			// red led blinking period

	u32 time;

	// outgoing fifo
	fifo_t out;
	dpt_frame_t out_buf[OUT_SIZE];
	dpt_frame_t fr_out;			// a buffer frame

	// response handling
	dpt_frame_t fr_rsp;			// a buffer frame

	// incoming handling variables
	dpt_frame_t fr_in;			// a buffer frame
	volatile u8 rxed;			// flag set to OK when frame is rxed

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
	CMN.fr_rsp.t_id = CMN.fr_in.t_id;
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


static PT_THREAD( CMN_blink(pt_t* pt) )
{
	blink_t b;

	PT_BEGIN(pt);

	PT_WAIT_UNTIL(pt, CMN.time + 250 * TIME_1_MSEC < TIME_get());

	// get current time
	CMN.time += 250 * TIME_1_MSEC;

	// led blinking periods depend on the flight phase
	switch (CMN.state) {
		case READY:
			CMN.green_led_period = 2 * TIME_1_SEC;
			CMN.orange_led_period = TIME_MAX;
			CMN.red_led_period = TIME_MAX;
			break;

		case WAIT_TAKE_OFF:
			CMN.green_led_period = 500 * TIME_1_MSEC;
			CMN.orange_led_period = TIME_MAX;
			CMN.red_led_period = TIME_MAX;
			break;

		case WAIT_TAKE_OFF_CONF:
			CMN.green_led_period = TIME_MAX;
			CMN.orange_led_period = 500 * TIME_1_MSEC;
			CMN.red_led_period = TIME_MAX;
			break;

		case FLYING:
			CMN.green_led_period = TIME_MAX;
			CMN.orange_led_period = 1 * TIME_1_SEC;
			CMN.red_led_period = TIME_MAX;
			break;

		case WAIT_DOOR_OPEN:
			CMN.green_led_period = TIME_MAX;
			CMN.orange_led_period = 250 * TIME_1_MSEC;
			CMN.red_led_period = TIME_MAX;
			break;

		case RECOVERY:
			CMN.green_led_period = TIME_MAX;
			CMN.orange_led_period = TIME_MAX;
			CMN.red_led_period = 2 * TIME_1_SEC;
			break;

		case BUZZER:
			CMN.green_led_period = TIME_MAX;
			CMN.orange_led_period = TIME_MAX;
			CMN.red_led_period = 250 * TIME_1_MSEC;
			break;

		case DOOR_OPENING:
			CMN.green_led_period = TIME_MAX;
			CMN.orange_led_period = TIME_MAX;
			CMN.red_led_period = TIME_MAX;
			break;

		case DOOR_OPEN:
			CMN.green_led_period = TIME_MAX;
			CMN.orange_led_period = TIME_MAX;
			CMN.red_led_period = TIME_MAX;
			break;

		case DOOR_CLOSING:
			CMN.green_led_period = TIME_MAX;
			CMN.orange_led_period = TIME_MAX;
			CMN.red_led_period = TIME_MAX;
			break;
	}

	b.led = GREEN_LED;
	b.pseudo_period = CMN.green_led_period;
	b.top = GREEN_TOP;
	b.bottom = GREEN_BOTTOM;
	blink_led(&b);
	PT_YIELD(pt);

	b.led = ORANGE_LED;
	b.pseudo_period = CMN.orange_led_period;
	b.top = ORANGE_TOP;
	b.bottom = ORANGE_BOTTOM;
	blink_led(&b);
	PT_YIELD(pt);

	b.led = RED_LED;
	b.pseudo_period = CMN.red_led_period;
	b.top = RED_TOP;
	b.bottom = RED_BOTTOM;
	blink_led(&b);

	PT_RESTART(pt);

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
	PT_INIT(&CMN.pt_blink);

	// variables init
	CMN.rxed = KO;
	CMN.state = READY;
	CMN.bus = NONE;
	CMN.green_led_period = TIME_MAX;
	CMN.orange_led_period = TIME_MAX;
	CMN.red_led_period = TIME_MAX;
	CMN.time = 0;

	// register own call-back for specific commands
	CMN.interf.channel = 3;
	CMN.interf.cmde_mask = _CM(FR_STATE) | _CM(FR_TIME_GET) | _CM(FR_MUX_POWER);
	CMN.interf.rx = CMN_dpt_rx;
	DPT_register(&CMN.interf);

	// set led port direction
	LED_DDR |= GREEN_LED | ORANGE_LED | RED_LED;

	// set I2C cmde direction and force the I2C bus low
	DDRA |= _BV(PA7);
	PORTA &= ~_BV(PA7);
}


// common module run method
void CMN_run(void)
{
	// send response if any
	(void)PT_SCHEDULE(CMN_tx(&CMN.pt_tx));

	// handle command if any
	(void)PT_SCHEDULE(CMN_rx(&CMN.pt_rx));

	// blink the leds
	(void)PT_SCHEDULE(CMN_blink(&CMN.pt_blink));
}
