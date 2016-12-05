import os
troll_path = os.environ['TROLL_PROJECTS']
scalp_path = troll_path + '/scalp'

scalp = [
    'scalp.c',
    'alive.c',
    'basic.c',
    'common.c',
    'dispatcher.c',
    'dna.c',
    'log.c',
    #'nat.c',
    #'reconf.c',
    'time_sync.c',
    'routing_tables.c',
    'cpu.c',
]


Import('env')

env.Library('scalp', scalp)

# autogen scalp.[ch] files
scalp_c = scalp_path + '/scalp.c'
scalp_h = scalp_path + '/scalp.h'
env.Depends( [scalp_c, scalp_h], scalp_path + '/scalp.py')
env.Command('scalp.c', '', scalp_path + '/scalp.py ')
env.Command('scalp.h', '', scalp_path + '/scalp.py ')
