#ifndef __FRAMES_H__
# define __FRAMES_H__


typedef enum {
	// ---------------------------------
	// dispatcher commands
	//

	FR_I2C_READ,				// 0x00 : I2C protocol read
	// argv #0:
	//	- number of octets to be read
	// argv #1, #2, #3 :
	// 	- used if necessary

	FR_I2C_WRITE,				// 0x01 : I2C protocol write
	// no response frame will be received on success
	// argv #0:
	// 	- number of octets to be written (from 0 to 3)
	// argv #1, #2, #3 :
	// 	- used if necessary

	// ---------------------------------
	// basic services
	//

	FR_NO_CMDE,				// 0x02 : no command
	// no arg

	FR_RAM_READ,				// 0x03 : RAM read
	// argv #0, #1 :
	// 	- RAM address to read (MSB first)
	// argv #2 :
	// 	- read data at address
	// argv #3 :
	// 	- read data at address + 1 octet

	FR_RAM_WRITE,				// 0x04 : RAM write
	// argv #0, #1 :
	// 	- RAM address to write (MSB first)
	// argv #2, #3 :
	// 	- in cmde, data to be written
	// 	- in resp, data read back

	FR_EEP_READ,				// 0x05 : EEPROM read
	// argv #0, #1 :
	// 	- RAM address to read (MSB first)
	// argv #2 :
	// 	- read data at address
	// argv #3 :
	// 	- read data at address + 1 octet

	FR_EEP_WRITE,				// 0x06 : EEPROM write
	// argv #0, #1 :
	// 	- RAM address to write (MSB first)
	// argv #2, #3 :
	// 	- in cmde, data to be written
	// 	- in resp, data read back

	FR_FLH_READ,				// 0x07 : FLASH read
	// argv #0, #1 :
	// 	- FLASH address to read (MSB first)
	// argv #2 :
	// 	- read data at address
	// argv #3 :
	// 	- read data at address + 1 octet

	FR_FLH_WRITE,				// 0x08 : FLASH write (possibly implemented)
	// argv #0, #1 :
	// 	- RAM address to write (MSB first)
	// argv #2, #3 :
	// 	- in cmde, data to be written
	// 	- in resp, data read back

	FR_WAIT,				// 0x09 : wait some time given in ms
	// argv #0,#1 value :
	// 	- time in ms, MSB in #0

	FR_CONTAINER,				// 0x0a : encapsulated several other frames used for event handling
	// argv #0, #1 value :
	// 	- offset in EEPROM for the first encapsulated frame (MSB first)
	// argv #2 value :
	// 	- 0xVV : nb encapsulated frames

	// ---------------------------------
	// DNA commands
	//

	FR_REGISTER,				// 0x0b : register
	// argv #0 value :
	// 	- 0xVV : IS desired address
	// argv #1 value :
	// 	- 0xVV : IS type

	FR_LIST,				// 0x0c : list
	// argv #0 value :
	// 	- 0xVV : IS number
	// argv #1 value :
	// 	- 0xVV : BS number

	FR_LINE,				// 0x0d : line
	// argv #0 value :
	// 	- 0xVV : line index
	// argv #1 value :
	// 	- 0xVV : IS or BS type
	// argv #2 value :
	// 	- 0xVV : IS or BS i2c address

	// ---------------------------------
	// mission specific commands
	//

	FR_STATE,				// 0x0e : retrieve/modify state
	// argv #0 value :
	// 	- 0x00 : get
	// 	- 0x7a : set state and bus
	// 	- 0x8b : set state only
	// 	- 0x9c : set bus only
	// argv #1 value :
	// 	- 0x00 : ready				(default out of reset)
	//	- 0x01 : waiting for take-off
	// 	- 0x02 : waiting take-off confirmation	(minuterie only)
	// 	- 0x03 : flying
	// 	- 0x04 : waiting door open		(minuterie only)
	// 	- 0x05 : recovery
	// 	- 0x06 : buzzer				(minuterie only)
	// 	- 0x07 : door opening asked		(minuterie only)
	// 	- 0x08 : door open			(minuterie only)
	// 	- 0x09 : door closing asked		(minuterie only)
	// argv #2 value :
	// 	- bit 7 <=> nominal bus U0 OK
	// 	- bit 6 <=> nominal bus U1 OK
	// 	- bit 5 <=> nominal bus U2 OK
	// 	- bit 4 <=> nominal bus Uext ON
	// 	- bit 3 <=> redundant bus U0 OK
	// 	- bit 2 <=> redundant bus U1 OK
	// 	- bit 1 <=> redundant bus U2 OK
	// 	- bit 0 <=> redundant bus Uext ON

	FR_TIME_GET,				// 0x0f : retrieve on-board time
	// argv #0,#1,#2,#3 value :
	// 	- MSB to LSB on-board time in 10 us

	FR_MUX_POWER,				// 0x10 : switch ON/off the Nominal/redudant bus multiplexer
	// argv #0 value :
	// 	- 0x00 : off
	// 	- 0xff : ON


	// ---------------------------------
	// reconf commands
	//

	FR_RECONF_FORCE_MODE,			// 0x11 : force bus mode
	// argv #0 value :
	// 	- 0x00 : set force mode
	// 	- 0xff : get force mode
	// argv #1 value :
	// 	- 0x00 : force nominal bus active
	// 	- 0x01 : force redundant bus active
	// 	- 0x02 : force no bus active
	// 	- 0x03 : bus mode is automatic
	// argv #2 value (in response only) :
	// 	- 0x00 : nominal bus active
	// 	- 0x01 : redundant bus active
	// 	- 0x02 : no bus active

	// ---------------------------------
	// minuterie specific commands
	//

	FR_MINUT_TAKE_OFF,			// 0x12 : take-off detected
	// no arg

	FR_MINUT_THRES,				// 0x13 : take-off detection threshold
	// argv #0 value :
	// 	- take-off threshold duration in *10 ms (0 -> 2550 ms)

	FR_MINUT_OPEN_TIME,			// 0x14 : save/read culmination time
	// argv #0 value :
	// 	- 0x00 : save
	// 	- 0xff : read
	// argv #1 value :
	// 	- 0xVV : open time [0.0; 25.5] seconds

	FR_MINUT_DOOR_CMD,			// 0x15 : open/close door command
	// argv #0 value :
	// 	- 0x00 : open
	// 	- 0xff : close
	// 	- other : servo turn off

	FR_MINUT_POS,				// 0x16 : save/read servo position for open/close door
	// argv #0 value :
	// 	- 0x00 : save
	// 	- 0xff : read
	// argv #1 value :
	// 	- 0x00 : open position
	// 	- 0xff : close position
	// argv #2 value :
	// 	- 0xVV : servo position [-100; 100] degrees

	// ---------------------------------
	// surveillance specific commands
	//

	FR_SURV_PWR_CMD,			// 0x17 : switch nominal/redundant power supply ON/off
	// argv #0 value :
	// 	- 0x00 : redundant
	// 	- 0xff : nominal
	// argv #1 value :
	// 	- 0x00 : off
	// 	- 0xff : ON

	FR_SURV_UX_GET,				// 0x18 : read nominal/redundant power supply voltage #0/#1/#2/#3
	// argv #0 value :
	// 	- 0x00 : nominal voltage #0
	// 	- 0x01 : nominal voltage #1
	// 	- 0x02 : nominal voltage #2
	// 	- 0x0e : nominal external voltage
	// 	- 0xf0 : redundant voltage #0
	// 	- 0xf1 : redundant voltage #1
	// 	- 0xf2 : redundant voltage #2
	// 	- 0xfe : redundant external voltage
	// argv #1 value :
	// 	- 0xVV : voltage [0.0; 25.5] Volts


	FR_BUZZER_CMD,				// 0x19 : turn ON/off buzzer
	// argv #0 value :
	// 	- 0x00 : off
	// 	- 0xff : ON
	// 	- other : ignored


	// ---------------------------------
	// various masks and bits
	//

	//FR_CMDE_MASK	= 0x1f,			// command part mask
	//FR_NAT		= 0x20,			// Network Address Translation bit
	//FR_ERROR	= 0x40,			// error bit
	//FR_RESP		= 0x80,			// response bit

} fr_cmdes_t;

#endif	// __FRAMES_H__
