import os

scalp	= [
	'fr_cmdes.c',			\
	'alive.c',			\
	'basic.c',			\
	'common.c',			\
	'dispatcher.c',		\
	'dna.c',			\
	'log.c',			\
	'nat.c',			\
	#'reconf.c',			\
	'time_sync.c',		\
	'routing_tables.c',	\
	'cpu.c',			\
]



MCU_TARGET      = 'atmega328p'
OPTIMIZE        = '-Os -mcall-prologues -fshort-enums '
includes	= ['.', os.environ['TROLL_PROJECTS'] + '/nanoK']
CFLAGS		= '-g -Wall ' + OPTIMIZE + '-mmcu=' + MCU_TARGET

env = Environment(
	ENV = os.environ,       \
	CC = 'avr-gcc',		\
	AR = 'avr-ar',		\
	CFLAGS = CFLAGS,	\
	CPPPATH = includes,	\
)

env.Library('scalp', scalp)

# autogen fr_cmdes.[ch] file
env.Depends( [os.environ['TROLL_PROJECTS'] + '/scalp/fr_cmdes.c', os.environ['TROLL_PROJECTS'] + '/scalp/fr_cmdes.h'], os.environ['TROLL_PROJECTS'] + '/interface_server/frame.py')
env.Command('fr_cmdes.c', '', os.environ['TROLL_PROJECTS'] + '/interface_server/frame.py ' + os.environ['TROLL_PROJECTS'] + '/scalp/fr_cmdes.c' + ' ' + os.environ['TROLL_PROJECTS'] + '/scalp/fr_cmdes.h')
env.Command('fr_cmdes.h', '', os.environ['TROLL_PROJECTS'] + '/interface_server/frame.py ' + os.environ['TROLL_PROJECTS'] + '/scalp/fr_cmdes.c' + ' ' + os.environ['TROLL_PROJECTS'] + '/scalp/fr_cmdes.h')

# suppress reliquat files
env.Alias('clean', '', 'rm -f *~ *o libscalp.a')
env.AlwaysBuild('clean')

# display sections size
env.Alias('size', 'libscalp.a', 'avr-size -t libscalp.a')
env.AlwaysBuild('size')
