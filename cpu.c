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

#define NB_HIST  2


//-----------------------------------------------
// private variables
//

static struct {
        pt_t pt;

        struct scalp_dpt_interface interf; // dispatcher interface
        struct scalp sclp;

        u32 time;                          // stats update time-out
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

void scalp_cpu_stats(void)
{
        u32 time;

        // check if time loop is elapsed
        time = nnk_time_get();
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
        if (nnk_slp_request(cpu.slp) == OK) {
                cpu.stat.slp++;
        }
}


static PT_THREAD(scalp_cpu_com(pt_t* pt))
{
        PT_BEGIN(pt);

        // every 1 second (when indexes are different)
        PT_WAIT_UNTIL(pt, cpu.save_idx != cpu.send_idx);

        // build the stats
        cpu.sclp.dest = SCALP_DPT_BROADCAST_ADDR;
        cpu.sclp.orig = SCALP_DPT_SELF_ADDR;
        cpu.sclp.cmde = SCALP_CPU;
        cpu.sclp.argv[0] = (cpu.hist[cpu.send_idx].cnt >> 8) & 0x00ff;
        cpu.sclp.argv[1] = (cpu.hist[cpu.send_idx].cnt >> 0) & 0x00ff;
        cpu.sclp.argv[2] = (cpu.max >> 8) & 0x00ff;
        cpu.sclp.argv[3] = (cpu.max >> 0) & 0x00ff;
        cpu.sclp.argv[4] = (cpu.min >> 8) & 0x00ff;
        cpu.sclp.argv[5] = (cpu.min >> 0) & 0x00ff;

        // then send them
        scalp_dpt_lock(&cpu.interf);
        PT_WAIT_UNTIL(pt, scalp_dpt_tx(&cpu.interf, &cpu.sclp));
        scalp_dpt_unlock(&cpu.interf);

        cpu.send_idx = cpu.save_idx;

        // loop back for next scalp
        PT_RESTART(pt);

        PT_END(pt);
}


//-----------------------------------------------
// module functions
//

void scalp_cpu_init(void)
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

        // if cpu module registers to sleep module
        // it will prevent any switch to sleep mode
        // something must be done in cpu_run()
        // to call nnk_slp_request() 
        // and to remind the numbers of sleeps
        // during a full period of stats
        cpu.slp = nnk_slp_register();

        // register to dispatcher
        cpu.interf.channel = 10;
        cpu.interf.queue = NULL;
        cpu.interf.cmde_mask = 0;
        scalp_dpt_register(&cpu.interf);

        PT_INIT(&cpu.pt);
}


void scalp_cpu_run(void)
{
        scalp_cpu_stats();

        (void)scalp_cpu_com(&cpu.pt);
}

