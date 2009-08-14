#include <string.h>

#include "test.h"

#include "type_def.h"

#include "scalp/dispatcher.h"
#include "scalp/dna.h"

#include "utils/time.h"

#include "avr/io.h"


// ------------------------------------------------
// stubs
//


typedef struct {
	void (*rx)(dpt_frame_t* fr);
	u8 tx_ret;
	u8 t_id;
} DPT_t;

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


struct {
	u32 t;
} TIME;

u32 TIME_get(void)
{
	TIME.t += TIME_1_MSEC;
	TEST_log("TIME_get : t = %d", TIME.t);

	return TIME.t;
}


struct {
	u16 data;
	dpt_frame_t container;
} EEP;

void eeprom_read_block(void* data, const void* addr, int len)
{
	if ( len == sizeof(dpt_frame_t) ) {
		TEST_log("eeprom_read_block : addr = 0x%04x, len = %d", addr, len);
		memcpy(data, &EEP.container, len);
	}
}


u8 PORTD;

// ------------------------------------------------
// tests suite
//

static void test_init_BC(void)
{
	DNA_init(DNA_BC);
	TEST_check("DPT_register : channel #2, cmde [0x00003803]");
	TEST_check("DPT_lock : channel #2");
}


static void test_BC_retry_tx(void)
{
	DNA_init(DNA_BC);
	TEST_check("DPT_register : channel #2, cmde [0x00003803]");
	TEST_check("DPT_lock : channel #2");

	DPT.tx_ret = KO;
	DNA_run();
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x08, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");

	DPT.tx_ret = OK;
	DNA_run();
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x08, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
}


static void test_BC_find_free_at_3th_try(void)
{
	dpt_frame_t fr_0x08_occupied = {
		.dest = 0x00,
		.orig = 0x08,
		.resp = 1,
		.error = 0,
		.cmde = 0x00,
		.argv = { 0x00, 0x00, 0x00, 0x00 }
	};

	dpt_frame_t fr_0x09_occupied = {
		.dest = 0x00,
		.orig = 0x09,
		.resp = 1,
		.error = 0,
		.cmde = 0x00,
		.argv = { 0x00, 0x00, 0x00, 0x00 }
	};

	dpt_frame_t fr_0x0a_free = {
		.dest = 0x00,
		.orig = 0x0a,
		.resp = 1,
		.error = 1,
		.cmde = 0x00,
		.argv = { 0x00, 0x00, 0x00, 0x00 }
	};


	DNA_init(DNA_BC);
	TEST_check("DPT_register : channel #2, cmde [0x00003803]");
	TEST_check("DPT_lock : channel #2");

	DPT.tx_ret = OK;
	DNA_run();

	DPT.rx(&fr_0x08_occupied);
	DNA_run();
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x08, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");

	DPT.rx(&fr_0x09_occupied);
	DNA_run();
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x09, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");

	DPT.rx(&fr_0x0a_free);
	DNA_run();
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x0a, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_set_sl_addr : 0x0a");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x02, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
}


static void test_BC_scan_BS(void)
{
	int i;
	TEST_check("DPT_register : channel #2, cmde [0x00003803]");
	TEST_check("DPT_lock : channel #2");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x08, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_set_sl_addr : 0x08");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x02, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x03, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x04, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x05, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x06, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x07, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x10, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x11, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x12, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x13, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x14, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x15, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x16, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x17, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x18, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x19, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x1a, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x1b, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x1c, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x1d, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x1e, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x1f, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x20, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x21, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x22, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x23, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x24, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x25, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x26, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x27, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x28, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x29, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x2a, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x2b, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x2c, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x2d, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x2e, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x2f, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x30, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x31, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x32, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x33, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x34, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x35, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x36, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x37, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x38, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x39, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x3a, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x3b, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x3c, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x3d, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x3e, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x3f, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x40, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x41, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x42, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x43, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x44, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x45, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x46, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x47, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x48, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x49, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x4a, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x4b, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x4c, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x4d, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x4e, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x4f, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x50, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x51, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x52, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x53, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x54, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x55, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x56, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x57, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x58, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x59, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x5a, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x5b, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x5c, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x5d, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x5e, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x5f, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x60, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x61, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x62, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x63, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x64, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x65, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x66, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x67, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x68, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x69, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x6a, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x6b, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x6c, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x6d, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x6e, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x6f, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
		// PCA9540B address skipped "DPT_tx : channel #2, fr = { .dest = 0x70, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x71, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x72, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x73, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x74, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x75, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x76, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x77, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x78, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x79, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x7a, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x7b, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x7c, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x7d, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x7e, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x7f, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_unlock : channel #2");
	TEST_check("DPT_gen_call : 0x01");
	TEST_check("DPT_lock : channel #2");
	TEST_check("DPT_unlock : channel #2");
	//TEST_check("DPT_lock : channel #2");
	//TEST_check("DPT_unlock : channel #2");

	dpt_frame_t fr_0x08_free = {
		.dest = 0x00,
		.orig = 0x08,
		.resp = 1,
		.error = 1,
		.cmde = 0x00,
		.argv = { 0x00, 0x00, 0x00, 0x00 }
	};

	dpt_frame_t fr_BS_resp;


	DNA_init(DNA_BC);

	DPT.tx_ret = OK;
	DNA_run();

	DPT.rx(&fr_0x08_free);
	DNA_run();

	for ( i = 0; i < 0x76; i++ ) {
		fr_BS_resp.dest = 0x00;
		fr_BS_resp.orig = i;
		fr_BS_resp.resp = 1;
		fr_BS_resp.error = 1;
		fr_BS_resp.cmde = 0x00;

		// some BS are present at addresses : 0x21 (33), 0x24 (37)
		if ( ( i == 23 )||( i == 27 ) ) {
			fr_BS_resp.resp = 1;
			fr_BS_resp.error = 0;
			fr_BS_resp.cmde = 0x00;
		}

		DPT.rx(&fr_BS_resp);
		DNA_run();
	}
}


static void test_BC_get_IS_regs(void)
{
	int i;

	TEST_check("DPT_register : channel #2, cmde [0x00003803]");
	TEST_check("DPT_lock : channel #2");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x08, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_set_sl_addr : 0x08");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x02, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x03, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x04, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x05, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x06, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x07, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x10, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x11, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x12, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x13, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x14, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x15, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x16, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x17, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x18, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x19, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x1a, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x1b, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x1c, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x1d, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x1e, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x1f, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x20, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x21, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x22, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x23, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x24, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x25, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x26, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x27, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x28, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x29, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x2a, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x2b, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x2c, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x2d, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x2e, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x2f, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x30, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x31, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x32, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x33, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x34, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x35, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x36, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x37, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x38, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x39, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x3a, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x3b, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x3c, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x3d, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x3e, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x3f, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x40, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x41, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x42, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x43, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x44, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x45, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x46, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x47, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x48, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x49, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x4a, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x4b, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x4c, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x4d, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x4e, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x4f, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x50, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x51, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x52, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x53, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x54, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x55, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x56, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x57, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x58, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x59, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x5a, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x5b, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x5c, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x5d, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x5e, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x5f, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x60, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x61, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x62, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x63, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x64, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x65, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x66, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x67, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x68, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x69, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x6a, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x6b, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x6c, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x6d, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x6e, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x6f, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
		// PCA9540B address skipped "DPT_tx : channel #2, fr = { .dest = 0x70, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x71, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x72, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x73, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x74, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x75, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x76, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x77, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x78, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x79, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x7a, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x7b, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x7c, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x7d, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x7e, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x7f, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_unlock : channel #2");
	TEST_check("DPT_gen_call : 0x01");
	TEST_check("DPT_lock : channel #2");
	TEST_check("DPT_unlock : channel #2");
	TEST_check("DPT_lock : channel #2");
	//TEST_check("DPT_unlock : channel #2");
	//TEST_check("DPT_lock : channel #2");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x0d, .orig = 0x08, .cmde = 0x8b, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x00, .orig = 0x08, .cmde = 0x0d, .argv = 0x02 05 0d 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x00, .orig = 0x08, .cmde = 0x0d, .argv = 0x03 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x00, .orig = 0x08, .cmde = 0x0d, .argv = 0x04 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x00, .orig = 0x08, .cmde = 0x0d, .argv = 0x05 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x00, .orig = 0x08, .cmde = 0x0d, .argv = 0x06 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x00, .orig = 0x08, .cmde = 0x0d, .argv = 0x07 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x00, .orig = 0x08, .cmde = 0x0d, .argv = 0x08 02 25 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x00, .orig = 0x08, .cmde = 0x0d, .argv = 0x09 02 21 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x00, .orig = 0x08, .cmde = 0x0c, .argv = 0x01 02 00 00 }");
	TEST_check("DPT_unlock : channel #2");
	TEST_check("DPT_lock : channel #2");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x09, .orig = 0x08, .cmde = 0x8b, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x00, .orig = 0x08, .cmde = 0x0d, .argv = 0x02 05 0d 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x00, .orig = 0x08, .cmde = 0x0d, .argv = 0x03 05 09 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x00, .orig = 0x08, .cmde = 0x0d, .argv = 0x04 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x00, .orig = 0x08, .cmde = 0x0d, .argv = 0x05 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x00, .orig = 0x08, .cmde = 0x0d, .argv = 0x06 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x00, .orig = 0x08, .cmde = 0x0d, .argv = 0x07 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x00, .orig = 0x08, .cmde = 0x0d, .argv = 0x08 02 25 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x00, .orig = 0x08, .cmde = 0x0d, .argv = 0x09 02 21 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x00, .orig = 0x08, .cmde = 0x0c, .argv = 0x02 02 00 00 }");

	TEST_check("DPT_unlock : channel #2");
	TEST_check("DPT_lock : channel #2");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x0a, .orig = 0x08, .cmde = 0x8b, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x00, .orig = 0x08, .cmde = 0x0d, .argv = 0x02 05 0d 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x00, .orig = 0x08, .cmde = 0x0d, .argv = 0x03 05 09 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x00, .orig = 0x08, .cmde = 0x0d, .argv = 0x04 04 0a 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x00, .orig = 0x08, .cmde = 0x0d, .argv = 0x05 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x00, .orig = 0x08, .cmde = 0x0d, .argv = 0x06 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x00, .orig = 0x08, .cmde = 0x0d, .argv = 0x07 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x00, .orig = 0x08, .cmde = 0x0d, .argv = 0x08 02 25 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x00, .orig = 0x08, .cmde = 0x0d, .argv = 0x09 02 21 00 }");

	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x00, .orig = 0x08, .cmde = 0x0c, .argv = 0x03 02 00 00 }");
	TEST_check("DPT_unlock : channel #2");

	TEST_check("DPT_lock : channel #2");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x0a, .orig = 0x08, .cmde = 0x8d, .argv = 0x02 05 0d 00 }");
	TEST_check("DPT_unlock : channel #2");
	TEST_check("DPT_lock : channel #2");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x0a, .orig = 0x08, .cmde = 0x8c, .argv = 0x03 02 00 00 }");
	TEST_check("DPT_unlock : channel #2");


	dpt_frame_t fr_0x08_free = {
		.dest = 0x00,
		.orig = 0x08,
		.resp = 1,
		.error = 1,
		.cmde = 0x00,
		.argv = { 0x00, 0x00, 0x00, 0x00 }
	};

	dpt_frame_t fr_BS_resp = {
		.resp = 0,
		.error = 0,
		.argv = { 0x00, 0x00, 0x00, 0x00 }
	};

	dpt_frame_t fr_IS_reg_0x0d = {
		.dest = 0x08,
		.orig = 0x0d,
		.resp = 0,
		.error = 0,
		.cmde = FR_REGISTER,
		.argv = { 0x0d, 0x05, 0x00, 0x00 }
	};

	dpt_frame_t fr_IS_reg_0x09 = {
		.dest = 0x08,
		.orig = 0x09,
		.resp = 0,
		.error = 0,
		.cmde = FR_REGISTER,
		.argv = { 0x09, 0x05, 0x00, 0x00 }
	};

	dpt_frame_t fr_IS_reg_0x0a = {
		.dest = 0x08,
		.orig = 0x0a,
		.resp = 0,
		.error = 0,
		.cmde = FR_REGISTER,
		.argv = { 0x0a, 0x04, 0x00, 0x00 }
	};

	dpt_frame_t fr_IS_line_0x0a = {
		.dest = 0x08,
		.orig = 0x0a,
		.resp = 0,
		.error = 0,
		.cmde = FR_LINE,
		.argv = { 0x02, 0x00, 0x00, 0x00 }
	};

	dpt_frame_t fr_IS_list_0x0a = {
		.dest = 0x08,
		.orig = 0x0a,
		.resp = 0,
		.error = 0,
		.cmde = FR_LIST,
		.argv = { 0x00, 0x00, 0x00, 0x00 }
	};


	DNA_init(DNA_BC);

	DPT.tx_ret = OK;
	DNA_run();

	DPT.rx(&fr_0x08_free);
	DNA_run();

	for ( i = 0; i < 0x75; i++ ) {
		fr_BS_resp.dest = 0x00;
		fr_BS_resp.orig = i;
		fr_BS_resp.resp = 1,
		fr_BS_resp.error = 1,
		fr_BS_resp.cmde = 0x00;

		// some BS are present at addresses : 0x21 (33), 0x25 (37)
		if ( ( i == 23 )||( i == 27 ) ) {
			fr_BS_resp.resp = 1,
			fr_BS_resp.error = 0,
			fr_BS_resp.cmde = 0x00;
		}

		DPT.rx(&fr_BS_resp);
		DNA_run();
	}

	for ( i = 0; i < 100; i++ )
		DNA_run();

	DPT.rx(&fr_0x08_free);
	for ( i = 0; i < 100; i++ )
		DNA_run();

	DPT.rx(&fr_IS_reg_0x0d);
	for ( i = 0; i < 100; i++ )
		DNA_run();

	DPT.rx(&fr_IS_reg_0x09);
	DNA_run();
	DNA_run();
	DNA_run();
	DPT.rx(&fr_IS_reg_0x0a);	// this registering during list sending is ignored

	for ( i = 0; i < 100; i++ )
		DNA_run();


	// check line request
	DPT.rx(&fr_IS_line_0x0a);
	DNA_run();

	// check list request
	DPT.rx(&fr_IS_list_0x0a);
	DNA_run();
}


static void test_init_IS(void)
{
	TEST_check("DPT_register : channel #2, cmde [0x00003803]");
	TEST_check("DPT_lock : channel #2");

	DNA_init(DNA_MINUT);	// could be DNA_MINUT or DNA_XP
}


static void test_IS_retry_tx(void)
{
	TEST_check("DPT_register : channel #2, cmde [0x00003803]");
	TEST_check("DPT_lock : channel #2");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x08, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x08, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");

	DNA_init(DNA_XP);

	DPT.tx_ret = KO;
	DNA_run();

	DPT.tx_ret = OK;
	DNA_run();
}


static void test_IS_find_free_at_2nd_try(void)
{
	TEST_check("DPT_register : channel #2, cmde [0x00003803]");
	TEST_check("DPT_lock : channel #2");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x08, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x09, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_set_sl_addr : 0x09");
	TEST_check("TIME_get : t = 10");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x00, .orig = 0x09, .cmde = 0x0b, .argv = 0x09 04 00 00 }");
	TEST_check("TIME_get : t = 20");

	dpt_frame_t fr_0x08_occupied = {
		.dest = 0x00,
		.orig = 0x08,
		.resp = 1,
		.error = 0,
		.cmde = 0x00,
		.argv = { 0x00, 0x00, 0x00, 0x00 }
	};

	dpt_frame_t fr_0x09_free = {
		.dest = 0x00,
		.orig = 0x08,
		.resp = 1,
		.error = 1,
		.cmde = 0x00,
		.argv = { 0x00, 0x00, 0x00, 0x00 }
	};


	DNA_init(DNA_XP);

	DPT.tx_ret = OK;
	DNA_run();
	DNA_run();
	DNA_run();

	DPT.rx(&fr_0x08_occupied);
	DNA_run();
	DNA_run();
	DNA_run();

	DPT.rx(&fr_0x09_free);
	DNA_run();
}


static void test_IS_reg(void)
{
	int i;

	TEST_check("DPT_register : channel #2, cmde [0x00003803]");
	TEST_check("DPT_lock : channel #2");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x08, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x09, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_set_sl_addr : 0x09");
	TEST_check("TIME_get : t = 10010");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x00, .orig = 0x09, .cmde = 0x0b, .argv = 0x09 04 00 00 }");
	TEST_check("TIME_get : t = 10020");
	TEST_check("TIME_get : t = 10030");
	TEST_check("TIME_get : t = 10040");
	TEST_check("TIME_get : t = 10050");
	TEST_check("TIME_get : t = 10060");
	TEST_check("TIME_get : t = 10070");
	TEST_check("TIME_get : t = 10080");
	TEST_check("TIME_get : t = 10090");
	TEST_check("TIME_get : t = 10100");
	TEST_check("TIME_get : t = 10110");
	TEST_check("TIME_get : t = 10120");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x00, .orig = 0x09, .cmde = 0x0b, .argv = 0x09 04 00 00 }");
	TEST_check("TIME_get : t = 10130");
	TEST_check("TIME_get : t = 10140");
	TEST_check("TIME_get : t = 10150");
	TEST_check("TIME_get : t = 10160");
	TEST_check("TIME_get : t = 10170");
	TEST_check("TIME_get : t = 10180");
	TEST_check("TIME_get : t = 10190");
	TEST_check("TIME_get : t = 10200");
	TEST_check("TIME_get : t = 10210");
	TEST_check("TIME_get : t = 10220");
	TEST_check("TIME_get : t = 10230");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x00, .orig = 0x09, .cmde = 0x0b, .argv = 0x09 04 00 00 }");
	TEST_check("TIME_get : t = 10240");
	TEST_check("TIME_get : t = 10250");
	TEST_check("TIME_get : t = 10260");
	TEST_check("TIME_get : t = 10270");
	TEST_check("TIME_get : t = 10280");
	TEST_check("TIME_get : t = 10290");
	TEST_check("TIME_get : t = 10300");
	TEST_check("DPT_gen_call : 0x01");
	TEST_check("DPT_unlock : channel #2");
	TEST_check("DNA_list : nb_is = 3, nb_bs = 4");
	TEST_check("DNA_line #0 : {type,addr} = {0x04, 0x09}");
	TEST_check("DNA_line #1 : {type,addr} = {0x01, 0x08}");
	TEST_check("DNA_line #2 : {type,addr} = {0x00, 0x00}");
	TEST_check("DNA_line #3 : {type,addr} = {0x07, 0x0a}");
	TEST_check("DNA_line #4 : {type,addr} = {0x00, 0x00}");
	TEST_check("DNA_line #5 : {type,addr} = {0x00, 0x00}");
	TEST_check("DNA_line #6 : {type,addr} = {0x00, 0x00}");
	TEST_check("DNA_line #7 : {type,addr} = {0x00, 0x00}");
	TEST_check("DNA_line #8 : {type,addr} = {0x00, 0x00}");
	TEST_check("DNA_line #9 : {type,addr} = {0x00, 0x00}");
	TEST_check("DNA_list : nb_is = 4, nb_bs = 5");
	TEST_check("DNA_line #0 : {type,addr} = {0x04, 0x09}");
	TEST_check("DNA_line #1 : {type,addr} = {0x01, 0x08}");
	TEST_check("DNA_line #2 : {type,addr} = {0x00, 0x00}");
	TEST_check("DNA_line #3 : {type,addr} = {0x07, 0x0a}");
	TEST_check("DNA_line #4 : {type,addr} = {0x08, 0x09}");
	TEST_check("DNA_line #5 : {type,addr} = {0x00, 0x00}");
	TEST_check("DNA_line #6 : {type,addr} = {0x00, 0x00}");
	TEST_check("DNA_line #7 : {type,addr} = {0x00, 0x00}");
	TEST_check("DNA_line #8 : {type,addr} = {0x00, 0x00}");
	TEST_check("DNA_line #9 : {type,addr} = {0x00, 0x00}");

	dpt_frame_t fr_0x08_occupied = {
		.dest = 0x00,
		.orig = 0x08,
		.resp = 1,
		.error = 0,
		.cmde = 0x00,
		.argv = { 0x00, 0x00, 0x00, 0x00 }
	};

	dpt_frame_t fr_0x09_free = {
		.dest = 0x00,
		.orig = 0x08,
		.resp = 1,
		.error = 1,
		.cmde = 0x00,
		.argv = { 0x00, 0x00, 0x00, 0x00 }
	};

	dpt_frame_t fr_reg_acq = {
		.dest = 0x09,
		.orig = 0x08,
		.resp = 1,
		.error = 0,
		.cmde = 0x0b,
		.argv = { 0x00, 0x00, 0x00, 0x00 }
	};

	dpt_frame_t fr_list_resp = {
		.dest = 0x09,
		.orig = 0x08,
		.resp = 1,
		.error = 0,
		.cmde = 0x0c,
		.argv = { 0x03, 0x04, 0x00, 0x00 }
	};

	dpt_frame_t fr_line_resp = {
		.dest = 0x09,
		.orig = 0x08,
		.resp = 1,
		.error = 0,
		.cmde = 0x0d,
		.argv = { 0x03, 0x07, 0x0a, 0x00 }
	};

	dpt_frame_t fr_list_cmde = {
		.dest = 0x09,
		.orig = 0x08,
		.cmde = 0x0c,
		.argv = { 0x04, 0x05, 0x00, 0x00 }
	};

	dpt_frame_t fr_line_cmde = {
		.dest = 0x09,
		.orig = 0x08,
		.cmde = 0x0d,
		.argv = { 0x04, 0x08, 0x09, 0x00 }
	};

	dna_list_t* list;
	u8 nb_is;
	u8 nb_bs;


	TIME.t = 10000;
	DNA_init(DNA_XP);

	DPT.tx_ret = OK;
	DNA_run();

	DPT.rx(&fr_0x08_occupied);
	DNA_run();

	DPT.rx(&fr_0x09_free);
	DNA_run();

	// test time-out handling
	for ( i = 0; i < 25; i++)
		DNA_run();

	// register acquitment before time-out
	DPT.rx(&fr_reg_acq);
	for ( i = 0; i < 25; i++)
		DNA_run();

	// ignore unrelated command
	DPT.rx(&fr_reg_acq);
	for ( i = 0; i < 25; i++)
		DNA_run();


	// receiving list and line responses
	DPT.rx(&fr_list_resp);
	DNA_run();
	DPT.rx(&fr_line_resp);
	DNA_run();

	list = DNA_list(&nb_is, &nb_bs);
	TEST_log("DNA_list : nb_is = %d, nb_bs = %d", nb_is, nb_bs);
	TEST_log("DNA_line #0 : {type,addr} = {0x%02x, 0x%02x}", (list + 0)->type, (list + 0)->i2c_addr);
	TEST_log("DNA_line #1 : {type,addr} = {0x%02x, 0x%02x}", (list + 1)->type, (list + 1)->i2c_addr);
	TEST_log("DNA_line #2 : {type,addr} = {0x%02x, 0x%02x}", (list + 2)->type, (list + 2)->i2c_addr);
	TEST_log("DNA_line #3 : {type,addr} = {0x%02x, 0x%02x}", (list + 3)->type, (list + 3)->i2c_addr);
	TEST_log("DNA_line #4 : {type,addr} = {0x%02x, 0x%02x}", (list + 4)->type, (list + 4)->i2c_addr);
	TEST_log("DNA_line #5 : {type,addr} = {0x%02x, 0x%02x}", (list + 5)->type, (list + 5)->i2c_addr);
	TEST_log("DNA_line #6 : {type,addr} = {0x%02x, 0x%02x}", (list + 6)->type, (list + 6)->i2c_addr);
	TEST_log("DNA_line #7 : {type,addr} = {0x%02x, 0x%02x}", (list + 7)->type, (list + 7)->i2c_addr);
	TEST_log("DNA_line #8 : {type,addr} = {0x%02x, 0x%02x}", (list + 8)->type, (list + 8)->i2c_addr);
	TEST_log("DNA_line #9 : {type,addr} = {0x%02x, 0x%02x}", (list + 9)->type, (list + 9)->i2c_addr);

	// receiving list and line commands
	DPT.rx(&fr_list_cmde);
	DNA_run();
	DPT.rx(&fr_line_cmde);
	DNA_run();

	list = DNA_list(&nb_is, &nb_bs);
	TEST_log("DNA_list : nb_is = %d, nb_bs = %d", nb_is, nb_bs);
	TEST_log("DNA_line #0 : {type,addr} = {0x%02x, 0x%02x}", (list + 0)->type, (list + 0)->i2c_addr);
	TEST_log("DNA_line #1 : {type,addr} = {0x%02x, 0x%02x}", (list + 1)->type, (list + 1)->i2c_addr);
	TEST_log("DNA_line #2 : {type,addr} = {0x%02x, 0x%02x}", (list + 2)->type, (list + 2)->i2c_addr);
	TEST_log("DNA_line #3 : {type,addr} = {0x%02x, 0x%02x}", (list + 3)->type, (list + 3)->i2c_addr);
	TEST_log("DNA_line #4 : {type,addr} = {0x%02x, 0x%02x}", (list + 4)->type, (list + 4)->i2c_addr);
	TEST_log("DNA_line #5 : {type,addr} = {0x%02x, 0x%02x}", (list + 5)->type, (list + 5)->i2c_addr);
	TEST_log("DNA_line #6 : {type,addr} = {0x%02x, 0x%02x}", (list + 6)->type, (list + 6)->i2c_addr);
	TEST_log("DNA_line #7 : {type,addr} = {0x%02x, 0x%02x}", (list + 7)->type, (list + 7)->i2c_addr);
	TEST_log("DNA_line #8 : {type,addr} = {0x%02x, 0x%02x}", (list + 8)->type, (list + 8)->i2c_addr);
	TEST_log("DNA_line #9 : {type,addr} = {0x%02x, 0x%02x}", (list + 9)->type, (list + 9)->i2c_addr);
}


static void test_IS_cant_reg(void)
{
	int i;

	TEST_check("DPT_register : channel #2, cmde [0x00003803]");
	TEST_check("DPT_lock : channel #2");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x08, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x09, .orig = 0x00, .cmde = 0x00, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_set_sl_addr : 0x09");
	TEST_check("TIME_get : t = 10010");
		// 1st try
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x00, .orig = 0x09, .cmde = 0x0b, .argv = 0x09 04 00 00 }");
	TEST_check("TIME_get : t = 10020");
	TEST_check("TIME_get : t = 10030");
	TEST_check("TIME_get : t = 10040");
	TEST_check("TIME_get : t = 10050");
	TEST_check("TIME_get : t = 10060");
	TEST_check("TIME_get : t = 10070");
	TEST_check("TIME_get : t = 10080");
	TEST_check("TIME_get : t = 10090");
	TEST_check("TIME_get : t = 10100");
	TEST_check("TIME_get : t = 10110");
	TEST_check("TIME_get : t = 10120");
		// 2nd try
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x00, .orig = 0x09, .cmde = 0x0b, .argv = 0x09 04 00 00 }");
	TEST_check("TIME_get : t = 10130");
	TEST_check("TIME_get : t = 10140");
	TEST_check("TIME_get : t = 10150");
	TEST_check("TIME_get : t = 10160");
	TEST_check("TIME_get : t = 10170");
	TEST_check("TIME_get : t = 10180");
	TEST_check("TIME_get : t = 10190");
	TEST_check("TIME_get : t = 10200");
	TEST_check("TIME_get : t = 10210");
	TEST_check("TIME_get : t = 10220");
	TEST_check("TIME_get : t = 10230");
		// 3rd try
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x00, .orig = 0x09, .cmde = 0x0b, .argv = 0x09 04 00 00 }");
	TEST_check("TIME_get : t = 10240");
	TEST_check("TIME_get : t = 10250");
	TEST_check("TIME_get : t = 10260");
	TEST_check("TIME_get : t = 10270");
	TEST_check("TIME_get : t = 10280");
	TEST_check("TIME_get : t = 10290");
	TEST_check("TIME_get : t = 10300");
	TEST_check("TIME_get : t = 10310");
	TEST_check("TIME_get : t = 10320");
	TEST_check("TIME_get : t = 10330");
	TEST_check("TIME_get : t = 10340");
		// 4th try
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x00, .orig = 0x09, .cmde = 0x0b, .argv = 0x09 04 00 00 }");
	TEST_check("TIME_get : t = 10350");
	TEST_check("TIME_get : t = 10360");
	TEST_check("TIME_get : t = 10370");
	TEST_check("TIME_get : t = 10380");
	TEST_check("TIME_get : t = 10390");
	TEST_check("TIME_get : t = 10400");
	TEST_check("TIME_get : t = 10410");
	TEST_check("TIME_get : t = 10420");
	TEST_check("TIME_get : t = 10430");
	TEST_check("TIME_get : t = 10440");
	TEST_check("TIME_get : t = 10450");
		// 5th try
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x00, .orig = 0x09, .cmde = 0x0b, .argv = 0x09 04 00 00 }");
	TEST_check("TIME_get : t = 10460");
	TEST_check("TIME_get : t = 10470");
	TEST_check("TIME_get : t = 10480");
	TEST_check("TIME_get : t = 10490");
	TEST_check("TIME_get : t = 10500");
	TEST_check("TIME_get : t = 10510");
	TEST_check("TIME_get : t = 10520");
	TEST_check("TIME_get : t = 10530");
	TEST_check("TIME_get : t = 10540");
	TEST_check("TIME_get : t = 10550");
		// retries limit reached
		// bus force mode frame sent
	TEST_check("DPT_tx : channel #2, fr = { .dest = 0x01, .orig = 0x01, .cmde = 0x11, .argv = 0x00 02 00 00 }");
	TEST_check("DPT_unlock : channel #2");

	dpt_frame_t fr_0x08_occupied = {
		.dest = 0x00,
		.orig = 0x08,
		.resp = 1,
		.error = 0,
		.cmde = 0x00,
		.argv = { 0x00, 0x00, 0x00, 0x00 }
	};

	dpt_frame_t fr_0x09_free = {
		.dest = 0x00,
		.orig = 0x08,
		.resp = 1,
		.error = 1,
		.cmde = 0x00,
		.argv = { 0x00, 0x00, 0x00, 0x00 }
	};


	TIME.t = 10000;
	DNA_init(DNA_XP);

	DPT.tx_ret = OK;
	DNA_run();

	DPT.rx(&fr_0x08_occupied);
	DNA_run();

	DPT.rx(&fr_0x09_free);
	DNA_run();

	// test time-out handling and retries limit
	for ( i = 0; i < 10 * 5; i++)
		DNA_run();
}


static void start(void)
{
	TIME.t = 0;
}

int main(int argc, char* argv[])
{
	static TEST_list_t l = {
		.start = start,
		.stop = NULL,
		.list = {
			{ test_init_BC,			"init BC" },
			{ test_BC_retry_tx,		"test BC retry tx" },
			{ test_BC_find_free_at_3th_try,	"test BC find free at 3th try" },
			{ test_BC_scan_BS,		"test BC scan BS" },
			{ test_BC_get_IS_regs,		"test BC get IS regs" },

			{ test_init_IS,			"init IS" },
			{ test_IS_retry_tx,		"test_IS retry tx" },
			{ test_IS_find_free_at_2nd_try,	"test IS find free at 2nd try" },
			{ test_IS_reg,			"test_IS reg" },
			{ test_IS_cant_reg,		"test IS can't reg" },
			{ NULL,				NULL }
		},
	};

	TEST_run(&l, argv[1]);

	return 0;
}
