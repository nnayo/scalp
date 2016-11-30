"""
set of scalps for the basic component
"""

from scalp_frame import Scalp


class Null(Scalp):
    """
    no command
    no arg
    """

    def __str__(self):
        dest = self._dest_str()
        orig = self._orig_str()
        t_id = self._t_id_str()
        status = self._status_str()

        return 'Null(%s, %s, %s, %s)' % (
                    dest,
                    orig,
                    t_id,
                    status,
                )


class TwiRead(Scalp):
    """
    raw I2C read
    status.len : number of octets to be read
    argv #0-... : read octets
    """


class TwiWrite(Scalp):
    """
    raw I2C write
    status.len : number of octets to be written
    argv #0-... : octets to be written
    """


class RamRead(Scalp):
    """
    RAM read
    argv #0, #1 :
        - RAM address to read (MSB first)
    argv #2 :
        - read data at address
    argv #3 :
        - read data at address + 1 octet
    """


class RamWrite(Scalp):
    """
    RAM write
    argv #0, #1 :
        - RAM address to write (MSB first)
    argv #2, #3 :
        - in cmde, data to be written
        - in resp, data read back
    """


class EepRead(Scalp):
    """
    EEPROM read
    argv #0, #1 :
        - RAM address to read (MSB first)
    argv #2 :
        - read data at address
    argv #3 :
        - read data at address + 1 octet
    """


class EepWrite(Scalp):
    """
    EEPROM write
    argv #0, #1 :
        - RAM address to write (MSB first)
    argv #2, #3 :
        - in cmde, data to be written
        - in resp, data read back
    """


class FlhRead(Scalp):
    """
    FLASH read
    argv #0, #1 :
        - FLASH address to read (MSB first)
    argv #2 :
        - read data at address
    argv #3 :
        - read data at address + 1 octet
    """


class FlhWrite(Scalp):
    """
    FLASH write (possibly implemented)
    argv #0, #1 :
        - RAM address to write (MSB first)
    argv #2, #3 :
        - in cmde, data to be written
        - in resp, data read back
    """


class SpiRead(Scalp):
    """
    SPI read
    status.len : number of octets to be read
    argv #0-... : used if necessary
    """


class SpiWrite(Scalp):
    """
    SPI write
    status.len : number of octets to be written
    argv #0-... : used if necessary
    """


class Wait(Scalp):
    """
    wait some time given in ms
    argv #0,#1 value :
        - time in ms, MSB in #0
    """
    def __init__(self, dest, orig, t_id, status, *argv):
        super().__init__(dest, orig, t_id, status, argv)

    def _argv_decode(self):
        self._delay = (self.argv[0] << 8) + self.argv[1]

    def _argv_encode(self):
        argv = [
            (self._delay & 0xff00) >> 8,
            (self._delay & 0x00ff) >> 0,
        ]

        return argv

    def __str__(self):
        dest = self._dest_str()
        orig = self._orig_str()
        t_id = self._t_id_str()
        status = self._status_str()

        return 'Wait(%s, %s, %s, %s, %d ms)' % (
                    dest,
                    orig,
                    t_id,
                    status,
                    self._delay
                )

class Container(Scalp):
    """
    encapsulated several other frames used for event handling
    argv #0-1 value :
        - offset in memory for the first encapsulated frame (MSB first)
    argv #2 value :
        - 0xVV : nb encapsulated frames
    argv #3 memory type or container number in eeprom:
        - 0x00-0x09 : predefined eeprom container \
        (in this case the other parameters are useless)
        - 0xee eeprom,
        - 0xff flash,
        - 0xaa ram,
    """

    # storage memory type
    PRE_0 = 0x00
    PRE_1 = 0x01
    PRE_2 = 0x02
    PRE_3 = 0x03
    PRE_4 = 0x04
    PRE_5 = 0x05
    PRE_6 = 0x06
    PRE_7 = 0x07
    PRE_8 = 0x08
    PRE_9 = 0x09
    RAM = 0xaa
    EEPROM = 0xee
    FLASH = 0xff

    defines = {
        'PRE_0_STORAGE': '0x00',
        'PRE_1_STORAGE': '0x01',
        'PRE_2_STORAGE': '0x02',
        'PRE_3_STORAGE': '0x03',
        'PRE_4_STORAGE': '0x04',
        'PRE_5_STORAGE': '0x05',
        'PRE_6_STORAGE': '0x06',
        'PRE_7_STORAGE': '0x07',
        'PRE_8_STORAGE': '0x08',
        'PRE_9_STORAGE': '0x09',
        'RAM_STORAGE': '0xaa',
        'EEPROM_STORAGE': '0xee',
        'FLASH_STORAGE': '0xff',
    }


