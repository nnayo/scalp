"""
set of scalps for the reconf component
"""

from scalp_frame import Scalp


class Reconf(Scalp):
    """
    force bus mode
    argv #0 value :
        - 0x00 : set force mode
        - 0xff : get force mode
    argv #1 value :
        - 0x00 : force nominal bus active
        - 0x01 : force redundant bus active
        - 0x02 : force no bus active
        - 0x03 : bus mode is automatic
    argv #2 value (in response only) :
        - 0x00 : nominal bus active
        - 0x01 : redundant bus active
        - 0x02 : no bus active
    """

    defines = {
        'MODE_SET': '0x00',
        'MODE_GET': '0xff',
    }
