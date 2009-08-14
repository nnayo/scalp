#include <string.h>	// memset

#include "test.h"

#include "type_def.h"
#include "scalp/dispatcher.h"

#include "scalp/basic.h"

#include "utils/time.h"


// ------------------------------------------------
// stubs
//

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
	if ( len == sizeof(u16) ) {
		TEST_log("eeprom_read_block : addr = 0x%04x, len = %d", addr, len);
		memcpy(data, &EEP.data, len);
	}

	if ( len == sizeof(dpt_frame_t) ) {
		TEST_log("eeprom_read_block : addr = 0x%04x, len = %d", addr, len);
		memcpy(data, &EEP.container, len);
	}
}


void eeprom_write_block(void* data, const void* addr, int len)
{
	u16 dt = *(u16*)data;
	TEST_log("eeprom_write_block : addr = %p, len = %d, data = 0x%04x", addr, len, dt);
}


struct {
	u16 data;
} PGM;


u16 pgm_read_word(u16 addr)
{
	TEST_log("pgm_read_word : addr = 0x%04x", addr);

	return PGM.data;
}

void memcpy_P()
{
}


//----------------------------------------
// public types
//

struct {
	void (*rx)(dpt_frame_t* fr);
	u8 tx_ret;
} DPT;


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


// ------------------------------------------------
// tests suite
//

static void test_init(void)
{
	// check correct initialization
	// DPT_register is called every test via the start function
	TEST_check("DPT_register : channel #0, cmde [0x000007fc]");
	TEST_check("eeprom_read_block : addr = 0x0000, len = 8");

	BSC_run();
	TEST_check("DPT_lock : channel #0");
	TEST_check("DPT_tx : channel #0, fr = { .dest = 0x08, .orig = 0x0b, .cmde = 0x82, .argv = 0x12 34 00 00 }");

	DPT.tx_ret = OK;
	BSC_run();
	TEST_check("DPT_tx : channel #0, fr = { .dest = 0x08, .orig = 0x0b, .cmde = 0x82, .argv = 0x12 34 00 00 }");
	TEST_check("DPT_unlock : channel #0");

	// others do nothing
	BSC_run();
}


static void test_rx_frame(void)
{
	dpt_frame_t fr_error = {
		.dest = 0x0a,
		.orig = 0x07,
		.error = 1,
	};

	dpt_frame_t fr_resp = {
		.dest = 0x0a,
		.orig = 0x07,
		.resp = 1,
	};

	// DPT_register is called every test via the start function
	TEST_check("DPT_register : channel #0, cmde [0x000007fc]");
	TEST_check("eeprom_read_block : addr = 0x0000, len = 8");
	TEST_check("DPT_lock : channel #0");

	DPT.tx_ret = OK;
	DPT.rx(&fr_error);

	BSC_run();
	TEST_check("DPT_tx : channel #0, fr = { .dest = 0x08, .orig = 0x0b, .cmde = 0x82, .argv = 0x12 34 00 00 }");
	BSC_run();
	BSC_run();

	DPT.rx(&fr_resp);

	BSC_run();
	BSC_run();
	BSC_run();
	TEST_check("DPT_unlock : channel #0");
}


static void test_unknown_cmde(void)
{
	dpt_frame_t fr = {
		.dest = 0x0a,
		.orig = 0x07,
		.cmde = 0x15
	};

	// DPT_register is called every test via the start function
	TEST_check("DPT_register : channel #0, cmde [0x000007fc]");
	TEST_check("eeprom_read_block : addr = 0x0000, len = 8");
	TEST_check("DPT_lock : channel #0");

	// first run will send the response to reset frame
	DPT.tx_ret = OK;
	BSC_run();
	TEST_check("DPT_tx : channel #0, fr = { .dest = 0x08, .orig = 0x0b, .cmde = 0x82, .argv = 0x12 34 00 00 }");
	TEST_check("DPT_unlock : channel #0");

	// next runs do nothing
	DPT.tx_ret = KO;
	BSC_run();
	BSC_run();
	BSC_run();

	// receiving a frame with unknown command
	DPT.tx_ret = OK;
	DPT.rx(&fr);
	BSC_run();
	TEST_check("DPT_lock : channel #0");
	TEST_check("DPT_tx : channel #0, fr = { .dest = 0x07, .orig = 0x0a, .cmde = 0xd5, .argv = 0x00 00 00 00 }");


	// dispatcher is unlocked
	BSC_run();
	TEST_check("DPT_unlock : channel #0");
}


static void test_dpt_retry(void)
{
	dpt_frame_t fr = {
		.dest = 0x0a,
		.orig = 0x07,
		.cmde = 0x15
	};

	// DPT_register is called every test via the start function
	TEST_check("DPT_register : channel #0, cmde [0x000007fc]");
	TEST_check("eeprom_read_block : addr = 0x0000, len = 8");
	TEST_check("DPT_lock : channel #0");
	TEST_check("DPT_tx : channel #0, fr = { .dest = 0x08, .orig = 0x0b, .cmde = 0x82, .argv = 0x12 34 00 00 }");

	// first run will send reset response
	DPT.tx_ret = OK;
	BSC_run();
	TEST_check("DPT_unlock : channel #0");

	// next runs do nothing
	DPT.tx_ret = KO;
	BSC_run();
	BSC_run();
	BSC_run();

	// enqueue 1 frame
	DPT.rx(&fr);

	// first try to send the response is rejected
	DPT.tx_ret = KO;
	BSC_run();
	TEST_check("DPT_lock : channel #0");
	TEST_check("DPT_tx : channel #0, fr = { .dest = 0x07, .orig = 0x0a, .cmde = 0xd5, .argv = 0x00 00 00 00 }");

	// next try is successful
	DPT.tx_ret = OK;
	BSC_run();
	TEST_check("DPT_tx : channel #0, fr = { .dest = 0x07, .orig = 0x0a, .cmde = 0xd5, .argv = 0x00 00 00 00 }");

	// dispatcher is unlocked
	BSC_run();
	TEST_check("DPT_unlock : channel #0");
}


static void test_rx_fifo_deepness(void)
{
	dpt_frame_t fr = {
		.dest = 0x0a,
		.orig = 0x07,
		.cmde = 0x15
	};

	// DPT_register is called every test via the start function
	TEST_check("DPT_register : channel #0, cmde [0x000007fc]");
	TEST_check("eeprom_read_block : addr = 0x0000, len = 8");
	TEST_check("DPT_lock : channel #0");
	TEST_check("DPT_tx : channel #0, fr = { .dest = 0x08, .orig = 0x0b, .cmde = 0x82, .argv = 0x12 34 00 00 }");

	// first run will send reset response
	DPT.tx_ret = OK;
	BSC_run();
	TEST_check("DPT_unlock : channel #0");

	// next runs do nothing
	DPT.tx_ret = KO;
	BSC_run();
	BSC_run();
	BSC_run();

	DPT.tx_ret = OK;

	// enqueue 4 frames
	DPT.rx(&fr);
	TEST_check("DPT_lock : channel #0");
	DPT.rx(&fr);
	TEST_check("DPT_lock : channel #0");
	DPT.rx(&fr);
	TEST_check("DPT_lock : channel #0");
	DPT.rx(&fr);
	TEST_check("DPT_lock : channel #0");

	// only 3 frames are treated (because incoming fifo is only 3 frames deep)
	BSC_run();
	TEST_check("DPT_tx : channel #0, fr = { .dest = 0x07, .orig = 0x0a, .cmde = 0xd5, .argv = 0x00 00 00 00 }");
	BSC_run();
	TEST_check("DPT_tx : channel #0, fr = { .dest = 0x07, .orig = 0x0a, .cmde = 0xd5, .argv = 0x00 00 00 00 }");
	BSC_run();
	TEST_check("DPT_tx : channel #0, fr = { .dest = 0x07, .orig = 0x0a, .cmde = 0xd5, .argv = 0x00 00 00 00 }");

	// dispatcher is unlocked
	BSC_run();
	TEST_check("DPT_unlock : channel #0");
}


static void test_no_cmde(void)
{
	dpt_frame_t fr = {
		.dest = 0x0a,
		.orig = 0x07,
		.cmde = FR_NO_CMDE
	};

	// DPT_register is called every test via the start function
	TEST_check("DPT_register : channel #0, cmde [0x000007fc]");
	TEST_check("eeprom_read_block : addr = 0x0000, len = 8");
	TEST_check("DPT_lock : channel #0");

	// first run will send reset response
	DPT.tx_ret = OK;
	BSC_run();
	TEST_check("DPT_tx : channel #0, fr = { .dest = 0x08, .orig = 0x0b, .cmde = 0x82, .argv = 0x12 34 00 00 }");

	// next runs do nothing
	DPT.tx_ret = KO;
	BSC_run();
	TEST_check("DPT_unlock : channel #0");
	BSC_run();
	BSC_run();

	DPT.tx_ret = OK;
	DPT.rx(&fr);
	TEST_check("DPT_lock : channel #0");

	BSC_run();
	TEST_check("DPT_tx : channel #0, fr = { .dest = 0x07, .orig = 0x0a, .cmde = 0x82, .argv = 0x00 00 00 00 }");

	// dispatcher is unlocked
	BSC_run();
	TEST_check("DPT_unlock : channel #0");
}


// RAM read and write can't be done due to memory space addressing difference
// on AVR, it is only 16 bit long
// on PC, it is 32 or 64 bit long

static void test_eeprom_read(void)
{
	dpt_frame_t fr = {
		.dest = 0x0a,
		.orig = 0x07,
		.cmde = FR_EEP_READ,
		.argv = { 0x12, 0x34, 0xab, 0xcd }
	};

	// DPT_register is called every test via the start function
	TEST_check("DPT_register : channel #0, cmde [0x000007fc]");
	TEST_check("eeprom_read_block : addr = 0x0000, len = 8");
	TEST_check("DPT_lock : channel #0");

	// first run will send reset response
	DPT.tx_ret = OK;
	BSC_run();
	TEST_check("DPT_tx : channel #0, fr = { .dest = 0x08, .orig = 0x0b, .cmde = 0x82, .argv = 0x12 34 00 00 }");

	// next runs do nothing
	DPT.tx_ret = KO;
	BSC_run();
	TEST_check("DPT_unlock : channel #0");
	BSC_run();
	BSC_run();

	DPT.rx(&fr);
	TEST_check("DPT_lock : channel #0");

	EEP.data = 0x6789;
	DPT.tx_ret = OK;
	BSC_run();
	TEST_check("eeprom_read_block : addr = 0x1234, len = 2");
	TEST_check("DPT_tx : channel #0, fr = { .dest = 0x07, .orig = 0x0a, .cmde = 0x85, .argv = 0x12 34 67 89 }");

	// dispatcher is unlocked
	BSC_run();
	TEST_check("DPT_unlock : channel #0");
}


static void test_eeprom_write(void)
{
	dpt_frame_t fr = {
		.dest = 0x0a,
		.orig = 0x07,
		.cmde = FR_EEP_WRITE,
		.argv = { 0x12, 0x34, 0xab, 0xcd }
	};

	// DPT_register is called every test via the start function
	TEST_check("DPT_register : channel #0, cmde [0x000007fc]");
	TEST_check("eeprom_read_block : addr = 0x0000, len = 8");

	// first run will send reset response
	DPT.tx_ret = OK;
	BSC_run();
	TEST_check("DPT_lock : channel #0");
	TEST_check("DPT_tx : channel #0, fr = { .dest = 0x08, .orig = 0x0b, .cmde = 0x82, .argv = 0x12 34 00 00 }");

	// next runs do nothing
	DPT.tx_ret = KO;
	BSC_run();
	TEST_check("DPT_unlock : channel #0");
	BSC_run();
	BSC_run();

	DPT.rx(&fr);
	TEST_check("DPT_lock : channel #0");

	EEP.data = 0x6789;
	DPT.tx_ret = OK;
	BSC_run();
	TEST_check("eeprom_write_block : addr = 0x1234, len = 2, data = 0xabcd");
	TEST_check("eeprom_read_block : addr = 0x1234, len = 2");
	TEST_check("DPT_tx : channel #0, fr = { .dest = 0x07, .orig = 0x0a, .cmde = 0x86, .argv = 0x12 34 67 89 }");

	// dispatcher is unlocked
	BSC_run();
	TEST_check("DPT_unlock : channel #0");
}


static void test_flash_read(void)
{
	dpt_frame_t fr = {
		.dest = 0x0a,
		.orig = 0x07,
		.cmde = FR_FLH_READ,
		.argv = { 0x12, 0x34, 0xab, 0xcd }
	};

	// DPT_register is called every test via the start function
	TEST_check("DPT_register : channel #0, cmde [0x000007fc]");
	TEST_check("eeprom_read_block : addr = 0x0000, len = 8");
	TEST_check("DPT_lock : channel #0");

	// first run will send reset response
	DPT.tx_ret = OK;
	BSC_run();
	TEST_check("DPT_tx : channel #0, fr = { .dest = 0x08, .orig = 0x0b, .cmde = 0x82, .argv = 0x12 34 00 00 }");

	// next runs do nothing
	DPT.tx_ret = KO;
	BSC_run();
	TEST_check("DPT_unlock : channel #0");
	BSC_run();
	BSC_run();

	DPT.rx(&fr);
	TEST_check("DPT_lock : channel #0");

	PGM.data = 0x6789;
	DPT.tx_ret = OK;
	BSC_run();
	TEST_check("pgm_read_word : addr = 0x1234");
	TEST_check("DPT_tx : channel #0, fr = { .dest = 0x07, .orig = 0x0a, .cmde = 0x87, .argv = 0x12 34 67 89 }");

	// dispatcher is unlocked
	BSC_run();
	TEST_check("DPT_unlock : channel #0");
}


static void test_flash_write(void)
{
	dpt_frame_t fr = {
		.dest = 0x0a,
		.orig = 0x07,
		.cmde = FR_FLH_WRITE,
		.argv = { 0x12, 0x34, 0xab, 0xcd }
	};

	// DPT_register is called every test via the start function
	TEST_check("DPT_register : channel #0, cmde [0x000007fc]");
	TEST_check("eeprom_read_block : addr = 0x0000, len = 8");
	TEST_check("DPT_lock : channel #0");

	// first run will send reset response
	DPT.tx_ret = OK;
	BSC_run();
	TEST_check("DPT_tx : channel #0, fr = { .dest = 0x08, .orig = 0x0b, .cmde = 0x82, .argv = 0x12 34 00 00 }");

	// next runs do nothing
	DPT.tx_ret = KO;
	BSC_run();
	TEST_check("DPT_unlock : channel #0");
	BSC_run();
	BSC_run();

	DPT.rx(&fr);
	TEST_check("DPT_lock : channel #0");

	PGM.data = 0x6789;
	DPT.tx_ret = OK;
	BSC_run();
	TEST_check("DPT_tx : channel #0, fr = { .dest = 0x07, .orig = 0x0a, .cmde = 0xc8, .argv = 0x12 34 00 00 }");

	// dispatcher is unlocked
	BSC_run();
	TEST_check("DPT_unlock : channel #0");
}


static void test_wait(void)
{
	int i;
	dpt_frame_t fr = {
		.dest = 0x0a,
		.orig = 0x07,
		.cmde = FR_WAIT,
		.argv = { 0x00, 0x0a, 0xab, 0xcd }
	};

	// DPT_register is called every test via the start function
	TEST_check("DPT_register : channel #0, cmde [0x000007fc]");
	TEST_check("eeprom_read_block : addr = 0x0000, len = 8");
	TEST_check("DPT_lock : channel #0");

	// first run will send reset response
	DPT.tx_ret = OK;
	BSC_run();
	TEST_check("DPT_tx : channel #0, fr = { .dest = 0x08, .orig = 0x0b, .cmde = 0x82, .argv = 0x12 34 00 00 }");

	// next runs do nothing
	DPT.tx_ret = KO;
	BSC_run();
	TEST_check("DPT_unlock : channel #0");
	BSC_run();
	BSC_run();

	DPT.rx(&fr);
	TEST_check("DPT_lock : channel #0");

	TIME.t = 10000;
	DPT.tx_ret = OK;

	BSC_run();
	TEST_check("TIME_get : t = 10010");

	// run until the time_out elapses
	// and finally the dispatcher is unlocked after the response is sent
	for ( i = 0; i < 11; i++) {
		TEST_check("TIME_get : t = %5d", TIME.t);
		BSC_run();
	}
	//TEST_check("TIME_get : t = 10110");
	TEST_check("DPT_tx : channel #0, fr = { .dest = 0x07, .orig = 0x0a, .cmde = 0x89, .argv = 0x00 0a 00 00 }");
	BSC_run();
	TEST_check("DPT_unlock : channel #0");

	BSC_run();
}


static void test_cmde_sequence_with_wait(void)
{
	int i;

	dpt_frame_t fr_no_cmde = {
		.dest = 0x0a,
		.orig = 0x07,
		.cmde = FR_NO_CMDE
	};

	dpt_frame_t fr_wait = {
		.dest = 0x0a,
		.orig = 0x07,
		.cmde = FR_WAIT,
		.argv = { 0x00, 0x0a, 0xab, 0xcd }
	};

	// DPT_register is called every test via the start function
	TEST_check("DPT_register : channel #0, cmde [0x000007fc]");
	TEST_check("eeprom_read_block : addr = 0x0000, len = 8");
	TEST_check("DPT_lock : channel #0");

	// first run will send reset response
	DPT.tx_ret = OK;
	BSC_run();
	TEST_check("DPT_tx : channel #0, fr = { .dest = 0x08, .orig = 0x0b, .cmde = 0x82, .argv = 0x12 34 00 00 }");

	// next runs do nothing
	DPT.tx_ret = KO;
	BSC_run();
	TEST_check("DPT_unlock : channel #0");
	BSC_run();
	BSC_run();

	DPT.rx(&fr_no_cmde);
	DPT.rx(&fr_wait);
	DPT.rx(&fr_no_cmde);
	TEST_check("DPT_lock : channel #0");
	TEST_check("DPT_lock : channel #0");
	TEST_check("DPT_lock : channel #0");

	TIME.t = 10000;
	DPT.tx_ret = OK;

	BSC_run();
	TEST_check("DPT_tx : channel #0, fr = { .dest = 0x07, .orig = 0x0a, .cmde = 0x82, .argv = 0x00 00 00 00 }");
	TEST_check("TIME_get : t = 10010");

	// run until the time_out elapses
	// and finally the dispatcher is unlocked after the response is sent
	for ( i = 0; i < 11; i++) {
		BSC_run();
		TEST_check("TIME_get : t = %5d", TIME.t);
	}

	TEST_check("DPT_tx : channel #0, fr = { .dest = 0x07, .orig = 0x0a, .cmde = 0x89, .argv = 0x00 0a 00 00 }");
	BSC_run();
	TEST_check("DPT_tx : channel #0, fr = { .dest = 0x07, .orig = 0x0a, .cmde = 0x82, .argv = 0x00 00 00 00 }");
	BSC_run();
	TEST_check("DPT_unlock : channel #0");
}


static void test_container(void)
{
	dpt_frame_t container = {
		.dest = 0x0b,
		.orig = 0x08,
		.t_id = 0,
		.cmde = FR_NO_CMDE,
		.argv = { 0x12, 0x34, 0x56, 0x78 }
	};

	dpt_frame_t fr = {
		.dest = 0x0a,
		.orig = 0x07,
		.t_id = 0,
		.cmde = FR_CONTAINER,
		.argv = {  0x00, 0x08, 0x03, 0xee}
	};

	// DPT_register is called every test via the start function
	TEST_check("DPT_register : channel #0, cmde [0x000007fc]");
	TEST_check("eeprom_read_block : addr = 0x0000, len = 8");
	TEST_check("DPT_lock : channel #0");

	// first run will send reset response
	DPT.tx_ret = OK;
	BSC_run();
	TEST_check("DPT_tx : channel #0, fr = { .dest = 0x08, .orig = 0x0b, .cmde = 0x82, .argv = 0x12 34 00 00 }");

	// next runs do nothing
	DPT.tx_ret = KO;
	BSC_run();
	TEST_check("DPT_unlock : channel #0");
	BSC_run();
	BSC_run();

	DPT.rx(&fr);
	TEST_check("DPT_lock : channel #0");

	EEP.container = container;
	DPT.tx_ret = OK;
	BSC_run();
	TEST_check("eeprom_read_block : addr = 0x0008, len = 8");
	TEST_check("eeprom_read_block : addr = 0x0010, len = 8");
	TEST_check("eeprom_read_block : addr = 0x0018, len = 8");
	BSC_run();
	TEST_check("DPT_tx : channel #0, fr = { .dest = 0x0b, .orig = 0x08, .cmde = 0x02, .argv = 0x12 34 56 78 }");
	BSC_run();
	TEST_check("DPT_tx : channel #0, fr = { .dest = 0x0b, .orig = 0x08, .cmde = 0x02, .argv = 0x12 34 56 78 }");
	BSC_run();
	TEST_check("DPT_tx : channel #0, fr = { .dest = 0x0b, .orig = 0x08, .cmde = 0x02, .argv = 0x12 34 56 78 }");
	BSC_run();
	TEST_check("DPT_tx : channel #0, fr = { .dest = 0x07, .orig = 0x0a, .cmde = 0x8a, .argv = 0x00 08 00 00 }");

	// dispatcher is unlocked
	BSC_run();
	TEST_check("DPT_unlock : channel #0");
}


static void start(void)
{
	dpt_frame_t container = {
		.dest = 0x0b,
		.orig = 0x08,
		.t_id = 0x00,
		.resp = 0,
		.error = 0,
		.nat = 0,
		.cmde = FR_NO_CMDE,
		.argv = { 0x12, 0x34, 0x56, 0x78 }
	};

	EEP.container = container;

	DPT.tx_ret = KO;

	TIME.t = 7777777;
	BSC_init();
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
			{ test_init,					"init" },
			{ test_rx_frame,				"test rx frame" },
			{ test_unknown_cmde,			"test unknown cmde" },
			{ test_dpt_retry,				"test dpt retry" },
			{ test_rx_fifo_deepness,		"test rx fifo deepness" },
			{ test_no_cmde,					"test no cmde" },
			{ test_eeprom_read,				"test eeprom read" },
			{ test_eeprom_write,			"test eeprom write" },
			{ test_flash_read,				"test flash read" },
			{ test_flash_write,				"test flash write" },
			{ test_wait,					"test wait" },
			{ test_cmde_sequence_with_wait,	"test cmde sequence with wait" },
			{ test_container,				"test container" },
			{ NULL,							NULL }
		},
	};

	TEST_run(&l, argv[1]);

	return 0;
}
