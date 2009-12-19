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
// thus a software bus is dedicated to locap applications exchanges
// while distant ones go through the I2C bus.
//
// the dispatcher offers several prioritized channels to
// which the applications shall register (1 appli <=> 1 channel)
//
// the prioritized channels permit to block applications
// while sensible activity is proceeded.
//
// the dispatcher defines specific frame format.
// thanks to this format, the dispatcher can distribute
// the messages to their destination applications.
//
// the dispatcher treats the sent frames one after the other.
//
//


#ifndef __DISPATCHER_H__
# define __DISPATCHER_H__


# include "type_def.h"

# include "scalp/fr_cmdes.h"

# include "utils/fifo.h"


//----------------------------------------
// public defines
//

# define DPT_CHAN_NB	10				// dispatcher available channels number

# define DPT_ARGC		FRAME_NB_ARGS	// frame number of arguments

# define DPT_BROADCAST_ADDR	0x00		// frame broadcast address
# define DPT_SELF_ADDR		0x01		// reserved I2C address used for generic local node
# define DPT_FIRST_ADDR		0x02		// first I2C address
# define DPT_LAST_ADDR		0x7f		// last I2C address


#define _CM(x)		(u64)(1L << (x))	// compute command mask


//----------------------------------------
// public types
//

typedef struct {
	u8		dest;			// msg destination
	u8		orig;			// msg origin
	u8		t_id;			// transaction id
	fr_cmdes_t	cmde;		// msg command
	union {
		u8 status;			// status field
		struct {			// and its sub-parts
			u8 error:1;		// msg error flag
			u8 resp:1;		// msg response flag
			u8 time_out:1;	// msg time-out flag
			u8 eth:1;		// msg eth nat flag
			u8 serial:1;	// msg serial nat flag
			u8 reserved:3;	// reserved for future use
		};
	};
	u8		argv[DPT_ARGC];	// msg command argument(s) if any
} dpt_frame_t;				// dispatcher frame format


typedef struct {
	u8 channel;			// requested channel
	u64 cmde_mask;		// bit mask for frame filtering
	fifo_t* queue;		// queue filled by received frames
} dpt_interface_t;


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

#define DPT_ARGS(fr_name, _argv0, _argv1, _argv2, _argv3)				\
	fr_name.argv[0] = _argv0;											\
	fr_name.argv[1] = _argv1;											\
	fr_name.argv[2] = _argv2;											\
	fr_name.argv[3] = _argv3;


//----------------------------------------
// public functions
//

// dispatcher initialization
extern void DPT_init(void);


// dispatcher thread
// in charge of the time-out handling
// and frame dispatching
extern void DPT_run(void);


// dispatcher registering function
// the application needs to register itself to the dispatcher
// in order to be able to send and receive frames
//
// the parameter is an interface containing :
//  - the requested channel
//  - the command range that is used to transmit the received frame to the application : the low and high values are inclusive
//  - the command mask only authorizes the commands corresponding to the set bits
//  - the queue is filled by the dispatcher when a frame is received 
//		(the associated channel is locked if the frame is enqueued)
//
// the available channel is directly set in the structure
// if it is 0xff, it means no more channel are available
extern void DPT_register(dpt_interface_t* interf);


// dispatcher channel lock
//
// lock the given channel for exclusive use
// only channel with lower value
// (i.e. higher priority) shall be able to
// send frames.
extern void DPT_lock(dpt_interface_t* interf);


// dispatcher channel unlock
//
// unlock the given channel
// channels with lower priority will
// be able to send frames
extern void DPT_unlock(dpt_interface_t* interf);


// dispatcher frame sending function
//
// request a frame to be sent
// if a frame can't be sent,
// KO is returned and sending the frame
// must be retried
// if a frame can't be sent, it probably
// means the channel is not locked or
// a channel of higher priority is locked
extern u8 DPT_tx(dpt_interface_t* interf, dpt_frame_t* frame);


// dispatcher set TWI slave address function
//
void DPT_set_sl_addr(u8 addr);


// dispatcher enable TWI general call recognition function
//
void DPT_gen_call(u8 flag);


#endif	// __DISPATCHER_H__
