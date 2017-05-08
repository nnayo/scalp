"""
set of scalps for the minut component
"""

from scalp_frame import Scalp

class MinutEvent(Scalp):
    """
    minut events
    argv #0 value :
        - 0x00 : none
        - 0x01 : mpu ready
        - 0x02 : sepa open
        - 0x03 : sepa closed
        - 0x04 : time out
        - 0x05 : take-off
        - 0x06 : balistic up
        - 0x07 : balistic down
        - 0x08 : lateral acc trigger
    """

    defines = {
        'EV_NONE': '0x00',
        'EV_MPU_READY': '0x01',
        'EV_SEPA_OPEN': '0x02',
        'EV_SEPA_CLOSED': '0x03',
        'EV_TIME_OUT': '0x04',
        'EV_TAKE_OFF': '0x05',
        'EV_BALISTIC_UP': '0x06',
        'EV_BALISTIC_DOWN': '0x07',
        'EV_LAT_ACC_TRIG': '0x08',
    }


class MinutTakeOffThres(Scalp):
    """
    take-off detection threshold config
    argv #0 value :
        - take-off longitudinal acceleration threshold in 0.01G : [0.00G; 25.5G]
    argv #1 value :
        - apogee longitudinal acceleration threshold in 0.01G : [0.00G; 25.5G]
    argv #2 value :
        - apogee lateral acceleration threshold in 0.01GG : [0.00GG; 25.5GG]
    """


class MinutTimeOut(Scalp):
    """
    set/get time-out
    argv #0 value :
        - 0x5e : set relative to current time
        - 0xff : set to infinite
    argv #1 value :
        - 0xVV : offset from current time [0.0; 25.5] seconds
    """

    defines = {
        'SET': '0x5e',
        'INF': '0xff',
    }
