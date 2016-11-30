"""
set of scalps for the cpu component
"""

from scalp_frame import Scalp

class cpu(Scalp):
    """
    CPU usage
    argv #0 - #1 : MSB - LSB last 100 ms
    argv #2 - #3 : MSB - LSB max value
    argv #4 - #5 : MSB - LSB min value
    """
