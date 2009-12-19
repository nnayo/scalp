scalp	= [
	'alive.c',			\
	'basic.c',			\
	'common.c',			\
	'dispatcher.c',		\
	'dna.c',			\
	'log.c',			\
	'nat.c',			\
	'reconf.c',			\
	'time_sync.c',		\
	'routing_tables.c',	\
]



MCU_TARGET      = 'atmega324p'
OPTIMIZE        = '-Os -mcall-prologues -fshort-enums '
includes	= ['..', './nanoK']
CFLAGS		= '-g -Wall ' + OPTIMIZE + '-mmcu=' + MCU_TARGET

env = Environment(
	CC = 'avr-gcc',		\
	AR = 'avr-ar',		\
	CFLAGS = CFLAGS,	\
	CPPPATH = includes,	\
)

env.Library('scalp', scalp)

# suppress reliquat files
env.Alias('clean', '', 'rm -f *~ *o libscalp.a')
env.AlwaysBuild('clean')

# suppress reliquat files
env.Alias('size', 'libscalp.a', 'avr-size -t libscalp.a')
env.AlwaysBuild('size')
