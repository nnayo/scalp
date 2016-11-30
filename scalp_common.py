"""
set of scalps for the common component
"""

from scalp_frame import Scalp


class State(Scalp):
    """
    state (retrieve / modify)
    argv #0 value :
        - 0x9e : get state
        - 0x5e : set state
    argv #1 value :
        - 0x00 : init                (default out of reset)
        - 0x01 : all other values are project dependant
    """

    defines = {
        'GET': '0x9e',
        'SET': '0x5e',

        'INIT': '0x00',
        'PARA_OPENING': '0x01',
        'PARA_CLOSING': '0x02',
        'WAITING': '0x04',
        'FLIGHT': '0x08',
        'PARACHUTE': '0x10',
    }


class Time(Scalp):
    """
    retrieve on-board time
    argv #0,#1,#2,#3 value :
        - MSB to LSB on-board time in 10 us
    """


class Mux(Scalp):
    """
    force / release the reset of the Nominal/redudant bus multiplexer
    argv #0 value :
        - 0x00 : unreset
        - 0xff : reset
    """

    defines = {
        'UNRESET': '0x00',
        'RESET': '0xff',
    }


class Led(Scalp):
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

    defines = {
        'ALIVE': '0xa1',
        'SIGNAL': '0x51',
        'SET': '0x00',
        'GET': '0xff',
    }


