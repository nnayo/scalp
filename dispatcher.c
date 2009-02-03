#include "scalp/dispatcher.h"

#include "drivers/twi.h"

#include "utils/time.h"

#include <string.h>	// memset


//----------------------------------------
// private defines
//

#define DPT_FRAME_DEST_OFFSET	0
#define DPT_FRAME_ORIG_OFFSET	1
#define DPT_FRAME_T_ID_OFFSET	2
#define DPT_FRAME_CMDE_OFFSET	3
#define DPT_FRAME_ARGV_OFFSET	4

//----------------------------------------
// private variables
//

static struct {
	dpt_interface_t* channels[DPT_CHAN_NB];	// available channels
	u8 lock;				// lock bitfield

	u8 sl_addr;				// own I2C slave address
	volatile u8 txed;			// flag telling whether the frame is sent
	u32 time_out;				// tx time-out time

	dpt_frame_t out;			// out going frame buffer
	dpt_frame_t in;				// incoming frame buffer

	u8 t_id;				// current transaction id value
} DPT;


//----------------------------------------
// private functions
//

// dispatch the frame to each registered listener
static void DPT_dispatch(dpt_frame_t* fr)
{
	u8 i;
	fr_cmdes_t cmde = fr->cmde;

	// for each registered commands ranges
	for (i = 0; i < DPT_CHAN_NB; i++) {
		// if channel is not registered
		if ( DPT.channels[i] == NULL )
			// skip to following
			continue;

		// if command is in a range
		if ( DPT.channels[i]->cmde_mask & _CM(cmde) ) {
			// call the associated call-back if registered
			if (DPT.channels[i]->rx != NULL)
				DPT.channels[i]->rx(fr);
		}

#if 0
		// if command is I2C read or write
		if ( (cmde == FR_I2C_READ)||(cmde == FR_I2C_WRITE) ) {
			// it is always transmit
			// call the associated call-back if registered
			if (DPT.channels[i]->rx != NULL)
				DPT.channels[i]->rx(fr);
		}
#endif
	}
}


// I2C reception call-back
static void DPT_I2C_call_back(twi_state_t state, u8 nb_data, void* misc)
{
	//(void)nb_data;
	(void)misc;

	// upon the state
	switch(state) {
		case TWI_NO_SL:
			// if the slave doesn't respond
			// whether the I2C address is free, so take it
			// or the slave has crached
			// whatever the problem, put a failed resp in rx frame
			DPT.in.dest = DPT.sl_addr;
			DPT.in.orig = DPT.out.orig;
			DPT.in.cmde = DPT.out.cmde;
			DPT.in.nat = DPT.out.nat;
			DPT.in.resp = 1;
			DPT.in.error = 1;

			// dispatch the response
			DPT_dispatch(&DPT.in);

			// even if it fails, the sending is over
			DPT.txed = OK;

			// and stop the com
			TWI_stop();

			break;

		case TWI_MS_RX_END:
			// reading data ends
		case TWI_MS_TX_END:
			// writing data ends

			// fill header
			DPT.in.dest = DPT_SELF_ADDR;
			DPT.in.orig = DPT.out.dest;
			DPT.in.cmde = DPT.out.cmde;
			DPT.in.nat = DPT.out.nat;
			DPT.in.resp = 1;
			DPT.in.error = 0;

			// dispatch the response
			DPT_dispatch(&DPT.in);

			// signal the end of transmission
			DPT.txed = OK;

			// and stop the com
			TWI_stop();

			break;

		case TWI_SL_RX_BEGIN:
			// just provide a buffer to store the incoming frame
			// only the origin, the cmde/resp and the arguments are received
			DPT.in.dest = DPT.sl_addr;
			TWI_sl_rx(sizeof(dpt_frame_t) - DPT_FRAME_ORIG_OFFSET, (u8*)&DPT.in + DPT_FRAME_ORIG_OFFSET);

			break;

		case TWI_SL_RX_END:
			// if the msg len is correct
			if ( nb_data == (sizeof(dpt_frame_t) - DPT_FRAME_ORIG_OFFSET)) {
				// dispatch the incoming frame
				DPT_dispatch(&DPT.in);
			}
			// else it is ignored

			// release the bus
			TWI_stop();

			break;

		case TWI_SL_TX_BEGIN:
			// don't want to send a single byte
			TWI_sl_tx(0, NULL);

			break;

		case TWI_SL_TX_END:
			// release the bus
			TWI_stop();

			break;

		case TWI_GENCALL_BEGIN:
			// just provide a buffer to store the incoming frame
			// only the origin, the cmde/resp and the arguments are received
			DPT.in.dest = DPT.sl_addr;
			TWI_sl_rx(sizeof(dpt_frame_t) - DPT_FRAME_ORIG_OFFSET, (u8*)&DPT.in + DPT_FRAME_ORIG_OFFSET);

			break;

		case TWI_GENCALL_END:
			// if the msg len is correct
			if ( nb_data == (sizeof(dpt_frame_t) - DPT_FRAME_ORIG_OFFSET)) {
				// dispatch the incoming frame
				DPT_dispatch(&DPT.in);
			}
			// else it is ignored

			// release the bus
			TWI_stop();

			break;

		default:
			// error or time-out state
			DPT.in.dest = DPT.sl_addr;
			DPT.in.orig = DPT.out.orig;
			DPT.in.cmde = DPT.out.cmde;
			DPT.in.resp = 1;
			DPT.in.error = 1;
			DPT.in.nat = DPT.out.nat;

			// dispatch the response
			DPT_dispatch(&DPT.in);

			// sending is over
			DPT.txed = OK;

			// reset time-out
			DPT.time_out = TIME_MAX;

			// and release the bus
			TWI_stop();

			break;
	}
}


//----------------------------------------
// public functions
//

// dispatcher initialization
void DPT_init(void)
{
	// reset internal structure
	memset(&DPT, 0, sizeof DPT);
	DPT.sl_addr = DPT_SELF_ADDR;
	DPT.txed = OK;
	DPT.time_out = TIME_MAX;

	// start TWI layer
	TWI_init(DPT_I2C_call_back, NULL);
}


// dispatcher time-out handling
void DPT_run(void)
{
	// if current time is above the computed time-out
	if (TIME_get() > DPT.time_out) {
		// fake an interrupt with twi layer error
		DPT_I2C_call_back(TWI_ERROR, 0, NULL);
	}
}


// dispatcher registering function
// the application needs to register itself to the dispatcher
// in order to be able to send and receive frames
//
// the parameter is an interface containing :
//  - the requested channel
//  - the command range that is used to transmit the received frame to the application : the low and high values are inclusive
//  - the write function is called by the dispatcher when a frame is received
//  - the status function is called to give the transmission status
//
// the available channel is directly set in the structure
// if it is 0xff, it means no more channel are available
void DPT_register(dpt_interface_t* interf)
{
	u8 i;

	// check if interface is invalid
	if ( interf == NULL ) {
		// then quit immediatly
		return;
	}

	// check if channel is invalid
	if ( interf->channel >= DPT_CHAN_NB ) {
		// then quit immediatly with invalid channel
		interf->channel = 0xff;
		return;
	}

	// check if requested channel is free
	// else find and use the next free
	for ( i = interf->channel; i < DPT_CHAN_NB; i++ ) {
		if ( DPT.channels[i] == NULL ) {
			break;
		}
	}
	// if none free, return error (0xff)
	if ( i == DPT_CHAN_NB ) {
		interf->channel = 0xff;
		return;
	}

	// store interface for used channel
	DPT.channels[i] = interf;

	// set the available channel
	interf->channel = i;
}


void DPT_lock(dpt_interface_t* interf)
{
	// set the lock bit associated to the channel
	DPT.lock |= 1 << interf->channel;
}


void DPT_unlock(dpt_interface_t* interf)
{
	// reset the lock bit associated to the channel
	DPT.lock &= ~(1 << interf->channel);
}


u8 DPT_tx(dpt_interface_t* interf, dpt_frame_t* fr)
{
	u8 twi_res;
	u8 i;

	// if the tx is locked by a channel of higher priority
	for ( i = 0; (i < DPT_CHAN_NB) && (i < interf->channel); i++ ) {
		if ( DPT.lock & (1 << i) ) {
			// the sender shall retry
			// so return KO
			return KO;
		}
	}

	// if the sender didn't lock the channel
	if (  !(DPT.lock & (1 << interf->channel)) ) {
		// it can't send the frame
		return KO;
	}

	// if a frame is currently sent
	if ( DPT.txed == KO ) {
		// the sender shall retry
		// so return KO
		return KO;
	}

	// block other frame sending
	DPT.txed = KO;

	// save frame
	DPT.out = *fr;

	// if the frame is not a response
	if ( !fr->resp ) {
		// increment transaction id
		DPT.t_id++;

		// and set it in the current frame
		fr->t_id = DPT.t_id;
	}

	// if the frame destination is only local
	if ( (fr->dest == DPT_SELF_ADDR)||(fr->dest == DPT.sl_addr) ) {
		// dispatch the frame
		DPT_dispatch(fr);

		// the frame has been sent to its destination
		DPT.txed = OK;
		return OK;
	}

	// if the frame is broadcasted
	if ( fr->dest == TWI_BROADCAST_ADDR ) {
		// it is also sent to local node
		DPT_dispatch(fr);
	}

	// the frame is broadcasted and/or distant node addressed
	//
	// read from and write to an I2C component are handled specificly
	switch ( fr->cmde ) {
		case FR_I2C_READ:
		//case FR_NAT|FR_I2C_READ:
			twi_res = TWI_ms_rx(DPT.out.dest, DPT.out.argv[0], (u8*)&DPT.in + DPT_FRAME_ARGV_OFFSET + 1);
			break;

		case FR_I2C_WRITE:
		//case FR_NAT|FR_I2C_WRITE:
			twi_res = TWI_ms_tx(DPT.out.dest, DPT.out.argv[0], (u8*)&DPT.out + DPT_FRAME_ARGV_OFFSET + 1);
			break;

		default:
			twi_res = TWI_ms_tx(DPT.out.dest, sizeof(dpt_frame_t) - DPT_FRAME_ORIG_OFFSET, (u8*)&DPT.out + DPT_FRAME_ORIG_OFFSET);
			break;
	}

	// if the TWI is not able to sent the frame
	if ( twi_res == KO ) {
		// release the frame sending blocking flag
		DPT.txed = OK;

		// ask for transmission retry
		return KO;
	}

	// compute and save time-out limit
	DPT.time_out = TIME_get() + 50 * TIME_1_MSEC;

	// transmission is started by hardware
	// its end will be signalled by TWI driver
	return OK;
}


void DPT_set_sl_addr(u8 addr)
{
	// save slave address
	DPT.sl_addr = addr;

	// set slave address at TWI level
	TWI_set_sl_addr(addr);
}


void DPT_gen_call(u8 flag)
{
	// just set the general call recognition mode
	TWI_gen_call(flag);
}
