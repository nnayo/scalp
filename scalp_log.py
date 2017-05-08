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
        - 0xc7 : enable command filter (bitfield)
            - argv #1 - #3 value : filter value (MSB first)
        - 0xc8 : disable command filter (bitfield)
            - argv #1 - #3 value : filter value (MSB first)
        - 0xc9 : get command filter
            - argv #1 - #3 resp : filter value (MSB first)
        - 0x47 : enable response filter (bitfield)
            - argv #1 - #3 value : filter value (MSB first)
        - 0x48 : disable response filter (bitfield)
            - argv #1 - #3 value : filter value (MSB first)
        - 0x49 : get response filter
            - argv #1 - #3 resp : filter value (MSB first)
        - 0xf7 : set 5 origin filters :
                 - 0x00 : log from all nodes,
                 - 0xVV : log from given node
                 - 0xff : don't log
            - argv #1 - #6 value : filter value
        - 0xf9 : get 5 origin filters
            - argv #1 - #6 resp : filter value
    """

    defines = {
        'OFF': '0x00',
        'RAM': '0x14',
        'SDCARD': '0x1a',
        'EEPROM': '0x1e',
        'CMD_ENABLE': '0xc7',
        'CMD_DISABLE': '0xc8',
        'CMD_GET': '0xc9',
        'RSP_ENABLE': '0x47',
        'RSP_DISABLE': '0x48',
        'RSP_GET': '0x49',
        'ORIG_SET': '0xf7',
        'ORIG_GET': '0xf9',
    }

