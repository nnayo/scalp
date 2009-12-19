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

#include "scalp/log.h"

#include "scalp/dispatcher.h"

#include "utils/pt.h"
#include "utils/fifo.h"
#include "utils/time.h"

#include "avr/io.h"


//----------------------------------------
// private defines
//

#define NB_FRAMES	10

#define MINIMAL_FILTER	_CM(FR_EEP_READ) | _CM(FR_EEP_WRITE) | _CM(FR_LOG_CMD)


//----------------------------------------
// private defines
//

typedef struct {
	u8	index;			// log index (unique for each log session)
	u8	time[2];		// middle octets of the TIME (2.56 ms resol / 46.6 h scale)
	dpt_frame_t fr;		// complete frame
} log_t;


//----------------------------------------
// private variables
//

static struct {
	dpt_interface_t interf;		// dispatcher interface
	pt_t	pt;					// context
	pt_t	pt2;				// spawn context
	pt_t	pt3;				// spawn context

	fifo_t	in_fifo;			// reception fifo
	dpt_frame_t in_buf[NB_FRAMES];

	dpt_frame_t fr_in;			// incoming frame
	dpt_frame_t fr_out;			// outgoing frame
	dpt_frame_t fr_filter;		// filtered frame 

	u8 enable;					// logging state

	u16	addr;					// address in eeprom
	u8	index;					// session index
} LOG;


//----------------------------------------
// private functions
//

static void LOG_command(dpt_frame_t* fr)
{
	u32 filter;

	// upon the sub-command
	switch ( fr->argv[0] ) {
	case 0x00:	// off
		LOG.enable = FALSE;
		break;

	case 0xff:	// ON
		LOG.enable = TRUE;
		break;

	case 0xa1:	// AND filter MSB value with current MSB value
		filter = LOG.interf.cmde_mask;
		filter &= (u32)fr->argv[1] << 24;
		filter &= (u32)fr->argv[2] << 16;

		// preserve the logging basic communication
		filter |= MINIMAL_FILTER;

		// set the new filter value
		LOG.interf.cmde_mask = filter;
		break;

	case 0xa0:	// AND filter LSB value with current LSB value
		filter = LOG.interf.cmde_mask;
		filter &= (u32)fr->argv[1] << 8;
		filter &= (u32)fr->argv[2] << 0;

		// preserve the logging basic communication
		filter |= MINIMAL_FILTER;

		// set the new filter value
		LOG.interf.cmde_mask = filter;
		break;

	case 0x51:	// OR filter MSB value with current MSB value
		filter = LOG.interf.cmde_mask;
		filter |= (u32)fr->argv[1] << 24;
		filter |= (u32)fr->argv[2] << 16;

		// preserve the logging basic communication
		filter |= MINIMAL_FILTER;

		// set the new filter value
		LOG.interf.cmde_mask = filter;
		break;

	case 0x50:	// OR filter LSB value with current LSB value
		filter = LOG.interf.cmde_mask;
		filter |= (u32)fr->argv[1] << 8;
		filter |= (u32)fr->argv[2] << 0;

		// preserve the logging basic communication
		filter |= MINIMAL_FILTER;

		// set the new filter value
		LOG.interf.cmde_mask = filter;
		break;

	default:
		// unknown sub-command
		fr->error = 1;
		break;
	}

	// set response bit
	fr->resp = 1;
}


static PT_THREAD( LOG_write(pt_t* pt) )
{
	PT_BEGIN(pt);

	// send the prepared frame
	PT_WAIT_UNTIL(pt, OK == DPT_tx(&LOG.interf, &LOG.fr_out));

	// wait for the response

	PT_END(pt);
}


static PT_THREAD( LOG_find_start(pt_t* pt) )
{
	u16 addr;
	PT_BEGIN(pt);

	PT_WAIT_UNTIL(pt, FIFO_get(&LOG.in_fifo , &LOG.fr_out));

	// during the search for the log start,
	// eeprom read response frames must received and treated
	addr = LOG.fr_out.argv[0] << 8;
	addr += LOG.fr_out.argv[1] << 0;

	if ( (LOG.fr_out.cmde == FR_EEP_READ) && LOG.fr_out.resp && (LOG.addr == addr) ) {
		// as it is not an event to log, quit
		PT_RESTART(pt);
	}

	// read the 2 first octets of the log at LOG.addr
	DPT_HEADER(LOG.fr_out, DPT_SELF_ADDR, DPT_SELF_ADDR, FR_EEP_READ, 0, 0, 0, 0)
	LOG.fr_out.argv[0] = (u8)((LOG.addr & 0xff00) >> 8);
	LOG.fr_out.argv[1] = (u8)((LOG.addr & 0x00ff) >> 0);

	PT_SPAWN(pt, &LOG.pt3, LOG_write(&LOG.pt3));

	// if the read octets are erased eeprom
	if ( (LOG.fr_in.argv[2] == 0xff) && (LOG.fr_in.argv[3] == 0xff) ) {
		// the new log session start address is found (here)
		//
		// the previous index is known from the previous read (see below)
		// so increment it
		LOG.index++;

		// finally the scan is over
		PT_EXIT(pt);
	}

	// extract index
	LOG.index = (LOG.fr_in.argv[2] & 0xf0) >> 4;
	
	// check the next log head
	LOG.addr += sizeof(log_t);

	// if address is out of range
	if ( LOG.addr >= END_ADDR ) {
		// eeprom is full
		// so give up
		// the log thread protection will prevent overwriting
		PT_EXIT(pt);
	}

	// restart the check to the new address
	PT_RESTART(pt);

	PT_END(pt);
}


static PT_THREAD( LOG_log(pt_t* pt) )
{
	union {
		u32 full;
		u8 part[4];
	} time;
	u16 addr;

	PT_BEGIN(pt);

	// if address is out of range
	if ( LOG.addr >= END_ADDR ) {
		// logging is no more possible
		// so quit
		PT_EXIT(pt);
	}

	// wait while no frame is present in the fifo
	PT_WAIT_WHILE(pt, KO == FIFO_get(&LOG.in_fifo, &LOG.fr_filter));

	// but during the search for the log start,
	// eeprom read response frames must received and treated
	addr = LOG.fr_filter.argv[0] << 8;
	addr += LOG.fr_filter.argv[1] << 0;

	// and during the logging,
	// eeprom write response frames must be received and treated
	if ( (LOG.fr_filter.cmde == FR_EEP_WRITE) && LOG.fr_filter.resp && (LOG.addr == addr) ) {
		// as it is not an event to log, quit
		PT_RESTART(pt);
	}
	// if it is a log command
	if ( (LOG.fr_filter.cmde == FR_LOG_CMD) && (!LOG.fr_filter.resp) ) {
		// treat it
		LOG_command(&LOG.fr_filter);

		// send the response
		PT_WAIT_UNTIL(pt, OK == DPT_tx(&LOG.interf, &LOG.fr_filter));

		// and wait till the next frame
		PT_RESTART(pt);
	}

	// if logging is disabled
	if ( !LOG.enable ) {
		// wait till the next frame
		PT_RESTART(pt);
	}

	// the log packet will be sent by pieces of 2 octets
	// the header remains the same for all writes
	DPT_HEADER(LOG.fr_out, DPT_SELF_ADDR, DPT_SELF_ADDR, FR_EEP_WRITE, 0, 0, 0, 0)

	// first, index, orig and cmde fields
	LOG.fr_out.argv[0] = (u8)((LOG.addr & 0xff00) >> 8);
	LOG.fr_out.argv[1] = (u8)((LOG.addr & 0x00ff) >> 0);
	LOG.fr_out.argv[2] = ((LOG.index & 0x0f) << 4) + ((LOG.fr_filter.orig & 0x0f) << 0);
	LOG.fr_out.argv[3] = LOG.fr_filter.cmde;
	PT_SPAWN(pt, &LOG.pt3, LOG_write(&LOG.pt3));

	// increment write address
	LOG.addr += 2;

	// save time
	time.full = TIME_get();
	LOG.fr_out.argv[0] = (u8)((LOG.addr & 0xff00) >> 8);
	LOG.fr_out.argv[1] = (u8)((LOG.addr & 0x00ff) >> 0);
	LOG.fr_out.argv[2] = time.part[2];
	LOG.fr_out.argv[3] = time.part[1];
	PT_SPAWN(pt, &LOG.pt3, LOG_write(&LOG.pt3));

	// increment write address
	LOG.addr += 2;

	// save the first 2 argv
	LOG.fr_out.argv[0] = (u8)((LOG.addr & 0xff00) >> 8);
	LOG.fr_out.argv[1] = (u8)((LOG.addr & 0x00ff) >> 0);
	LOG.fr_out.argv[2] = LOG.fr_filter.argv[0];
	LOG.fr_out.argv[3] = LOG.fr_filter.argv[1];
	PT_SPAWN(pt, &LOG.pt3, LOG_write(&LOG.pt3));

	// increment write address
	LOG.addr += 2;

	// save the third argv
	LOG.fr_out.argv[0] = (u8)((LOG.addr & 0xff00) >> 8);
	LOG.fr_out.argv[1] = (u8)((LOG.addr & 0x00ff) >> 0);
	LOG.fr_out.argv[2] = LOG.fr_filter.argv[2];
	LOG.fr_out.argv[3] = 0xff;	// keep eeprom in erase state
	PT_SPAWN(pt, &LOG.pt3, LOG_write(&LOG.pt3));

	// address must be rewinded by one
	// to take into account that only 7 octets were really written
	LOG.addr--;

	// loop back to treat the next frame to log
	PT_RESTART(pt);

	PT_END(pt);
}


static PT_THREAD( LOG_main(pt_t* pt) )
{
	PT_BEGIN(pt);

	DPT_lock(&LOG.interf);

	// first, find the end of the last recent used log
	PT_SPAWN(pt, &LOG.pt2, LOG_find_start(&LOG.pt2));

	// then start logging the frames
	LOG.interf.cmde_mask = _CM(FR_STATE)
				| _CM(FR_TIME_GET)
				| _CM(FR_MUX_POWER)
				| _CM(FR_RECONF_FORCE_MODE)
				| _CM(FR_MINUT_TAKE_OFF)
				| _CM(FR_MINUT_DOOR_CMD)
				| _CM(FR_SURV_PWR_CMD)
				| MINIMAL_FILTER;
	PT_SPAWN(pt, &LOG.pt2, LOG_log(&LOG.pt2));

	DPT_unlock(&LOG.interf);

	PT_END(pt);
}


//----------------------------------------
// public functions
//

// log module initialization
void LOG_init(void)
{
	// init context and fifo
	PT_INIT(&LOG.pt);
	FIFO_init(&LOG.in_fifo, &LOG.in_buf, NB_FRAMES, sizeof(LOG.in_buf[0]));

	// reset scan start address and index
	LOG.addr = START_ADDR;
	LOG.index = 0;

	// register own dispatcher call-back
	// while the begin the log is not found,
	// no event shall be logged
	LOG.interf.channel = 6;
	LOG.interf.cmde_mask = MINIMAL_FILTER;
	LOG.interf.queue = &LOG.in_fifo;
	DPT_register(&LOG.interf);

	LOG.enable = FALSE;
}


// log run method
void LOG_run(void)
{
	(void)PT_SCHEDULE(LOG_main(&LOG.pt));
}
