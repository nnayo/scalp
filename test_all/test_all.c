#include <string.h>

#include "test.h"
#include "twi_stub.h"

#include "type_def.h"

#include "scalp/dispatcher.h"
#include "scalp/basic.h"
#include "scalp/reconf.h"
#include "scalp/dna.h"
#include "scalp/common.h"
#include "scalp/nat.h"

#include "utils/time.h"

#include "drivers/rs.h"

#include "avr/io.h"


// ------------------------------------------------
// stubs
//

struct {
	u32 t;
} TIME;


u32 TIME_get(void)
{
	TEST_log("TIME_get : t = %d", TIME.t);

	return TIME.t;
}


struct {
	u8* data;
} EEP;

static void eeprom_init(void)
{
	static u8 surv_data[] = {
		0x01, 0x01, 0x00, 0x0e, 0x7a, 0x00, 0x00, 0xff,
		0x01, 0x01, 0x00, 0x0a, 0x00, 0x30, 0x05, 0xff,
		0x01, 0x01, 0x00, 0x0a, 0x00, 0x58, 0x05, 0xff,
		0x01, 0x01, 0x00, 0x10, 0x00, 0xff, 0xff, 0xff,
		0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff,

		//-- start of extended zone --
		0x01, 0x01, 0x00, 0x10, 0x00, 0xff, 0xff, 0xff,
		0x01, 0x01, 0x00, 0x09, 0x00, 0x64, 0xff, 0xff,
		0x01, 0x01, 0x00, 0x10, 0xff, 0xff, 0xff, 0xff,
		0x70, 0x01, 0x00, 0x01, 0x01, 0x04, 0xff, 0xff,
		0x01, 0x01, 0x00, 0x17, 0xff, 0xff, 0xff, 0xff,
		0x01, 0x01, 0x00, 0x10, 0x00, 0xff, 0xff, 0xff,
		0x01, 0x01, 0x00, 0x09, 0x00, 0x64, 0xff, 0xff,
		0x01, 0x01, 0x00, 0x10, 0xff, 0xff, 0xff, 0xff,
		0x70, 0x01, 0x00, 0x01, 0x01, 0x05, 0xff, 0xff,
		0x01, 0x01, 0x00, 0x17, 0x00, 0xff, 0xff, 0xff,
	};

#if 0
	int i;
	u8 buf;

	// due to x86 number representation
	// each command is seen :
	// cmde:5 resp:1 error:1 nat:1
	// where as
	// resp:1 error:1 nat:1 cmde:5
	// is awaited
	// so it is needed to correct that

	for (i = 0; i < sizeof(surv_data) / sizeof(dpt_frame_t); i++) {
		buf = (surv_data[i * sizeof(dpt_frame_t) + 2] & 0x1f) << 3;
		buf |= (surv_data[i * sizeof(dpt_frame_t) + 2] & 0x80) >> 5;
		buf |= (surv_data[i * sizeof(dpt_frame_t) + 2] & 0x40) >> 5;
		buf |= (surv_data[i * sizeof(dpt_frame_t) + 2] & 0x10) >> 5;
		surv_data[i * sizeof(dpt_frame_t) + 2] = buf;
	}
#endif

	EEP.data = surv_data;
}


void eeprom_read_block(void* data, const void* addr, int len)
{
	union {
		void* ptr;
		int addr;
	} ptr2addr;
	u8* data_ptr;

	ptr2addr.ptr = (void*)addr;
	data_ptr = EEP.data + ptr2addr.addr;

	TEST_log("eeprom_read_block : addr = 0x%04x, len = %d", addr, len);
	memcpy(data, data_ptr, len);
	//TEST_log(" -> fr = { dest = 0x%02x, orig = 0x%02x, cmde = 0x%02x, argv[] = { 0x%02x, 0x%02x, 0x%02x, 0x%02x} }", *(data_ptr + 0), *(data_ptr + 1), *(data_ptr + 2), *(data_ptr + 3), *(data_ptr + 4), *(data_ptr + 5), *(data_ptr + 6));
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


int* get_char_ret;
int get_char_step;

void RS_init(u8 baud)
{
	get_char_step = 0;
}


// stub for getchar(void)
int _IO_getc (_IO_FILE *__fp)
{
	get_char_step--;

	if (get_char_step <= 0) {
		get_char_step = 0;
		return EOF;
	}

	return get_char_ret[get_char_step];
}


u8 DDRD;
u8 PORTD;
u8 PIND;

u8 DDRA;
u8 PORTA;


// ------------------------------------------------
// tests suite
//

static void all_run(void)
{
	BSC_run();
	RCF_run();
	DNA_run();
	CMN_run();
	NAT_run();
}

static void test_init(void)
{
	// check correct initialization
	TEST_check("TWI_init");
	TEST_check("eeprom_read_block : addr = 0x0000, len = 8");
		//" -> fr = { dest = 0x01, orig = 0x01, cmde = 0x0e, argv[] = { 0x7a, 0x00, 0x00, 0xff} }",
}


static void test_run_surv(void)
{
	int i;

	TEST_check("TWI_init");
	TEST_check("eeprom_read_block : addr = 0x0000, len = 8");

	TEST_check("TIME_get : t = 0");
	TEST_check("TIME_get : t = 0");
	TEST_check("eeprom_read_block : addr = 0x0008, len = 8");
	TEST_check("TIME_get : t = 0");
	TEST_check("eeprom_read_block : addr = 0x0030, len = 8");
	TEST_check("eeprom_read_block : addr = 0x0038, len = 8");
	TEST_check("eeprom_read_block : addr = 0x0040, len = 8");
	TEST_check("eeprom_read_block : addr = 0x0048, len = 8");
	TEST_check("eeprom_read_block : addr = 0x0050, len = 8");
	TEST_check("TIME_get : t = 0");
	TEST_check("TIME_get : t = 0");
	TEST_check("TIME_get : t = 0");
	TEST_check("TIME_get : t = 0");
	TEST_check("TIME_get : t = 0");
	TEST_check("TIME_get : t = 0");
	TEST_check("TIME_get : t = 0");
	TEST_check("TIME_get : t = 0");
	TEST_check("TIME_get : t = 0");
	TEST_check("TIME_get : t = 0");
	TEST_check("TIME_get : t = 0");
	TEST_check("TIME_get : t = 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_ms_tx : 0x70, 1");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_ms_rx : 0x08, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x09, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_set_sl_addr : 0x09");
	TEST_check("TWI_ms_rx : 0x02, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");

		// tag #0

	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x03, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x04, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x05, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x06, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x07, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x10, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x11, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x12, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x13, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x14, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x15, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x16, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x17, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x18, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x19, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x1a, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x1b, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x1c, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x1d, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x1e, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x1f, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x20, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x21, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x22, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x23, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x24, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x25, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x26, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x27, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x28, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x29, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x2a, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x2b, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x2c, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x2d, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x2e, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x2f, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x30, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x31, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x32, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x33, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x34, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x35, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x36, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x37, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x38, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x39, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x3a, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x3b, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x3c, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x3d, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x3e, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x3f, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x40, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x41, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x42, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x43, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x44, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x45, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x46, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x47, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x48, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x49, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x4a, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x4b, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x4c, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x4d, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x4e, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x4f, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x50, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x51, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x52, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x53, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x54, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x55, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x56, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x57, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x58, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x59, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x5a, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x5b, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x5c, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x5d, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x5e, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x5f, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x60, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x61, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x62, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x63, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x64, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x65, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x66, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x67, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x68, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x69, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x6a, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x6b, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x6c, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x6d, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x6e, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x6f, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x71, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x72, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x73, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x74, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x75, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x76, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x77, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x78, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x79, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x7a, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x7b, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x7c, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x7d, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x7e, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x7f, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_gen_call : 1");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");

		// tag #1

	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");

	DNA_init(DNA_BC);

	// ----------------------------
	// tests
	//

	// reconf has detected the nominal bus is ON
	for ( i = 0; i < 10; i++ ) {
		all_run();
	}

	// wait frame before reconfiguring the MUX
	TIME.t = 101 * TIME_1_MSEC;
	TWI.ms_tx_resp = OK;
	TWI.ms_rx_resp = OK;

	all_run();
	// end of wait

	all_run();
	TWI.call_back(TWI_MS_TX_END, 0, NULL);
	all_run();

	// then force nominal alim ON
	all_run();
	TWI.call_back(TWI_MS_TX_END, 0, NULL);
	all_run();

	// BC is scanning the bus to find a free address
	all_run();

	// first address is already taken
	all_run();

	TWI.call_back(TWI_MS_RX_END, 0, NULL);

	// second address is free
	all_run();

	TWI.call_back(TWI_NO_SL, 0, NULL);
	all_run();

	// tag #0

	//TWI.call_back(TWI_NO_SL, 0, NULL);
	//all_run();

	for ( i = 0; i < 118; i++ ) {
		all_run();
		TWI.call_back(TWI_NO_SL, 0, NULL);
	}

	// tag #1

	for ( i = 0; i < 10; i++ ) {
		all_run();
	}
}


static void test_run_surv_nat(void)
{
	int i;
	int nat_resp[2];

	TEST_check("TWI_init");
	TEST_check("eeprom_read_block : addr = 0x0000, len = 8");

	TEST_check("TIME_get : t = 0");
	TEST_check("TIME_get : t = 0");
	TEST_check("eeprom_read_block : addr = 0x0008, len = 8");
	TEST_check("TIME_get : t = 0");
	TEST_check("eeprom_read_block : addr = 0x0030, len = 8");
	TEST_check("eeprom_read_block : addr = 0x0038, len = 8");
	TEST_check("eeprom_read_block : addr = 0x0040, len = 8");
	TEST_check("eeprom_read_block : addr = 0x0048, len = 8");
	TEST_check("eeprom_read_block : addr = 0x0050, len = 8");
	TEST_check("TIME_get : t = 0");
	TEST_check("TIME_get : t = 0");
	TEST_check("TIME_get : t = 0");
	TEST_check("TIME_get : t = 0");
	TEST_check("TIME_get : t = 0");
	TEST_check("TIME_get : t = 0");
	TEST_check("TIME_get : t = 0");
	TEST_check("TIME_get : t = 0");
	TEST_check("TIME_get : t = 0");
	TEST_check("TIME_get : t = 0");
	TEST_check("TIME_get : t = 0");
	TEST_check("TIME_get : t = 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_ms_tx : 0x70, 1");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_ms_rx : 0x08, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x09, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_set_sl_addr : 0x09");
	TEST_check("TWI_ms_rx : 0x02, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");

		// tag #0

	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x03, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x04, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x05, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x06, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x07, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x10, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x11, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x12, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x13, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x14, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x15, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x16, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x17, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x18, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x19, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x1a, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x1b, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x1c, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x1d, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x1e, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x1f, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x20, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x21, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x22, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x23, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x24, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x25, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x26, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x27, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x28, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x29, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x2a, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x2b, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x2c, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x2d, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x2e, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x2f, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x30, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x31, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x32, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x33, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x34, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x35, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x36, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x37, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x38, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x39, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x3a, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x3b, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x3c, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x3d, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x3e, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x3f, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x40, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x41, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x42, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x43, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x44, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x45, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x46, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x47, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x48, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x49, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x4a, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x4b, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x4c, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x4d, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x4e, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x4f, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x50, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x51, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x52, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x53, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x54, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x55, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x56, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x57, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x58, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x59, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x5a, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x5b, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x5c, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x5d, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x5e, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x5f, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x60, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x61, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x62, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x63, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x64, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x65, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x66, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x67, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x68, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x69, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x6a, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x6b, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x6c, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x6d, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x6e, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x6f, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x71, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x72, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x73, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x74, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x75, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x76, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x77, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x78, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x79, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x7a, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x7b, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x7c, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x7d, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x7e, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_ms_rx : 0x7f, 0");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TWI_gen_call : 1");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");

		// tag #1

	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");

		// tag nat
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_ms_tx : 0x0a, 7");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TWI_stop");
	TEST_check("TIME_get : t = 1010");
	TEST_check("TIME_get : t = 1010");

	DNA_init(DNA_BC);

	// ----------------------------
	// tests
	//

	// reconf has detected the nominal bus is ON
	for ( i = 0; i < 10; i++ ) {
		all_run();
	}

	// wait frame before reconfiguring the MUX
	TIME.t = 101 * TIME_1_MSEC;
	TWI.ms_tx_resp = OK;
	TWI.ms_rx_resp = OK;

	all_run();
	// end of wait

	all_run();
	TWI.call_back(TWI_MS_TX_END, 0, NULL);
	all_run();

	// then force nominal alim ON
	all_run();
	TWI.call_back(TWI_MS_TX_END, 0, NULL);
	all_run();

	// BC is scanning the bus to find a free address
	all_run();

	// first address is already taken
	all_run();

	TWI.call_back(TWI_MS_RX_END, 0, NULL);

	// second address is free
	all_run();

	TWI.call_back(TWI_NO_SL, 0, NULL);
	all_run();

	// tag #0

	//TWI.call_back(TWI_NO_SL, 0, NULL);
	//all_run();

	for ( i = 0; i < 118; i++ ) {
		all_run();
		TWI.call_back(TWI_NO_SL, 0, NULL);
	}

	// tag #1

	for ( i = 0; i < 10; i++ ) {
		all_run();
	}

	// tag nat
	get_char_ret = nat_resp;
	get_char_step = 2;
	nat_resp[1] = 0x0a;	// dest
	all_run();
	get_char_step = 2;
	nat_resp[1] = 0x09;	// orig
	all_run();
	get_char_step = 2;
	nat_resp[1] = 0x0e;	// cmde
	all_run();
	get_char_step = 2;
	nat_resp[1] = 0x00;	// argv #0
	all_run();
	get_char_step = 2;
	nat_resp[1] = 0x05;	// argv #1
	all_run();
	get_char_step = 2;
	nat_resp[1] = 0x00;	// argv #2
	all_run();
	get_char_step = 2;
	nat_resp[1] = 0x00;	// argv #3
	all_run();

	all_run();
	TWI.call_back(TWI_MS_TX_END, 0, NULL);
	all_run();
	all_run();
}


static void start(void)
{
	PIND = _BV(PD4);	// nominal bus ON
	TIME.t = 0;

	DPT_init();
	BSC_init();
	RCF_init();
	CMN_init();
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
			{ test_init,		"test init" },
			{ test_run_surv,	"test run surv" },
			{ test_run_surv_nat,	"test run surv nat" },
			{ NULL,			NULL }
		},
	};

	eeprom_init();

	TEST_run(&l, argv[1]);

	return 0;
}
