// GPL v3 : copyright Yann GOUY
//
//
// BASIC : goal and description
//
// this package provides basic services :
//
//	- wait : frames treatment blocked for the given amount of time
//	- RAM read/write : access to RAM memory
//	- FLASH read/write : access to FLASH memory
//	- EEPROM read/write : access to EEPROM memory
//	- container : handling of container frames
//


#ifndef __BASIC_H__
#define __BASIC_H__


#include "fr_cmdes.h"

//----------------------------------------
// public defines
//


//----------------------------------------
// public types
//

//----------------------------------------
// public functions
//

// basic module initialization
extern void BSC_init(void);

// basic module computing function
extern void BSC_run(void);

#endif	// __BASIC_H__
