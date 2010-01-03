#ifndef __FRAMES_H__
# define __FRAMES_H__

// number of arguments for each frame
#define FRAME_NB_ARGS	7

typedef enum {
	// ---------------------------------
	// dispatcher commands
	//

	FR_I2C_READ,				// 0x00 : I2C protocol read
	// argv #0:
	//	- number of octets to be read
	// argv #1, ..., #(FRAME_NB_ARGS - 1) :
	// 	- used if necessary

	FR_I2C_WRITE,				// 0x01 : I2C protocol write
	// no response frame will be received on success
	// argv #0:
	// 	- number of octets to be written (from 0 to (FRAME_NB_ARGS - 1))
	// argv #1, ..., #(FRAME_NB_ARGS - 1) :
	// 	- used if necessary

	// ---------------------------------
	// basic services
	//

	FR_NO_CMDE,					// 0x02 : no command
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

	FR_SPI_READ,				// I2C protocol read
	// argv #0:
	//	- number of octets to be read
	// argv #1, ..., #(FRAME_NB_ARGS - 1) :
	// 	- used if necessary

	FR_SPI_WRITE,				// I2C protocol write
	// argv #0:
	//	- number of octets to be written
	// argv #1, ..., #(FRAME_NB_ARGS - 1) :
	// 	- used if necessary

	FR_WAIT,				// 0x09 : wait some time given in ms
	// argv #0,#1 value :
	// 	- time in ms, MSB in #0

	FR_CONTAINER,				// 0x0a : encapsulated several other frames used for event handling
	// argv #0, #1 value :
	// 	- offset in memory for the first encapsulated frame (MSB first)
	// argv #2 value :
	// 	- 0xVV : nb encapsulated frames
	// argv #3 memory type or container number in eeprom: 
	//  - 0xee eeprom, 
	//  - 0xff flash, 
	//  - 0xaa ram, 
	//  - 0x00-0x05 : predefined eeprom container (in this case the other parameters are useless)

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
	// 	- 0x06 : door opening asked		(minuterie only)
	// 	- 0x07 : door open			(minuterie only)
	// 	- 0x08 : door closing asked		(minuterie only)
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

	FR_MINUT_THRES,				// 0x13 : take-off detection threshold and buzzer config
	// argv #0 value :
	// 	- take-off threshold duration in *10 ms (0 -> 2550 ms)
	// argv #1 value :
	// 	- buzzer off gap time in *1 s (0 -> 255 s)
	// argv #2 value :
	// 	- buzzer on time in *1 s (0 -> 255 s)

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


	// ---------------------------------
	// safeguard emitter commands
	//

	FR_EMITTER_CMD,				// 0x19 : safeguard emitter control
	// argv #0 value :
	// 	- 0x00 : switch off
	// 	- 0x0f : switch ON
	// 	- 0x12 : take-off signaling
	// 	- 0x4e : recovery mode
	// 	- 0x7e : get current mode (from the 4 previous states)
	// 	- 0xa5 : set take-off signal duration
	// 	- 0xaa : get take-off signal duration
	// 	- 0xc5 : set recovery ON duration
	// 	- 0xca : get recovery ON duration
	// 	- 0xe5 : set recovery off duration
	// 	- 0xea : get recovery off duration
	// 	- other : ignored
	// argv #1 value : for durations only (in 1/10 s)


	// ---------------------------------
	// logger commands
	//

	FR_LOG_CMD,					// 0x1a : modify logging setup
	// argv #0 cmde :
	// 	- 0x00 : off
	// 	- 0x1a : ON to sdcard
	// 	- 0x1e : ON to eeprom
	// 	- 0x27 : set command filter LSB part (bitfield for AND mask)
	// 		- argv #1 - #3 value : filter value (MSB first)
	// 	- 0x28 : set command filter MSB part (bitfield for AND mask)
	// 		- argv #1 - #3 value : filter value (MSB first)
	// 	- 0x2e : get command filter LSB part
	// 		- argv #1 - #3 resp : filter value (MSB first)
	// 	- 0x2f : get command filter MSB part
	// 		- argv #1 - #3 resp : filter value (MSB first)
	// 	- 0x3c : set origin filter (6 values : 0x00 logs from all nodes, 0xVV logs from given node, 0xff doesn't log)
	// 		- argv #1 - #6 value : filter value
	// 	- 0x3f : get origin filter
	// 		- argv #1 - #6 resp : filter value


	FR_ROUT_LIST,				// : number of set routes
	// argv #0 response : number of set routes

	FR_ROUT_LINE,				// retrieve a line content
	// argv #0 request : requested line
	// argv #1 response : virtual address
	// argv #2 response : routed address
	// argv #3 response : result OK (1) or ko (0)

	FR_ROUT_ADD,				// add a new route
	// argv #0 request : virtual address
	// argv #1 request : routed address
	// argv #2 response : result OK (1) or ko (0)

	FR_ROUT_DEL,				// delete a route
	// argv #0 request : virtual address
	// argv #1 request : routed address
	// argv #2 response : result OK (1) or ko (0)

	FR_DATA_ACC,				// acceleration data
	// argv #0 - #1 : MSB - LSB X acceleration
	// argv #2 - #3 : MSB - LSB Y acceleration
	// argv #4 - #5 : MSB - LSB Z acceleration

	FR_DATA_PRES,				// pressure data
	// argv #0 - #1 : MSB - LSB x10 pressure with offset
	// argv #2 - #3 : MSB - LSB raw pressure

	FR_DATA_IO,					// IO
	// argv #0 : IO bits value

	FR_DATA_ADC0,				// ADC values
	// argv #0 - #1 : MSB - LSB ADC 0
	// argv #2 - #3 : MSB - LSB ADC 1
	// argv #4 - #5 : MSB - LSB ADC 2

	FR_DATA_ADC3,				// ADC values
	// argv #0 - #1 : MSB - LSB ADC 3
	// argv #2 - #3 : MSB - LSB ADC 4
	// argv #4 - #5 : MSB - LSB ADC 5

	FR_DATA_ADC6,				// ADC values
	// argv #0 - #1 : MSB - LSB ADC 6
	// argv #2 - #3 : MSB - LSB ADC 7

	FR_CPU,						// CPU usage
	// argv #0 - #1 : MSB - LSB last 100 ms
	// argv #2 - #3 : MSB - LSB max value
	// argv #4 - #5 : MSB - LSB min value

} fr_cmdes_t;

// ---------------------------------
//
// IP config :
//	- MAC address (set/get)
//	- IP address (set/get)
//	- IP mask (set/get)
//
// PID :
//	- P/I/D gain (set/get)
//	- thresholds (set/get)
//	- status (over-speeds, over-positions,...) (set/get)
//	- position X/Y (set (command) / get (measure) )
//	- streaming (ON/off)
//	- mode (manuel/auto)
//

// memory storage types for container frame
#define EEPROM_STORAGE	0xee
#define RAM_STORAGE		0xaa
#define FLASH_STORAGE	0xff


#endif	// __FRAMES_H__
