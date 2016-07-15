#!/usr/bin/env python

"""
each frame are composed of :
	- destination field 	offset + 0
	- origin field 		offset + 1
	- transaction id 	offset + 2
	- state 		offset + 3
	- command field 	offset + 4
	- argument #0 field 	offset + 5
	- argument #1 field 	offset + 6
	- argument #2 field 	offset + 7
	- argument #3 field 	offset + 8
	- argument #4 field 	offset + 9
	- argument #5 field 	offset + 10
"""

import sys


class size_error(Exception):
	pass


# Frame base class
#
class Frame(object):
	# I2C addresses
	I2C_BROADCAST_ADDR	= 0x00
	I2C_SELF_ADDR		= 0x01

	# fake transaction ID
	T_ID				= None

	# status bits
	ERROR				= 0x80
	RESP				= 0x40
	TIME_OUT			= 0x20
	ETH					= 0x10
	SERIAL				= 0x08
	CMD					= 0x00
	CMD_MASK			= 0xf8
	LEN_MASK			= 0x07


	nb_args = 6

	t_id = 0

	def __init__(self, dest = None, orig = None, t_id = None, cmde = None, status = None, *argv):
		"""
		create an empty frame with completely unsignfull content by default
		"""
		self.dest = dest
		self.orig = orig
		if t_id is None:
			self.t_id = Frame.t_id
		else:
			self.t_id = t_id
		self.cmde = cmde
		self.stat = status
		if len(argv) > Frame.nb_args:
			raise size_error()
		self.argv = argv

		Frame.t_id += 1


	@staticmethod
	def get_derived_class():
		# retrieve all subclass derivated from Frame
		dc = {}
		for l in globals():
			t = eval(l)
			if type(t) == type(object) and t.__bases__[0].__name__ == 'Frame':
				dc[t.cmde] = t

		return dc


	@staticmethod
	def frame_update(fr):
		if fr.__class__ != Frame().__class__:
			return None

		return Frame.frame(fr.dest, fr.orig, fr.t_id, fr.cmde, fr.stat, fr.argv)


	@staticmethod
	def frame(dest, orig, t_id=None, cmde=None, stat=None, *argv):
		dc = Frame.get_derived_class()

		# create the frame associated to the command value
		if dc.has_key(cmde):
			e = '%s(%s, %s, %s, %s, ' % (dc[cmde].__name__, dest, orig, t_id, stat)
			argv = argv[0]
			for i in range(len(argv)):
				e += '%s, ' % argv[i]
			e = e[:-2]
			e += ')'

			fr = eval(e)
			return fr

		# no recognized command so create an anonymous frame
		return Frame()


	def __len__(self):
		"""
		return the frame length in bytes
		"""
		return 5 + Frame.nb_args


	def response_set(self):
		self.stat |= Frame.RESP


	def response_unset(self):
		self.stat &= ~Frame.RESP


	def error_set(self):
		self.stat |= Frame.ERROR


	def error_unset(self):
		self.stat &= ~Frame.ERROR


	def time_out_set(self):
		self.stat |= Frame.TIME_OUT


	def time_out_unset(self):
		self.stat &= ~Frame.TIME_OUT


	def from_bytes(self, list):
		"""
		help decoding a byte sequence
		"""
		self.dest = list[0]
		self.orig = list[1]
		self.t_id = list[2]
		self.cmde = list[3]
		self.stat = list[4]
		self.argv = list[5:]


	def to_bytes(self):
		"""
		return the frame encoded as a tuple
		"""
		ret = (self.dest, self.orig, self.t_id, self.cmde, self.stat)
		for i in range(len(self.argv)):
			ret += (self.argv[i], )


	def from_string(self, s):
		"""
		help decoding a string
		"""
		self.dest = ord(s[0])
		self.orig = ord(s[1])
		self.t_id = ord(s[2])
		self.cmde = ord(s[3])
		self.stat = ord(s[4])
		self.argv = []
		for i in range(Frame.nb_args):
			self.argv.append(ord(s[5 + i]))


	def to_string(self):
		"""
		return the frame encoded as a string
		"""
		st = ''
		try:
			for c in self.to_bytes():
				st += chr(c)
		except:
			print '%r' % self
			raise
		return st

	def to_dict(self):
		"""
		return the frame encoded as a dict
		"""
		d = {}
		d['dest'] = self.dest
		d['orig'] = self.orig
		d['t_id'] = self.t_id
		d['cmde'] = self.cmde
		d['stat'] = self.stat
		for i in range(len(self.argv)):
			d['argv%d' % i] = self.argv[i]

		return d

	def _addr_decode(self, val):
		if val == Frame.I2C_BROADCAST_ADDR:
			return 'bcst'
		if val == Frame.I2C_SELF_ADDR:
			return 'self'
		if val == None:
			return 'None'
		return '0x%02x' % val


	def _stat_decode(self):
		st = ''

		cmd = self.stat & Frame.CMD_MASK
		if cmd & Frame.RESP:
			st += 'r'
		else:
			st += 'c'
		if cmd & Frame.ERROR:
			st += 'e'
		else:
			st += ' '
		if cmd & Frame.TIME_OUT:
			st += 't'
		else:
			st += ' '
		if cmd & Frame.SERIAL:
			st += 's'
		else:
			st += ' '
		if cmd & Frame.ETH:
			st += 'n'
		else:
			st += ' '

		st += '%d' % (self.stat & Frame.LEN_MASK)

		return st


	def _cmde_decode(self):
		try:
			max_size = self.max_cmde_size
		except:
			self.max_cmde_size = 0
			for dc in Frame.get_derived_class().values():
				if self.max_cmde_size < len(dc.__name__):
					self.max_cmde_size = len(dc.__name__)
			max_size = self.max_cmde_size

		st = '%s' % self.__class__.__name__
		st = st.ljust(max_size)
		return st


	def _argv_str_decode(self):
		"""
		can be overloaded for specific argument decoding
		"""
		st = '[0x'
		for i in range(len(self.argv)):
			st += '%02x ' % self.argv[i]
		st = st[:-1]
		st += ']'
		return st


	def cmde_name(self):
		return self._cmde_decode()


	def __str__(self):
		# '\033[0m' default
		# '\033[1m' bold
		# '\033[2m' dim
		# '\033[22m' normal brightness
		# foreground
		# '\033[30m' black
		# '\033[31m' red
		# '\033[32m' green
		# '\033[33m' orange
		# '\033[34m' blue
		# '\033[35m' violet
		# '\033[36m' cyan
		# '\033[37m' white
		# background
		# '\033[40m' black
		# '\033[41m' red
		# '\033[42m' green
		# '\033[43m' orange
		# '\033[44m' blue
		# '\033[45m' violet
		# '\033[46m' cyan
		# '\033[47m' white

		st = '\033[35m%s\033[32m {' % self._cmde_decode()
		st += 'dest: %s, ' % self._addr_decode(self.dest)
		st += 'orig: %s, ' % self._addr_decode(self.orig)
		st += 't_id: 0x%02x, ' % self.t_id
		st += 'stat: %s, ' % self._stat_decode()
		st += 'argv: %s' % self._argv_str_decode()
		st += '}'

		return st

	def __repr__(self):
		try:
			st = '%s(' % self._cmde_decode()
			st += '0x%02x, ' % self.dest
			st += '0x%02x, ' % self.orig
			st += '0x%02x, ' % self.t_id
			st += '0x%02x, ' % self.stat
			st += '['
			for i in range(len(self.argv)):
				st += '0x%02x, ' % self.argv[i]
			st += '])'

		except TypeError:
			st = 'invalid frame'

		return st


	def __getitem__(self, key):
		"""
		retrieve a field value by its name or its index
		"""
		if type(key) is str:
			if key == 'dest':
				return self.dest
			if key == 'orig':
				return self.orig
			if key == 't_id':
				return self.t_id
			if key == 'cmde':
				return self.cmde
			if key == 'stat':
				return self.stat
			if key[0:4] == 'argv':
				return self.argv[int(key[4:])]

			raise KeyError

		if type(key) is int:
			if key == 0:
				return self.dest
			if key == 1:
				return self.orig
			if key == 2:
				return self.t_id
			if key == 3:
				return self.cmde
			if key == 4:
				return self.stat
			if key >= 5:
				try:
					# handle two complement
					return (256 + self.argv[key - 5]) & 0xff
				except:
					return 0xff

			raise IndexError

		raise TypeError


	def __setitem__(self, key, value):
		"""
		modify a field value by its name or its index
		"""
		if type(key) is str:
			if key == 'dest':
				self.dest = value
				return
			if key == 'orig':
				self.orig = value
				return
			if key == 't_id':
				self.t_id = value
				return
			if key == 'cmde':
				self.cmde = value
				return
			if key == 'stat':
				self.stat = value
				return
			if key[0:4] == 'argv':
				self.argv[int(key[4:])] = value
				return

			raise KeyError

		if type(key) is int:
			if key == 0:
				self.dest = value
				return
			if key == 1:
				self.orig = value
				return
			if key == 2:
				self.t_id = value
				return
			if key == 3:
				self.cmde = value
				return
			if key == 4:
				self.stat = value
				return
			if key >= 5:
				self.argv[key - 5] = value
				return

			raise IndexError

		raise TypeError


class i2c_read(Frame):
	"""
	raw I2C read
	status.len : number of octets to be read
	argv #0-... : read octets
	"""
	cmde = 0x00
	def __init__(self, dest, orig, t_id, stat, *argv):
		super(self.__class__, self).__init__(dest, orig, t_id, self.__class__.cmde, stat, *argv)


class i2c_write(Frame):
	"""
	raw I2C write
	status.len : number of octets to be written
	argv #0-... : octets to be written
	"""
	cmde = 0x01
	def __init__(self, dest, orig, t_id, stat, *argv):
		super(self.__class__, self).__init__(dest, orig, t_id, self.__class__.cmde, stat, *argv)


class no_cmde(Frame):
	"""
	no command
	no arg
	"""
	cmde = 0x02
	def __init__(self, dest, orig, t_id, stat, *argv):
		super(self.__class__, self).__init__(dest, orig, t_id, self.__class__.cmde, stat, *argv)


class ram_read(Frame):
	"""
	RAM read
	argv #0, #1 :
		- RAM address to read (MSB first)
	argv #2 :
		- read data at address
	argv #3 :
		- read data at address + 1 octet
	"""
	cmde = 0x03
	def __init__(self, dest, orig, t_id, stat, *argv):
		super(self.__class__, self).__init__(dest, orig, t_id, self.__class__.cmde, stat, *argv)


class ram_write(Frame):
	"""
	RAM write
	argv #0, #1 :
		- RAM address to write (MSB first)
	argv #2, #3 :
		- in cmde, data to be written
		- in resp, data read back
	"""
	cmde = 0x04
	def __init__(self, dest, orig, t_id, stat, *argv):
		super(self.__class__, self).__init__(dest, orig, t_id, self.__class__.cmde, stat, *argv)


class eep_read(Frame):
	"""
	EEPROM read
	argv #0, #1 :
		- RAM address to read (MSB first)
	argv #2 :
		- read data at address
	argv #3 :
		- read data at address + 1 octet
	"""
	cmde = 0x05
	def __init__(self, dest, orig, t_id, stat, *argv):
		super(self.__class__, self).__init__(dest, orig, t_id, self.__class__.cmde, stat, *argv)


class eep_write(Frame):
	"""
	EEPROM write
	argv #0, #1 :
		- RAM address to write (MSB first)
	argv #2, #3 :
		- in cmde, data to be written
		- in resp, data read back
	"""
	cmde = 0x06
	def __init__(self, dest, orig, t_id, stat, *argv):
		super(self.__class__, self).__init__(dest, orig, t_id, self.__class__.cmde, stat, *argv)


class flh_read(Frame):
	"""
	FLASH read
	argv #0, #1 :
		- FLASH address to read (MSB first)
	argv #2 :
		- read data at address
	argv #3 :
		- read data at address + 1 octet
	"""
	cmde = 0x07
	def __init__(self, dest, orig, t_id, stat, *argv):
		super(self.__class__, self).__init__(dest, orig, t_id, self.__class__.cmde, stat, *argv)


class flh_write(Frame):
	"""
	FLASH write (possibly implemented)
	argv #0, #1 :
		- RAM address to write (MSB first)
	argv #2, #3 :
		- in cmde, data to be written
		- in resp, data read back
	"""
	cmde = 0x08
	def __init__(self, dest, orig, t_id, stat, *argv):
		super(self.__class__, self).__init__(dest, orig, t_id, self.__class__.cmde, stat, *argv)


class spi_read(Frame):
	"""
	SPI read
	status.len : number of octets to be read
	argv #0-... : used if necessary
	"""
	cmde = 0x09
	def __init__(self, dest, orig, t_id, stat, *argv):
		super(self.__class__, self).__init__(dest, orig, t_id, self.__class__.cmde, stat, *argv)


class spi_write(Frame):
	"""
	SPI write
	status.len : number of octets to be written
	argv #0-... : used if necessary
	"""
	cmde = 0x0a
	def __init__(self, dest, orig, t_id, stat, *argv):
		super(self.__class__, self).__init__(dest, orig, t_id, self.__class__.cmde, stat, *argv)


class wait(Frame):
	"""
	wait some time given in ms
	argv #0,#1 value :
		- time in ms, MSB in #0
	"""
	cmde = 0x0b
	def __init__(self, dest, orig, t_id, stat, *argv):
		super(self.__class__, self).__init__(dest, orig, t_id, self.__class__.cmde, stat, *argv)


class container(Frame):
	"""
	encapsulated several other frames used for event handling
	argv #0-1 value :
		- offset in memory for the first encapsulated frame (MSB first)
	argv #2 value :
		- 0xVV : nb encapsulated frames
	argv #3 memory type or container number in eeprom: 
		- 0x00-0x09 : predefined eeprom container (in this case the other parameters are useless)
		- 0xee eeprom, 
		- 0xff flash, 
		- 0xaa ram, 
	"""
	cmde = 0x0c

	# storage memory type
	PRE_0	= 0x00
	PRE_1	= 0x01
	PRE_2	= 0x02
	PRE_3	= 0x03
	PRE_4	= 0x04
	PRE_5	= 0x05
	PRE_6	= 0x06
	PRE_7	= 0x07
	PRE_8	= 0x08
	PRE_9	= 0x09
	RAM	= 0xaa
	EEPROM	= 0xee
	FLASH	= 0xff

	defines = { 
		'PRE_0_STORAGE':'0x00',
		'PRE_1_STORAGE':'0x01',
		'PRE_2_STORAGE':'0x02',
		'PRE_3_STORAGE':'0x03',
		'PRE_4_STORAGE':'0x04',
		'PRE_5_STORAGE':'0x05',
		'PRE_6_STORAGE':'0x06',
		'PRE_7_STORAGE':'0x07',
		'PRE_8_STORAGE':'0x08',
		'PRE_9_STORAGE':'0x09',
		'RAM_STORAGE':'0xaa',
		'EEPROM_STORAGE':'0xee',
		'FLASH_STORAGE':'0xff',
	}

	def __init__(self, dest, orig, t_id, stat, *argv):
		super(self.__class__, self).__init__(dest, orig, t_id, self.__class__.cmde, stat, *argv)


class dna_register(Frame):
	"""
	DNA register
	argv #0 value :
		- 0xVV : IS desired address
	argv #1 value :
		- 0xVV : IS type
	"""
	cmde = 0x0d
	def __init__(self, dest, orig, t_id, stat, *argv):
		super(self.__class__, self).__init__(dest, orig, t_id, self.__class__.cmde, stat, *argv)


class dna_list(Frame):
	"""
	DNA list
	argv #0 value :
		- 0xVV : IS number
	argv #1 value :
		- 0xVV : BS number
	"""
	cmde = 0x0e
	def __init__(self, dest, orig, t_id, stat, *argv):
		super(self.__class__, self).__init__(dest, orig, t_id, self.__class__.cmde, stat, *argv)


class dna_line(Frame):
	"""
	DNA line
	argv #0 value :
		- 0xVV : line index
	argv #1 value :
		- 0xVV : IS or BS type
	argv #2 value :
		- 0xVV : IS or BS i2c address
	"""
	cmde = 0x0f
	def __init__(self, dest, orig, t_id, stat, *argv):
		super(self.__class__, self).__init__(dest, orig, t_id, self.__class__.cmde, stat, *argv)


class state(Frame):
	"""
	state (retrieve / modify)
	argv #0 value :
		- 0x9e : get state
		- 0x5e : set state
	argv #1 value :
		- 0x00 : init				(default out of reset)
		- 0x01 : cone opening
		- 0x02 : aero opening
		- 0x03 : aero open
		- 0x04 : cone closing
		- 0x05 : cone closed
		- 0x10 : waiting
		- 0x20 : flight
		- 0x30 : cone open
		- 0x40 : braking
		- 0x50 : parachute
	"""
	cmde = 0x10

	defines = { 
		'FR_STATE_GET':'0x9e',
		'FR_STATE_SET':'0x5e',

		'FR_STATE_INIT':'0x00',
		'FR_STATE_PARA_OPENING':'0x01',
		'FR_STATE_PARA_CLOSING':'0x02',
		'FR_STATE_WAITING':'0x04',
		'FR_STATE_FLIGHT':'0x08',
		'FR_STATE_PARACHUTE':'0x10',
	}

	def __init__(self, dest, orig, t_id, stat, *argv):
		super(self.__class__, self).__init__(dest, orig, t_id, self.__class__.cmde, stat, *argv)


class time_get(Frame):
	"""
	retrieve on-board time
	argv #0,#1,#2,#3 value :
		- MSB to LSB on-board time in 10 us
	"""
	cmde = 0x11
	def __init__(self, dest, orig, t_id, stat, *argv):
		super(self.__class__, self).__init__(dest, orig, t_id, self.__class__.cmde, stat, *argv)


class mux_reset(Frame):
	"""
	force / release the reset of the Nominal/redudant bus multiplexer
	argv #0 value :
		- 0x00 : unreset
		- 0xff : reset
	"""
	cmde = 0x12

	defines = { 
		'FR_MUX_RESET_UNRESET':'0x00',
		'FR_MUX_RESET_RESET':'0xff',
	}

	def __init__(self, dest, orig, t_id, stat, *argv):
		super(self.__class__, self).__init__(dest, orig, t_id, self.__class__.cmde, stat, *argv)


class reconf_mode(Frame):
	"""
	force bus mode
	argv #0 value :
		- 0x00 : set force mode
		- 0xff : get force mode
	argv #1 value :
		- 0x00 : force nominal bus active
		- 0x01 : force redundant bus active
		- 0x02 : force no bus active
		- 0x03 : bus mode is automatic
	argv #2 value (in response only) :
		- 0x00 : nominal bus active
		- 0x01 : redundant bus active
		- 0x02 : no bus active
	"""
	cmde = 0x13

	defines = {
		'FR_RECONF_MODE_SET':'0x00',
		'FR_RECONF_MODE_GET':'0xff',
	}

	def __init__(self, dest, orig, t_id, stat, *argv):
		super(self.__class__, self).__init__(dest, orig, t_id, self.__class__.cmde, stat, *argv)


class take_off(Frame):
	"""
	take-off detected
	no arg
	"""
	cmde = 0x14
	def __init__(self, dest, orig, t_id, stat, *argv):
		super(self.__class__, self).__init__(dest, orig, t_id, self.__class__.cmde, stat, *argv)


class take_off_thres(Frame):
	"""
	take-off detection threshold config
	argv #0 value :
		- take-off threshold duration in *100 ms : [0ms; 25.5s]
	argv #1 value :
		- longitudinal acceleration threshold for take-off in 0.1G : [-12.8G; 12.7G]
	"""
	cmde = 0x15
	def __init__(self, dest, orig, t_id, stat, *argv):
		super(self.__class__, self).__init__(dest, orig, t_id, self.__class__.cmde, stat, *argv)


class minut_time_out(Frame):
	"""
	save/read culmination time
	argv #0 value :
		- 0x00 : save
		- 0xff : read
	argv #1 value :
		- 0xVV : open time [0.0; 25.5] seconds
	"""
	cmde = 0x16
	def __init__(self, dest, orig, t_id, stat, *argv):
		super(self.__class__, self).__init__(dest, orig, t_id, self.__class__.cmde, stat, *argv)


class minut_servo_cmd(Frame):
	"""
	open/close servo command
	argv #0 value :
		- 0xc0 : cone
		- 0xae : aero
	argv #1 value :
		- 0x09 : open
		- 0xc1 : close
		- 0x0f : servo turn off
	"""
	cmde = 0x17

	defines = {
		'FR_SERVO_PARA':'0xc0',
		'FR_SERVO_OPEN':'0x09',
		'FR_SERVO_CLOSE':'0xc1',
		'FR_SERVO_OFF':'0x0f',
	}

	def __init__(self, dest, orig, t_id, stat, *argv):
		super(self.__class__, self).__init__(dest, orig, t_id, self.__class__.cmde, stat, *argv)


class minut_servo_info(Frame):
	"""
	save/read servo position
	argv #0 value :
		- 0xc0 : cone
		- 0xae : aero
	argv #1 value :
		- 0x5a : save
		- 0x4e : read
	argv #2 value :
		- 0x09 : open position
		- 0xc1 : close position
	argv #3 value :
		- 0xVV : servo position [-100; 100] degrees
	"""
	cmde = 0x18

	defines = {
		'FR_SERVO_PARA':'0xc0',		# reuse from minut_servo_cmd
		'FR_SERVO_OPEN':'0x09',		# reuse from minut_servo_cmd
		'FR_SERVO_CLOSE':'0xc1',	# reuse from minut_servo_cmd
		'FR_SERVO_OFF':'0x0f',		# reuse from minut_servo_cmd
		'FR_SERVO_SAVE':'0x5a',
		'FR_SERVO_READ':'0x4e',
	}

	def __init__(self, dest, orig, t_id, stat, *argv):
		super(self.__class__, self).__init__(dest, orig, t_id, self.__class__.cmde, stat, *argv)


class switch_power(Frame):
	"""
	switch nominal/redundant power supply ON/off
	argv #0 value :
		- 0x00 : redundant
		- 0xff : nominal
	argv #1 value :
		- 0x00 : off
		- 0xff : ON
	"""
	cmde = 0x19
	def __init__(self, dest, orig, t_id, stat, *argv):
		super(self.__class__, self).__init__(dest, orig, t_id, self.__class__.cmde, stat, *argv)


class read_voltages(Frame):
	"""
	read nominal/redundant power supply voltage #0/#1/#2/#3
	argv #0 value :
		- 0x00 : nominal voltage #0
		- 0x01 : nominal voltage #1
		- 0x02 : nominal voltage #2
		- 0x0e : nominal external voltage
		- 0xf0 : redundant voltage #0
		- 0xf1 : redundant voltage #1
		- 0xf2 : redundant voltage #2
		- 0xfe : redundant external voltage
	argv #1 value :
		- 0xVV : voltage [0.0; 25.5] Volts
	"""
	cmde = 0x1a
	def __init__(self, dest, orig, t_id, stat, *argv):
		super(self.__class__, self).__init__(dest, orig, t_id, self.__class__.cmde, stat, *argv)


class emitter_cmd(Frame):
	"""
	safeguard emitter control
	argv #0 value :
		- 0x00 : switch off
		- 0x0f : switch ON
		- 0x12 : take-off signaling
		- 0x4e : recovery mode
		- 0x7e : get current mode (from the 4 previous states)
		- 0xa5 : set take-off signal duration
		- 0xaa : get take-off signal duration
		- 0xc5 : set recovery ON duration
		- 0xca : get recovery ON duration
		- 0xe5 : set recovery off duration
		- 0xea : get recovery off duration
		- other : ignored
	argv #1 value : for durations only (in 1/10 s)
	"""
	cmde = 0x1b
	def __init__(self, dest, orig, t_id, stat, *argv):
		super(self.__class__, self).__init__(dest, orig, t_id, self.__class__.cmde, stat, *argv)


class log_cmd(Frame):
	"""
	modify logging setup
	argv #0 cmde :
		- 0x00 : off
		- 0x14 : ON to RAM
		- 0x1a : ON to sdcard
		- 0x1e : ON to eeprom
		- 0x27 : set command filter LSB part (bitfield for AND mask)
			- argv #1 - #3 value : filter value (MSB first)
		- 0x28 : set command filter MSB part (bitfield for AND mask)
			- argv #1 - #3 value : filter value (MSB first)
		- 0x2e : get command filter LSB part
			- argv #1 - #3 resp : filter value (MSB first)
		- 0x2f : get command filter MSB part
			- argv #1 - #3 resp : filter value (MSB first)
		- 0x3c : set origin filter (6 values : 0x00 logs from all nodes, 0xVV logs from given node, 0xff doesn't log)
			- argv #1 - #6 value : filter value
		- 0x3f : get origin filter
			- argv #1 - #6 resp : filter value
	"""
	cmde = 0x1c

	defines = { 
		'FR_LOG_CMD_OFF':'0x00',
		'FR_LOG_CMD_RAM':'0x14',
		'FR_LOG_CMD_SDCARD':'0x1a',
		'FR_LOG_CMD_EEPROM':'0x1e',
		'FR_LOG_CMD_SET_LSB':'0x27',
		'FR_LOG_CMD_SET_MSB':'0x28',
		'FR_LOG_CMD_GET_LSB':'0x2e',
		'FR_LOG_CMD_GET_MSB':'0x2f',
		'FR_LOG_CMD_SET_ORIG':'0x3c',
		'FR_LOG_CMD_GET_ORIG':'0x3f',
	}

	def __init__(self, dest, orig, t_id, stat, *argv):
		super(self.__class__, self).__init__(dest, orig, t_id, self.__class__.cmde, stat, *argv)


class rout_list(Frame):
	"""
	number of set routes
	argv #0 response : number of set routes
	"""
	cmde = 0x1d
	def __init__(self, dest, orig, t_id, stat, *argv):
		super(self.__class__, self).__init__(dest, orig, t_id, self.__class__.cmde, stat, *argv)


class rout_line(Frame):
	"""
	retrieve a line content
	argv #0 request : requested line
	argv #1 response : virtual address
	argv #2 response : routed address
	argv #3 response : result OK (1) or ko (0)
	"""
	cmde = 0x1e
	def __init__(self, dest, orig, t_id, stat, *argv):
		super(self.__class__, self).__init__(dest, orig, t_id, self.__class__.cmde, stat, *argv)


class rout_add(Frame):
	"""
	add a new route
	argv #0 request : virtual address
	argv #1 request : routed address
	argv #2 response : result OK (1) or ko (0)
	"""
	cmde = 0x1f
	def __init__(self, dest, orig, t_id, stat, *argv):
		super(self.__class__, self).__init__(dest, orig, t_id, self.__class__.cmde, stat, *argv)


class rout_del(Frame):
	"""
	delete a route
	argv #0 request : virtual address
	argv #1 request : routed address
	argv #2 response : result OK (1) or ko (0)
	"""
	cmde = 0x20
	def __init__(self, dest, orig, t_id, stat, *argv):
		super(self.__class__, self).__init__(dest, orig, t_id, self.__class__.cmde, stat, *argv)


class data_acc(Frame):
	"""
	acceleration data
	argv #0 - #1 : MSB - LSB X acceleration
	argv #2 - #3 : MSB - LSB Y acceleration
	argv #4 - #5 : MSB - LSB Z acceleration
	"""
	cmde = 0x21
	def __init__(self, dest, orig, t_id, stat, *argv):
		super(self.__class__, self).__init__(dest, orig, t_id, self.__class__.cmde, stat, *argv)


class data_gyr(Frame):
	"""
	acceleration data
	argv #0 - #1 : MSB - LSB X gyro
	argv #2 - #3 : MSB - LSB Y gyro
	argv #4 - #5 : MSB - LSB Z gyro
	"""
	cmde = 0x22
	def __init__(self, dest, orig, t_id, stat, *argv):
		super(self.__class__, self).__init__(dest, orig, t_id, self.__class__.cmde, stat, *argv)


class data_pres(Frame):
	"""
	pressure data
	argv #0 - #1 : MSB - LSB x10 pressure with offset
	argv #2 - #3 : MSB - LSB raw pressure
	"""
	cmde = 0x23
	def __init__(self, dest, orig, t_id, stat, *argv):
		super(self.__class__, self).__init__(dest, orig, t_id, self.__class__.cmde, stat, *argv)


class data_io(Frame):
	"""
	IO
	argv #0 : IO bits value
	"""
	cmde = 0x24
	def __init__(self, dest, orig, t_id, stat, *argv):
		super(self.__class__, self).__init__(dest, orig, t_id, self.__class__.cmde, stat, *argv)


class data_adc0(Frame):
	"""
	ADC values
	argv #0 - #1 : MSB - LSB ADC 0
	argv #2 - #3 : MSB - LSB ADC 1
	argv #4 - #5 : MSB - LSB ADC 2
	"""
	cmde = 0x25
	def __init__(self, dest, orig, t_id, stat, *argv):
		super(self.__class__, self).__init__(dest, orig, t_id, self.__class__.cmde, stat, *argv)


class data_adc3(Frame):
	"""
	ADC values
	argv #0 - #1 : MSB - LSB ADC 3
	argv #2 - #3 : MSB - LSB ADC 4
	argv #4 - #5 : MSB - LSB ADC 5
	"""
	cmde = 0x26
	def __init__(self, dest, orig, t_id, stat, *argv):
		super(self.__class__, self).__init__(dest, orig, t_id, self.__class__.cmde, stat, *argv)


class data_adc6(Frame):
	"""
	ADC values
	argv #0 - #1 : MSB - LSB ADC 6
	argv #2 - #3 : MSB - LSB ADC 7
	"""
	cmde = 0x27
	def __init__(self, dest, orig, t_id, stat, *argv):
		super(self.__class__, self).__init__(dest, orig, t_id, self.__class__.cmde, stat, *argv)


class cpu(Frame):
	"""
	CPU usage
	argv #0 - #1 : MSB - LSB last 100 ms
	argv #2 - #3 : MSB - LSB max value
	argv #4 - #5 : MSB - LSB min value
	"""
	cmde = 0x28
	def __init__(self, dest, orig, t_id, stat, *argv):
		super(self.__class__, self).__init__(dest, orig, t_id, self.__class__.cmde, stat, *argv)


class led_cmd(Frame):
	"""
	set/get led blink rate
	argv #0 value :
		- 0xa1 : alive
		- 0x09 : open
	argv #1 value :
		- 0x00 : set
		- 0xff : get
	argv #2 value :
		- 0xVV : low duration [0.00; 2.55] s
	argv #3 value :
		- 0xVV : high duration [0.00; 2.55] s
	"""
	cmde = 0x2a

	defines = { 
		'FR_LED_ALIVE':'0xa1',
		'FR_LED_OPEN':'0x09',
		'FR_LED_SET':'0x00',
		'FR_LED_GET':'0xff',
	}

	def __init__(self, dest, orig, t_id, stat, *argv):
		super(self.__class__, self).__init__(dest, orig, t_id, self.__class__.cmde, stat, *argv)


class appli_start(Frame):
	"""
	application start signal
	and last command in list
	"""
	cmde = 0x3f
	def __init__(self, dest, orig, t_id, stat, *argv):
		super(self.__class__, self).__init__(dest, orig, t_id, self.__class__.cmde, stat, *argv)


def frame(dest=None, orig=None, stat=None, cmde=None, *argv):
	"""
	helper function to create a new frame
	"""
	return Frame.frame(dest, orig, None, stat, cmde, *argv)


# extract frame description from frame object doc
def frame_description(h, doc):
	# extract command name in uppercase
	h.write('\tFR_%s = 0x%02x,\n' % (doc.__name__.upper(), doc.cmde))

	# if a doc is provided, use it
	if doc.__doc__:
		# split the multi-line doc
		lines = doc.__doc__.splitlines()

		# print every line except empty ones
		for l in lines:
			l = l.strip()
			if len(l):
				h.write('\t// %s\n' % l)

	else:
		h.write('\t// description missing, please one !!!!\n')

	h.write('\n')


def frame_defines(h, doc):
	# retrieve defines from class
	if 'defines' in dir(doc):
		h.write('// %s\n' % doc.__name__.upper())

		# print every define with its values
		for d, v in doc.defines.items():
			h.write('# define %s\t%s\n' % (d.strip(), v.strip()))

		h.write('\n')


def generate_h(h):
	h.write('#ifndef __FRAMES_H__\n')
	h.write('# define __FRAMES_H__\n')
	h.write('\n')
	h.write('# include "type_def.h"\n')
	h.write('\n')
	h.write('\n')
	h.write('// --------------------------------------------\n')
	h.write('// public defines\n')
	h.write('//\n')
	h.write('\n')
	h.write('// number of arguments for each frame\n')
	h.write('# define FRAME_NB_ARGS\t%d\n' % Frame.nb_args)
	h.write('\n')
	h.write('// fields offsets\n')
	h.write('# define FRAME_DEST_OFFSET\t0\n')
	h.write('# define FRAME_ORIG_OFFSET\t1\n')
	h.write('# define FRAME_T_ID_OFFSET\t2\n')
	h.write('# define FRAME_CMDE_OFFSET\t3\n')
	h.write('# define FRAME_STAT_OFFSET\t4\n')
	h.write('# define FRAME_ARGV_OFFSET\t5\n')
	h.write('\n')
	# if some commands needs particular defines
	for dc in Frame.get_derived_class().values():
		# extract frame defines
		frame_defines(h, dc)

	h.write('\n')
	h.write('// --------------------------------------------\n')
	h.write('// public types\n')
	h.write('//\n')
	h.write('\n')
	h.write('typedef enum {\n')
	for dc in Frame.get_derived_class().values():
		# extract frame description
		frame_description(h, dc)
	h.write('} fr_cmdes_t;\n')
	h.write('\n')
	h.write('\n')

	h.write('// frame format (header + arguments)\n')
	h.write('typedef struct {\n')
	h.write('\tu8 dest;\t\t\t\t// message destination\n')
	h.write('\tu8 orig;\t\t\t\t// message origin\n')
	h.write('\tu8 t_id;\t\t\t\t// transaction identifier\n')
	h.write('\tfr_cmdes_t cmde;\t\t// message command\n')
	h.write('\tunion {\n')
	h.write('\t\tu8 status;\t\t\t// status field\n')
	h.write('\t\tstruct {\t\t\t// and its sub-parts\n')
	h.write('\t\t\tu8 error:1;\t\t// error flag\n')
	h.write('\t\t\tu8 resp:1;\t\t// response flag\n')
	h.write('\t\t\tu8 time_out:1;\t// time-out flag\n')
	h.write('\t\t\tu8 eth:1;\t\t// eth nat flag\n')
	h.write('\t\t\tu8 serial:1;\t// serial nat flag\n')
	h.write('\t\t\tu8 len:3;\t// length / number of arguments\n')
	h.write('\t\t};\n')
	h.write('\t};\n')
	h.write('\tu8 argv[FRAME_NB_ARGS];\t// msg command argument(s) if any\n')
	h.write('} frame_t;\n')
	h.write('\n')
	h.write('\n')
	for i in range(Frame.nb_args + 1):
		h.write('extern u8 frame_set_%d(frame_t* fr, u8 dest, u8 orig, fr_cmdes_t cmde, u8 len' % i)
		for j in range(i):
			h.write(', u8 argv%d' % j)
		h.write(');\n')
		h.write('\n')
	h.write('\n')
	h.write('#endif\t// __FRAMES_H__\n')


def generate_c(c):
	c.write('#include "fr_cmdes.h"\n')
	c.write('\n')
	c.write('\n')
	for i in range(Frame.nb_args + 1):
		c.write('u8 frame_set_%d(frame_t* fr, u8 dest, u8 orig, fr_cmdes_t cmde, u8 len' % i)
		for j in range(i):
			c.write(', u8 argv%d' % j)
		c.write(')\n')
		c.write('{\n')
		c.write('\tfr->dest = dest;\n')
		c.write('\tfr->orig = orig;\n')
		c.write('\tfr->cmde = cmde;\n')
		c.write('\tfr->status = 0;\n')
		c.write('\tfr->len = len;\n')
		for j in range(i):
			c.write('\tfr->argv[%d] = argv%d;\n' % (j, j))
		c.write('\n')
		c.write('\treturn OK;\n')
		c.write('}\n')
		c.write('\n')
		c.write('\n')


if __name__ == '__main__':
	#f = frame(0x00, 0x01, 0x11, Frame.RESP|Frame.TIME_OUT, 0x00, 7, 8, 9, 10, 11)
	#print(f)

	c_file = open(sys.argv[1], 'w+')
	h_file = open(sys.argv[2], 'w+')

	# C code generation
	generate_c(c_file)
	generate_h(h_file)

	c_file.close()
	h_file.close()
