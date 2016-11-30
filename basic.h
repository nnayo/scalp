// GPL v3 : copyright Yann GOUY
//
//
// BASIC (BSC) : goal and description
//
// this package provides basic services :
//
//	- wait : frames treatment blocked for the given amount of time
//	- RAM read/write : access to RAM memory
//	- FLASH read/write : access to FLASH memory
//	- EEPROM read/write : access to EEPROM memory
//	- container : handling of container frames
//


#ifndef __SCALP_BASIC_H__
#define __SCALP_BASIC_H__

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
extern void scalp_bsc_init(void);

// basic module computing function
extern void scalp_bsc_run(void);

#endif	// __SCALP_BASIC_H__
