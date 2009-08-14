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
// Log
//

#ifndef __LOG_H__
# define __LOG_H__

# include "type_def.h"


//--------------------------------------
// configuration defines
//

#define START_ADDR	155		// the place before is reserved for event frames
#define END_ADDR	1024	// end of eeprom memory


//--------------------------------------
// typedef
//


//--------------------------------------
// function prototypes
//

// init the Log
extern void LOG_init(void);

// log module run method
extern u8 LOG_run(void);


#endif	// __LOG_H__
