#include <string.h>

#include "test.h"

#include "type_def.h"

#include "scalp/alive.h"
#include "scalp/dna.h"

#include "utils/time.h"

#include "avr/io.h"

#include "scalp/fr_cmdes.h"


// ------------------------------------------------
// stubs
//

struct {
	u32 t;
	u32 incr;
} TIME;


u32 TIME_get(void)
{
	return TIME.t;
}


struct {
	u8 nb_is;
	u8 nb_bs;
	dna_list_t list[9];
} DNA;

dna_list_t* DNA_list(u8* nb_is, u8* nb_bs)
{
	*nb_is = DNA.nb_is;
	*nb_bs = DNA.nb_bs;

	return DNA.list;
}

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


// ------------------------------------------------
// tests suite
//

static void test_init(void)
{
	// check correct initialization
	TEST_check("DPT_register : channel #9, cmde [0x00004000]");
}


static void test_ignore_unrelated_frame(void)
{
	dpt_frame_t fr = {
		.dest = 0x09,
		.orig = 0x08,
		.cmde = FR_LINE,
		.argv = { 0x00, 0x00, 0x00, 0x00 }
	};

	TEST_check("DPT_register : channel #9, cmde [0x00004000]");

	TIME.t = TIME_1_SEC + 5 * TIME_1_MSEC;
	DPT.rx(&fr);

	DPT.tx_ret = OK;
	ALV_run();
	ALV_run();
	ALV_run();
	ALV_run();
}


static void test_alive_send(void)
{
	int i;

	TEST_check("DPT_register : channel #9, cmde [0x00004000]");

	// set dna list
	DNA.list[0].i2c_addr = 0x07;	// self
	DNA.list[0].type = 0x02;
	DNA.list[1].i2c_addr = 0x06;	// BC
	DNA.list[1].type = 0x03;
	DNA.list[2].i2c_addr = 0x05;	// another IS
	DNA.list[2].type = 0x04;
	DNA.list[3].i2c_addr = 0x04;	// another MNT
	DNA.list[3].type = 0x02;
	DNA.nb_is = 4;
	DNA.nb_bs = 0;

	// set local time not modulo 1 s
	TIME.t = 0.1 * TIME_1_SEC;

	DPT.tx_ret = OK;
	for ( i = 0; i < 10; i++ ) {
		ALV_run();
	}

	// no FR_STATE frame is sent before time is modulo 1 s
	TIME.t = 1 * TIME_1_SEC;
	ALV_run();
	TEST_check("DPT_lock : channel #9");
	TEST_check("DPT_tx : channel #9, fr = { .dest = 0x04, .orig = 0x07, .cmde = 0x0e, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_unlock : channel #9");

	TIME.t = 1.1 * TIME_1_SEC;
	for ( i = 0; i < 10; i++ ) {
		ALV_run();
	}

	// no FR_STATE frame is sent before time is modulo 2 s
	TIME.t = 2 * TIME_1_SEC;
	ALV_run();
	TEST_check("DPT_lock : channel #9");
	TEST_check("DPT_tx : channel #9, fr = { .dest = 0x04, .orig = 0x07, .cmde = 0x0e, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_unlock : channel #9");
}


static void test_alive_failed(void)
{
	TEST_check("DPT_register : channel #9, cmde [0x00004000]");

	// set dna list
	DNA.list[0].i2c_addr = 0x07;	// self
	DNA.list[0].type = 0x02;
	DNA.list[1].i2c_addr = 0x06;	// BC
	DNA.list[1].type = 0x03;
	DNA.list[2].i2c_addr = 0x05;	// another IS
	DNA.list[2].type = 0x04;
	DNA.nb_is = 3;
	DNA.nb_bs = 0;

	// set local time to 1 s
	TIME.t = 1 * TIME_1_SEC;
	ALV_run();
	TEST_check("DPT_lock : channel #9");
	TEST_check("DPT_tx : channel #9, fr = { .dest = 0x7f, .orig = 0x07, .cmde = 0x0e, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_unlock : channel #9");
}


static void test_alive_trigger(void)
{
	dpt_frame_t fr = {
		.dest = 0x07,
		.orig = 0x06,
		.resp = 1,
		.error = 1,
		.nat = 0,
		.cmde = FR_STATE,
		.argv = { 0x00, 0x00, 0x00, 0x00 }
	};

	int i;

	TEST_check("DPT_register : channel #9, cmde [0x00004000]");

	// set dna list
	DNA.list[0].i2c_addr = 0x07;	// self
	DNA.list[0].type = 0x02;
	DNA.list[1].i2c_addr = 0x06;	// BC
	DNA.list[1].type = 0x03;
	DNA.list[2].i2c_addr = 0x05;	// another IS
	DNA.list[2].type = 0x04;
	DNA.list[3].i2c_addr = 0x04;	// another MNT
	DNA.list[3].type = 0x02;
	DNA.list[4].i2c_addr = 0x03;	// another MNT
	DNA.list[4].type = 0x02;
	DNA.nb_is = 5;
	DNA.nb_bs = 0;

	// set local time to not 1 s
	TIME.t = 0.1 * TIME_1_SEC;

	DPT.tx_ret = OK;
	for ( i = 0; i < 4; i++ ) {
		TIME.t = TIME_1_SEC + i * TIME_1_SEC;
		ALV_run();
	}

	// no FR_STATE frame is sent if time is not modulo 1 s
	TIME.t += 0.1 * TIME_1_SEC;
	for ( i = 0; i < 10; i++ ) {
		ALV_run();
	}
	// when time is set to 1 s
	TEST_check("DPT_lock : channel #9");
	TEST_check("DPT_tx : channel #9, fr = { .dest = 0x04, .orig = 0x07, .cmde = 0x0e, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_unlock : channel #9");

	// no FR_STATE frame is sent if time is not modulo 1 s
	TIME.t += 0.9 * TIME_1_SEC;
	ALV_run();
	TIME.t += 0.1 * TIME_1_SEC;

	// send 5 failed get state responses
	// nothing happens

	// send 5 failed get state responses
	DPT.rx(&fr);
	ALV_run();

	// time is set to 2 s
	TIME.t = 1.9 * TIME_1_SEC;
	TEST_check("DPT_lock : channel #9");
	TEST_check("DPT_tx : channel #9, fr = { .dest = 0x03, .orig = 0x07, .cmde = 0x0e, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_unlock : channel #9");

	DPT.rx(&fr);
	ALV_run();

	// time is set to 3 s
	TIME.t = 2.9 * TIME_1_SEC;
	TEST_check("DPT_lock : channel #9");
	TEST_check("DPT_tx : channel #9, fr = { .dest = 0x04, .orig = 0x07, .cmde = 0x0e, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_unlock : channel #9");

	DPT.rx(&fr);
	ALV_run();

	// time is set to 4 s
	TIME.t = 3.9 * TIME_1_SEC;
	TEST_check("DPT_lock : channel #9");
	TEST_check("DPT_tx : channel #9, fr = { .dest = 0x03, .orig = 0x07, .cmde = 0x0e, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_unlock : channel #9");

	DPT.rx(&fr);
	ALV_run();

	// time is set to 5 s
	TIME.t = 4.9 * TIME_1_SEC;
	TEST_check("DPT_lock : channel #9");
	TEST_check("DPT_tx : channel #9, fr = { .dest = 0x04, .orig = 0x07, .cmde = 0x0e, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_unlock : channel #9");

	DPT.rx(&fr);
	ALV_run();

	TEST_check("DPT_lock : channel #9");
	TEST_check("DPT_tx : channel #9, fr = { .dest = 0x03, .orig = 0x07, .cmde = 0x0e, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_unlock : channel #9");

	// to trigger anti-bounce counter test
	// set time to 6 s
	// so first receive the get state request
	TIME.t = 6.0 * TIME_1_SEC;
	ALV_run();

	// then get the force bus to none request
	TEST_check("DPT_lock : channel #9");
	TEST_check("DPT_tx : channel #9, fr = { .dest = 0x01, .orig = 0x01, .cmde = 0x11, .argv = 0x02 00 00 00 }");
	TEST_check("DPT_unlock : channel #9");
}


static void test_mnt_disappear(void)
{
	TEST_check("DPT_register : channel #9, cmde [0x00004000]");

	// set dna list
	DNA.list[0].i2c_addr = 0x07;	// self
	DNA.list[0].type = 0x02;
	DNA.list[1].i2c_addr = 0x06;	// BC
	DNA.list[1].type = 0x03;
	DNA.list[2].i2c_addr = 0x05;	// another IS
	DNA.list[2].type = 0x04;
	DNA.list[3].i2c_addr = 0x04;	// another MNT
	DNA.list[3].type = 0x02;
	DNA.list[4].i2c_addr = 0x03;	// another MNT
	DNA.list[4].type = 0x02;
	DNA.nb_is = 5;
	DNA.nb_bs = 0;

	DPT.tx_ret = OK;
	// time is set to 1 s
	TIME.t = 1 * TIME_1_SEC;
	ALV_run();

	TEST_check("DPT_lock : channel #9");
	TEST_check("DPT_tx : channel #9, fr = { .dest = 0x04, .orig = 0x07, .cmde = 0x0e, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_unlock : channel #9");

	// time is set to 2 s
	TIME.t = 2 * TIME_1_SEC;
	ALV_run();
	TEST_check("DPT_lock : channel #9");
	TEST_check("DPT_tx : channel #9, fr = { .dest = 0x03, .orig = 0x07, .cmde = 0x0e, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_unlock : channel #9");

	// time is set to 3 s
	TIME.t = 3 * TIME_1_SEC;
	ALV_run();
	TEST_check("DPT_lock : channel #9");
	TEST_check("DPT_tx : channel #9, fr = { .dest = 0x04, .orig = 0x07, .cmde = 0x0e, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_unlock : channel #9");

	// first MNT disappear
	// time is set to 4 s
	DNA.nb_is = 4;
	TIME.t = 4 * TIME_1_SEC;
	ALV_run();
	TEST_check("DPT_lock : channel #9");
	TEST_check("DPT_tx : channel #9, fr = { .dest = 0x04, .orig = 0x07, .cmde = 0x0e, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_unlock : channel #9");

	// time is set to 5 s
	TIME.t = 5 * TIME_1_SEC;
	ALV_run();
	TEST_check("DPT_lock : channel #9");
	TEST_check("DPT_tx : channel #9, fr = { .dest = 0x04, .orig = 0x07, .cmde = 0x0e, .argv = 0x00 00 00 00 }");
	TEST_check("DPT_unlock : channel #9");
}


static void start(void)
{
	TIME.t = 0;
	ALV_init();
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
			{ test_alive_send,		"test alive send" },
			{ test_alive_failed,		"test alive failed" },
			{ test_alive_trigger,		"test alive trigger" },
			{ test_mnt_disappear,		"test mnt disappear" },
			{ NULL,				NULL }
		},
	};

	TEST_run(&l, argv[1]);

	return 0;
}
