// GPL v3 : copyright Yann GOUY
//

#include "scalp/alive.h"

#include "scalp/dispatcher.h"
#include "scalp/dna.h"

#include "utils/time.h"
#include "utils/pt.h"


//----------------------------------------
// private defines
//

#define ALV_ANTI_BOUNCE_LIMIT	5
#define ALV_TIME_INTERVAL	TIME_1_SEC


//----------------------------------------
// private variables
//

static struct {
	dpt_interface_t interf;		// dispatcher interface

	pt_t pt;					// thread context

	dpt_frame_t fr;				// a buffer frame

	u8 anti_bounce;				// anti-bounce counter

	u8 nb_mnt;					// number of other found minuteries
	u8 mnt_addr[2];				// addresses of the other minuteries
	u8 cur_mnt;					// current minuterie index

	u32 time_out;				// time-out for scan request sending
} ALV;


//----------------------------------------
// private functions
//

static u8 ALV_nodes_addresses(void)
{
	dna_list_t* list;
	u8 nb_is;
	u8 nb_bs;
	u8 i;
	u8 nb = 0;

	// retrieve nodes addresses
	list = DNA_list(&nb_is, &nb_bs);

	// extract the other minuteries addresses
	for ( i = 0; i < nb_is; i++ ) {
		// same type but different address
		if ( (list[i].type == DNA_SELF_TYPE(list)) &&
			( list[i].i2c_addr != DNA_SELF_ADDR(list)) ) {
			// an other minuterie is found
			// save its address
			ALV.mnt_addr[nb] = list[i].i2c_addr;

			// increment nb of found minuteries in this scan
			nb++;
		}
	}

	// finally save the number of found minuteries
	ALV.nb_mnt = nb;

	// return the current node address
	return DNA_SELF_ADDR(list);
}


static u8 ALV_next_address(void)
{
	u8 addr;

	// if no other minuterie is visible
	if ( ALV.nb_mnt == 0 ) {
		// increment anti-bounce counter
		// because we're in trouble as we're alone
		ALV.anti_bounce++;

		// return an invalid address (it is in the range of the BS)
		return DPT_LAST_ADDR;
	}

	// so at least 1 other minuterie is declared
	// ensure the current MNT index is valid
	ALV.cur_mnt %= ALV.nb_mnt;

	// retrieve its address
	addr = ALV.mnt_addr[ALV.cur_mnt];

	// finally prepare the access to the next minuterie
	ALV.cur_mnt++;

	// return the computed address
	return addr;
}


static void ALV_dpt_rx(dpt_frame_t* fr)
{
	// if not a status response
	if ( !fr->resp ) {
		// throw it away
		return;
	}

	// if response is in error
	if ( fr->error ) {
		// increment anti-counter
		ALV.anti_bounce++;

		return;
	}

	// everything is ok, so reset the anti-bounce counter
	ALV.anti_bounce = 0;
}


static PT_THREAD( ALV_pt(pt_t* pt) )
{
	u8 self_addr;

	PT_BEGIN(pt);

	// every second
	PT_WAIT_UNTIL(pt, TIME_get() > ALV.time_out);

	// update time-out
	ALV.time_out += ALV_TIME_INTERVAL;

	// extract visible other minuteries addresses
	self_addr = ALV_nodes_addresses();

	// build the get state request
	ALV.fr.orig = self_addr;
	ALV.fr.dest = ALV_next_address();
	ALV.fr.resp = 0;
	ALV.fr.error = 0;
	ALV.fr.nat = 0;
	ALV.fr.cmde = FR_STATE;
	ALV.fr.argv[0] = 0x00;

	// send the status request
	DPT_lock(&ALV.interf);
	PT_WAIT_UNTIL(pt, OK == DPT_tx(&ALV.interf, &ALV.fr));
	DPT_unlock(&ALV.interf);

	// check if the anti-bounve counter has reached its limit
	if ( ALV.anti_bounce >= ALV_ANTI_BOUNCE_LIMIT ) {
		// then set the mode to autonomous
		ALV.fr.orig = DPT_SELF_ADDR;
		ALV.fr.dest = DPT_SELF_ADDR;
		ALV.fr.resp = 0;
		ALV.fr.error = 0;
		ALV.fr.nat = 0;
		ALV.fr.cmde = FR_RECONF_FORCE_MODE;
		ALV.fr.argv[0] = 0x00;
		ALV.fr.argv[1] = 0x02;

		DPT_lock(&ALV.interf);
		PT_WAIT_UNTIL(pt, OK == DPT_tx(&ALV.interf, &ALV.fr));
		DPT_unlock(&ALV.interf);

		// and finally block
		PT_WAIT_WHILE(pt, OK);
	}

	// loop back
	PT_RESTART(pt);

	PT_END(pt);
}


//----------------------------------------
// public functions
//

// ALIVE module initialization
void ALV_init(void)
{
	// thread context init
	PT_INIT(&ALV.pt);

	// variables init
	ALV.anti_bounce = 0;
	ALV.nb_mnt = 0;
	ALV.cur_mnt = 0;
	ALV.time_out = ALV_TIME_INTERVAL;

	// register own call-back for specific commands
	ALV.interf.channel = 9;
	ALV.interf.cmde_mask = _CM(FR_STATE);
	ALV.interf.rx = ALV_dpt_rx;
	DPT_register(&ALV.interf);
}


// ALIVE module run method
void ALV_run(void)
{
	// send response if any
	(void)PT_SCHEDULE(ALV_pt(&ALV.pt));
}
