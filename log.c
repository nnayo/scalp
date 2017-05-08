//---------------------
//  Copyright (C) 2000-2009  <Yann GOUY>
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; see the file COPYING.  If not, write to
//  the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
//  Boston, MA 02111-1307, USA.
//
//  you can write to me at <yann_gouy@yahoo.fr>
//

#include "log.h"

#include "dispatcher.h"

#include "utils/pt.h"
#include "utils/fifo.h"
#include "utils/time.h"

#include "drivers/eeprom.h"
#include "externals/sdcard.h"

#include "avr/io.h"

#include <string.h>                // memset()


//----------------------------------------
// private defines
//

#define NB_IN_FRAMES  5
#define NB_OUT_FRAMES  2

#define MINIMAL_FILTER  SCALP_DPT_CM(SCALP_LOG)

#define NB_ORIG_FILTER  5

// enable the save mediums
#define LOG_IN_RAM_ENABLED
//#define LOG_IN_EEPROM_ENABLED
//#define LOG_IN_SDCARD_ENABLED

#ifdef LOG_IN_RAM_ENABLED
# define RAM_BUFFER_SIZE  12
#endif


//----------------------------------------
// private defines
//

struct log {
        u8 index;    // log index (unique for each log session)
        u8 time[2];  // middle octets of the time (2.56 ms resol / 46.6 h scale)
        struct scalp sclp;  // complete scalp
};

enum log_state {
        LOG_OFF,
        LOG_RAM,
#ifdef LOG_IN_EEPROM_ENABLE
        LOG_EEPROM,
#endif
#ifdef LOG_IN_SDCARD_ENABLE
        LOG_SDCARD,
#endif
};


//----------------------------------------
// private variables
//

static struct {
        struct scalp_dpt_interface interf;  // dispatcher interface
        pt_t in_pt;                         // context
        pt_t out_pt;                        // context

        struct nnk_fifo in_fifo;            // reception fifo
        struct scalp in_buf[NB_IN_FRAMES];
        struct nnk_fifo out_fifo;           // emission fifo
        struct scalp out_buf[NB_OUT_FRAMES];
        struct scalp fr;                    // command frame 

        enum log_state state;               // logging state

#ifdef LOG_IN_RAM_ENABLED
        struct {
                u8 index;
                struct log buffer[RAM_BUFFER_SIZE];
        } ram;
#endif
#ifdef LOG_IN_EEPROM_ENABLE
        struct {
                u16 addr;                   // address in eeprom
        } eeprom;
#endif
#ifdef LOG_IN_SDCARD_ENABLE
        struct {
                u64 addr;                   // address in sdcard
        } sdcard;
#endif
        u8 index;                           // session index

        struct {
                u8 orig[NB_ORIG_FILTER];    // origin node filter
                u32 cmd;                    // command filter
                u32 rsp;                    // response filter
        } filter;

        struct log block;
} log;


//----------------------------------------
// private functions
//

#ifdef LOG_IN_EEPROM_ENABLE
// find the start address the new logging session
// and return the log index found in eeprom
static u8 scalp_log_eeprom_start_find(void)
{
        u8 buf[2];

        // scan the whole eeprom
        while (1) {
                // read the 2 first octets of the log at log.addr
                nnk_eep_read(log.eeprom.addr, buf, sizeof(buf));

                // wait end of reading
                while ( !nnk_eep_is_fini() )
                        ;

                // if the read octets are erased eeprom
                if ( (buf[0] == 0xff) && (buf[1] == 0xff) ) {
                        // the new log session start address is found (here)
                        //
                        // the previous index is known from the previous read (see below)
                        // so increment it
                        log.index++;

                        // finally the scan is over
                        return log.index;
                }

                // extract index
                log.index = buf[0];
        
                // check the next log block
                log.eeprom.addr += sizeof(struct log);

                // if address is out of range
                if ( log.eeprom.addr >= EEPROM_END_ADDR ) {
                        // eeprom is full
                        // so give up
                        // the log thread protection will prevent overwriting
                        return 0xff;
                }
        }
}
#endif


#ifdef LOG_IN_SDCARD_ENABLE
// find the start address in sdcard for this new logging session
// and set the common log index for eeprom and sdcard
static void scalp_log_sdcard_start_find(u8 start_index)
{
        u8 buf[2];

        // scan the whole sdcard
        while (true) {
                // read the 2 first octets of the log at log.addr
                SD_read(log.sdcard.addr, buf, sizeof(buf));
                while ( !SD_is_fini() )
                        ;

                // if the read octets are erased eeprom
                if ((buf[0] == 0xff) && (buf[1] == 0xff)) {
                        // the new log session start address is found (here)
                        // the previous index is known from the previous read (see below)
                        // so increment it
                        log.index++;

                        // index found during eeprom scanning is higher
                        if (start_index > log.index) {
                                // use the eeprom index
                                log.index = start_index;
                        }

                        // finally the scan is over
                        return;
                }

                // extract index
                log.index = buf[0];
        
                // check the next log block
                log.sdcard.addr += sizeof(struct log);

                // if address is out of range
                if (log.sdcard.addr >= SDCARD_END_ADDR) {
                        // sdcard is full
                        // so give up
                        // the log thread protection will prevent overwriting
                        log.index = 0xff;
                }
        }
}
#endif


void scalp_log_command(struct scalp* fr)
{
        u32 filter;

        // upon the sub-command
        switch (fr->argv[0]) {
        case SCALP_LOG_OFF:  // off
                log.state = LOG_OFF;
                break;

#ifdef LOG_IN_RAM_ENABLED
        case SCALP_LOG_RAM:  // log to RAM
                log.state = LOG_RAM;
                break;
#endif

#ifdef LOG_IN_EEPROM_ENABLED
        case SCALP_LOG_EEPROM:  // log to eeprom
                log.state = LOG_EEPROM;
                break;
#endif

#ifdef LOG_IN_SDCARD_ENABLED
        case SCALP_LOG_SDCARD:  // log to sdcard
                log.state = LOG_SDCARD;
                break;
#endif

        case SCALP_LOG_CMD_ENABLE:  // enable command filter
                filter  = (u32)fr->argv[1] << 24;
                filter |= (u32)fr->argv[2] << 16;
                filter |= (u32)fr->argv[3] <<  8;
                filter |= (u32)fr->argv[4] <<  0;

                // enable the commands
                log.filter.cmd |= filter;

                // modify filtering preserving logging communication
                log.interf.cmde_mask = MINIMAL_FILTER 
                        | log.filter.cmd | log.filter.rsp;
                break;

        case SCALP_LOG_CMD_DISABLE:  // disable command filter
                filter  = (u32)fr->argv[1] << 24;
                filter |= (u32)fr->argv[2] << 16;
                filter |= (u32)fr->argv[3] <<  8;
                filter |= (u32)fr->argv[4] <<  0;

                // disable the commands
                log.filter.cmd &= ~filter;

                // modify filtering preserving logging communication
                log.interf.cmde_mask = MINIMAL_FILTER 
                        | log.filter.cmd | log.filter.rsp;
                break;

        case SCALP_LOG_CMD_GET:  // get command filter
                // get the filter value
                filter = log.filter.cmd;

                fr->argv[1] = (u8)(filter >> 24);
                fr->argv[2] = (u8)(filter >> 16);
                fr->argv[3] = (u8)(filter >>  8);
                fr->argv[4] = (u8)(filter >>  0);
                break;

        case SCALP_LOG_RSP_ENABLE:  // enable response filter
                filter  = (u32)fr->argv[1] << 24;
                filter |= (u32)fr->argv[2] << 16;
                filter |= (u32)fr->argv[3] <<  8;
                filter |= (u32)fr->argv[4] <<  0;

                // enable the responses
                log.filter.rsp |= filter;

                // modify filtering preserving logging communication
                log.interf.cmde_mask = MINIMAL_FILTER 
                        | log.filter.cmd | log.filter.rsp;
                break;

        case SCALP_LOG_RSP_DISABLE:  // disable response filter
                filter  = (u32)fr->argv[1] << 24;
                filter |= (u32)fr->argv[2] << 16;
                filter |= (u32)fr->argv[3] <<  8;
                filter |= (u32)fr->argv[4] <<  0;

                // disable the responses
                log.filter.rsp &= ~filter;

                // modify filtering preserving logging communication
                log.interf.cmde_mask = MINIMAL_FILTER 
                        | log.filter.cmd | log.filter.rsp;
                break;

        case SCALP_LOG_RSP_GET:  // get response filter
                // get the filter value
                filter = log.filter.rsp;

                fr->argv[1] = (u8)(filter >> 24);
                fr->argv[2] = (u8)(filter >> 16);
                fr->argv[3] = (u8)(filter >>  8);
                fr->argv[4] = (u8)(filter >>  0);
                break;

        case SCALP_LOG_ORIG_SET:  // set origin filter
                memcpy(log.filter.orig, &fr->argv[1], NB_ORIG_FILTER);
                break;

        case SCALP_LOG_ORIG_GET:  // get origin filter
                memcpy(&fr->argv[1], log.filter.orig, NB_ORIG_FILTER);
                break;

        default:
                // unknown sub-command
                fr->error = 1;
                break;
        }

        // set response bit
        fr->resp = 1;

        // enqueue the scalp for response sending
        nnk_fifo_put(&log.out_fifo, &fr);

        // restore scalp intial state
        fr->resp = fr->error = 0;
}


static PT_THREAD( scalp_log_in(pt_t* pt) )
{
        struct scalp fr;

        PT_BEGIN(pt);

        // wait while no frame is present in the fifo
        PT_WAIT_UNTIL(pt, nnk_fifo_get(&log.in_fifo, &fr));

        // if it is a log command and not a response
        if (fr.cmde == SCALP_LOG && !fr.resp)
                // treat it
                scalp_log_command(&fr);

        // by default, every frame is filtered
        u8 is_filtered = OK;

        // filter the frame according to its origin
        for (u8 i = 0; i < sizeof(log.filter.orig); i++) {
                // passthrough or frame origin and filter acceptance match
                if ((log.filter.orig[i] == 0x00) || (log.filter.orig[i] == fr.orig)) {
                        is_filtered = KO;
                        break;
                }
        }

        // filter the scalp according to its command
        if (!fr.resp && !(log.filter.cmd & SCALP_DPT_CM(fr.cmde)))
                is_filtered = OK;

        // filter the scalp according to its response status
        if (fr.resp && !(log.filter.rsp & SCALP_DPT_CM(fr.cmde)))
                is_filtered = OK;

        // if frame is filtered away
        if (is_filtered) {
                // lop back for next frame
                PT_RESTART(pt);
        }

        // build the log packet
        log.block.index = log.index;
        u32 time = nnk_time_get();
        log.block.time[0] = (u8)(time >> 16);
        log.block.time[1] = (u8)(time >>  8);
        log.block.sclp = fr;

        switch (log.state) {
        case LOG_OFF:
                // nothing to do
                break;

#ifdef LOG_IN_RAM_ENABLED
        case LOG_RAM:
                log.ram.buffer[log.ram.index] = log.block;
                log.ram.index++;
                if (log.ram.index >= RAM_BUFFER_SIZE)
                        log.ram.index = 0;
                break;
#endif

#ifdef LOG_IN_EEPROM_ENABLED
        case LOG_EEPROM:
                // if address is out of range
                if (log.eeprom_addr >= EEPROM_END_ADDR)
                        // logging is no more possible
                        // so quit
                        PT_EXIT(pt);

                // save it to eeprom
                PT_WAIT_UNTIL(pt, nnk_eep_write(log.eeprom_addr, (u8*)&log.block, sizeof(struct log)));

                // wait until saving is done
                PT_WAIT_UNTIL(pt, nnk_eep_is_fini());
                break;
#endif

#ifdef LOG_IN_SDCARD_ENABLED
        case LOG_SDCARD:
                // if address is out of range
                if (log.sdcard_addr >= SDCARD_END_ADDR)
                        // logging is no more possible
                        // so quit
                        PT_EXIT(pt);

                // save it to sdcard (fill the write buffer)
                PT_WAIT_UNTIL(pt, SD_write(log.sdcard_addr, (u8*)&log.block, sizeof(struct log)));

                // wait until saving is done

                break;
#endif

        default:
                // shall never happen but just in case
                // loop back for next frame
                break;
        }

        // loop back to treat the next frame to log
        PT_RESTART(pt);

        PT_END(pt);
}

static PT_THREAD( scalp_log_out(pt_t* pt) )
{
        PT_BEGIN(pt);

        // wait while no frame is present in the fifo
        PT_WAIT_UNTIL(pt, nnk_fifo_get(&log.out_fifo, &log.fr));

        // send the response
        scalp_dpt_lock(&log.interf);
        PT_WAIT_UNTIL(pt, scalp_dpt_tx(&log.interf, &log.fr));
        scalp_dpt_unlock(&log.interf);

        // loop back to treat the next frame to log
        PT_RESTART(pt);

        PT_END(pt);
}


//----------------------------------------
// public functions
//

// log module initialization
void scalp_log_init(void)
{
        // init context and fifo
        PT_INIT(&log.in_pt);
        nnk_fifo_init(&log.in_fifo, &log.in_buf, NB_IN_FRAMES, sizeof(log.in_buf[0]));
        PT_INIT(&log.out_pt);
        nnk_fifo_init(&log.out_fifo, &log.out_buf, NB_OUT_FRAMES, sizeof(log.out_buf[0]));

        // reset scan start address and index
#ifdef LOG_IN_RAM_ENABLED
        log.ram.index = 0;
#endif
#ifdef LOG_IN_EEPROM_ENABLED
        log.eeprom_addr = EEPROM_START_ADDR;
#endif
#ifdef LOG_IN_SDCARD_ENABLED
        log.sdcard_addr = SDCARD_START_ADDR;
#endif
        log.index = 0;

        // origin filter blocks no node by default
        memset(&log.filter.orig, 0x00, NB_ORIG_FILTER);

        // logging is off by default
        log.state = LOG_OFF;

        // find the start address for this session
#ifdef LOG_IN_EEPROM_ENABLED
        log.index = scalp_log_find_eeprom_start();
#endif
#ifdef LOG_IN_SDCARD_ENABLED
        scalp_log_find_sdcard_start(log.index);
#endif

        // register to dispatcher
        log.interf.channel = 6;
        log.interf.queue = &log.in_fifo;
        log.interf.cmde_mask = MINIMAL_FILTER;
        scalp_dpt_register(&log.interf);
}


// log run method
void scalp_log_run(void)
{
        // incoming scalp handling
        (void)PT_SCHEDULE(scalp_log_in(&log.in_pt));

        // outgoing scalp handling
        (void)PT_SCHEDULE(scalp_log_out(&log.out_pt));
}
