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

struct cpu {
	u16 cnt;
	u16 slp;
};


//-----------------------------------------------
// private defines
//

#define NB_HIST		2


//-----------------------------------------------
// private variables
//

static struct {
	pt_t pt;

	struct scalp_dpt_interface interf;		// dispatcher interface
	struct scalp fr;

	u32 time;					// stats update time-out
	struct cpu stat;

	u8 save_idx:1;
	u8 send_idx:1;
	struct cpu hist[NB_HIST];
	u16 min;
	u16 max;

	u16 slp;
} cpu;


//-----------------------------------------------
// private functions
//

void cpu_stats(void)
{
	u32 time;

	// check if time loop is elapsed
	time = time_get();
	if (time > cpu.time) {
		// update new time loop end
		cpu.time += TIME_1_SEC;

		// save last loop count
		cpu.hist[cpu.save_idx] = cpu.stat;
		cpu.save_idx++;

		// update stats
		if (cpu.stat.cnt > cpu.max)
			cpu.max = cpu.stat.cnt;
		if (cpu.stat.cnt < cpu.min)
			cpu.min = cpu.stat.cnt;

		// reset stat
		cpu.stat.cnt = 0;
		cpu.stat.slp = 0;
	}

	cpu.stat.cnt++;
	if (slp_request(cpu.slp) == OK) {
		cpu.stat.slp++;
	}
}


static PT_THREAD(cpu_com(pt_t* pt))
{
	PT_BEGIN(pt);

	// every 1 second (when indexes are different)
	PT_WAIT_UNTIL(pt, cpu.save_idx != cpu.send_idx);

	// send the stats
	cpu.fr.dest = DPT_BROADCAST_ADDR;
	cpu.fr.orig = DPT_SELF_ADDR;
	cpu.fr.cmde = SCALP_CPU;
	cpu.fr.argv[0] = (cpu.hist[cpu.send_idx].cnt >> 8) & 0x00ff;
	cpu.fr.argv[1] = (cpu.hist[cpu.send_idx].cnt >> 0) & 0x00ff;
	cpu.fr.argv[2] = (cpu.max >> 8) & 0x00ff;
	cpu.fr.argv[3] = (cpu.max >> 0) & 0x00ff;
	cpu.fr.argv[4] = (cpu.min >> 8) & 0x00ff;
	cpu.fr.argv[5] = (cpu.min >> 0) & 0x00ff;

	// send the response
	scalp_dpt_lock(&cpu.interf);
	PT_WAIT_UNTIL(pt, scalp_dpt_tx(&cpu.interf, &cpu.fr));
	scalp_dpt_unlock(&cpu.interf);

	cpu.send_idx = cpu.save_idx;

	// loop back for next frame
	PT_RESTART(pt);

	PT_END(pt);
}


//-----------------------------------------------
// module functions
//

void cpu_init(void)
{
	u8 i;

	cpu.time = 0;
	cpu.stat.cnt = 0;
	cpu.stat.slp = 0;

	// reset stats
	for (i = 0; i < NB_HIST; i++) {
		cpu.hist[i].cnt = 0;
		cpu.hist[i].slp = 0;
	}
	cpu.save_idx = 0;
	cpu.send_idx = 0;
	cpu.max = 0;
	cpu.min = 0xffff;

	// if cpu module registers to sleep modume
	// it will prevent any switch to sleep mode
	// something must be done in cpu_run()
	// to call SLP_request() 
	// and to remind the numbers of sleeps
	// during a full period of stats
	cpu.slp = slp_register();

	// register to dispatcher
	cpu.interf.channel = 10;
	cpu.interf.queue = NULL;
	cpu.interf.cmde_mask = 0;
	scalp_dpt_register(&cpu.interf);

	PT_INIT(&cpu.pt);
}


void cpu_run(void)
{
	cpu_stats();

	(void)cpu_com(&cpu.pt);
}

