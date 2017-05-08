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

#define OUT_SIZE    1
#define IN_SIZE     5

#define LED_PORT0    PORTB
#define LED_DDR0     DDRB
#define GREEN_LED   _BV(PB5)

#define LED_PORT1    PORTD
#define LED_DDR1     DDRD
#define BLUE_LED    _BV(PD6)
#define RED_LED     _BV(PD7)

#define CNT_RESET 0  // the signal is drifting slowly each time


//--------------------------------------------
// private structures
//

enum led_color {
        GREEN,
        BLUE,
        RED,
        NB_LED,
};

struct led {
        u8 lo;   // low level duration in 10 ms
        u8 hi;   // high level duration in 10 ms
        u8 cnt;  // current counter
};

//----------------------------------------
// private variables
//

static struct {
        struct scalp_dpt_interface interf;  // dispatcher interface

        pt_t in_pt;     // reception thread context
        pt_t out_pt;    // emission thread context
        pt_t blink_pt;  // leds blinking thread context

        struct led led[NB_LED];  // led blinking condition

        u32 time;

        // outgoing fifo
        struct nnk_fifo out_fifo;
        struct scalp out_buf[OUT_SIZE];

        // incoming fifo
        struct nnk_fifo in_fifo;
        struct scalp in_buf[IN_SIZE];

        struct scalp sclp;  // a buffer

        u8 state;  // board state
} cmn;


//----------------------------------------
// private functions
//

static PT_THREAD( scalp_cmn_out(pt_t* pt) )
{
        struct scalp sclp;
        PT_BEGIN(pt);

        // dequeue a response if any
        PT_WAIT_UNTIL(pt, OK == nnk_fifo_get(&cmn.out_fifo, &sclp));

        // make sure to send the response
        scalp_dpt_lock(&cmn.interf);
        if (KO == scalp_dpt_tx(&cmn.interf, &sclp)) {
                // else requeue the scalp
                nnk_fifo_unget(&cmn.out_fifo, &sclp);
        }

        // loop back
        PT_RESTART(pt);

        PT_END(pt);
}


static void scalp_cmn_led(struct scalp* sclp)
{
        struct led* led;

        switch (sclp->argv[0]) {
        case SCALP_LED_ALIVE:        // green led
                led = &cmn.led[GREEN];
                break;

        case SCALP_LED_SIGNAL:       // blue led
                led = &cmn.led[BLUE];
                break;

        case SCALP_LED_ERROR:        // red led
                led = &cmn.led[RED];
                break;

        default:
                // reject command
                sclp->error = 1;
                return;
                break;
        }

        switch (sclp->argv[1]) {
        case SCALP_LED_SET:        // set
                led->lo = sclp->argv[2];
                led->hi = sclp->argv[3];
                break;

        case SCALP_LED_GET:        // get
                sclp->argv[2] = led->lo;
                sclp->argv[3] = led->hi;
                break;

        default:
                // reject command
                sclp->error = 1;
                break;
        }
}


static PT_THREAD( scalp_cmn_in(pt_t* pt) )
{
        union {
                u8 part[4];
                u32 full;
        } time;

        PT_BEGIN(pt);

        // wait incoming scalp
        PT_WAIT_UNTIL(pt, nnk_fifo_get(&cmn.in_fifo, &cmn.sclp));

        // if scalp is a response
        if (cmn.sclp.resp) {
                // ignore it
                PT_RESTART(pt);
        }

        // update default response scalp header
        cmn.sclp.resp = 1;
        cmn.sclp.error = 0;

        switch (cmn.sclp.cmde) {
        case SCALP_STATE:
                switch (cmn.sclp.argv[0]) {
                case SCALP_STAT_GET:        // get state
                        // build the scalp with the node state
                        cmn.sclp.argv[1] = cmn.state;
                        break;

                case SCALP_STAT_SET:        // set state
                        // save new node state
                        cmn.state = cmn.sclp.argv[1];
                        break;

                default:
                        cmn.sclp.error = 1;
                        break;
                }
                break;

        case SCALP_TIME:
                // get local time
                time.full = nnk_time_get();

                // build scalp
                // (in AVR u32 representation is little endian)
                cmn.sclp.argv[0] = time.part[3];
                cmn.sclp.argv[1] = time.part[2];
                cmn.sclp.argv[2] = time.part[1];
                cmn.sclp.argv[3] = time.part[0];
                break;

        case SCALP_MUX:
                switch (cmn.sclp.argv[0]) {
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
                        cmn.sclp.error = 1;
                        break;
                }
                break;

        case SCALP_LED:
                scalp_cmn_led(&cmn.sclp);
                break;

        default:
                // reject scalp
                cmn.sclp.error = 1;
                break;
        }

        // enqueue the response
        PT_WAIT_UNTIL(pt, OK == nnk_fifo_put(&cmn.out_fifo, &cmn.sclp));

        PT_RESTART(pt);

        PT_END(pt);
}


static PT_THREAD( scalp_cmn_blink(pt_t* pt) )
{
        PT_BEGIN(pt);

        PT_WAIT_UNTIL(pt, nnk_time_get() >= cmn.time);

        // update time-out
        cmn.time += 100 * TIME_1_MSEC;

        struct led* led;

        // blink green led
        led = &cmn.led[GREEN];
        if (led->cnt < led->lo)
                LED_PORT0 &= ~GREEN_LED;
        else if (led->cnt < led->lo + led->hi)
                LED_PORT0 |= GREEN_LED;

        led->cnt++;
        if (led->cnt >= led->lo + led->hi)
                led->cnt = CNT_RESET;

        // blink blue led
        led = &cmn.led[BLUE];
        if (led->cnt < led->lo)
                LED_PORT1 &= ~BLUE_LED;
        else if (led->cnt < led->lo + led->hi)
                LED_PORT1 |= BLUE_LED;

        led->cnt++;
        if (led->cnt >= led->lo + led->hi)
                led->cnt = CNT_RESET;

        // blink red led
        led = &cmn.led[RED];
        if (led->cnt < led->lo)
                LED_PORT1 &= ~RED_LED;
        else if (led->cnt < led->lo + led->hi)
                LED_PORT1 |= RED_LED;

        led->cnt++;
        if (led->cnt >= led->lo + led->hi)
                led->cnt = CNT_RESET;

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
        nnk_fifo_init(&cmn.out_fifo, &cmn.out_buf, OUT_SIZE, sizeof(cmn.out_buf[0]));        
        nnk_fifo_init(&cmn.in_fifo, &cmn.in_buf, IN_SIZE, sizeof(cmn.in_buf[0]));        

        // thread context init
        PT_INIT(&cmn.out_pt);
        PT_INIT(&cmn.in_pt);
        PT_INIT(&cmn.blink_pt);

        // variables init
        cmn.state = SCALP_STAT_INIT;
        struct led zero = {0, 0, 0};
        for (enum led_color l = GREEN; l <= RED; l++)
                cmn.led[l] = zero;
        cmn.time = 0;

        // register own call-back for specific commands
        cmn.interf.channel = 3;
        cmn.interf.cmde_mask = SCALP_DPT_CM(SCALP_STATE)
                             | SCALP_DPT_CM(SCALP_TIME)
                             | SCALP_DPT_CM(SCALP_MUX)
                             | SCALP_DPT_CM(SCALP_LED);
        cmn.interf.queue = &cmn.in_fifo;
        scalp_dpt_register(&cmn.interf);

        // set led port direction
        LED_DDR0 |= GREEN_LED;
        LED_DDR1 |= BLUE_LED | RED_LED;
}


// common module run method
void scalp_cmn_run(void)
{
        // handle command if any
        (void)PT_SCHEDULE(scalp_cmn_in(&cmn.in_pt));

        // send response if any
        (void)PT_SCHEDULE(scalp_cmn_out(&cmn.out_pt));

        // if all scalps are handled
        if ( ( nnk_fifo_full(&cmn.out_fifo) == 0 ) && ( nnk_fifo_full(&cmn.in_fifo) == 0 ) ) {
                // unlock the dispatcher
                scalp_dpt_unlock(&cmn.interf);
        }

        // blink the leds
        (void)PT_SCHEDULE(scalp_cmn_blink(&cmn.blink_pt));
}
