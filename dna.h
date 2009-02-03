//---------------------------------------------------------------------------------------
//  Copyright (C) 2000-2007  <Yann GOUY>
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
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

//--------------------------------------
// Dynamic Node Addressing 
//
// this layer is an answer to the problem of dynamic address attribution over the I2C bus
//
// it plugs over the TWI layer
//
// TWI and I2C are the same
//
//


#if 0

DNA (was DHCP over I2C)

the main idea is to allow to "intelligent" I2C components to register when connecting the bus

the context is:
 - a master that is the "Bus Controller" (BC)
 - several basic slaves (BS) (no intelligence, they only respond to their address)
 - several intelligent slaves (IS) (they behave as described bellow)


the BC protocole is:
 1- the BC chooses an random address (outside BS reserved ranges, see annex)
 2- the BC checks whether the address is already taken sending a CHECK REQUEST
 3- if the address is not free, restarts at step 1
 4- once the BC has its address, it answers to this address and accepts broadast writings
 5- if the BC receives a REGISTER COMMAND, it sends back an REGISTER RESPONSE to the IS that is now registered
 6- if the BC receives a LIST COMMAND, it sends back to the requesting IS the data via a LIST RESPONSE
 7- if the BC receives a LIST DETAILS COMMAND, it sends back to the requesting IS the details via a LIST DETAILS RESPONSE
 8- loop to step 5


the IS protocole is:
 1- the IS chooses an random address (outside BS reserved ranges, see annex)
 2- the IS checks whether the address is already taken sending a CHECK REQUEST
 3- if the address is not free, restarts at step 1
 4- now the IS answers to this address and only to this address (no broadcast)
 5- the IS starts a timer (1 second seems clever)
 6- the IS sends a REGISTER COMMAND containing its address and its type
 7- on time-out, loop to step 5
 8- if the REGISTER RESPONSE is received, continue proceeding else loop to step 7
 9- the IS is registered to the BC, it can now answer on broadcast
	

communication start description

 - ISs and BC search for a free address
 - then 2 possibilities:
	- the BC is ready first, then every IS requests will be satisfied as long as the BC can handle multiple requests in parallel
	- some or whole the IS are ready before the BC, they will have to send several REGISTER FRAMEs waiting for the BC to answer.
remark: it is of the responsability of the IS to make sure it is registered.


frames details

CHECK REQUEST
 - I2C address + read bit


REGISTER
* command
 - I2C broadcast address + write bit
 - frame identifier
 - IS I2C address right padded (MSB is no significant but is zero)
 - IS type

* response
 - IS I2C address + write bit
 - frame identifier
 - BC I2C address


LIST FRAME (BC -> IS)
 - broadcast address + write bit
 - frame identifier
 - nb IS
 - nb BS


LINE FRAME (BC -> IS)
 - broadcast address + write bit
 - frame identifier
 - list line
 - node type
 - node i2c address
LIST TRAME (IS -> BC)
 - BC I2C address + write bit
 - frame identifier


ANNEXES

I2C reserved address ranges
 - 0000000 : general call
 - 0011xxx : I/O expander
 - 0101xxx : ADC
 - 01101xx : analogic switch
 - 1001xxx : ADC
 - 10011xx : analogic switch
 - 1010xxx : FRAM
 - 1100xxx : led driver
 - 1110xxx : numeric switch
 - 1111xxx : reserved for futur use and 10 bits address extension

BC and ISs prefered address range
 - 0001xxx : quite low adresses so quite prioritary


reminder:
for I2C, 0 bits are dominant upon 1 bits
then addresses starting with 0 are prioritary upon the others


#endif


#ifndef __DNA_H__
# define __DNA_H__

# include "scalp/dna_list.h"	// all node types list

# include "type_def.h"

# include "utils/pt.h"		// pt_t


//--------------------------------------
// configuration defines
//

// prefered I2C address range
# define DNA_I2C_ADDR_MIN	0x08	// 0b0001000
# define DNA_I2C_ADDR_MAX	0x0f	// 0b0001111

// total size of the DNA I2C registered nodes
# define DNA_LIST_SIZE		10	// only 8 IS + BS as index 0 is for self and 1 for BC


//--------------------------------------
// typedef
//



// properties of an I2C connected component (i2c address and type)
typedef struct {
	dna_t type;
	u8 i2c_addr;
} dna_list_t;


//--------------------------------------
// function prototypes
//

// init the DNA protocol
//
// mode : BC or IS or other
extern void DNA_init(dna_t mode);

// DNA run method
//
// the return value shall be ignored
extern u8 DNA_run(void);


// DNA list composition is
//  addr type
// +----+----+
// |    |    | self
// +----+----+
// |    |    | BC
// +----+----+
// |    |    | IS
// +----+----+
// .
// .		somewhere in the list, our own address is duplicated
// .
// +----+----+
// |    |    | last registered IS
// +----+----+
// .
// .		an hole filled with zeroes
// .
// +----+----+
// |    |    | last found BS
// +----+----+
// .
// .		BS are stored from the end of the list
// .
// +----+----+
// |    |    | first found BS
// +----+----+
//
//
// get DNA I2C registered components (IS) list
//
// nb_is : number of registered IS
// nb_bs : number of found BS
//
extern dna_list_t* DNA_list(u8* nb_is, u8* nb_bs);

//--------------------------------------
// helper macros
//

// retrieve self I2C address from registered nodes full list
# define DNA_SELF_ADDR(list)	\
	list[DNA_SELF].i2c_addr

// retrieve self type from registered nodes full list
# define DNA_SELF_TYPE(list)	\
	list[DNA_SELF].type

// retrieve self I2C address from registered nodes full list
# define DNA_BC_ADDR(list)	\
	list[DNA_BC].i2c_addr

// retreive first IS index from registered nodes full list
# define DNA_FIRST_IS_INDEX(nb_is)	\
	2

// retreive last IS index from registered nodes full list
# define DNA_LAST_IS_INDEX(nb_is)	\
	(2 + nb_is - 1)


#endif	// __DNA_H__
