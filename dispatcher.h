// GPL v3 : copyright Yann GOUY
//
//
// DISPATCHER
//
// goal and description
//
// the dispatcher goal is to allow several applications
// to send and receive data.
// it also abstracts local and distant applications.
//
// thus a software bus is dedicated to local applications exchanges
// while distant ones go through the I2C bus.
//
// the dispatcher offers several prioritized channels to
// which the applications shall register (1 appli <=> 1 channel)
//
// the prioritized channels permit to block applications
// while sensible activity is proceeded.
//
// the dispatcher defines specific scalp format.
// thanks to this format, the dispatcher can distribute
// the messages to their destination applications.
//
// the dispatcher treats the sent scalps one after the other.
//
//


#ifndef __DISPATCHER_H__
# define __DISPATCHER_H__


# include "type_def.h"

# include "scalp.h"

# include "utils/fifo.h"


//----------------------------------------
// public defines
//

# define DPT_CHAN_NB	12				// dispatcher available channels number

# define DPT_BROADCAST_ADDR	0x00		// scalp broadcast address
# define DPT_SELF_ADDR		0x01		// reserved I2C address used for generic local node
# define DPT_FIRST_ADDR		0x02		// first I2C address
# define DPT_LAST_ADDR		0x7f		// last I2C address


#define _CM(x)		(u64)(1LL << (x))	// compute command mask


//----------------------------------------
// public types
//

struct scalp_dpt_interface {
	u8 channel;			// requested channel
	u64 cmde_mask;		// bit mask for scalp filtering
	struct fifo* queue;		// queue filled by received scalps
};


//----------------------------------------
// public macros
//

#define DPT_HEADER(fr_name, _dest, _orig, _cmde, _error, _resp, _eth, _serial)	\
	fr_name.dest = _dest;												\
	fr_name.orig = _orig;												\
	fr_name.cmde = _cmde;												\
	fr_name.error = _error;												\
	fr_name.resp = _resp;												\
	fr_name.eth = _eth;													\
	fr_name.serial = _serial;

#define DPT_ARGS(fr_name, _argv0, _argv1, _argv2, _argv3, _argv4, _argv5)	\
	fr_name.argv[0] = _argv0;											\
	fr_name.argv[1] = _argv1;											\
	fr_name.argv[2] = _argv2;											\
	fr_name.argv[1] = _argv3;												\
	fr_name.argv[2] = _argv4;												\
	fr_name.argv[3] = _argv5;


//----------------------------------------
// public functions
//

// dispatcher initialization
extern void scalp_dpt_init(void);


// dispatcher thread
// in charge of the time-out handling
// and scalp dispatching
extern void scalp_dpt_run(void);


// dispatcher registering function
// the application needs to register itself to the dispatcher
// in order to be able to send and receive scalps
//
// the parameter is an interface containing :
//  - the requested channel
//  - the command range that is used to transmit the received scalp to the application : the low and high values are inclusive
//  - the command mask only authorizes the commands corresponding to the set bits
//  - the queue is filled by the dispatcher when a scalp is received 
//		(the associated channel is locked if the scalp is enqueued)
//
// the available channel is directly set in the structure
// if it is 0xff, it means no more channel are available
extern void scalp_dpt_register(struct scalp_dpt_interface* interf);


// dispatcher channel lock
//
// lock the given channel for exclusive use
// only channel with lower value
// (i.e. higher priority) shall be able to
// send scalps.
extern void scalp_dpt_lock(struct scalp_dpt_interface* interf);


// dispatcher channel unlock
//
// unlock the given channel
// channels with lower priority will
// be able to send scalps
extern void scalp_dpt_unlock(struct scalp_dpt_interface* interf);


// dispatcher scalp sending function
//
// request a scalp to be sent
// if a scalp can't be sent,
// KO is returned and sending the scalp
// must be retried
// if a scalp can't be sent, it probably
// means the channel is not locked or
// a channel of higher priority is locked
extern u8 scalp_dpt_tx(struct scalp_dpt_interface* interf, struct scalp* scalp);


// dispatcher set TWI slave address function
//
void scalp_dpt_sl_addr_set(u8 addr);


// dispatcher enable TWI general call recognition function
//
void scalp_dpt_gen_call(u8 flag);


#endif	// __DISPATCHER_H__
