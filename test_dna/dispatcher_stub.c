#include "dispatcher_stub.h"

#include <string.h>	// memset

#include "test.h"


//----------------------------------------
// private defines
//


//----------------------------------------
// private variables
//


//----------------------------------------
// private functions
//

//----------------------------------------
// public functions
//

DPT_t DPT;

// dispatcher initialization
void DPT_init(void)
{
	TEST_log("DPT_init");
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
	TEST_log("DPT_register : channel #%d, cmde [0x%08x]", interf->channel, interf->cmde_mask);

	DPT.rx = interf->rx;
}


void DPT_lock(dpt_interface_t* interf)
{
	TEST_log("DPT_lock : channel #%d", interf->channel);
}


void DPT_unlock(dpt_interface_t* interf)
{
	TEST_log("DPT_unlock : channel #%d", interf->channel);
}


u8 DPT_tx(dpt_interface_t* interf, dpt_frame_t* fr)
{
	TEST_log("DPT_tx : channel #%d, fr = { .dest = 0x%02x, .orig = 0x%02x, .cmde = 0x%02x, .argv = 0x%02x %02x %02x %02x }", interf->channel, fr->dest, fr->orig, (fr->resp << 7) | (fr->error << 6) | (fr->nat << 5) | fr->cmde, fr->argv[0], fr->argv[1], fr->argv[2], fr->argv[3]);

	return DPT.tx_ret;
}


void DPT_set_sl_addr(u8 addr)
{
	TEST_log("DPT_set_sl_addr : 0x%02x", addr);
}


void DPT_gen_call(u8 flag)
{
	TEST_log("DPT_gen_call : 0x%02x", flag);
}
