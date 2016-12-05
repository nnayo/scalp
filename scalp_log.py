"""
set of scalps for the log component
"""

from scalp_frame import Scalp


class Log(Scalp):
    """
    modify logging setup
    argv #0 cmde :
        - 0x00 : off
        - 0x14 : ON to RAM
        - 0x1a : ON to sdcard
        - 0x1e : ON to eeprom
        - 0x27 : set command filter LSB part (bitfield for AND mask)
            - argv #1 - #3 value : filter value (MSB first)
        - 0x28 : set command filter MSB part (bitfield for AND mask)
            - argv #1 - #3 value : filter value (MSB first)
        - 0x2e : get command filter LSB part
            - argv #1 - #3 resp : filter value (MSB first)
        - 0x2f : get command filter MSB part
            - argv #1 - #3 resp : filter value (MSB first)
        - 0x3c : set origin filter \
        (6 values : 0x00 logs from all nodes, \
        0xVV logs from given node, 0xff doesn't log)
            - argv #1 - #6 value : filter value
        - 0x3f : get origin filter
            - argv #1 - #6 resp : filter value
    """

    defines = {
        'OFF': '0x00',
        'RAM': '0x14',
        'SDCARD': '0x1a',
        'EEPROM': '0x1e',
        'SET_LSB': '0x27',
        'SET_MSB': '0x28',
        'GET_LSB': '0x2e',
        'GET_MSB': '0x2f',
        'SET_ORIG': '0x3c',
        'GET_ORIG': '0x3f',
    }

