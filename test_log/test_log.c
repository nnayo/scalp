#include <string.h>

#include "test.h"

#include "type_def.h"

#include "scalp/log.h"
#include "scalp/dispatcher.h"

#include "utils/time.h"



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
} TIME;


u32 TIME_get(void)
{
	TEST_log("TIME_get : t = %d", TIME.t);

	TIME.t += TIME_1_MSEC;
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


// ------------------------------------------------
// tests suite
//

static void test_init(void)
{
	// check correct initialization
	// DPT_register is called every test via the start function
	TEST_check("DPT_register : channel #6, cmde [0x00000060]");
}


static void test_scan_empty_log(void)
{
	TEST_check("DPT_register : channel #6, cmde [0x00000060]");
	TEST_check("DPT_lock : channel #6");
	TEST_check("DPT_tx : channel #6, fr = { .dest = 0x01, .orig = 0x01, .cmde = 0x05, .argv = 0x00 9b 00 00 }");
	TEST_check("DPT_tx : channel #6, fr = { .dest = 0x01, .orig = 0x01, .cmde = 0x05, .argv = 0x00 9b 00 00 }");

	dpt_frame_t fr_empty_log = {
		.dest = 0x01,
		.orig = 0x01,
		.resp = 1,
		.error = 0,
		.nat = 0,
		.cmde = FR_EEP_READ,
		.argv = { 0x00, 0x9b, 0xff, 0xff }
	};


	// read request at start of eeprom log zone
	LOG_run();
	DPT.tx_ret = OK;
	LOG_run();

	// request response with empty eeprom
	DPT.rx(&fr_empty_log);

	LOG_run();
	LOG_run();
}


static void test_scan_not_empty_log(void)
{
	TEST_check("DPT_register : channel #6, cmde [0x00000060]");
	TEST_check("DPT_lock : channel #6");
	TEST_check("DPT_tx : channel #6, fr = { .dest = 0x01, .orig = 0x01, .cmde = 0x05, .argv = 0x00 9b 00 00 }");
	TEST_check("DPT_tx : channel #6, fr = { .dest = 0x01, .orig = 0x01, .cmde = 0x05, .argv = 0x00 a2 00 00 }");
	TEST_check("DPT_tx : channel #6, fr = { .dest = 0x01, .orig = 0x01, .cmde = 0x05, .argv = 0x00 a9 00 00 }");

	dpt_frame_t fr_not_empty_log_1 = {
		.dest = 0x01,
		.orig = 0x01,
		.resp = 1,
		.error = 0,
		.nat = 0,
		.cmde = FR_EEP_READ,
		.argv = { 0x00, 0x9b, 0x00, 0x00 }
	};

	dpt_frame_t fr_not_empty_log_2 = {
		.dest = 0x01,
		.orig = 0x01,
		.resp = 1,
		.error = 0,
		.nat = 0,
		.cmde = FR_EEP_READ,
		.argv = { 0x00, 0xa2, 0x00, 0x00 }
	};

	dpt_frame_t fr_empty_log = {
		.dest = 0x01,
		.orig = 0x01,
		.resp = 1,
		.error = 0,
		.nat = 0,
		.cmde = FR_EEP_READ,
		.argv = { 0x00, 0xa9, 0xff, 0xff }
	};


	// read request at start of eeprom log zone
	LOG_run();
	DPT.tx_ret = OK;
	LOG_run();

	// request response with not empty eeprom
	DPT.rx(&fr_not_empty_log_1);
	LOG_run();
	LOG_run();

	DPT.rx(&fr_not_empty_log_2);
	LOG_run();
	LOG_run();

	DPT.rx(&fr_empty_log);
	LOG_run();
	LOG_run();
}


static void test_log_cmde(void)
{
	TEST_check("DPT_register : channel #6, cmde [0x00000060]");
	TEST_check("DPT_lock : channel #6");
	TEST_check("DPT_tx : channel #6, fr = { .dest = 0x01, .orig = 0x01, .cmde = 0x05, .argv = 0x00 9b 00 00 }");
	TEST_check("DPT_tx : channel #6, fr = { .dest = 0x01, .orig = 0x01, .cmde = 0x05, .argv = 0x00 a2 00 00 }");
	TEST_check("DPT_tx : channel #6, fr = { .dest = 0x01, .orig = 0x01, .cmde = 0x05, .argv = 0x00 a9 00 00 }");
	TEST_check("DPT_tx : channel #6, fr = { .dest = 0x01, .orig = 0x01, .cmde = 0x06, .argv = 0x00 a9 1a 0e }");
	TEST_check("TIME_get : t = 10000");
	TEST_check("DPT_tx : channel #6, fr = { .dest = 0x01, .orig = 0x01, .cmde = 0x06, .argv = 0x00 ab 00 27 }");
	TEST_check("DPT_tx : channel #6, fr = { .dest = 0x01, .orig = 0x01, .cmde = 0x06, .argv = 0x00 ad 00 07 }");
	TEST_check("DPT_tx : channel #6, fr = { .dest = 0x01, .orig = 0x01, .cmde = 0x06, .argv = 0x00 af f0 ff }");

	dpt_frame_t fr_not_empty_log_1 = {
		.dest = 0x01,
		.orig = 0x01,
		.resp = 1,
		.error = 0,
		.nat = 0,
		.cmde = FR_EEP_READ,
		.argv = { 0x00, 0x9b, 0x00, 0x00 }
	};

	dpt_frame_t fr_not_empty_log_2 = {
		.dest = 0x01,
		.orig = 0x01,
		.resp = 1,
		.error = 0,
		.nat = 0,
		.cmde = FR_EEP_READ,
		.argv = { 0x00, 0xa2, 0x00, 0x00 }
	};

	dpt_frame_t fr_empty_log = {
		.dest = 0x01,
		.orig = 0x01,
		.resp = 1,
		.error = 0,
		.nat = 0,
		.cmde = FR_EEP_READ,
		.argv = { 0x00, 0xa9, 0xff, 0xff }
	};

	dpt_frame_t fr_cmde_state = {
		.dest = 0x01,
		.orig = 0x0a,
		.resp = 0,
		.error = 0,
		.nat = 0,
		.cmde = FR_STATE,
		.argv = { 0x00, 0x07, 0xf0, 0xff }
	};

	dpt_frame_t fr_eep_write_resp_1 = {
		.dest = 0x01,
		.orig = 0x01,
		.resp = 1,
		.error = 0,
		.nat = 0,
		.cmde = FR_EEP_WRITE,
		.argv = { 0x00, 0xa9, 0x1a, 0x0e }
	};

	dpt_frame_t fr_eep_write_resp_2 = {
		.dest = 0x01,
		.orig = 0x01,
		.resp = 1,
		.error = 0,
		.nat = 0,
		.cmde = FR_EEP_WRITE,
		.argv = { 0x00, 0xab, 0x00, 0x27 }
	};

	dpt_frame_t fr_eep_write_resp_3 = {
		.dest = 0x01,
		.orig = 0x01,
		.resp = 1,
		.error = 0,
		.nat = 0,
		.cmde = FR_EEP_WRITE,
		.argv = { 0x00, 0xad, 0x00, 0x07 }
	};

	dpt_frame_t fr_eep_write_resp_4 = {
		.dest = 0x01,
		.orig = 0x01,
		.resp = 1,
		.error = 0,
		.nat = 0,
		.cmde = FR_EEP_WRITE,
		.argv = { 0x00, 0xaf, 0xf0, 0xff }
	};


	// read request at start of eeprom log zone
	LOG_run();
	DPT.tx_ret = OK;
	LOG_run();

	// request response with not empty eeprom
	DPT.rx(&fr_not_empty_log_1);
	LOG_run();
	LOG_run();

	DPT.rx(&fr_not_empty_log_2);
	LOG_run();
	LOG_run();

	DPT.rx(&fr_empty_log);
	LOG_run();
	LOG_run();

	DPT.rx(&fr_cmde_state);
	LOG_run();
	LOG_run();

	TIME.t = 10000;
	DPT.rx(&fr_eep_write_resp_1);
	LOG_run();
	LOG_run();

	DPT.rx(&fr_eep_write_resp_2);
	LOG_run();
	LOG_run();

	DPT.rx(&fr_eep_write_resp_3);
	LOG_run();
	LOG_run();

	DPT.rx(&fr_eep_write_resp_4);
	LOG_run();
	LOG_run();

	LOG_run();
	LOG_run();
	LOG_run();
	LOG_run();
	LOG_run();
}


static void start(void)
{
	LOG_init();
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
			{ test_scan_empty_log,		"test scan empty log" },
			{ test_scan_not_empty_log,	"test scan not empty log" },
			{ test_log_cmde,		"test log cmde" },
			{ NULL,				NULL }
		},
	};

	TEST_run(&l, argv[1]);

	return 0;
}
