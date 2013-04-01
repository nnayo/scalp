#ifndef __FRAMES_H__
# define __FRAMES_H__

# include "type_def.h"


// --------------------------------------------
// public defines
//

// number of arguments for each frame
# define FRAME_NB_ARGS	6

// fields offsets
# define FRAME_DEST_OFFSET	0
# define FRAME_ORIG_OFFSET	1
# define FRAME_T_ID_OFFSET	2
# define FRAME_CMDE_OFFSET	3
# define FRAME_STAT_OFFSET	4
# define FRAME_ARGV_OFFSET	5

// CONTAINER
# define PRE_1_STORAGE	0x01
# define FLASH_STORAGE	0xff
# define PRE_3_STORAGE	0x03
# define RAM_STORAGE	0xaa
# define PRE_5_STORAGE	0x05
# define PRE_0_STORAGE	0x00
# define PRE_2_STORAGE	0x02
# define PRE_4_STORAGE	0x04
# define EEPROM_STORAGE	0xee

// STATE
# define FR_STATE_BRAKING	0x40
# define FR_STATE_CONE_CLOSED	0x05
# define FR_STATE_PARACHUTE	0x50
# define FR_STATE_INIT	0x00
# define FR_STATE_AERO_OPEN	0x03
# define FR_STATE_WAITING	0x10
# define FR_STATE_GET	0x9e
# define FR_STATE_CONE_OPENING	0x01
# define FR_STATE_SET	0x5e
# define FR_STATE_FLIGHT	0x20
# define FR_STATE_CONE_OPEN	0x30
# define FR_STATE_AERO_OPENING	0x02
# define FR_STATE_CONE_CLOSING	0x04

// MUX_RESET
# define FR_MUX_RESET_UNRESET	0x00
# define FR_MUX_RESET_RESET	0xff

// RECONF_MODE
# define FR_RECONF_MODE_GET	0xff
# define FR_RECONF_MODE_SET	0x00

// MINUT_SERVO_CMD
# define FR_SERVO_OFF	0x0f
# define FR_SERVO_CLOSE	0xc1
# define FR_SERVO_OPEN	0x09
# define FR_SERVO_CONE	0xc0
# define FR_SERVO_AERO	0xae

// MINUT_SERVO_INFO
# define FR_SERVO_SAVE	0x5a
# define FR_SERVO_OFF	0x0f
# define FR_SERVO_OPEN	0x09
# define FR_SERVO_AERO	0xae
# define FR_SERVO_CONE	0xc0
# define FR_SERVO_CLOSE	0xc1
# define FR_SERVO_READ	0x4e

// LOG_CMD
# define FR_LOG_CMD_SET_ORIG	0x3c
# define FR_LOG_CMD_RAM	0x14
# define FR_LOG_CMD_GET_LSB	0x2e
# define FR_LOG_CMD_GET_ORIG	0x3f
# define FR_LOG_CMD_EEPROM	0x1e
# define FR_LOG_CMD_OFF	0x00
# define FR_LOG_CMD_SDCARD	0x1a
# define FR_LOG_CMD_GET_MSB	0x2f
# define FR_LOG_CMD_SET_MSB	0x28
# define FR_LOG_CMD_SET_LSB	0x27

// LED_CMD
# define FR_LED_GET	0xff
# define FR_LED_OPEN	0x09
# define FR_LED_ALIVE	0xa1
# define FR_LED_SET	0x00


// --------------------------------------------
// public types
//

typedef enum {
	FR_I2C_READ = 0x00,
	// raw I2C read
	// status.len : number of octets to be read
	// argv #0-... : read octets

	FR_I2C_WRITE = 0x01,
	// raw I2C write
	// status.len : number of octets to be written
	// argv #0-... : octets to be written

	FR_NO_CMDE = 0x02,
	// no command
	// no arg

	FR_RAM_READ = 0x03,
	// RAM read
	// argv #0, #1 :
	// - RAM address to read (MSB first)
	// argv #2 :
	// - read data at address
	// argv #3 :
	// - read data at address + 1 octet

	FR_RAM_WRITE = 0x04,
	// RAM write
	// argv #0, #1 :
	// - RAM address to write (MSB first)
	// argv #2, #3 :
	// - in cmde, data to be written
	// - in resp, data read back

	FR_EEP_READ = 0x05,
	// EEPROM read
	// argv #0, #1 :
	// - RAM address to read (MSB first)
	// argv #2 :
	// - read data at address
	// argv #3 :
	// - read data at address + 1 octet

	FR_EEP_WRITE = 0x06,
	// EEPROM write
	// argv #0, #1 :
	// - RAM address to write (MSB first)
	// argv #2, #3 :
	// - in cmde, data to be written
	// - in resp, data read back

	FR_FLH_READ = 0x07,
	// FLASH read
	// argv #0, #1 :
	// - FLASH address to read (MSB first)
	// argv #2 :
	// - read data at address
	// argv #3 :
	// - read data at address + 1 octet

	FR_FLH_WRITE = 0x08,
	// FLASH write (possibly implemented)
	// argv #0, #1 :
	// - RAM address to write (MSB first)
	// argv #2, #3 :
	// - in cmde, data to be written
	// - in resp, data read back

	FR_SPI_READ = 0x09,
	// SPI read
	// status.len : number of octets to be read
	// argv #0-... : used if necessary

	FR_SPI_WRITE = 0x0a,
	// SPI write
	// status.len : number of octets to be written
	// argv #0-... : used if necessary

	FR_WAIT = 0x0b,
	// wait some time given in ms
	// argv #0,#1 value :
	// - time in ms, MSB in #0

	FR_CONTAINER = 0x0c,
	// encapsulated several other frames used for event handling
	// argv #0-1 value :
	// - offset in memory for the first encapsulated frame (MSB first)
	// argv #2 value :
	// - 0xVV : nb encapsulated frames
	// argv #3 memory type or container number in eeprom:
	// - 0x00-0x05 : predefined eeprom container (in this case the other parameters are useless)
	// - 0xee eeprom,
	// - 0xff flash,
	// - 0xaa ram,

	FR_DNA_REGISTER = 0x0d,
	// DNA register
	// argv #0 value :
	// - 0xVV : IS desired address
	// argv #1 value :
	// - 0xVV : IS type

	FR_DNA_LIST = 0x0e,
	// DNA list
	// argv #0 value :
	// - 0xVV : IS number
	// argv #1 value :
	// - 0xVV : BS number

	FR_DNA_LINE = 0x0f,
	// DNA line
	// argv #0 value :
	// - 0xVV : line index
	// argv #1 value :
	// - 0xVV : IS or BS type
	// argv #2 value :
	// - 0xVV : IS or BS i2c address

	FR_STATE = 0x10,
	// state (retrieve / modify)
	// argv #0 value :
	// - 0x9e : get state
	// - 0x5e : set state
	// argv #1 value :
	// - 0x00 : init				(default out of reset)
	// - 0x01 : cone opening
	// - 0x02 : aero opening
	// - 0x03 : aero open
	// - 0x04 : cone closing
	// - 0x05 : cone closed
	// - 0x10 : waiting
	// - 0x20 : flight
	// - 0x30 : cone open
	// - 0x40 : braking
	// - 0x50 : parachute

	FR_TIME_GET = 0x11,
	// retrieve on-board time
	// argv #0,#1,#2,#3 value :
	// - MSB to LSB on-board time in 10 us

	FR_MUX_RESET = 0x12,
	// force / release the reset of the Nominal/redudant bus multiplexer
	// argv #0 value :
	// - 0x00 : unreset
	// - 0xff : reset

	FR_RECONF_MODE = 0x13,
	// force bus mode
	// argv #0 value :
	// - 0x00 : set force mode
	// - 0xff : get force mode
	// argv #1 value :
	// - 0x00 : force nominal bus active
	// - 0x01 : force redundant bus active
	// - 0x02 : force no bus active
	// - 0x03 : bus mode is automatic
	// argv #2 value (in response only) :
	// - 0x00 : nominal bus active
	// - 0x01 : redundant bus active
	// - 0x02 : no bus active

	FR_TAKE_OFF = 0x14,
	// take-off detected
	// no arg

	FR_TAKE_OFF_THRES = 0x15,
	// take-off detection threshold config
	// argv #0 value :
	// - take-off threshold duration in *100 ms : [0ms; 25.5s]
	// argv #1 value :
	// - longitudinal acceleration threshold for take-off in 0.1G : [-12.8G; 12.7G]

	FR_MINUT_TIME_OUT = 0x16,
	// save/read culmination time
	// argv #0 value :
	// - 0x00 : save
	// - 0xff : read
	// argv #1 value :
	// - 0xVV : open time [0.0; 25.5] seconds

	FR_MINUT_SERVO_CMD = 0x17,
	// open/close servo command
	// argv #0 value :
	// - 0xc0 : cone
	// - 0xae : aero
	// argv #1 value :
	// - 0x09 : open
	// - 0xc1 : close
	// - 0x0f : servo turn off

	FR_MINUT_SERVO_INFO = 0x18,
	// save/read servo position
	// argv #0 value :
	// - 0xc0 : cone
	// - 0xae : aero
	// argv #1 value :
	// - 0x5a : save
	// - 0x4e : read
	// argv #2 value :
	// - 0x09 : open position
	// - 0xc1 : close position
	// argv #3 value :
	// - 0xVV : servo position [-100; 100] degrees

	FR_SWITCH_POWER = 0x19,
	// switch nominal/redundant power supply ON/off
	// argv #0 value :
	// - 0x00 : redundant
	// - 0xff : nominal
	// argv #1 value :
	// - 0x00 : off
	// - 0xff : ON

	FR_READ_VOLTAGES = 0x1a,
	// read nominal/redundant power supply voltage #0/#1/#2/#3
	// argv #0 value :
	// - 0x00 : nominal voltage #0
	// - 0x01 : nominal voltage #1
	// - 0x02 : nominal voltage #2
	// - 0x0e : nominal external voltage
	// - 0xf0 : redundant voltage #0
	// - 0xf1 : redundant voltage #1
	// - 0xf2 : redundant voltage #2
	// - 0xfe : redundant external voltage
	// argv #1 value :
	// - 0xVV : voltage [0.0; 25.5] Volts

	FR_EMITTER_CMD = 0x1b,
	// safeguard emitter control
	// argv #0 value :
	// - 0x00 : switch off
	// - 0x0f : switch ON
	// - 0x12 : take-off signaling
	// - 0x4e : recovery mode
	// - 0x7e : get current mode (from the 4 previous states)
	// - 0xa5 : set take-off signal duration
	// - 0xaa : get take-off signal duration
	// - 0xc5 : set recovery ON duration
	// - 0xca : get recovery ON duration
	// - 0xe5 : set recovery off duration
	// - 0xea : get recovery off duration
	// - other : ignored
	// argv #1 value : for durations only (in 1/10 s)

	FR_LOG_CMD = 0x1c,
	// modify logging setup
	// argv #0 cmde :
	// - 0x00 : off
	// - 0x14 : ON to RAM
	// - 0x1a : ON to sdcard
	// - 0x1e : ON to eeprom
	// - 0x27 : set command filter LSB part (bitfield for AND mask)
	// - argv #1 - #3 value : filter value (MSB first)
	// - 0x28 : set command filter MSB part (bitfield for AND mask)
	// - argv #1 - #3 value : filter value (MSB first)
	// - 0x2e : get command filter LSB part
	// - argv #1 - #3 resp : filter value (MSB first)
	// - 0x2f : get command filter MSB part
	// - argv #1 - #3 resp : filter value (MSB first)
	// - 0x3c : set origin filter (6 values : 0x00 logs from all nodes, 0xVV logs from given node, 0xff doesn't log)
	// - argv #1 - #6 value : filter value
	// - 0x3f : get origin filter
	// - argv #1 - #6 resp : filter value

	FR_ROUT_LIST = 0x1d,
	// number of set routes
	// argv #0 response : number of set routes

	FR_ROUT_LINE = 0x1e,
	// retrieve a line content
	// argv #0 request : requested line
	// argv #1 response : virtual address
	// argv #2 response : routed address
	// argv #3 response : result OK (1) or ko (0)

	FR_ROUT_ADD = 0x1f,
	// add a new route
	// argv #0 request : virtual address
	// argv #1 request : routed address
	// argv #2 response : result OK (1) or ko (0)

	FR_ROUT_DEL = 0x20,
	// delete a route
	// argv #0 request : virtual address
	// argv #1 request : routed address
	// argv #2 response : result OK (1) or ko (0)

	FR_DATA_ACC = 0x21,
	// acceleration data
	// argv #0 - #1 : MSB - LSB X acceleration
	// argv #2 - #3 : MSB - LSB Y acceleration
	// argv #4 - #5 : MSB - LSB Z acceleration

	FR_DATA_GYR = 0x22,
	// acceleration data
	// argv #0 - #1 : MSB - LSB X gyro
	// argv #2 - #3 : MSB - LSB Y gyro
	// argv #4 - #5 : MSB - LSB Z gyro

	FR_DATA_PRES = 0x23,
	// pressure data
	// argv #0 - #1 : MSB - LSB x10 pressure with offset
	// argv #2 - #3 : MSB - LSB raw pressure

	FR_DATA_IO = 0x24,
	// IO
	// argv #0 : IO bits value

	FR_DATA_ADC0 = 0x25,
	// ADC values
	// argv #0 - #1 : MSB - LSB ADC 0
	// argv #2 - #3 : MSB - LSB ADC 1
	// argv #4 - #5 : MSB - LSB ADC 2

	FR_DATA_ADC3 = 0x26,
	// ADC values
	// argv #0 - #1 : MSB - LSB ADC 3
	// argv #2 - #3 : MSB - LSB ADC 4
	// argv #4 - #5 : MSB - LSB ADC 5

	FR_DATA_ADC6 = 0x27,
	// ADC values
	// argv #0 - #1 : MSB - LSB ADC 6
	// argv #2 - #3 : MSB - LSB ADC 7

	FR_CPU = 0x28,
	// CPU usage
	// argv #0 - #1 : MSB - LSB last 100 ms
	// argv #2 - #3 : MSB - LSB max value
	// argv #4 - #5 : MSB - LSB min value

	FR_LED_CMD = 0x2a,
	// set/get led blink rate
	// argv #0 value :
	// - 0xa1 : alive
	// - 0x09 : open
	// argv #1 value :
	// - 0x00 : set
	// - 0xff : get
	// argv #2 value :
	// - 0xVV : rate [0.00; 2.55] s

	FR_APPLI_START = 0x3f,
	// application start signal
	// and last command in list

} fr_cmdes_t;


// frame format (header + arguments)
typedef struct {
	u8 dest;				// message destination
	u8 orig;				// message origin
	u8 t_id;				// transaction identifier
	fr_cmdes_t cmde;		// message command
	union {
		u8 status;			// status field
		struct {			// and its sub-parts
			u8 error:1;		// error flag
			u8 resp:1;		// response flag
			u8 time_out:1;	// time-out flag
			u8 eth:1;		// eth nat flag
			u8 serial:1;	// serial nat flag
			u8 len:3;	// length / number of arguments
		};
	};
	u8 argv[FRAME_NB_ARGS];	// msg command argument(s) if any
} frame_t;


extern u8 frame_set_0(frame_t* fr, u8 dest, u8 orig, fr_cmdes_t cmde, u8 len);

extern u8 frame_set_1(frame_t* fr, u8 dest, u8 orig, fr_cmdes_t cmde, u8 len, u8 argv0);

extern u8 frame_set_2(frame_t* fr, u8 dest, u8 orig, fr_cmdes_t cmde, u8 len, u8 argv0, u8 argv1);

extern u8 frame_set_3(frame_t* fr, u8 dest, u8 orig, fr_cmdes_t cmde, u8 len, u8 argv0, u8 argv1, u8 argv2);

extern u8 frame_set_4(frame_t* fr, u8 dest, u8 orig, fr_cmdes_t cmde, u8 len, u8 argv0, u8 argv1, u8 argv2, u8 argv3);

extern u8 frame_set_5(frame_t* fr, u8 dest, u8 orig, fr_cmdes_t cmde, u8 len, u8 argv0, u8 argv1, u8 argv2, u8 argv3, u8 argv4);

extern u8 frame_set_6(frame_t* fr, u8 dest, u8 orig, fr_cmdes_t cmde, u8 len, u8 argv0, u8 argv1, u8 argv2, u8 argv3, u8 argv4, u8 argv5);


#endif	// __FRAMES_H__
