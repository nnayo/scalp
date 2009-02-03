#include "test.h"

#include "drivers/twi.h"

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
	TWI.ms_rx_resp = KO;
	TWI.ms_tx_resp = KO;
	TWI.sl_rx_resp = KO;
	TWI.sl_tx_resp = KO;
	TWI.sl_addr = 0;

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
