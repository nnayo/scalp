#include <string.h>

#include "test.h"

#include "type_def.h"

#include "scalp/time_sync.h"
#include "scalp/dna.h"
#include "scalp/dispatcher.h"

#include "utils/time.h"

#include "avr/io.h"


// ------------------------------------------------
// stubs
//

typedef struct {
	void (*rx)(dpt_frame_t* fr);
	u8 tx_ret;
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
	u32 incr;
} TIME;


u32 TIME_get(void)
{
	return TIME.t;
}


void TIME_set_incr(u32 incr)
{
	TIME.incr = incr;
	TEST_log("TIME_set_incr = %d", TIME.incr);
}



struct {
	dna_list_t list[3];
} DNA;

dna_list_t* DNA_list(u8* nb_is, u8* nb_bs)
{
	*nb_is = 3;
	*nb_bs = 0;

	return DNA.list;
}


// ------------------------------------------------
// tests suite
//

static void test_init(void)
{
	// check correct initialization
	TEST_check("TIME_set_incr = 100");
	TEST_check("DPT_register : channel #8, cmde [0x00008000]");
}


static void test_ignore_unrelated_frame(void)
{
	TEST_check("TIME_set_incr = 100");
	TEST_check("DPT_register : channel #8, cmde [0x00008000]");

	dpt_frame_t fr = {
		.dest = 0x09,
		.orig = 0x08,
		.cmde = FR_LINE,
		.argv = { 0x00, 0x00, 0x00, 0x00 }
	};

	TIME.t = TIME_1_SEC + 5 * TIME_1_MSEC;
	DPT.rx(&fr);

	DPT.tx_ret = OK;
	TSN_run();
	TSN_run();
	TSN_run();
	TSN_run();
}


static void test_time_ajust(void)
{
	TEST_check("TIME_set_incr = 100");
	TEST_check("DPT_register : channel #8, cmde [0x00008000]");
	TEST_check("DPT_lock : channel #8");
	TEST_check("DPT_tx : channel #8, fr = { .dest = 0x06, .orig = 0x07, .cmde = 0x0f, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_unlock : channel #8");
	TEST_check("TIME_set_incr = 99");
	TEST_check("DPT_lock : channel #8");
	TEST_check("DPT_tx : channel #8, fr = { .dest = 0x06, .orig = 0x07, .cmde = 0x0f, .argv = 0x00 00 27 1a }");
	TEST_check("DPT_unlock : channel #8");
	TEST_check("TIME_set_incr = 100");

	dpt_frame_t fr = {
		.dest = 0x09,
		.orig = 0x08,
		.resp = 1,
		.error = 0,
		.nat = 0,
		.cmde = FR_TIME_GET,
		.argv = { 0x00, 0x00, 0x27, 0x1a }	// remote time = TIME_1_SEC + 1 * TIME_1_MSEC = 10010 = 0x0000271a
	};

	// set dna list
	DNA.list[0].i2c_addr = 0x07;	// self
	DNA.list[0].type = 0x02;
	DNA.list[1].i2c_addr = 0x06;	// BC
	DNA.list[1].type = 0x03;
	DNA.list[2].i2c_addr = 0x05;	// another IS
	DNA.list[2].type = 0x04;

	// set local time just before the second to elaps
	TIME.t = TIME_1_SEC - 1 * TIME_1_MSEC;

	DPT.tx_ret = OK;
	TSN_run();
	TSN_run();
	TSN_run();
	TSN_run();

	// no FR_TIME_GET frame is sent before the second to elaps
	TIME.t = TIME_1_SEC;
	TSN_run();
	TSN_run();
	TSN_run();
	TSN_run();

	// while waiting for the FR_TIME_GET response
	// modify local time to be in the future compared to the BC
	// so that the time increment will be (TIME_1_MSEC - 1)
	TIME.t = TIME_1_SEC + 2 * TIME_1_MSEC;
	DPT.rx(&fr);
	TSN_run();
	TSN_run();
	TSN_run();
	TSN_run();

	// modify local time to be in the past compared to the BC
	// so that the time increment will be (TIME_1_MSEC + 1 - 1)
	TIME.t = 2 * TIME_1_SEC;
	TSN_run();
	TSN_run();
	TSN_run();

	DPT.rx(&fr);
	TIME.t = 1 * TIME_1_SEC - TIME_1_MSEC;
	TSN_run();
	TSN_run();
}


static void start(void)
{
	TIME.t = 0;
	TSN_init();
}


static void stop(void)
{
}


int main(int argc, char* argv[])
{
	static TEST_list_t l = {
		.start = start,
		.stop = stop,
		.list = {
			{ test_init,			"init" },
			{ test_ignore_unrelated_frame,	"test ignore unrelated frame" },
			{ test_time_ajust,		"test time ajust" },
			{ NULL,				NULL }
		},
	};

	TEST_run(&l, argv[1]);

	return 0;
}
