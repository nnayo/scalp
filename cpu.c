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
#define FIFO_SIZE	1


//-----------------------------------------------
// private variables
//

static struct {
	dpt_interface_t interf;		// dispatcher interface
	fifo_t fifo;
	frame_t buf[FIFO_SIZE];
	frame_t fr;
	pt_t pt;

	u32 time;					// stats update time-out
	cpu_t stat;

	u8 index;
	cpu_t hist[NB_HIST];
	u16 min;
	u16 max;

	slp_t slp;
} CPU;


//-----------------------------------------------
// private functions
//

static void CPU_stats(void)
{
	u32 time;

	// check if time loop is elapsed
	time = TIME_get();
	if (time > CPU.time) {
		// update new time loop end
		CPU.time = TIME_get() + 100 * TIME_1_MSEC;

		// save last loop count
		CPU.hist[CPU.index] = CPU.stat;
		CPU.index++;
		if (CPU.index >= NB_HIST)
			CPU.index = 0;

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
	u8 index;

	PT_BEGIN(pt);

	// wait until a new frame is received
	PT_WAIT_UNTIL(pt, FIFO_get(&CPU.fifo, &CPU.fr) && !CPU.fr.resp);

	// build the response
	CPU.fr.resp = 1;
	index = (CPU.index - 1) % NB_HIST;
	CPU.fr.argv[0] = (CPU.hist[index].cnt >> 8) & 0x00ff;
	CPU.fr.argv[1] = (CPU.hist[index].cnt >> 0) & 0x00ff;
	CPU.fr.argv[2] = (CPU.max >> 8) & 0x00ff;
	CPU.fr.argv[3] = (CPU.max >> 0) & 0x00ff;
	CPU.fr.argv[4] = (CPU.min >> 8) & 0x00ff;
	CPU.fr.argv[5] = (CPU.min >> 0) & 0x00ff;

	// send the response
	DPT_lock(&CPU.interf);
	PT_WAIT_UNTIL(pt, DPT_tx(&CPU.interf, &CPU.fr));
	DPT_unlock(&CPU.interf);

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
	CPU.index = 0;
	CPU.max = 0;
	CPU.min = 0xffff;

	// if CPU registers to SLP
	// it will prevent any switch to sleep mode
	// sometime must be done in CPU_run()
	// to call SLP_request() 
	// and to remind the numbers of sleeps
	// during a full period of stats
	CPU.slp = SLP_register();

	FIFO_init(&CPU.fifo, &CPU.buf, FIFO_SIZE, sizeof(CPU.buf[0]));

	// register to dispatcher
	CPU.interf.channel = 10;
	CPU.interf.queue = &CPU.fifo;
	CPU.interf.cmde_mask = _CM(FR_CPU);
	DPT_register(&CPU.interf);

	PT_INIT(&CPU.pt);
}


void CPU_run(void)
{
	CPU_stats();

	(void)PT_SCHEDULE(CPU_com(&CPU.pt));
}

