#include "test.h"

#include "drivers/twi.h"

extern struct {
	u8* buf;

	void (*call_back)(twi_state_t state, u8 nb_data, void* misc);

	u8 ms_rx_resp;
	u8 ms_tx_resp;
	u8 sl_rx_resp;
	u8 sl_tx_resp;
	u8 sl_addr;
} TWI;
