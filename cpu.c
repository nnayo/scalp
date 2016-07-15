//-----------------------------------------------
// CPU load measurment
//
// status : 
//

#include "cpu.h"

#include "dispatcher.h"

#include "utils/time.h"
#include "utils/fifo.h"
#include "utils/pt.h"

#include "drivers/sleep.h"


//-----------------------------------------------
// private types
//

typedef struct {
	u16 cnt;
	u16 slp;
} cpu_t;


//-----------------------------------------------
// private defines
//

#define NB_HIST		2


//-----------------------------------------------
// private variables
//

static struct {
	pt_t pt;

	dpt_interface_t interf;		// dispatcher interface
	frame_t fr;

	u32 time;					// stats update time-out
	cpu_t stat;

	u8 save_idx:1;
	u8 send_idx:1;
	cpu_t hist[NB_HIST];
	u16 min;
	u16 max;

	slp_t slp;
} CPU;


//-----------------------------------------------
// private functions
//

void CPU_stats(void)
{
	u32 time;

	// check if time loop is elapsed
	time = TIME_get();
	if (time > CPU.time) {
		// update new time loop end
		CPU.time += TIME_1_SEC;

		// save last loop count
		CPU.hist[CPU.save_idx] = CPU.stat;
		CPU.save_idx++;

		// update stats
		if (CPU.stat.cnt > CPU.max)
			CPU.max = CPU.stat.cnt;
		if (CPU.stat.cnt < CPU.min)
			CPU.min = CPU.stat.cnt;

		// reset stat
		CPU.stat.cnt = 0;
		CPU.stat.slp = 0;
	}

	CPU.stat.cnt++;
	if (SLP_request(CPU.slp) == OK) {
		CPU.stat.slp++;
	}
}


static PT_THREAD(CPU_com(pt_t* pt))
{
	PT_BEGIN(pt);

	// every 1 second (when indexes are different)
	PT_WAIT_UNTIL(pt, CPU.save_idx != CPU.send_idx);

	// send the stats
	CPU.fr.dest = DPT_BROADCAST_ADDR;
	CPU.fr.orig = DPT_SELF_ADDR;
	CPU.fr.cmde = FR_CPU;
	CPU.fr.argv[0] = (CPU.hist[CPU.send_idx].cnt >> 8) & 0x00ff;
	CPU.fr.argv[1] = (CPU.hist[CPU.send_idx].cnt >> 0) & 0x00ff;
	CPU.fr.argv[2] = (CPU.max >> 8) & 0x00ff;
	CPU.fr.argv[3] = (CPU.max >> 0) & 0x00ff;
	CPU.fr.argv[4] = (CPU.min >> 8) & 0x00ff;
	CPU.fr.argv[5] = (CPU.min >> 0) & 0x00ff;

	// send the response
	dpt_lock(&CPU.interf);
	PT_WAIT_UNTIL(pt, dpt_tx(&CPU.interf, &CPU.fr));
	dpt_unlock(&CPU.interf);

	CPU.send_idx = CPU.save_idx;

	// loop back for next frame
	PT_RESTART(pt);

	PT_END(pt);
}


//-----------------------------------------------
// module functions
//

void CPU_init(void)
{
	u8 i;

	CPU.time = 0;
	CPU.stat.cnt = 0;
	CPU.stat.slp = 0;

	// reset stats
	for (i = 0; i < NB_HIST; i++) {
		CPU.hist[i].cnt = 0;
		CPU.hist[i].slp = 0;
	}
	CPU.save_idx = 0;
	CPU.send_idx = 0;
	CPU.max = 0;
	CPU.min = 0xffff;

	// if CPU registers to SLP
	// it will prevent any switch to sleep mode
	// something must be done in CPU_run()
	// to call SLP_request() 
	// and to remind the numbers of sleeps
	// during a full period of stats
	CPU.slp = SLP_register();

	// register to dispatcher
	CPU.interf.channel = 10;
	CPU.interf.queue = NULL;
	CPU.interf.cmde_mask = 0;
	dpt_register(&CPU.interf);

	PT_INIT(&CPU.pt);
}


void CPU_run(void)
{
	CPU_stats();

	(void)CPU_com(&CPU.pt);
}

