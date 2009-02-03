// GPL v3 : copyright Yann GOUY
//
//
// DISPATCHER : goal and description
//
// the dispatcher goal is to allow several applications
// to send and receive data throught the I2C bus.
// it also abstracts local and distant applications.
//
// the dispatcher offers several prioritized channels to
// which the applications shall register (1 appli <=> 1 channel)
//
// the prioritized channels permit to block applications
// while sensible activity is proceeded to the I2C bus.
//
// the dispatcher defines specific frame format.
// thanks to this format, the dispatcher can distribute
// the messages to their destination applications.
//
// the dispatcher treats the sent frames one after the other.
//
// on frame reception, the dispatcher call the registered call-back
// if it exists, else the frame is ignored.
//


#ifndef __DISPATCHER_H__
# define __DISPATCHER_H__


# include "type_def.h"

# include "common/fr_cmdes.h"

# include <avr/io.h>

//----------------------------------------
// public defines
//

# define DPT_CHAN_NB	6	// dispatcher available channels number

# define DPT_ARGC	4	// frame number of arguments

# define DPT_BROADCAST_ADDR	0x00	// frame broadcast address
# define DPT_SELF_ADDR		0x01	// reserved I2C address used for generic local node
# define DPT_FIRST_ADDR		0x02	// first I2C address
# define DPT_LAST_ADDR		0x7f	// last I2C address



//----------------------------------------
// public types
//

typedef struct {
	u8		dest;		// msg destination
	u8		orig;		// msg origin
	u8		t_id;		// transaction id
	fr_cmdes_t	cmde:5;		// msg command
	u8		nat:1;		// msg nat flag
	u8		error:1;	// msg error flag
	u8		resp:1;		// msg response flag
	u8		argv[DPT_ARGC];	// msg command argument(s) if any
} dpt_frame_t;	// dispatcher frame format


typedef struct {
	u8 channel;			// requested channel
	u32 cmde_mask;			// command filter mask
	void (*rx)(dpt_frame_t* fr);	// receive frame function (when returning, the frame is no longer available)
} dpt_interface_t;


typedef struct {
	void (*rx)(dpt_frame_t* fr);
	u8 tx_ret;
} DPT_t;

extern DPT_t DPT;

//----------------------------------------
// public functions
//

// dispatcher initialization
extern void DPT_init(void);


// dispatcher registering function
// the application needs to register itself to the dispatcher
// in order to be able to send and receive frames
//
// the parameter is an interface containing :
//  - the requested channel
//  - the command range that is used to transmit the received frame to the application : the low and high values are inclusive
//  - the rx function is called by the dispatcher when a frame is received
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
