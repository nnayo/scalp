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

#include "routing_tables.h"

#include "scalp/dispatcher.h"

#include "utils/pt.h"
#include "utils/fifo.h"

//------------------------------------------
// defines
//

#define ROUT_NB_RX		3


//------------------------------------------
// private types
//

typedef struct {
	u8 virtual_addr;
	u8 routed_addr;
} rout_elem_t;


//------------------------------------------
// private variables
//

static struct {
	// routing table
	u8 nb_pairs;
	rout_elem_t table[MAX_ROUTES];

	// interface
	
	// reception fifo
	fifo_t in_fifo;
	dpt_frame_t in_buf[ROUT_NB_RX];

	dpt_frame_t fr;

	dpt_interface_t interf;		// dispatcher interface

	pt_t pt;					// thread context
} ROUT;


//------------------------------------------
// private functions
//

// retrieve the number of registered pairs
static void ROUT_list(u8* nb_pairs)
{
	*nb_pairs = ROUT.nb_pairs;
}


// retrieve the content of the given line if it exists
static u8 ROUT_line(const u8 line, u8* virtual_addr, u8* routed_addr)
{
	// check if the required line exists
	if ( line > ROUT.nb_pairs ) {
		return KO;
	}

	// retrieve the line
	*virtual_addr = ROUT.table[line].virtual_addr;
	*routed_addr = ROUT.table[line].routed_addr;

	return OK;
}


// add a new pair if possible
static u8 ROUT_add(const u8 virtual_addr, const u8 routed_addr)
{
	// check if there is no more place left
	if ( ROUT.nb_pairs >= MAX_ROUTES ) {
		return KO;
	}

	// add the new pair at the end of the list
	ROUT.table[ROUT.nb_pairs].virtual_addr = virtual_addr;
	ROUT.table[ROUT.nb_pairs].routed_addr = routed_addr;

	// update the pairs counter
	ROUT.nb_pairs++;

	return OK;
}


// suppress a pair if it exists
static u8 ROUT_del(const u8 virtual_addr, const u8 routed_addr)
{
	u8 match_index = MAX_ROUTES;
	u8 i;

	// scan the table to find the matching pair
	for ( i = 0; i < ROUT.nb_pairs; i++ ) {
		if ( (ROUT.table[i].virtual_addr == virtual_addr) && (ROUT.table[i].routed_addr == routed_addr) ) {
			match_index = i;
			break;
		}
	}

	// if no matching pair is found
	if ( match_index == MAX_ROUTES ) {
		return KO;
	}

	// if a matching pair is found
	// it is deleted by shifting the end of the table by one
	for ( i = match_index; i < ROUT.nb_pairs - 1; i++ ) {
		ROUT.table[i] = ROUT.table[i + 1];
	}	

	// there is now one less pair
	ROUT.nb_pairs--;

	return OK;
}

static PT_THREAD( ROUT_pt(pt_t* pt) )
{
	PT_BEGIN(pt);

	// if a frame is received
	PT_WAIT_UNTIL(pt, FIFO_get(&ROUT.in_fifo, &ROUT.fr));

	// if it is a response
	if ( ROUT.fr.resp ) {
		// ignore it
		PT_RESTART(pt);
	}

	// treat it
	switch ( ROUT.fr.cmde ) {
		case FR_ROUT_LIST:
			ROUT_list(&ROUT.fr.argv[1]);
			break;

		case FR_ROUT_LINE:
			ROUT.fr.argv[3] = ROUT_line(ROUT.fr.argv[0], &ROUT.fr.argv[1], &ROUT.fr.argv[2]);
			break;

		case FR_ROUT_ADD:
			ROUT.fr.argv[2] = ROUT_add(ROUT.fr.argv[0], ROUT.fr.argv[1]);
			break;

		case FR_ROUT_DEL:
			ROUT.fr.argv[2] = ROUT_del(ROUT.fr.argv[0], ROUT.fr.argv[1]);
			break;

		default:
			ROUT.fr.error = 1;
			break;
	}

	// and send the response
	ROUT.fr.resp = 1;
	PT_WAIT_UNTIL(pt, DPT_tx(&ROUT.interf, &ROUT.fr));
	DPT_unlock(&ROUT.interf);

	// loop back for next frame
	PT_RESTART(pt);

	PT_END(pt);
}


//------------------------------------------
// pthread interface
//

void ROUT_init(void)
{
	// reset internals
	ROUT.nb_pairs = 0;
	FIFO_init(&ROUT.in_fifo, &ROUT.in_buf, ROUT_NB_RX, sizeof(ROUT.in_buf[0]));
	PT_INIT(&ROUT.pt);

	// register to dispatcher
	ROUT.interf.channel = 9;
	ROUT.interf.cmde_mask = _CM(FR_ROUT_LIST) | _CM(FR_ROUT_LINE) | _CM(FR_ROUT_ADD) | _CM(FR_ROUT_DEL);
	ROUT.interf.queue = &ROUT.in_fifo;
	DPT_register(&ROUT.interf);
}


void ROUT_run(void)
{
	// just handle the frame requests
	(void)PT_SCHEDULE(ROUT_pt(&ROUT.pt));
}


//------------------------------------------
// public functions
//

// retrieve the routed addresses from the specified address
void ROUT_route(const u8 addr, u8 list[MAX_ROUTES], u8* list_len)
{
	u8 i;
	u8 j = 0;

	// scan the routing table
	for ( i = 0; i < ROUT.nb_pairs; i++ ) {
		// append the routed address to the list up to the list size
		if ( ROUT.table[i].virtual_addr == addr ) {
			if ( j < *list_len ) {
				list[j] = ROUT.table[i].routed_addr;
				j++;
			}
		}
	}

	// return the number of routed addresses
	*list_len = j;
}
