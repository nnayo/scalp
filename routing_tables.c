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

#include "dispatcher.h"

#include "utils/pt.h"
#include "utils/fifo.h"

//------------------------------------------
// defines
//

#define ROUT_NB_RX		3


//------------------------------------------
// private types
//

struct route_item {
	u8 virtual_addr;
	u8 routed_addr;
};


//------------------------------------------
// private variables
//

static struct {
	// routing table
	u8 nb_pairs;
	struct route_item table[MAX_ROUTES];

	// interface
	
	// reception fifo
	struct nnk_fifo in_fifo;
	struct scalp in_buf[ROUT_NB_RX];

	struct scalp fr;

	struct scalp_dpt_interface interf;		// dispatcher interface

	pt_t pt;					// thread context
} route;


//------------------------------------------
// private functions
//

// retrieve the number of registered pairs
static void scalp_route_list(u8* nb_pairs)
{
	*nb_pairs = route.nb_pairs;
}


// retrieve the content of the given line if it exists
static u8 scalp_route_line(const u8 line, u8* virtual_addr, u8* routed_addr)
{
	// check if the required line exists
	if ( line > route.nb_pairs ) {
		return KO;
	}

	// retrieve the line
	*virtual_addr = route.table[line].virtual_addr;
	*routed_addr = route.table[line].routed_addr;

	return OK;
}


// add a new pair if possible
static u8 scalp_route_add(const u8 virtual_addr, const u8 routed_addr)
{
	// check if there is no more place left
	if ( route.nb_pairs >= MAX_ROUTES ) {
		return KO;
	}

	// add the new pair at the end of the list
	route.table[route.nb_pairs].virtual_addr = virtual_addr;
	route.table[route.nb_pairs].routed_addr = routed_addr;

	// update the pairs counter
	route.nb_pairs++;

	return OK;
}


// suppress a pair if it exists
static u8 scalp_route_del(const u8 virtual_addr, const u8 routed_addr)
{
	u8 match_index = MAX_ROUTES;
	u8 i;

	// scan the table to find the matching pair
	for ( i = 0; i < route.nb_pairs; i++ ) {
		if ( (route.table[i].virtual_addr == virtual_addr) && (route.table[i].routed_addr == routed_addr) ) {
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
	for ( i = match_index; i < route.nb_pairs - 1; i++ ) {
		route.table[i] = route.table[i + 1];
	}	

	// there is now one less pair
	route.nb_pairs--;

	return OK;
}

static PT_THREAD( scalp_route_rout(pt_t* pt) )
{
	PT_BEGIN(pt);

	// if a frame is received
	PT_WAIT_UNTIL(pt, nnk_fifo_get(&route.in_fifo, &route.fr));

	// if it is a response
	if ( route.fr.resp ) {
		// ignore it
		PT_RESTART(pt);
	}
        // else lock the channel until responsed
        scalp_dpt_lock(&route.interf);

	// treat it
	switch ( route.fr.cmde ) {
		case SCALP_ROUTELIST:
			scalp_route_list(&route.fr.argv[1]);
			break;

		case SCALP_ROUTELINE:
			route.fr.argv[3] = scalp_route_line(route.fr.argv[0], &route.fr.argv[1], &route.fr.argv[2]);
			break;

		case SCALP_ROUTEADD:
			route.fr.argv[2] = scalp_route_add(route.fr.argv[0], route.fr.argv[1]);
			break;

		case SCALP_ROUTEDEL:
			route.fr.argv[2] = scalp_route_del(route.fr.argv[0], route.fr.argv[1]);
			break;

		default:
			route.fr.error = 1;
			break;
	}

	// and send the response
	route.fr.resp = 1;
	PT_WAIT_UNTIL(pt, scalp_dpt_tx(&route.interf, &route.fr));

	// unlock the channel if no more frame are unqueued
	if ( nnk_fifo_full(&route.in_fifo) == 0 ) {
		scalp_dpt_unlock(&route.interf);
	}

	// loop back for next frame
	PT_RESTART(pt);

	PT_END(pt);
}


//------------------------------------------
// pthread interface
//

void scalp_route_init(void)
{
	// reset internals
	route.nb_pairs = 0;
	nnk_fifo_init(&route.in_fifo, &route.in_buf, ROUT_NB_RX, sizeof(route.in_buf[0]));
	PT_INIT(&route.pt);

	// register to dispatcher
	route.interf.channel = 9;
	route.interf.cmde_mask = _CM(SCALP_ROUTELIST)
                                | _CM(SCALP_ROUTELINE)
                                | _CM(SCALP_ROUTEADD)
                                | _CM(SCALP_ROUTEDEL);
	route.interf.queue = &route.in_fifo;
	scalp_dpt_register(&route.interf);
}


void scalp_route_run(void)
{
	// just handle the frame requests
	(void)PT_SCHEDULE(scalp_route_rout(&route.pt));
}


//------------------------------------------
// public functions
//

// retrieve the routed addresses from the specified address
void scalp_route_route(const u8 addr, u8 list[MAX_ROUTES], u8* list_len)
{
	u8 i;
	u8 j = 0;

	// scan the routing table
	for ( i = 0; i < route.nb_pairs; i++ ) {
		// append the routed address to the list up to the list size
		if ( route.table[i].virtual_addr == addr ) {
			if ( j < *list_len ) {
				list[j] = route.table[i].routed_addr;
				j++;
			}
		}
	}

	// return the number of routed addresses
	*list_len = j;
}
