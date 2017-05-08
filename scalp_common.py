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
        'LOCK0': '0x01',
        'LOCK1': '0x02',
        'LOCK2': '0x03',
        'WAITING': '0x04',
        'THRUSTING': '0x05',
        'BALISTIC': '0x06',
        'DETECTION': '0x07',
        'OPEN_SEQ': '0x08',
        'BRAKE': '0x09',
        'UNLOCK': '0x0a',
        'PARACHUTE': '0x0b',
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
        - 0xa1 : alive (green)
        - 0x51 : signal (blue)
        - 0xe4 : error (red)
    argv #1 value :
        - 0x00 : set
        - 0xff : get
    argv #2 value :
        - 0xVV : low duration [0.0; 12.7] s
    argv #3 value :
        - 0xVV : high duration [0.0; 12.7] s
    """

    defines = {
        'ALIVE': '0xa1',
        'SIGNAL': '0x51',
        'ERROR': '0xe4',
        'SET': '0x00',
        'GET': '0xff',
    }


