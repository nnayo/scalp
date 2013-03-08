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


// NAT : goal and description
//
// this package is a Node Address Translation.
//
// it is to be used as a serial or ethernet link gateway
// to access every node from a single one
// connected throught a serial or ethernet link.
//
//                                    I2C
//                                    bus
//                                     | <--> twi [Node V]
// [PC] tty <--> tty [Node W] twi <--> | 
//                                     | <--> twi [Node X]
//                                     | <--> twi [Node Y]
// [PC] eth <--> eth [Node Z] twi <--> | 


#ifndef __NAT_H__
# define __NAT_H__

# include "type_def.h"


//----------------------------------------
// public defines
//

// configuration flags to reduce memory usage
#define NAT_ENABLE_RS
#define NAT_FORCE_RS
//#define NAT_ENABLE_ETH

//----------------------------------------
// public types
//

//----------------------------------------
// public functions
//

// NAT module initialization
extern void NAT_init(void);


// NAT run method
extern void NAT_run(void);

#endif	// __NAT_H__
