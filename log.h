//---------------------------------------------------------------------------------------
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

//--------------------------------------
// Log
//

#ifndef __LOG_H__
# define __LOG_H__

# include "type_def.h"


//--------------------------------------
// configuration defines
//

// eeprom limits
#define EEPROM_START_ADDR	((u16)256)		// the place before is reserved for event frames
#define EEPROM_END_ADDR		((u16)1024)		// 1 Ko

// sdcard limits
#define SDCARD_START_ADDR	((u64)0x100)	// FAT headers
#define SDCARD_END_ADDR		((u64)2 * 1024 * 1024 * 1024)	// 2 Go


//--------------------------------------
// typedef
//


//--------------------------------------
// function prototypes
//

// init the log
extern void scalp_log_init(void);

// log module run method
extern void scalp_log_run(void);


#endif	// __LOG_H__
