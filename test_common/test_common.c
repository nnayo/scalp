#include <string.h>

#include "test.h"

#include "type_def.h"

#include "scalp/common.h"

#include "utils/time.h"

#include "avr/io.h"


// ------------------------------------------------
// stubs
//

# include "scalp/fr_cmdes.h"

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
	u32 cmde_mask;			// command mask
	void (*rx)(dpt_frame_t* fr);	// receive frame function (when returning, the frame is no longer available)
} dpt_interface_t;


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

	DPT.t_id = 0;
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
	if (DPT.tx_ret) {
		DPT.t_id++;
		fr->t_id = DPT.t_id;
	}

	TEST_log("DPT_tx : channel #%d, fr = { .dest = 0x%02x, .orig = 0x%02x, .t_id = 0x%02x, .cmde = 0x%02x, .argv = 0x%02x %02x %02x %02x }", interf->channel, fr->dest, fr->orig, fr->t_id, (fr->resp << 7) | (fr->error << 6) | (fr->nat << 6) | fr->cmde, fr->argv[0], fr->argv[1], fr->argv[2], fr->argv[3]);

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
	//TEST_log("TIME_get : t = %d", TIME.t);

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


u8 PORTA;
u8 DDRA;

// ------------------------------------------------
// tests suite
//

static void test_init(void)
{
	// check correct initialization
	TEST_check("DPT_init");
	TEST_check("DPT_register : channel #3, cmde [0x0001c000]");
	
	TEST_log("PORTA = 0x%02x", PORTA);
	TEST_log("DDRA = 0x%02x", DDRA);
	TEST_check("PORTA = 0x00");
	TEST_check("DDRA = 0x87");
}


static void test_reject_unrelated_frame(void)
{
	dpt_frame_t fr = {
		.dest = 0x09,
		.orig = 0x08,
		.cmde = FR_LINE,
		.argv = { 0x00, 0x00, 0x00, 0x00 }
	};

	TEST_check("DPT_init");
	TEST_check("DPT_register : channel #3, cmde [0x0001c000]");

	DPT.rx(&fr);
	TEST_check("DPT_lock : channel #3");

	DPT.tx_ret = OK;
	CMN_run();

	DPT.tx_ret = KO;
	CMN_run();
	TEST_check("DPT_tx : channel #3, fr = { .dest = 0x08, .orig = 0x09, .t_id = 0x00, .cmde = 0xcd, .argv = 0x00 00 00 00 }");
	CMN_run();
	TEST_check("DPT_tx : channel #3, fr = { .dest = 0x08, .orig = 0x09, .t_id = 0x00, .cmde = 0xcd, .argv = 0x00 00 00 00 }");

	DPT.tx_ret = OK;
	CMN_run();
	TEST_check("DPT_tx : channel #3, fr = { .dest = 0x08, .orig = 0x09, .t_id = 0x01, .cmde = 0xcd, .argv = 0x00 00 00 00 }");
	CMN_run();
	TEST_check("DPT_unlock : channel #3");
}


static void test_state(void)
{
	dpt_frame_t fr_get = {
		.dest = 0x09,
		.orig = 0x08,
		.cmde = FR_STATE,
		.argv = { 0x00, 0x00, 0x00, 0x00 }
	};

	dpt_frame_t fr_set_state_n_bus = {
		.dest = 0x09,
		.orig = 0x08,
		.cmde = FR_STATE,
		.argv = { 0x7a, 0x77, 0x33, 0x00 }
	};

	dpt_frame_t fr_set_state = {
		.dest = 0x09,
		.orig = 0x08,
		.cmde = FR_STATE,
		.argv = { 0x8b, 0x88, 0x44, 0x00 }
	};

	dpt_frame_t fr_set_bus = {
		.dest = 0x09,
		.orig = 0x08,
		.cmde = FR_STATE,
		.argv = { 0x9c, 0x99, 0x55, 0x00 }
	};

	dpt_frame_t fr_invalid_set = {
		.dest = 0x09,
		.orig = 0x08,
		.cmde = FR_STATE,
		.argv = { 0xad, 0xaa, 0x66, 0x00 }
	};


	TEST_check("DPT_init");
	TEST_check("DPT_register : channel #3, cmde [0x0001c000]");

	DPT.rx(&fr_set_state_n_bus);
	TEST_check("DPT_lock : channel #3");	// set/get state and bus

	DPT.tx_ret = OK;
	CMN_run();
	TEST_check("DPT_tx : channel #3, fr = { .dest = 0x08, .orig = 0x09, .t_id = 0x01, .cmde = 0x8e, .argv = 0x7a 77 33 00 }");
	TEST_check("DPT_unlock : channel #3");

	DPT.rx(&fr_get);
	TEST_check("DPT_lock : channel #3");
	CMN_run();
	TEST_check("DPT_tx : channel #3, fr = { .dest = 0x08, .orig = 0x09, .t_id = 0x02, .cmde = 0x8e, .argv = 0x00 77 33 00 }");
	TEST_check("DPT_unlock : channel #3");

	DPT.tx_ret = OK;
	CMN_run();
	TEST_check("DPT_lock : channel #3");	// set/get state
	TEST_check("DPT_tx : channel #3, fr = { .dest = 0x08, .orig = 0x09, .t_id = 0x03, .cmde = 0x8e, .argv = 0x8b 88 44 00 }");
	TEST_check("DPT_unlock : channel #3");

	DPT.rx(&fr_set_state);
	TEST_check("DPT_lock : channel #3");
	CMN_run();
	TEST_check("DPT_tx : channel #3, fr = { .dest = 0x08, .orig = 0x09, .t_id = 0x04, .cmde = 0x8e, .argv = 0x00 88 33 00 }");

	DPT.tx_ret = OK;
	CMN_run();
	TEST_check("DPT_unlock : channel #3");

	DPT.rx(&fr_get);
	TEST_check("DPT_lock : channel #3");	// set/get bus
	CMN_run();
	TEST_check("DPT_tx : channel #3, fr = { .dest = 0x08, .orig = 0x09, .t_id = 0x05, .cmde = 0x8e, .argv = 0x9c 99 55 00 }");

	DPT.tx_ret = OK;
	CMN_run();
	TEST_check("DPT_unlock : channel #3");

	DPT.rx(&fr_set_bus);
	TEST_check("DPT_lock : channel #3");
	CMN_run();
	TEST_check("DPT_tx : channel #3, fr = { .dest = 0x08, .orig = 0x09, .t_id = 0x06, .cmde = 0x8e, .argv = 0x00 88 55 00 }");

	DPT.tx_ret = OK;
	CMN_run();
	TEST_check("DPT_unlock : channel #3");

	DPT.rx(&fr_get);
	CMN_run();

	DPT.tx_ret = OK;
	CMN_run();

	DPT.rx(&fr_invalid_set);
	TEST_check("DPT_lock : channel #3");	// invalid set/get
	CMN_run();
	TEST_check("DPT_tx : channel #3, fr = { .dest = 0x08, .orig = 0x09, .t_id = 0x07, .cmde = 0xce, .argv = 0xad aa 66 00 }");

	DPT.tx_ret = OK;
	CMN_run();
	TEST_check("DPT_unlock : channel #3");

	DPT.rx(&fr_get);
	TEST_check("DPT_lock : channel #3");
	CMN_run();
	TEST_check("DPT_tx : channel #3, fr = { .dest = 0x08, .orig = 0x09, .t_id = 0x08, .cmde = 0x8e, .argv = 0x00 88 55 00 }");

	DPT.tx_ret = OK;
	CMN_run();
	TEST_check("DPT_unlock : channel #3");
}


static void test_get_time(void)
{
	dpt_frame_t fr = {
		.dest = 0x09,
		.orig = 0x08,
		.cmde = FR_TIME_GET,
		.argv = { 0x00, 0x00, 0x00, 0x00 }
	};

	TEST_check("DPT_init");
	TEST_check("DPT_register : channel #3, cmde [0x0001c000]");

	TIME.t = 0x12345678 - TIME_1_MSEC;
	DPT.rx(&fr);
	TEST_check("DPT_lock : channel #3");
	CMN_run();
	TEST_check("DPT_tx : channel #3, fr = { .dest = 0x08, .orig = 0x09, .t_id = 0x01, .cmde = 0x8f, .argv = 0x12 34 56 78 }");

	DPT.tx_ret = OK;
	CMN_run();
	TEST_check("DPT_unlock : channel #3");
}


static void test_mux_cmde(void)
{
	dpt_frame_t fr_mux_on = {
		.dest = 0x09,
		.orig = 0x08,
		.cmde = FR_MUX_POWER,
		.argv = { 0xff, 0x00, 0x00, 0x00 }
	};

	dpt_frame_t fr_mux_off = {
		.dest = 0x09,
		.orig = 0x08,
		.cmde = FR_MUX_POWER,
		.argv = { 0x00, 0x00, 0x00, 0x00 }
	};

	dpt_frame_t fr_mux_error = {
		.dest = 0x09,
		.orig = 0x08,
		.cmde = FR_MUX_POWER,
		.argv = { 0x55, 0x00, 0x00, 0x00 }
	};

	TEST_check("DPT_init");
	TEST_check("DPT_register : channel #3, cmde [0x0001c000]");

	DPT.tx_ret = OK;

	PORTA = 0x00;
	DPT.rx(&fr_mux_on);
	TEST_check("DPT_lock : channel #3");
	CMN_run();
	TEST_check("DPT_tx : channel #3, fr = { .dest = 0x08, .orig = 0x09, .t_id = 0x01, .cmde = 0x90, .argv = 0xff 00 00 00 }");
	CMN_run();
	TEST_log("PORTA = 0x%02x", PORTA);
	TEST_check("DPT_unlock : channel #3");
	TEST_check("PORTA = 0x00");

	PORTA = 0xff;
	DPT.rx(&fr_mux_off);
	TEST_check("DPT_lock : channel #3");
	CMN_run();
	TEST_check("DPT_tx : channel #3, fr = { .dest = 0x08, .orig = 0x09, .t_id = 0x02, .cmde = 0x90, .argv = 0x00 00 00 00 }");
	CMN_run();
	TEST_log("PORTA = 0x%02x", PORTA);
	TEST_check("DPT_unlock : channel #3");
	TEST_check("PORTA = 0xf8");

	PORTA = 0x00;
	DPT.rx(&fr_mux_error);
	TEST_check("DPT_lock : channel #3");
	CMN_run();
	TEST_check("DPT_tx : channel #3, fr = { .dest = 0x08, .orig = 0x09, .t_id = 0x03, .cmde = 0xd0, .argv = 0x55 00 00 00 }");
	CMN_run();
	TEST_log("PORTA = 0x%02x", PORTA);
	TEST_check("DPT_unlock : channel #3");
	TEST_check("PORTA = 0x00");

	PORTA = 0xff;
	DPT.rx(&fr_mux_error);
	TEST_check("DPT_lock : channel #3");
	CMN_run();
	TEST_check("DPT_tx : channel #3, fr = { .dest = 0x08, .orig = 0x09, .t_id = 0x04, .cmde = 0xd0, .argv = 0x55 00 00 00 }");
	CMN_run();
	TEST_log("PORTA = 0x%02x", PORTA);
	TEST_check("DPT_unlock : channel #3");
	TEST_check("PORTA = 0xf8");
}


static void test_leds(void)
{
	int i;

	dpt_frame_t fr_wait_take_off = {
		.dest = 0x09,
		.orig = 0x08,
		.cmde = FR_STATE,
		.argv = { 0x8b, 0x01, 0x00, 0x00 }
	};

	dpt_frame_t fr_wait_take_off_conf = {
		.dest = 0x09,
		.orig = 0x08,
		.cmde = FR_STATE,
		.argv = { 0x8b, 0x02, 0x00, 0x00 }
	};

	dpt_frame_t fr_recovery = {
		.dest = 0x09,
		.orig = 0x08,
		.cmde = FR_STATE,
		.argv = { 0x8b, 0x05, 0x00, 0x00 }
	};

	TEST_check("DPT_init");
	TEST_check("DPT_register : channel #3, cmde [0x0001c000]");
	TEST_check("PORTA = 0x00");
	TEST_check("DDRA = 0x87");

	DPT.tx_ret = OK;

	TEST_log("PORTA = 0x%02x", PORTA);
	TEST_log("DDRA = 0x%02x", DDRA);

	// WAIT_TAKE_OFF green period = 1 s
	DPT.rx(&fr_wait_take_off);
	TEST_check("DPT_lock : channel #3");
	for ( i = 0; i < 500; i++ )
		CMN_run();
	TEST_log("TIME = %d", TIME.t);
	TEST_log("PORTA = 0x%02x", PORTA);
	TEST_check("DPT_tx : channel #3, fr = { .dest = 0x08, .orig = 0x09, .t_id = 0x01, .cmde = 0x8e, .argv = 0x8b 01 00 00 }");
	TEST_check("DPT_unlock : channel #3");
	TEST_check("TIME = 5000");
	TEST_check("PORTA = 0x00");
	TEST_check("TIME = 5010");
	TEST_check("PORTA = 0x00");
	TEST_check("TIME = 10010");
	TEST_check("PORTA = 0x00");

	// transition off -> ON
	CMN_run();
	TEST_log("TIME = %d", TIME.t);
	TEST_log("PORTA = 0x%02x", PORTA);

	for ( i = 0; i < 500; i++ )
		CMN_run();
	// transition ON -> off
	TEST_log("TIME = %d", TIME.t);
	TEST_log("PORTA = 0x%02x", PORTA);

	// WAIT_TAKE_OFF_CONF orange period = 500 ms
	DPT.rx(&fr_wait_take_off_conf);
	TEST_check("DPT_lock : channel #3");
	TEST_check("DPT_tx : channel #3, fr = { .dest = 0x08, .orig = 0x09, .t_id = 0x02, .cmde = 0x8e, .argv = 0x8b 02 00 00 }");
	TEST_check("DPT_unlock : channel #3");
	TEST_check("TIME = 12500");
	TEST_check("PORTA = 0x00");
	TEST_check("TIME = 12510");
	TEST_check("PORTA = 0x02");
	TEST_check("TIME = 14990");
	TEST_check("PORTA = 0x02");
	TEST_check("TIME = 15000");
	TEST_check("PORTA = 0x00");

	// transition ON -> off
	for ( i = 0; i < 250 - 1; i++ )
		CMN_run();
	TEST_log("TIME = %d", TIME.t);
	TEST_log("PORTA = 0x%02x", PORTA);

	CMN_run();
	TEST_log("TIME = %d", TIME.t);
	TEST_log("PORTA = 0x%02x", PORTA);

	// transition off -> ON
	for ( i = 0; i < 250 - 2; i++ )
		CMN_run();
	TEST_log("TIME = %d", TIME.t);
	TEST_log("PORTA = 0x%02x", PORTA);

	CMN_run();
	TEST_log("TIME = %d", TIME.t);
	TEST_log("PORTA = 0x%02x", PORTA);


	// RECOVERY red period = 250 ms
	DPT.rx(&fr_recovery);
	TEST_check("DPT_lock : channel #3");

	// transition ON -> off
	for ( i = 0; i < 125; i++ )
		CMN_run();
	TEST_log("TIME = %d", TIME.t);
	TEST_log("PORTA = 0x%02x", PORTA);

	CMN_run();
	TEST_log("TIME = %d", TIME.t);
	TEST_log("PORTA = 0x%02x", PORTA);

	// transition off -> ON
	for ( i = 0; i < 125 - 2; i++ )
		CMN_run();
	TEST_log("TIME = %d", TIME.t);
	TEST_log("PORTA = 0x%02x", PORTA);
	TEST_check("DPT_tx : channel #3, fr = { .dest = 0x08, .orig = 0x09, .t_id = 0x03, .cmde = 0x8e, .argv = 0x8b 05 00 00 }");
	TEST_check("DPT_unlock : channel #3");
	TEST_check("TIME = 16250");
	TEST_check("PORTA = 0x01");
	TEST_check("TIME = 16260");
	TEST_check("PORTA = 0x01");
	TEST_check("TIME = 17490");
	TEST_check("PORTA = 0x01");
	TEST_check("TIME = 17500");
	TEST_check("PORTA = 0x01");

	CMN_run();
	TEST_log("TIME = %d", TIME.t);
	TEST_log("PORTA = 0x%02x", PORTA);
}


static void start(void)
{
	dpt_frame_t fr_reset = {
		.dest = 0x01,
		.orig = 0x01,
		.cmde = FR_STATE,
		.argv = { 0x7a, 0x00, 0x00, 0xff }
	};

	PORTA = 0x00;
	DDRA = 0x00;
	TIME.t = 0;

	memcpy(&EEP.container, &fr_reset, sizeof(fr_reset));

	DPT_init();
	CMN_init();
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
			{ test_reject_unrelated_frame,	"test reject unrelated frame" },
			{ test_state,			"test state" },
			{ test_get_time,		"test get time" },
			{ test_mux_cmde,		"test mux cmde" },
			{ test_leds,			"test leds" },
			{ NULL,				NULL }
		},
	};

	TEST_run(&l, argv[1]);

	return 0;
}
