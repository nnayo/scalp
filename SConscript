import os
troll_path = os.environ['TROLL_PROJECTS']
scalp_path = troll_path + '/scalp'

scalp	= [
	'fr_cmdes.c',		\
	'alive.c',			\
	'basic.c',			\
	'common.c',			\
	'dispatcher.c',		\
	'dna.c',			\
	'log.c',			\
	'nat.c',			\
	#'reconf.c',		\
	'time_sync.c',		\
	'routing_tables.c',	\
	'cpu.c',			\
]


Import('env')

env.Library('scalp', scalp)

# autogen fr_cmdes.[ch] file
fr_cmdes_c = scalp_path + '/fr_cmdes.c'
fr_cmdes_h = scalp_path + '/fr_cmdes.h'
env.Depends( [fr_cmdes_c, fr_cmdes_h], scalp_path + '/frame.py')
env.Command('fr_cmdes.c', '', scalp_path + '/frame.py ' + fr_cmdes_c + ' ' + fr_cmdes_h)
env.Command('fr_cmdes.h', '', scalp_path + '/frame.py ' + fr_cmdes_c + ' ' + fr_cmdes_h)
