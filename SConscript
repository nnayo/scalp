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


Import('env')

env.Library('scalp', scalp)

# autogen fr_cmdes.[ch] file
env.Depends( [os.environ['TROLL_PROJECTS'] + '/scalp/fr_cmdes.c', os.environ['TROLL_PROJECTS'] + '/scalp/fr_cmdes.h'], os.environ['TROLL_PROJECTS'] + '/interface_server/frame.py')
env.Command('fr_cmdes.c', '', os.environ['TROLL_PROJECTS'] + '/interface_server/frame.py ' + os.environ['TROLL_PROJECTS'] + '/scalp/fr_cmdes.c' + ' ' + os.environ['TROLL_PROJECTS'] + '/scalp/fr_cmdes.h')
env.Command('fr_cmdes.h', '', os.environ['TROLL_PROJECTS'] + '/interface_server/frame.py ' + os.environ['TROLL_PROJECTS'] + '/scalp/fr_cmdes.c' + ' ' + os.environ['TROLL_PROJECTS'] + '/scalp/fr_cmdes.h')
