#include "scalp/log.h"

#include "scalp/dispatcher.h"

#include "utils/pt.h"
#include "utils/fifo.h"
#include "utils/time.h"


//----------------------------------------
// private defines
//

#define NB_FRAMES	10


//----------------------------------------
// private defines
//

typedef struct {
	u8	index:4;	// log index (unique for each log session)
	u8	orig:4;		// i2c origin address
	u8	cmde;		// frame cmde
	u8	time[2];	// middle octets of the TIME (2.56 ms resol / 46.6 h scale)
	u8	argv[3];	// only the first 3 argv are logged (the most significant)
} log_t;


//----------------------------------------
// private variables
//

static struct {
	dpt_interface_t interf;		// dispatcher interface
	pt_t	pt;			// context
	pt_t	pt2;			// spawn context
	pt_t	pt3;			// spawn context

	fifo_t	fifo;			// fifo
	dpt_frame_t buf[NB_FRAMES];	// data fifo buffer

	dpt_frame_t fr_in;		// incoming frame
	dpt_frame_t fr_out;		// outgoing frame
	dpt_frame_t fr_filter;		// filtered frame 

	u16	addr;			// address in eeprom
	u8	index;			// session index
	volatile u8 rxed;		// flag for signaling incoming frame
} LOG;


//----------------------------------------
// private functions
//

static PT_THREAD( LOG_write(pt_t* pt) )
{
	PT_BEGIN(pt);

	// send the prepared frame
	PT_WAIT_UNTIL(pt, OK == DPT_tx(&LOG.interf, &LOG.fr_out));

	// wait for the response
	LOG.rxed = KO;
	PT_WAIT_UNTIL(pt, OK == LOG.rxed);

	PT_END(pt);
}


static PT_THREAD( LOG_find_start(pt_t* pt) )
{
	PT_BEGIN(pt);

	// read the 2 first octets of the log at LOG.addr
	LOG.fr_out.dest = DPT_SELF_ADDR;
	LOG.fr_out.orig = DPT_SELF_ADDR;
	LOG.fr_out.resp = 0;
	LOG.fr_out.error = 0;
	LOG.fr_out.nat = 0;
	LOG.fr_out.cmde = FR_EEP_READ;
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

	PT_BEGIN(pt);

	// if address is out of range
	if ( LOG.addr >= END_ADDR ) {
		// logging is no more possible
		// so quit
		PT_EXIT(pt);
	}

	// wait while no frame is present in the fifo
	PT_WAIT_WHILE(pt, KO == FIFO_get(&LOG.fifo, &LOG.fr_filter));

	// the log packet will be sent by pieces of 2 octets
	// the header remains the same for all writes
	LOG.fr_out.dest = DPT_SELF_ADDR;
	LOG.fr_out.orig = DPT_SELF_ADDR;
	LOG.fr_out.resp = 0;
	LOG.fr_out.error = 0;
	LOG.fr_out.nat = 0;
	LOG.fr_out.cmde = FR_EEP_WRITE;

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


static void LOG_rx(dpt_frame_t* fr)
{
	u16 addr;

	// only some frames are interessing
	//
	// so they have to be filtered
	// right now, the ones to keep are :
	// - status change
	// - take-off
	// - power change
	//
	// but during the search for the log start,
	// eeprom read response frames must received and treated
	addr = fr->argv[0] << 8;
	addr += fr->argv[1] << 0;
	//if ( ( (fr->cmde & (FR_RESP|FR_EEP_READ)) == (FR_RESP|FR_EEP_READ) )
	if ( (fr->cmde == FR_EEP_READ) && fr->resp && (LOG.addr == addr) ) {
		// store and signal it
		LOG.fr_in = *fr;
		LOG.rxed = OK;

		// as it is not an event to log, quit
		return;
	}

	// and during the logging,
	// eeprom write response frames must received and treated
	//if ( ( (fr->cmde & (FR_RESP|FR_EEP_WRITE)) == (FR_RESP|FR_EEP_WRITE) )
	if ( (fr->cmde == FR_EEP_WRITE) && fr->resp && (LOG.addr == addr) ) {
		// store and signal it
		LOG.fr_in = *fr;
		LOG.rxed = OK;

		// as it is not an event to log, quit
		return;
	}

	// filter the frame
	switch (fr->cmde) {
		case FR_STATE:
		case FR_MINUT_TAKE_OFF:
		case FR_SURV_PWR_CMD:
			// enqueue the frame
			FIFO_put(&LOG.fifo, fr);
			break;

		default:
			break;
	}
}


//----------------------------------------
// public functions
//

// log module initialization
void LOG_init(void)
{
	// init context and fifo
	PT_INIT(&LOG.pt);
	FIFO_init(&LOG.fifo, LOG.buf, NB_FRAMES, sizeof(dpt_frame_t));

	// reset scan start address and index
	LOG.addr = START_ADDR;
	LOG.index = 0;

	// register own dispatcher call-back
	// while the begin the log is not found,
	// no event shall be logged
	LOG.interf.channel = 6;
	LOG.interf.cmde_mask = _CM(FR_EEP_READ) | _CM(FR_EEP_WRITE);
	LOG.interf.rx = LOG_rx;
	DPT_register(&LOG.interf);
}


// log run method
u8 LOG_run(void)
{
	PT_BEGIN(&LOG.pt);

	DPT_lock(&LOG.interf);

	// first, find the end of the last recent used log
	PT_SPAWN(&LOG.pt, &LOG.pt2, LOG_find_start(&LOG.pt2));

	// then start logging the frames
	LOG.interf.cmde_mask = _CM(FR_STATE)
				| _CM(FR_TIME_GET)
				| _CM(FR_MUX_POWER)
				| _CM(FR_RECONF_FORCE_MODE)
				| _CM(FR_MINUT_TAKE_OFF)
				| _CM(FR_MINUT_DOOR_CMD)
				| _CM(FR_SURV_PWR_CMD);
	PT_SPAWN(&LOG.pt, &LOG.pt2, LOG_log(&LOG.pt2));

	DPT_unlock(&LOG.interf);

	PT_END(&LOG.pt);
}
