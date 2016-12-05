"""
set of scalps for the dna component
"""

from scalp_frame import Scalp


class DnaRegister(Scalp):
    """
    dna register
    argv #0 value :
        - 0xVV : IS desired address
    argv #1 value :
        - 0xVV : IS type
    """


class DnaList(Scalp):
    """
    dna list
    argv #0 value :
        - 0xVV : IS number
    argv #1 value :
        - 0xVV : BS number
    """


class DnaLine(Scalp):
    """
    dna line
    argv #0 value :
        - 0xVV : line index
    argv #1 value :
        - 0xVV : IS or BS type
    argv #2 value :
        - 0xVV : IS or BS i2c address
    """
