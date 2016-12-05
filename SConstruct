import os

MCU_TARGET = 'atmega328p'
OPTIMIZE = '-Os -mcall-prologues -fshort-enums '
includes = ['.', os.environ['TROLL_PROJECTS'] + '/nanoK']
CFLAGS = '-g -Wall -Wextra -Werror ' + OPTIMIZE + '-mmcu=' + MCU_TARGET

env = Environment(
    ENV = os.environ,
    CC = 'avr-gcc',
    AR = 'avr-ar',
    CFLAGS = CFLAGS,
    CPPPATH = includes,
)

Export('env')

SConscript(['SConscript', ], exports='env')


# suppress reliquat files
env.Alias('clean', '', 'rm -f *~ *o libscalp.a scalp.c scalp.h')
env.AlwaysBuild('clean')

# display sections size
env.Alias('size', 'libscalp.a', 'avr-size -t libscalp.a')
env.AlwaysBuild('size')
