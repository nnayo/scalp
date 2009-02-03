#include <string.h>

#include "test.h"

#include "type_def.h"

#include "scalp/nat.h"

#include "utils/time.h"

#include "drivers/rs.h"


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
	int get_ret;
} RS;

void RS_init(u8 baud)
{
	TEST_log("RS_init : baud = %d", baud);
}

// stub for putchar
int _IO_putc(int c, _IO_FILE *__fp)
{
	TEST_log("putchar : 0x%02x", c);

	return 0;
}

// stub for getchar(void)
int _IO_getc (_IO_FILE *__fp)
{
	int ret;

	TEST_log("getchar : 0x%02x", RS.get_ret);

	if ( RS.get_ret != EOF ) {
		ret = RS.get_ret;
		RS.get_ret = EOF;
		return ret;
	}

	return EOF;
}


struct {
	u32 time;
} TIME;


u32 TIME_get(void)
{
	return TIME.time;
}


// ------------------------------------------------
// tests suite
//

static void test_init(void)
{
	// check correct initialization
	TEST_check("RS_init : baud = 103");
	TEST_check("DPT_register : channel #5, cmde [0xffffffff]");
}


static void test_rx_twi_frame(void)
{
	dpt_frame_t fr_ignored = {
		.dest = 0x0b,
		.orig = 0x0c,
		.resp = 1,
		.error = 0,
		.nat = 0,
		.cmde = FR_NO_CMDE,
		.argv = { 0, 0, 0, 0 }
	};

	dpt_frame_t fr_nat_resp = {
		.dest = 0x0b,
		.orig = 0x0c,
		.resp = 1,
		.error = 0,
		.nat = 1,
		.cmde = FR_NO_CMDE,
		.argv = { 0, 0, 0, 0 }
	};

	TEST_check("RS_init : baud = 103");
	TEST_check("DPT_register : channel #5, cmde [0xffffffff]");

	DPT.rx(&fr_ignored);

	DPT.rx(&fr_nat_resp);
	TEST_check("putchar : 0x0b");
	TEST_check("putchar : 0x0c");
	TEST_check("putchar : 0x82");
	TEST_check("putchar : 0x00");
	TEST_check("putchar : 0x00");
	TEST_check("putchar : 0x00");
	TEST_check("putchar : 0x00");
}


static void test_rx_tty_frame(void)
{
	TEST_check("RS_init : baud = 103");
	TEST_check("DPT_register : channel #5, cmde [0xffffffff]");
	DPT.tx_ret = KO;

	RS.get_ret = EOF;
	NAT_run();
	TEST_check("getchar : 0xffffffff");
	NAT_run();
	TEST_check("getchar : 0xffffffff");
	NAT_run();
	TEST_check("getchar : 0xffffffff");

	RS.get_ret = 0x08;	// dest
	NAT_run();
	TEST_check("getchar : 0x08");
	TEST_check("getchar : 0xffffffff");

	RS.get_ret = 0x09;	// orig
	NAT_run();
	TEST_check("getchar : 0x09");
	TEST_check("getchar : 0xffffffff");

	RS.get_ret = 0x02;	// cmde
	NAT_run();
	TEST_check("getchar : 0x02");
	TEST_check("getchar : 0xffffffff");

	RS.get_ret = 0x11;	// argv0
	NAT_run();
	TEST_check("getchar : 0x11");
	TEST_check("getchar : 0xffffffff");

	RS.get_ret = 0x22;	// argv1
	NAT_run();
	TEST_check("getchar : 0x22");
	TEST_check("getchar : 0xffffffff");

	RS.get_ret = 0x33;	// argv2
	NAT_run();
	TEST_check("getchar : 0x33");
	TEST_check("getchar : 0xffffffff");

	RS.get_ret = 0x44;	// argv3
	NAT_run();
	TEST_check("getchar : 0x44");
	TEST_check("DPT_lock : channel #5");
	TEST_check("DPT_tx : channel #5, fr = { .dest = 0x08, .orig = 0x09, .cmde = 0x22, .argv = 0x11 22 33 44 }");
	NAT_run();
	TEST_check("DPT_tx : channel #5, fr = { .dest = 0x08, .orig = 0x09, .cmde = 0x22, .argv = 0x11 22 33 44 }");

	DPT.tx_ret = OK;
	NAT_run();
	TEST_check("DPT_tx : channel #5, fr = { .dest = 0x08, .orig = 0x09, .cmde = 0x22, .argv = 0x11 22 33 44 }");
	TEST_check("DPT_unlock : channel #5");
}


static void test_rx_tty_time_out(void)
{
	TEST_check("RS_init : baud = 103");
	TEST_check("DPT_register : channel #5, cmde [0xffffffff]");
	DPT.tx_ret = KO;

	RS.get_ret = EOF;
	NAT_run();
	TEST_check("getchar : 0xffffffff");
	NAT_run();
	TEST_check("getchar : 0xffffffff");
	NAT_run();
	TEST_check("getchar : 0xffffffff");

	RS.get_ret = 0x08;	// dest
	TIME.time = 0 * TIME_1_MSEC;
	NAT_run();
	TEST_check("getchar : 0x08");
	TEST_check("getchar : 0xffffffff");

	RS.get_ret = 0x09;	// orig
	TIME.time = 6 * TIME_1_MSEC;
	NAT_run();

	RS.get_ret = 0x08;	// dest
	NAT_run();
	TEST_check("getchar : 0x08");
	TEST_check("getchar : 0xffffffff");

	RS.get_ret = 0x09;	// orig
	NAT_run();
	TEST_check("getchar : 0x09");
	TEST_check("getchar : 0xffffffff");

	RS.get_ret = 0x02;	// cmde
	NAT_run();
	TEST_check("getchar : 0x02");
	TEST_check("getchar : 0xffffffff");

	RS.get_ret = 0x11;	// argv0
	NAT_run();
	TEST_check("getchar : 0x11");
	TEST_check("getchar : 0xffffffff");

	RS.get_ret = 0x22;	// argv1
	NAT_run();
	TEST_check("getchar : 0x22");
	TEST_check("getchar : 0xffffffff");

	RS.get_ret = 0x33;	// argv2
	NAT_run();
	TEST_check("getchar : 0x33");
	TEST_check("getchar : 0xffffffff");

	RS.get_ret = 0x44;	// argv3
	NAT_run();
	TEST_check("getchar : 0x44");
	TEST_check("DPT_lock : channel #5");
	TEST_check("DPT_tx : channel #5, fr = { .dest = 0x08, .orig = 0x09, .cmde = 0x22, .argv = 0x11 22 33 44 }");
	NAT_run();
	TEST_check("DPT_tx : channel #5, fr = { .dest = 0x08, .orig = 0x09, .cmde = 0x22, .argv = 0x11 22 33 44 }");

	DPT.tx_ret = OK;
	NAT_run();
	TEST_check("DPT_tx : channel #5, fr = { .dest = 0x08, .orig = 0x09, .cmde = 0x22, .argv = 0x11 22 33 44 }");
	TEST_check("DPT_unlock : channel #5");
}


static void start(void)
{
	NAT_init();
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
			{ test_rx_twi_frame,		"test_rx_twi_frame" },
			{ test_rx_tty_frame,		"test_rx_tty_frame" },
			{ test_rx_tty_time_out,		"test_rx_tty_time_out" },
			{ NULL,				NULL }
		},
	};

	TEST_run(&l, argv[1]);

	return 0;
}
