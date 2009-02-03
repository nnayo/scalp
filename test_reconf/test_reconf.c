#include <string.h>

#include "test.h"

#include "type_def.h"

#include "scalp/dispatcher.h"
#include "scalp/reconf.h"

#include "utils/time.h"

#include "avr/io.h"


// ------------------------------------------------
// stubs
//

//----------------------------------------
// public defines
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
u8 PIND;
u8 DDRD;


// ------------------------------------------------
// tests suite
//

static void test_init(void)
{
	// check correct initialization
	// DPT_register is called every test via the start function
	TEST_check("DPT_register : channel #1, cmde [0x00060000]");

	// while the bus state doesn't change, nothing shall happen
	RCF_run();
}


static void test_bus_none2nominal(void)
{
	int i;

	// DPT_register is called every test via the start function
	TEST_check("DPT_register : channel #1, cmde [0x00060000]");
	TEST_check("DPT_lock : channel #1");

	for ( i = 0; i < 100; i++ )
		RCF_run();

	PIND = _BV(PD4);
	DPT.tx_ret = KO;	// test DPT_tx retry handling
	RCF_run();
	TEST_check("DPT_tx : channel #1, fr = { .dest = 0x01, .orig = 0x01, .cmde = 0x0a, .argv = 0x00 08 01 00 }");
	DPT.tx_ret = OK;
	RCF_run();
	TEST_check("DPT_tx : channel #1, fr = { .dest = 0x01, .orig = 0x01, .cmde = 0x0a, .argv = 0x00 08 01 00 }");
	TEST_check("DPT_unlock : channel #1");

	for ( i = 0; i < 100; i++ )
		RCF_run();
}


static void test_bus_none2redundant(void)
{
	int i;

	// DPT_register is called every test via the start function
	TEST_check("DPT_register : channel #1, cmde [0x00060000]");
		//"eeprom_read_block : addr = 0x000e, len = 7");
	TEST_check("DPT_lock : channel #1");

	DPT.tx_ret = OK;

	for ( i = 0; i < 100; i++ )
		RCF_run();

	PIND = _BV(PD5);
		//"DPT_tx : channel #1, fr = { .dest = 0x07, .orig = 0xde, .cmde = 0x05, .argv = 0x12 34 56 78 }");
	TEST_check("DPT_tx : channel #1, fr = { .dest = 0x01, .orig = 0x01, .cmde = 0x0a, .argv = 0x00 10 01 00 }");
	TEST_check("DPT_unlock : channel #1");

	for ( i = 0; i < 100; i++ )
		RCF_run();
}


static void test_bus_none2nom_n_red(void)
{
	int i;

	// DPT_register is called every test via the start function
	TEST_check("DPT_register : channel #1, cmde [0x00060000]");
		//"eeprom_read_block : addr = 0x0007, len = 7");
	TEST_check("DPT_lock : channel #1");

	DPT.tx_ret = OK;

	for ( i = 0; i < 100; i++ )
		RCF_run();

	PIND = _BV(PD5)|_BV(PD4);
		//"DPT_tx : channel #1, fr = { .dest = 0x07, .orig = 0xde, .cmde = 0x05, .argv = 0x12 34 56 78 }");
	TEST_check("DPT_tx : channel #1, fr = { .dest = 0x01, .orig = 0x01, .cmde = 0x0a, .argv = 0x00 08 01 00 }");
	TEST_check("DPT_unlock : channel #1");

	for ( i = 0; i < 100; i++ )
		RCF_run();
}


static void test_bus_nom_n_red2red(void)
{
	int i;

	// DPT_register is called every test via the start function
	TEST_check("DPT_register : channel #1, cmde [0x00060000]");
		//"eeprom_read_block : addr = 0x0007, len = 7");
	TEST_check("DPT_lock : channel #1");

	DPT.tx_ret = OK;

	for ( i = 0; i < 100; i++ )
		RCF_run();

	PIND = _BV(PD5)|_BV(PD4);
		//"DPT_tx : channel #1, fr = { .dest = 0x07, .orig = 0xde, .cmde = 0x05, .argv = 0x12 34 56 78 }");
	TEST_check("DPT_tx : channel #1, fr = { .dest = 0x01, .orig = 0x01, .cmde = 0x0a, .argv = 0x00 08 01 00 }");
	TEST_check("DPT_unlock : channel #1");
		//"eeprom_read_block : addr = 0x000e, len = 7");

	for ( i = 0; i < 100; i++ )
		RCF_run();

	PIND = _BV(PD5);
	TEST_check("DPT_lock : channel #1");
		//"DPT_tx : channel #1, fr = { .dest = 0x08, .orig = 0xdd, .cmde = 0x03, .argv = 0x87 65 43 21 }");
	TEST_check("DPT_tx : channel #1, fr = { .dest = 0x01, .orig = 0x01, .cmde = 0x0a, .argv = 0x00 10 01 00 }");
	TEST_check("DPT_unlock : channel #1");

	for ( i = 0; i < 100; i++ )
		RCF_run();
}


static void test_bus_nom_n_red2nom(void)
{
	int i;

	// DPT_register is called every test via the start function
	TEST_check("DPT_register : channel #1, cmde [0x00060000]");
		//"eeprom_read_block : addr = 0x0007, len = 7");
	TEST_check("DPT_lock : channel #1");

	DPT.tx_ret = OK;

	for ( i = 0; i < 100; i++ )
		RCF_run();

	PIND = _BV(PD5)|_BV(PD4);
		//"DPT_tx : channel #1, fr = { .dest = 0x07, .orig = 0xde, .cmde = 0x05, .argv = 0x12 34 56 78 }");
	TEST_check("DPT_tx : channel #1, fr = { .dest = 0x01, .orig = 0x01, .cmde = 0x0a, .argv = 0x00 08 01 00 }");
	TEST_check("DPT_unlock : channel #1");

	for ( i = 0; i < 100; i++ )
		RCF_run();

	PIND = _BV(PD4);

	for ( i = 0; i < 100; i++ )
		RCF_run();
}


static void test_bus_nom_n_red2none(void)
{
	int i;

	// DPT_register is called every test via the start function
	TEST_check("DPT_register : channel #1, cmde [0x00060000]");
		//"eeprom_read_block : addr = 0x0007, len = 7");
	TEST_check("DPT_lock : channel #1");

	DPT.tx_ret = OK;

	for ( i = 0; i < 100; i++ )
		RCF_run();

	PIND = _BV(PD5)|_BV(PD4);
		//"DPT_tx : channel #1, fr = { .dest = 0x07, .orig = 0xde, .cmde = 0x05, .argv = 0x12 34 56 78 }");
	TEST_check("DPT_tx : channel #1, fr = { .dest = 0x01, .orig = 0x01, .cmde = 0x0a, .argv = 0x00 08 01 00 }");
	TEST_check("DPT_unlock : channel #1");
		//"eeprom_read_block : addr = 0x0015, len = 7");

	for ( i = 0; i < 100; i++ )
		RCF_run();

	PIND = 0x00;
	TEST_check("DPT_lock : channel #1");
		//"DPT_tx : channel #1, fr = { .dest = 0x08, .orig = 0xdd, .cmde = 0x03, .argv = 0x87 65 43 21 }");
	TEST_check("DPT_tx : channel #1, fr = { .dest = 0x01, .orig = 0x01, .cmde = 0x0a, .argv = 0x00 18 01 00 }");
	TEST_check("DPT_unlock : channel #1");

	for ( i = 0; i < 100; i++ )
		RCF_run();
}


static void test_rx_take_off_when_nom_n_red(void)
{
	int i;
	dpt_frame_t fr_take_off = {
		.dest = 0x07,
		.orig = 0xde,
		.resp = 1,
		.error = 1,
		.nat = 0,
		.cmde = 0x12,
		.argv = { 0x00, 0x00, 0x00, 0x00 }
	};

	// DPT_register is called every test via the start function
	TEST_check("DPT_register : channel #1, cmde [0x00060000]");
	TEST_check("DPT_lock : channel #1");

	DPT.tx_ret = OK;

	for ( i = 0; i < 100; i++ )
		RCF_run();

	PIND = _BV(PD5)|_BV(PD4);
	TEST_check("DPT_tx : channel #1, fr = { .dest = 0x01, .orig = 0x01, .cmde = 0x0a, .argv = 0x00 08 01 00 }");
	TEST_check("DPT_unlock : channel #1");

	for ( i = 0; i < 100; i++ )
		RCF_run();

	DPT.rx(&fr_take_off);

	for ( i = 0; i < 100; i++ )
		RCF_run();
}


static void test_rx_take_off_when_none(void)
{
	int i;
	dpt_frame_t fr_take_off = {
		.dest = 0x07,
		.orig = 0xde,
		.resp = 1,
		.error = 1,
		.cmde = FR_MINUT_TAKE_OFF,
		.argv = { 0x00, 0x00, 0x00, 0x00 }
	};

	// DPT_register is called every test via the start function
	TEST_check("DPT_register : channel #1, cmde [0x00060000]");
	TEST_check("DPT_lock : channel #1");

	DPT.tx_ret = OK;

	for ( i = 0; i < 100; i++ )
		RCF_run();

	DPT.rx(&fr_take_off);
	TEST_check("DPT_tx : channel #1, fr = { .dest = 0x01, .orig = 0x01, .cmde = 0x12, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_unlock : channel #1");

	for ( i = 0; i < 100; i++ )
		RCF_run();
}


static void test_rx_set_get_force_bus_mode(void)
{
	int i;
	dpt_frame_t fr_set_reconf_mode_nom = {
		.dest = 0x07,
		.orig = 0xde,
		.cmde = FR_RECONF_FORCE_MODE,
		.argv = { 0x00, 0x00, 0x00, 0x00 }
	};

	dpt_frame_t fr_set_reconf_mode_red = {
		.dest = 0x07,
		.orig = 0xde,
		.cmde = FR_RECONF_FORCE_MODE,
		.argv = { 0x00, 0x01, 0x00, 0x00 }
	};

	dpt_frame_t fr_set_reconf_mode_none = {
		.dest = 0x07,
		.orig = 0xde,
		.cmde = FR_RECONF_FORCE_MODE,
		.argv = { 0x00, 0x02, 0x00, 0x00 }
	};

	dpt_frame_t fr_set_reconf_mode_auto = {
		.dest = 0x07,
		.orig = 0xde,
		.cmde = FR_RECONF_FORCE_MODE,
		.argv = { 0x00, 0x03, 0x00, 0x00 }
	};

	dpt_frame_t fr_get_reconf_mode = {
		.dest = 0x07,
		.orig = 0xab,
		.cmde = FR_RECONF_FORCE_MODE,
		.argv = { 0xff, 0x00, 0x00, 0x00 }
	};

	dpt_frame_t fr;

	// DPT_register is called every test via the start function
	TEST_check("DPT_register : channel #1, cmde [0x00060000]");
		// get response
	TEST_check("DPT_lock : channel #1");

	DPT.tx_ret = OK;
	for ( i = 0; i < 100; i++ )
		RCF_run();

	fr = fr_get_reconf_mode;
	DPT.rx(&fr);
	for ( i = 0; i < 100; i++ )
		RCF_run();
	TEST_check("DPT_tx : channel #1, fr = { .dest = 0xab, .orig = 0x07, .cmde = 0x91, .argv = 0xff 03 02 00 }");
	TEST_check("DPT_unlock : channel #1");

		// set force mode = NOM
	TEST_check("DPT_lock : channel #1");
	TEST_check("DPT_tx : channel #1, fr = { .dest = 0xde, .orig = 0x07, .cmde = 0x91, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_unlock : channel #1");
		//"eeprom_read_block : addr = 0x0007, len = 7");

	fr = fr_set_reconf_mode_nom;
	DPT.rx(&fr);
	for ( i = 0; i < 100; i++ )
		RCF_run();
	TEST_check("DPT_lock : channel #1");
		//"DPT_tx : channel #1, fr = { .dest = 0x07, .orig = 0xde, .cmde = 0x05, .argv = 0x12 34 56 78 }");
	TEST_check("DPT_tx : channel #1, fr = { .dest = 0x01, .orig = 0x01, .cmde = 0x0a, .argv = 0x00 08 01 00 }");
	TEST_check("DPT_unlock : channel #1");
		// get response
	TEST_check("DPT_lock : channel #1");
	TEST_check("DPT_tx : channel #1, fr = { .dest = 0xab, .orig = 0x07, .cmde = 0x91, .argv = 0xff 00 00 00 }");
	TEST_check("DPT_unlock : channel #1");


	fr = fr_get_reconf_mode;
	DPT.rx(&fr);
	for ( i = 0; i < 100; i++ )
		RCF_run();

	fr = fr_set_reconf_mode_none;
	DPT.rx(&fr);
	for ( i = 0; i < 100; i++ )
		RCF_run();
		// set force mode = NONE
	TEST_check("DPT_lock : channel #1");
	TEST_check("DPT_tx : channel #1, fr = { .dest = 0xde, .orig = 0x07, .cmde = 0x91, .argv = 0x00 02 00 00 }");
	TEST_check("DPT_unlock : channel #1");
		//"eeprom_read_block : addr = 0x0015, len = 7");
	TEST_check("DPT_lock : channel #1");
		//"DPT_tx : channel #1, fr = { .dest = 0x07, .orig = 0xde, .cmde = 0x05, .argv = 0x12 34 56 78 }");
	TEST_check("DPT_tx : channel #1, fr = { .dest = 0x01, .orig = 0x01, .cmde = 0x0a, .argv = 0x00 18 01 00 }");
	TEST_check("DPT_unlock : channel #1");
		// get response
	TEST_check("DPT_lock : channel #1");
	TEST_check("DPT_tx : channel #1, fr = { .dest = 0xab, .orig = 0x07, .cmde = 0x91, .argv = 0xff 02 02 00 }");
	TEST_check("DPT_unlock : channel #1");


	fr = fr_get_reconf_mode;
	DPT.rx(&fr);
	for ( i = 0; i < 100; i++ )
		RCF_run();

	fr = fr_set_reconf_mode_red;
	DPT.rx(&fr);
	for ( i = 0; i < 100; i++ )
		RCF_run();
		// set force mode = RED
	TEST_check("DPT_lock : channel #1");
	TEST_check("DPT_tx : channel #1, fr = { .dest = 0xde, .orig = 0x07, .cmde = 0x91, .argv = 0x00 01 00 00 }");
	TEST_check("DPT_unlock : channel #1");
		//"eeprom_read_block : addr = 0x000e, len = 7");
	TEST_check("DPT_lock : channel #1");
		//"DPT_tx : channel #1, fr = { .dest = 0x07, .orig = 0xde, .cmde = 0x05, .argv = 0x12 34 56 78 }");
	TEST_check("DPT_tx : channel #1, fr = { .dest = 0x01, .orig = 0x01, .cmde = 0x0a, .argv = 0x00 10 01 00 }");
	TEST_check("DPT_unlock : channel #1");
		// get response
	TEST_check("DPT_lock : channel #1");
	TEST_check("DPT_tx : channel #1, fr = { .dest = 0xab, .orig = 0x07, .cmde = 0x91, .argv = 0xff 01 01 00 }");
	TEST_check("DPT_unlock : channel #1");


	fr = fr_get_reconf_mode;
	DPT.rx(&fr);
	for ( i = 0; i < 100; i++ )
		RCF_run();

	fr = fr_set_reconf_mode_auto;
	DPT.rx(&fr);
	for ( i = 0; i < 100; i++ )
		RCF_run();
		// set force mode = AUTO
	TEST_check("DPT_lock : channel #1");
	TEST_check("DPT_tx : channel #1, fr = { .dest = 0xde, .orig = 0x07, .cmde = 0x91, .argv = 0x00 03 00 00 }");
	TEST_check("DPT_unlock : channel #1");
		//"eeprom_read_block : addr = 0x0015, len = 7");
	TEST_check("DPT_lock : channel #1");
		//"DPT_tx : channel #1, fr = { .dest = 0x07, .orig = 0xde, .cmde = 0x05, .argv = 0x12 34 56 78 }");
	TEST_check("DPT_tx : channel #1, fr = { .dest = 0x01, .orig = 0x01, .cmde = 0x0a, .argv = 0x00 18 01 00 }");
	TEST_check("DPT_unlock : channel #1");
		// get response
	TEST_check("DPT_lock : channel #1");
	TEST_check("DPT_tx : channel #1, fr = { .dest = 0xab, .orig = 0x07, .cmde = 0x91, .argv = 0xff 03 02 00 }");
	TEST_check("DPT_unlock : channel #1");

	fr = fr_get_reconf_mode;
	DPT.rx(&fr);
	for ( i = 0; i < 100; i++ )
		RCF_run();
}


static void start(void)
{
	PORTD = 0x00;
	PIND = 0x00;
	RCF_init();
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
			{ test_init,				"init" },
			{ test_bus_none2nominal,		"test bus none2nominal" },
			{ test_bus_none2redundant,		"test bus none2redundant" },
			{ test_bus_none2nom_n_red,		"test bus none2nom n red" },
			{ test_bus_nom_n_red2nom,		"test bus nom n red2nom" },
			{ test_bus_nom_n_red2red,		"test bus nom n red2red" },
			{ test_bus_nom_n_red2none,		"test bus nom n red2none" },
			{ test_rx_take_off_when_nom_n_red,	"test rx take off when nom n red" },
			{ test_rx_take_off_when_none,		"test rx take off when none" },
			{ test_rx_set_get_force_bus_mode,	"test rx set get force bus mode" },
			{ NULL,				NULL }
		},
	};

	TEST_run(&l, argv[1]);

	return 0;
}
