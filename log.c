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

#include "log.h"

#include "dispatcher.h"

#include "utils/pt.h"
#include "utils/fifo.h"
#include "utils/time.h"

#include "drivers/eeprom.h"
#include "externals/sdcard.h"

#include "avr/io.h"

#include <string.h>		// memset()


//----------------------------------------
// private defines
//

#define NB_FRAMES	7

#define MINIMAL_FILTER	_CM(SCALP_LOG)

#define NB_ORIG_FILTER	6

#define SAVE_IN_RAM_ENABLED
#ifdef SAVE_IN_RAM_ENABLED
#ifndef DEBUG_EXTRA
#define DEBUG_EXTRA	0
#endif
# define RAM_BUFFER_SIZE	(30 + DEBUG_EXTRA)
#endif


//----------------------------------------
// private defines
//

struct log {
	u8	index;			// log index (unique for each log session)
	u8	time[2];		// middle octets of the time (2.56 ms resol / 46.6 h scale)
	struct scalp fr;		// complete frame
};

enum log_state {
	LOG_OFF,
	LOG_RAM,
	LOG_SDCARD,
	LOG_EEPROM,
};


//----------------------------------------
// private variables
//

static struct {
	struct scalp_dpt_interface interf;		// dispatcher interface
	pt_t	log_pt;				// context

	struct nnk_fifo in_fifo;			// reception fifo
	struct scalp in_buf[NB_FRAMES];
	struct scalp fr;				// command frame 

	enum log_state state;			// logging state

	u16	eeprom_addr;			// address in eeprom
	u64	sdcard_addr;			// address in sdcard
	u8	index;					// session index

	u8 orig_filter[NB_ORIG_FILTER];	// origin node filter

	struct log block;

#ifdef SAVE_IN_RAM_ENABLED
	u8 ram_index;
	struct log ram_buffer[RAM_BUFFER_SIZE];
#endif
} LOG;


//----------------------------------------
// private functions
//

// find the start address the new logging session
// and return the log index found in eeprom
static u8 scalp_log_find_eeprom_start(void)
{
	u8 buf[2];

	// scan the whole eeprom
	while (1) {
		// read the 2 first octets of the log at LOG.addr
		nnk_eep_read(LOG.eeprom_addr, buf, sizeof(buf));

		// wait end of reading
		while ( !nnk_eep_is_fini() )
			;

		// if the read octets are erased eeprom
		if ( (buf[0] == 0xff) && (buf[1] == 0xff) ) {
			// the new log session start address is found (here)
			//
			// the previous index is known from the previous read (see below)
			// so increment it
			LOG.index++;

			// finally the scan is over
			return LOG.index;
		}

		// extract index
		LOG.index = buf[0];
	
		// check the next log block
		LOG.eeprom_addr += sizeof(struct log);

		// if address is out of range
		if ( LOG.eeprom_addr >= EEPROM_END_ADDR ) {
			// eeprom is full
			// so give up
			// the log thread protection will prevent overwriting
			return 0xff;
		}
	}
}


// find the start address in sdcard for this new logging session
// and set the common log index for eeprom and sdcard
static void scalp_log_find_sdcard_start(u8 eeprom_index)
{
	u8 buf[2];

	// scan the whole eeprom
	while (1) {
		// read the 2 first octets of the log at LOG.addr
		SD_read(LOG.sdcard_addr, buf, sizeof(buf));
		while ( !SD_is_fini() )
			;

		// if the read octets are erased eeprom
		if ( (buf[0] == 0xff) && (buf[1] == 0xff) ) {
			// the new log session start address is found (here)
			//
			// the previous index is known from the previous read (see below)
			// so increment it
			LOG.index++;

			// index found during eeprom scanning is higher
			if ( eeprom_index > LOG.index ) {
				// use the eeprom index
				LOG.index = eeprom_index;
			}

			// finally the scan is over
			return;
		}

		// extract index
		LOG.index = buf[0];
	
		// check the next log block
		LOG.sdcard_addr += sizeof(struct log);

		// if address is out of range
		if ( LOG.sdcard_addr >= SDCARD_END_ADDR ) {
			// sdcard is full
			// so give up
			// the log thread protection will prevent overwriting
			LOG.index = 0xff;
		}
	}
}


static void scalp_log_command(struct scalp* fr)
{
	u64 filter;

	// upon the sub-command
	switch ( fr->argv[0] ) {
	case SCALP_LOG_OFF:	// off
		LOG.state = LOG_OFF;
		break;

	case SCALP_LOG_RAM:	// log to RAM
		LOG.state = LOG_RAM;
		break;

	case SCALP_LOG_SDCARD:	// log to sdcard
		LOG.state = LOG_SDCARD;
		break;

	case SCALP_LOG_EEPROM:	// log to eeprom
		LOG.state = LOG_EEPROM;
		break;

	case SCALP_LOG_SET_LSB:	// set command filter LSB part
		filter  = (u64)fr->argv[1] << 24;
		filter &= (u64)fr->argv[2] << 16;
		filter &= (u64)fr->argv[3] <<  8;
		filter &= (u64)fr->argv[4] <<  0;

		// preserve the logging basic communication
		filter |= MINIMAL_FILTER;

		// set the new filter value
		LOG.interf.cmde_mask = filter;
		break;

	case SCALP_LOG_SET_MSB:	// set command filter MSB part
		filter  = (u64)fr->argv[1] << 56;
		filter &= (u64)fr->argv[2] << 48;
		filter &= (u64)fr->argv[3] << 40;
		filter &= (u64)fr->argv[4] << 32;

		// preserve the logging basic communication
		filter |= MINIMAL_FILTER;

		// set the new filter value
		LOG.interf.cmde_mask = filter;
		break;

	case SCALP_LOG_GET_LSB:	// get command filter LSB part
		// get the filter value
		filter = LOG.interf.cmde_mask;

		fr->argv[1] = (u8)(filter >> 24);
		fr->argv[2] = (u8)(filter >> 16);
		fr->argv[3] = (u8)(filter >>  8);
		fr->argv[4] = (u8)(filter >>  0);
		break;

	case SCALP_LOG_GET_MSB:	// get command filter MSB part
		// get the filter value
		filter = LOG.interf.cmde_mask;

		fr->argv[1] = (u8)(filter >> 56);
		fr->argv[2] = (u8)(filter >> 48);
		fr->argv[3] = (u8)(filter >> 40);
		fr->argv[4] = (u8)(filter >> 32);
		break;

	case SCALP_LOG_SET_ORIG:	// set origin filter
		memcpy(LOG.orig_filter, &fr->argv[1], NB_ORIG_FILTER);
		break;

	case SCALP_LOG_GET_ORIG:	// get origin filter
		memcpy(&fr->argv[1], LOG.orig_filter, NB_ORIG_FILTER);
		break;

	default:
		// unknown sub-command
		fr->error = 1;
		break;
	}

	// set response bit
	fr->resp = 1;
}


static PT_THREAD( scalp_log_log(pt_t* pt) )
{
	u32 time;
	u8 is_filtered;
	u8 i;

	PT_BEGIN(pt);

	// systematically unlock the channel
	// because most of time no response is sent
	// when a response is needed, the channel will be locked
	scalp_dpt_unlock(&LOG.interf);

	switch ( LOG.state ) {
		case LOG_OFF:
		default:
			// empty the log fifo
			(void)nnk_fifo_get(&LOG.in_fifo, &LOG.fr);

			// loop back for next frame
			PT_RESTART(pt);
			break;

		case LOG_RAM:
#ifdef SAVE_IN_RAM_ENABLED
#endif
			break;

		case LOG_EEPROM:
			// if address is out of range
			if ( LOG.eeprom_addr >= EEPROM_END_ADDR ) {
				// logging is no more possible
				// so quit
				PT_EXIT(pt);
			}
			break;

		case LOG_SDCARD:
			// if address is out of range
			if ( LOG.sdcard_addr >= SDCARD_END_ADDR ) {
				// logging is no more possible
				// so quit
				PT_EXIT(pt);
			}
			break;
	}

	// wait while no frame is present in the fifo
	PT_WAIT_WHILE(pt, KO == nnk_fifo_get(&LOG.in_fifo, &LOG.fr));

	// if it is a log command
	if ( (LOG.fr.cmde == SCALP_LOG) && (!LOG.fr.resp) ) {
		// treat it
		scalp_log_command(&LOG.fr);

		// send the response
		PT_WAIT_UNTIL(pt, (scalp_dpt_lock(&LOG.interf), OK == scalp_dpt_tx(&LOG.interf, &LOG.fr)));
		scalp_dpt_unlock(&LOG.interf);

		// and wait till the next frame
		PT_RESTART(pt);
	}

	// filter the frame according to its origin
	is_filtered = OK;	// by default, every frame is filtered
	for ( i = 0; i < sizeof(LOG.orig_filter); i++ ) {
		// passthrough or frame origin and filter acceptance match
		if ( (LOG.orig_filter[i] == 0x00) || (LOG.orig_filter[i] == LOG.fr.orig) ){
			is_filtered = KO;
			break;
		}
	}

	// if frame is filtered away
	if ( is_filtered ) {
		// lop back for next frame
		PT_RESTART(pt);
	}

	// build the log packet
	LOG.block.index = LOG.index;
	time = nnk_time_get();
	LOG.block.time[0] = (u8)(time >> 16);
	LOG.block.time[1] = (u8)(time >>  8);
	LOG.block.fr = LOG.fr;

	switch ( LOG.state ) {
		case LOG_OFF:
		default:
			// shall never happen but just in case
			// loop back for next frame
			PT_RESTART(pt);
			break;

		case LOG_RAM:
#ifdef SAVE_IN_RAM_ENABLED
			LOG.ram_buffer[LOG.ram_index] = LOG.block;
			if ( LOG.ram_index < (RAM_BUFFER_SIZE - 1) ) {
				LOG.ram_index++;
			}
#endif
			break;

		case LOG_EEPROM:
			// save it to eeprom
			PT_WAIT_UNTIL(pt, nnk_eep_write(LOG.eeprom_addr, (u8*)&LOG.block, sizeof(struct log)));

			// wait until saving is done
			PT_WAIT_UNTIL(pt, nnk_eep_is_fini());
			break;

		case LOG_SDCARD:
			// save it to sdcard (fill the write buffer)
			PT_WAIT_UNTIL(pt, SD_write(LOG.sdcard_addr, (u8*)&LOG.block, sizeof(struct log)));

			// wait until saving is done

			break;
	}

	// loop back to treat the next frame to log
	PT_RESTART(pt);

	PT_END(pt);
}


//----------------------------------------
// public functions
//

// log module initialization
void scalp_log_init(void)
{
	u8 index;

	// init context and fifo
	PT_INIT(&LOG.log_pt);
	nnk_fifo_init(&LOG.in_fifo, &LOG.in_buf, NB_FRAMES, sizeof(LOG.in_buf[0]));

	// reset scan start address and index
	LOG.eeprom_addr = EEPROM_START_ADDR;
	LOG.sdcard_addr = SDCARD_START_ADDR;
	LOG.index = 0;

	// origin filter blocks every node by default
	memset(&LOG.orig_filter, 0xff, NB_ORIG_FILTER);
	memset(&LOG.orig_filter, 0x00, NB_ORIG_FILTER);	// for debug, no filtering

	LOG.state = LOG_RAM;

#ifdef SAVE_IN_RAM_ENABLED
	LOG.ram_index = 0;
#endif

	// find the start address for this session
	index = scalp_log_find_eeprom_start();
	scalp_log_find_sdcard_start(index);

	// register to dispatcher
	LOG.interf.channel = 6;
	LOG.interf.queue = &LOG.in_fifo;
#if 0	// for debug
	LOG.interf.cmde_mask = _CM(FR_STATE)
				| _CM(FR_MUX_RESET)
				| _CM(FR_RECONF_MODE)
				| _CM(FR_MINUT_TAKE_OFF)
				| _CM(FR_MINUT_DOOR)
				| _CM(FR_SWITCH_POWER)
				| MINIMAL_FILTER;
#else
	LOG.interf.cmde_mask = -1;
#endif
	scalp_dpt_register(&LOG.interf);
}


// log run method
void scalp_log_run(void)
{
	// logging job
	(void)PT_SCHEDULE(scalp_log_log(&LOG.log_pt));
}
