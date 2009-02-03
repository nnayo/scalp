#include <string.h>

#include "test/test.h"

#include "type_def.h"
#include "drivers/twi.h"
#include "utils/time.h"

#include "scalp/dispatcher.h"


// ------------------------------------------------
// stubs
//

struct {
	u32 time;
	u32 incr;
} TIME;


u32 TIME_get(void)
{
	return TIME.time;
}


struct {
	u8* buf;

	void (*call_back)(twi_state_t state, u8 nb_data, void* misc);

	u8 ms_rx_resp;
	u8 ms_tx_resp;
	u8 sl_rx_resp;
	u8 sl_tx_resp;
	u8 sl_addr;
} TWI;


void TWI_init(void(*call_back)(twi_state_t state, u8 nb_data, void* misc), void* misc)
{
	// save call back
	TWI.call_back = call_back;

	// log the init
	TEST_log("TWI_init");
}


void TWI_set_sl_addr(u8 sl_addr)
{
	// log the call
	TEST_log("TWI_set_sl_addr : 0x%02x", sl_addr);

	// save slave address
	TWI.sl_addr = sl_addr;
}

u8 TWI_ms_rx(u8 adr, u8 len, u8* data)
{
	// log the call
	TEST_log("TWI_ms_rx : 0x%02x, %d", adr, len);

	// save data buffer address
	TWI.buf = data;

	return TWI.ms_rx_resp;
}

u8 TWI_ms_tx(u8 adr, u8 len, u8* data)
{
	// log the call
	TEST_log("TWI_ms_tx : 0x%02x, %d", adr, len);

	return TWI.ms_tx_resp;
}

u8 TWI_sl_tx(u8 len, u8* data)
{
	// log the call
	TEST_log("TWI_sl_tx : %d", len);

	return TWI.sl_tx_resp;
}

u8 TWI_sl_rx(u8 len, u8* data)
{
	// log the call
	TEST_log("TWI_sl_rx : %d", len);

	// save data buffer address
	TWI.buf = data;

	return TWI.sl_rx_resp;
}

void TWI_stop(void)
{
	// log the stop
	TEST_log("TWI_stop");
}

u8 TWI_get_sl_addr(void)
{
	// log the call
	TEST_log("TWI_get_sl_addr");

	// return slave address
	return TWI.sl_addr;
}

void TWI_gen_call(u8 flag)
{
	// log the call
	TEST_log("TWI_gen_call : %d", flag);
}


// ------------------------------------------------
// tests suite
//

static void test_init(void)
{
	// check correct initialization
	// DPT_init is called every test via the start function

	DPT_run();

	TEST_check("TWI_init");
}


static void test_set_sl_addr(void)
{
	// check correct slave addr setting
	// DPT_init is called every test via the start function
	TEST_check("TWI_init");

	DPT_set_sl_addr(0x07);

	TEST_check("TWI_set_sl_addr : 0x07");
}


static void test_gen_call(void)
{
	// check general call setting
	// DPT_init is called every test via the start function

	DPT_gen_call(OK);
	DPT_gen_call(KO);

	TEST_check("TWI_init");
	TEST_check("TWI_gen_call : 1");
	TEST_check("TWI_gen_call : 0");
}


static void rx(dpt_frame_t* fr)
{
	// log the call
	TEST_log("frame rxed : dest 0x%02x, orig 0x%02x, cmde 0x%02x, argv 0x%02x %02x %02x %02x", fr->dest, fr->orig, (fr->resp << 7) | fr->cmde, fr->argv[0], fr->argv[1], fr->argv[2], fr->argv[3]);
	//printf("frame rxed : resp = %d, error = %d, nat = %d, cmde = 0x%02x\n", fr->resp, fr->error, fr->nat, fr->cmde);
}


static void test_distant_frame_transmit_without_lock(void)
{
	int dpt_res;

	dpt_interface_t interf = {
		.channel = 2,
		.cmde_mask = _CM(0x08),
		.rx = rx
	};

	dpt_frame_t fr = {
		.dest = 0x03,	// a distant node address
		//.orig = 7,	orig field is automatically filled
		.cmde = 0x08,
		.argv = { 0x11, 0x22, 0x33, 0x44 }
	};

	// check distant frame transmit
	// DPT_init is called every test via the start function

	TEST_check("TWI_init");

	DPT_set_sl_addr(0x07);
	TEST_check("TWI_set_sl_addr : 0x07");

	DPT_register(&interf);
	TEST_log("DPT_register : channel #%d", interf.channel);
	TEST_check("DPT_register : channel #2");

	dpt_res = DPT_tx(&interf, &fr);
	if (dpt_res == OK) {
		TEST_log("DPT_tx : OK");
	}
	else {
		TEST_log("DPT_tx : KO");
	}
	DPT_run();

	TEST_check("DPT_tx : KO");
}


static void test_distant_frame_transmit(void)
{
	int dpt_res;

	dpt_interface_t interf = {
		.channel = 2,
		.cmde_mask = _CM(0x05),
		.rx = rx
	};

	dpt_frame_t fr = {
		.dest = 0x03,	// a distant node address
		//.orig = 7,	orig field is automatically filled
		.cmde = 0x08,
		.argv = { 0x11, 0x22, 0x33, 0x44 }
	};

	// DPT_init is called every test via the start function
	TEST_check("TWI_init");

	DPT_set_sl_addr(0x07);
	TEST_check("TWI_set_sl_addr : 0x07");

	DPT_register(&interf);
	TEST_log("DPT_register : channel #%d", interf.channel);
	TEST_check("DPT_register : channel #2");

	DPT_lock(&interf);

	TWI.ms_tx_resp = OK;
	dpt_res = DPT_tx(&interf, &fr);
	TEST_check("TWI_ms_tx : 0x03, 7");
	if (dpt_res == OK) {
		TEST_log("DPT_tx : OK");
	}
	else {
		TEST_log("DPT_tx : KO");
	}
	DPT_run();
	TEST_check("DPT_tx : OK");

	DPT_unlock(&interf);
}


static void test_local_frame_transmit(void)
{
	int dpt_res;


	dpt_interface_t interf = {
		.channel = 2,
		.cmde_mask = _CM(0x05),
		.rx = rx
	};

	dpt_frame_t fr = {
		.dest = 0x01,	// local node address
		.orig = 7,
		.cmde = 0x05,
		.argv = { 0x11, 0x22, 0x33, 0x44 }
	};

	// DPT_init is called every test via the start function
	TEST_check("TWI_init");

	DPT_set_sl_addr(0x07);
	TEST_check("TWI_set_sl_addr : 0x07");

	DPT_register(&interf);
	TEST_log("DPT_register : channel #%d", interf.channel);
	TEST_check("DPT_register : channel #2");

	DPT_lock(&interf);

	dpt_res = DPT_tx(&interf, &fr);
	TEST_check("frame rxed : dest 0x01, orig 0x07, cmde 0x05, argv 0x11 22 33 44");
	if (dpt_res == OK) {
		TEST_log("DPT_tx : OK");
	}
	else {
		TEST_log("DPT_tx : KO");
	}
	TEST_check("DPT_tx : OK");
	DPT_run();

	DPT_unlock(&interf);
}


static void test_broadcast_frame_transmit(void)
{
	int dpt_res;


	dpt_interface_t interf = {
		.channel = 2,
		.cmde_mask = _CM(0x08),
		.rx = rx
	};

	dpt_frame_t fr = {
		.dest = 0x00,	// broadcast address
		.orig = 7,
		.cmde = 0x08,
		.argv = { 0x11, 0x22, 0x33, 0x44 }
	};

	// DPT_init is called every test via the start function
	TEST_check("TWI_init");

	DPT_set_sl_addr(0x07);
	TEST_check("TWI_set_sl_addr : 0x07");

	DPT_register(&interf);
	TEST_log("DPT_register : channel #%d", interf.channel);
	TEST_check("DPT_register : channel #2");

	DPT_lock(&interf);

	TWI.ms_tx_resp = OK;
	dpt_res = DPT_tx(&interf, &fr);
	TEST_check("frame rxed : dest 0x00, orig 0x07, cmde 0x08, argv 0x11 22 33 44");
	TEST_check("TWI_ms_tx : 0x00, 7");
	if (dpt_res == OK) {
		TEST_log("DPT_tx : OK");
	}
	else {
		TEST_log("DPT_tx : KO");
	}
	TEST_check("DPT_tx : OK");
	DPT_run();

	DPT_unlock(&interf);
}


static void test_2_consecutive_distant_frames_transmit(void)
{
	int dpt_res;

	dpt_interface_t interf = {
		.channel = 2,
		.cmde_mask = _CM(0x05),
		.rx = rx
	};

	dpt_frame_t fr = {
		.dest = 0x03,	// a distant node address
		//.orig = 7,	orig field is automatically filled
		.cmde = 0x08,
		.argv = { 0x11, 0x22, 0x33, 0x44 }
	};

	// DPT_init is called every test via the start function
	TEST_check("TWI_init");

	DPT_set_sl_addr(0x07);
	TEST_check("TWI_set_sl_addr : 0x07");

	DPT_register(&interf);
	TEST_log("DPT_register : channel #%d", interf.channel);
	TEST_check("DPT_register : channel #2");

	DPT_lock(&interf);

	TWI.ms_tx_resp = OK;
	dpt_res = DPT_tx(&interf, &fr);
	TEST_check("TWI_ms_tx : 0x03, 7");
	TEST_check("DPT_tx : OK");
	if (dpt_res == OK) {
		TEST_log("DPT_tx : OK");
	}
	else {
		TEST_log("DPT_tx : KO");
	}
	TEST_check("DPT_tx : KO");
	DPT_run();

	dpt_res = DPT_tx(&interf, &fr);
	if (dpt_res == OK) {
		TEST_log("DPT_tx : OK");
	}
	else {
		TEST_log("DPT_tx : KO");
	}
	DPT_run();
	TEST_check("DPT_tx : KO");

	dpt_res = DPT_tx(&interf, &fr);
	if (dpt_res == OK) {
		TEST_log("DPT_tx : OK");
	}
	else {
		TEST_log("DPT_tx : KO");
	}
	DPT_run();
	TEST_check("TWI_stop");

	TWI.call_back(TWI_MS_TX_END, 0, NULL);
	dpt_res = DPT_tx(&interf, &fr);
	TEST_check("TWI_ms_tx : 0x03, 7");
	if (dpt_res == OK) {
		TEST_log("DPT_tx : OK");
	}
	else {
		TEST_log("DPT_tx : KO");
	}
	DPT_run();
	TEST_check("DPT_tx : OK");

	DPT_unlock(&interf);
}


static void test_distant_frame_transmit_with_higher_lock(void)
{
	int dpt_res;


	dpt_interface_t interf = {
		.channel = 2,
		.cmde_mask = _CM(0x05),
		.rx = rx
	};

	dpt_interface_t interf_hi = {
		.channel = 1,
		.cmde_mask = _CM(0x05),
		.rx = rx
	};

	dpt_frame_t fr = {
		.dest = 0x03,	// a distant node address
		//.orig = 7,	orig field is automatically filled
		.cmde = 0x08,
		.argv = { 0x11, 0x22, 0x33, 0x44 }
	};

	// DPT_init is called every test via the start function
	TEST_check("TWI_init");

	DPT_set_sl_addr(0x07);
	TEST_check("TWI_set_sl_addr : 0x07");

	DPT_register(&interf);
	TEST_log("DPT_register : channel #%d", interf.channel);
	TEST_check("DPT_register : channel #2");

	DPT_register(&interf_hi);
	TEST_log("DPT_register : channel #%d", interf_hi.channel);
	TEST_check("DPT_register : channel #1");

	DPT_lock(&interf);
	DPT_lock(&interf_hi);

	dpt_res = DPT_tx(&interf, &fr);
	if (dpt_res == OK) {
		TEST_log("DPT_tx : OK");
	}
	else {
		TEST_log("DPT_tx : KO");
	}
	DPT_run();
	TEST_check("DPT_tx : KO");
}


static void test_register(void)
{
	dpt_interface_t interf = {
		.channel = 2,
		.cmde_mask = _CM(0x05),
		.rx = rx
	};

	dpt_interface_t interf_hi = {
		.channel = 0,
		.cmde_mask = _CM(0x05),
		.rx = rx
	};

	// DPT_init is called every test via the start function
	TEST_check("TWI_init");

	DPT_set_sl_addr(0x07);
	TEST_check("TWI_set_sl_addr : 0x07");

	DPT_register(&interf);
	TEST_log("DPT_register : channel #%d", interf.channel);
	TEST_check("DPT_register : channel #2");

	DPT_register(&interf_hi);
	TEST_log("DPT_register : channel #%d", interf_hi.channel);
	TEST_check("DPT_register : channel #0");

	DPT_register(&interf_hi);
	TEST_log("DPT_register : channel #%d", interf_hi.channel);
	TEST_check("DPT_register : channel #1");

	DPT_register(&interf_hi);
	TEST_log("DPT_register : channel #%d", interf_hi.channel);
	TEST_check("DPT_register : channel #3");

	DPT_register(&interf_hi);
	TEST_log("DPT_register : channel #%d", interf_hi.channel);
	TEST_check("DPT_register : channel #4");

	DPT_register(&interf_hi);
	TEST_log("DPT_register : channel #%d", interf_hi.channel);
	TEST_check("DPT_register : channel #5");

	DPT_register(&interf_hi);
	TEST_log("DPT_register : channel #%d", interf_hi.channel);
	TEST_check("DPT_register : channel #6");

	DPT_register(&interf_hi);
	TEST_log("DPT_register : channel #%d", interf_hi.channel);
	TEST_check("DPT_register : channel #7");

	DPT_register(&interf_hi);
	TEST_log("DPT_register : channel #%d", interf_hi.channel);
	TEST_check("DPT_register : channel #8");

	DPT_register(&interf_hi);
	TEST_log("DPT_register : channel #%d", interf_hi.channel);
	TEST_check("DPT_register : channel #9");

	DPT_register(&interf_hi);
	TEST_log("DPT_register : channel #%d", interf_hi.channel);
	TEST_check("DPT_register : channel #255");

	DPT_register(&interf_hi);
	TEST_log("DPT_register : channel #%d", interf_hi.channel);
	TEST_check("DPT_register : channel #255");
}


static void test_incoming_frame_to_unregistered_appli(void)
{
	u8 vect[] = { 8, 9, 10, 11, 12, 13 };

	dpt_interface_t interf = {
		.channel = 2,
		.cmde_mask = _CM(0x05),
		.rx = rx
	};

	// DPT_init is called every test via the start function
	TEST_check("TWI_init");

	DPT_set_sl_addr(0x07);
	TEST_check("TWI_set_sl_addr : 0x07");

	DPT_register(&interf);
	TEST_log("DPT_register : channel #%d", interf.channel);
	TEST_check("DPT_register : channel #2");

	// start receiving as a slave
	TWI.sl_rx_resp = OK;
	TWI.call_back(TWI_SL_RX_BEGIN, 0, NULL);
	TEST_check("TWI_sl_rx : 7");

	// fill reception buffer
	memcpy(TWI.buf, vect, sizeof(vect));

	// reception is complete
	TWI.call_back(TWI_SL_RX_END, 0, NULL);
	TEST_check("TWI_stop");
}


static void test_incoming_frame_with_bad_length(void)
{
	u8 vect[] = { 8, 9, 10, 11, 12, 13 };

	dpt_interface_t interf = {
		.channel = 2,
		.cmde_mask = _CM(0x05),
		.rx = rx
	};

	// DPT_init is called every test via the start function
	TEST_check("TWI_init");

	DPT_set_sl_addr(0x07);
	TEST_check("TWI_set_sl_addr : 0x07");

	DPT_register(&interf);
	TEST_log("DPT_register : channel #%d", interf.channel);
	TEST_check("DPT_register : channel #2");

	// start receiving as a slave
	TWI.sl_rx_resp = OK;
	TWI.call_back(TWI_SL_RX_BEGIN, 0, NULL);
	TEST_check("TWI_sl_rx : 7");

	// fill reception buffer
	memcpy(TWI.buf, vect, sizeof(vect));

	// reception is complete
	TWI.call_back(TWI_SL_RX_END, sizeof(dpt_frame_t) - 2, NULL);
	TEST_check("TWI_stop");
}


static void test_incoming_frame_to_registered_appli(void)
{
	dpt_frame_t vect1 = {
		.orig = 8,
		.resp = 0,
		.error = 0,
		.nat = 0,
		.cmde = 9,
		.argv[0] = 0x0a,
		.argv[1] = 0x0b,
		.argv[2] = 0x0c,
		.argv[3] = 0x0d
	};

	dpt_frame_t vect2 = {
		.orig = 8,
		.resp = 0,
		.error = 0,
		.nat = 0,
		.cmde = 5,
		.argv[0] = 0x0a,
		.argv[1] = 0x0b,
		.argv[2] = 0x0c,
		.argv[3] = 0x0d
	};

	dpt_interface_t interf = {
		.channel = 2,
		.cmde_mask = _CM(0x05),
		.rx = rx
	};

	// DPT_init is called every test via the start function
	TEST_check("TWI_init");

	DPT_set_sl_addr(0x07);
	TEST_check("TWI_set_sl_addr : 0x07");

	DPT_register(&interf);
	TEST_log("DPT_register : channel #%d", interf.channel);
	TEST_check("DPT_register : channel #2");

	// first case : command out of range
	// start receiving as a slave
	TWI.sl_rx_resp = OK;
	TWI.call_back(TWI_SL_RX_BEGIN, 0, NULL);
	TEST_check("TWI_sl_rx : 7");

	// fill reception buffer
	memcpy(TWI.buf, (u8*)&vect1 + 1, sizeof(vect1) -1);

	// reception is complete
	TWI.call_back(TWI_SL_RX_END, sizeof(dpt_frame_t) - 1, NULL);
	TEST_check("TWI_stop");

	// second case : command in range
	// start receiving as a slave
	TWI.call_back(TWI_SL_RX_BEGIN, 0, NULL);
	TEST_check("TWI_sl_rx : 7");
	TEST_check("frame rxed : dest 0x07, orig 0x08, cmde 0x05, argv 0x0a 0b 0c 0d");

	// fill reception buffer
	memcpy(TWI.buf, (u8*)&vect2 + 1, sizeof(vect2) - 1);

	// reception is complete
	TWI.call_back(TWI_SL_RX_END, sizeof(dpt_frame_t) - 1, NULL);
	TEST_check("TWI_stop");
}


static void test_distant_frame_transmit_with_TWI_error(void)
{
	int dpt_res;

	dpt_interface_t interf = {
		.channel = 2,
		.cmde_mask = _CM(0x05),
		.rx = rx
	};

	dpt_frame_t fr = {
		.dest = 0x03,	// a distant node address
		//.orig = 7,	orig field is automatically filled
		.cmde = 0x08,
		.argv = { 0x11, 0x22, 0x33, 0x44 }
	};

	// DPT_init is called every test via the start function
	TEST_check("TWI_init");

	DPT_set_sl_addr(0x07);
	TEST_check("TWI_set_sl_addr : 0x07");

	DPT_register(&interf);
	TEST_log("DPT_register : channel #%d", interf.channel);
	TEST_check("DPT_register : channel #2");

	DPT_lock(&interf);

	TWI.ms_tx_resp = KO;
	dpt_res = DPT_tx(&interf, &fr);
	TEST_check("TWI_ms_tx : 0x03, 7");
	if (dpt_res == OK) {
		TEST_log("DPT_tx : OK");
	}
	else {
		TEST_log("DPT_tx : KO");
	}
	DPT_run();
	TEST_check("DPT_tx : KO");

	TWI.ms_tx_resp = OK;
	dpt_res = DPT_tx(&interf, &fr);
	TEST_check("TWI_ms_tx : 0x03, 7");
	if (dpt_res == OK) {
		TEST_log("DPT_tx : OK");
	}
	else {
		TEST_log("DPT_tx : KO");
	}
	DPT_run();
	TEST_check("DPT_tx : OK");

	DPT_unlock(&interf);
}


static void test_i2c_read_cmde(void)
{
	int dpt_res;

	dpt_interface_t interf = {
		.channel = 2,
		.cmde_mask = _CM(0) | _CM(0x05),
		.rx = rx
	};

	u8 vect[] = { 0x0c, 0x0d };

	dpt_frame_t fr = {
		.dest = 0x03,	// a distant node address
		.orig = 0x04,	// orig field is the length
		.cmde = FR_I2C_READ,
		.argv = { 0x02, 0x99, 0xaa, 0xbb }
	};

	// DPT_init is called every test via the start function
	TEST_check("TWI_init");

	DPT_register(&interf);
	TEST_log("DPT_register : channel #%d", interf.channel);
	TEST_check("DPT_register : channel #2");

	DPT_lock(&interf);

	// send read request
	TWI.ms_rx_resp = OK;
	dpt_res = DPT_tx(&interf, &fr);
	TEST_check("TWI_ms_rx : 0x03, 2");
	if (dpt_res == OK) {
		TEST_log("DPT_tx : OK");
	}
	else {
		TEST_log("DPT_tx : KO");
	}
	DPT_run();
	TEST_check("DPT_tx : OK");

	// while first request is not achieved, KO
	dpt_res = DPT_tx(&interf, &fr);
	if (dpt_res == OK) {
		TEST_log("DPT_tx : OK");
	}
	else {
		TEST_log("DPT_tx : KO");
	}
	DPT_run();
	TEST_check("DPT_tx : KO");

	// read is complete
	memcpy(TWI.buf, (u8*)&vect, sizeof(vect));
	TWI.call_back(TWI_MS_RX_END, 2, NULL);
	TEST_check("frame rxed : dest 0x01, orig 0x03, cmde 0x80, argv 0x00 0c 0d 00");
	TEST_check("TWI_stop");

	// it is possible to send another read request
	dpt_res = DPT_tx(&interf, &fr);
	TEST_check("TWI_ms_rx : 0x03, 2");
	if (dpt_res == OK) {
		TEST_log("DPT_tx : OK");
	}
	else {
		TEST_log("DPT_tx : KO");
	}
	DPT_run();
	TEST_check("DPT_tx : OK");

	DPT_unlock(&interf);
}


static void test_i2c_write_cmde(void)
{
	int dpt_res;
	u8 vect[] = { 0x0c, 0x0d };

	dpt_interface_t interf = {
		.channel = 2,
		.cmde_mask = _CM(0x01),
		.rx = rx
	};

	dpt_frame_t fr = {
		.dest = 0x03,	// a distant node address
		.orig = 0x05,	// orig field is the length
		.cmde = FR_I2C_WRITE,
		.argv = { 0x03, 0x99, 0xaa, 0xbb }
	};

	// DPT_init is called every test via the start function
	TEST_check("TWI_init");

	DPT_register(&interf);
	TEST_log("DPT_register : channel #%d", interf.channel);
	TEST_check("DPT_register : channel #2");

	DPT_lock(&interf);

	// send read request
	TWI.ms_rx_resp = OK;
	dpt_res = DPT_tx(&interf, &fr);
	TEST_check("TWI_ms_tx : 0x03, 3");
	if (dpt_res == OK) {
		TEST_log("DPT_tx : OK");
	}
	else {
		TEST_log("DPT_tx : KO");
	}
	DPT_run();
	TEST_check("DPT_tx : OK");

	// while first request is not achieved, KO
	dpt_res = DPT_tx(&interf, &fr);
	if (dpt_res == OK) {
		TEST_log("DPT_tx : OK");
	}
	else {
		TEST_log("DPT_tx : KO");
	}
	DPT_run();
	TEST_check("DPT_tx : KO");

	// read is complete
	memcpy(TWI.buf, vect, sizeof(vect));
	TWI.call_back(TWI_MS_TX_END, 2, NULL);
	TEST_check("frame rxed : dest 0x01, orig 0x03, cmde 0x81, argv 0x00 0c 0d 00");
	TEST_check("TWI_stop");

	// it is possible to send another read request
	dpt_res = DPT_tx(&interf, &fr);
	TEST_check("TWI_ms_tx : 0x03, 3");
	if (dpt_res == OK) {
		TEST_log("DPT_tx : OK");
	}
	else {
		TEST_log("DPT_tx : KO");
	}
	DPT_run();
	TEST_check("DPT_tx : OK");

	DPT_unlock(&interf);
}


static void test_transmit_time_out(void)
{
	int dpt_res;

	dpt_interface_t interf = {
		.channel = 2,
		.cmde_mask = _CM(0x05),
		.rx = rx
	};

	dpt_frame_t fr = {
		.dest = 0x03,	// a distant node address
		//.orig = 7,	orig field is automatically filled
		.cmde = 0x08,
		.argv = { 0x11, 0x22, 0x33, 0x44 }
	};

	// DPT_init is called every test via the start function
	TEST_check("TWI_init");

	DPT_set_sl_addr(0x07);
	TEST_check("TWI_set_sl_addr : 0x07");

	DPT_register(&interf);
	TEST_log("DPT_register : channel #%d", interf.channel);
	TEST_check("DPT_register : channel #2");

	DPT_lock(&interf);

	TWI.ms_tx_resp = OK;
	dpt_res = DPT_tx(&interf, &fr);
	TEST_check("TWI_ms_tx : 0x03, 7");
	if (dpt_res == OK) {
		TEST_log("DPT_tx : OK");
	}
	else {
		TEST_log("DPT_tx : KO");
	}
	DPT_run();
	TEST_check("DPT_tx : OK");

	DPT_unlock(&interf);

	// wait response
	DPT_run();
	DPT_run();

	// trigger time-out
	TIME.time += 50 * TIME_1_MSEC + 1;
	DPT_run();
	TEST_check("TWI_stop");
}


static void start(void)
{
	DPT_init();

	TIME.time = TIME_1_SEC;
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
			{ test_init,		"init" },
			{ test_set_sl_addr,	"set_sl_addr" },
			{ test_gen_call,	"gen_call" },
			{ test_distant_frame_transmit_without_lock,	"distant frame transmit without lock" },
			{ test_distant_frame_transmit,	"test distant frame transmit" },
			{ test_local_frame_transmit,	"test local frame transmit" },
			{ test_broadcast_frame_transmit,	"test broadcast frame transmit" },
			{ test_2_consecutive_distant_frames_transmit,	"test 2 consecutive distant frames transmit" },
			{ test_distant_frame_transmit_with_higher_lock,	"test distant frame transmit with higher lock" },
			{ test_register,	"test register" },
			{ test_incoming_frame_to_unregistered_appli,	"test incoming frame to unregistered appli" },
			{ test_incoming_frame_with_bad_length,	"test incoming frame with bad length" },
			{ test_incoming_frame_to_registered_appli,	"test incoming frame to registered appli" },
			{ test_distant_frame_transmit_with_TWI_error,	"test distant frame transmit with TWI error" },
			{ test_i2c_read_cmde,	"test i2c read cmde" },
			{ test_i2c_write_cmde,	"test i2c write cmde" },
			{ test_transmit_time_out,	"test transmit time out" },
			{ NULL,			NULL }
		},
	};

	TEST_run(&l, argv[1]);

	return 0;
}
