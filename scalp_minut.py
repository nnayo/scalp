"""
set of scalps for the minut component
"""

from scalp_frame import Scalp

class MinutTakeOff(Scalp):
    """
    take-off detected
    no arg
    """


class MinutTakeOffThres(Scalp):
    """
    take-off detection threshold config
    argv #0 value :
        - take-off threshold duration in *100 ms : [0ms; 25.5s]
    argv #1 value :
        - take-off longitudinal acceleration threshold in 0.1G : \
        [-12.8G; 12.7G]
    """


class MinutTimeOut(Scalp):
    """
    save/read culmination time
    argv #0 value :
        - 0x00 : save
        - 0xff : read
    argv #1 value :
        - 0xVV : window begin time [0.0; 25.5] seconds
    argv #2 value :
        - 0xVV : window end time [0.0; 25.5] seconds
    """
