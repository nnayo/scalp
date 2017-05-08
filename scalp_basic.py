"""
set of scalps for the basic component
"""

from scalp_frame import Scalp


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


#class RamRead(Scalp):
#    """
#    RAM read
#    argv #0, #1 :
#        - RAM address to read (MSB first)
#    argv #2 :
#        - read data at address
#    argv #3 :
#        - read data at address + 1 octet
#    """
#
#
#class RamWrite(Scalp):
#    """
#    RAM write
#    argv #0, #1 :
#        - RAM address to write (MSB first)
#    argv #2, #3 :
#        - in cmde, data to be written
#        - in resp, data read back
#    """
#
#
#class EepRead(Scalp):
#    """
#    EEPROM read
#    argv #0, #1 :
#        - RAM address to read (MSB first)
#    argv #2 :
#        - read data at address
#    argv #3 :
#        - read data at address + 1 octet
#    """
#
#
#class EepWrite(Scalp):
#    """
#    EEPROM write
#    argv #0, #1 :
#        - RAM address to write (MSB first)
#    argv #2, #3 :
#        - in cmde, data to be written
#        - in resp, data read back
#    """
#
#
#class FlhRead(Scalp):
#    """
#    FLASH read
#    argv #0, #1 :
#        - FLASH address to read (MSB first)
#    argv #2 :
#        - read data at address
#    argv #3 :
#        - read data at address + 1 octet
#    """
#
#
#class FlhWrite(Scalp):
#    """
#    FLASH write (possibly implemented)
#    argv #0, #1 :
#        - RAM address to write (MSB first)
#    argv #2, #3 :
#        - in cmde, data to be written
#        - in resp, data read back
#    """
#
#
#class SpiRead(Scalp):
#    """
#    SPI read
#    status.len : number of octets to be read
#    argv #0-... : used if necessary
#    """
#
#
#class SpiWrite(Scalp):
#    """
#    SPI write
#    status.len : number of octets to be written
#    argv #0-... : used if necessary
#    """
#
#
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

    def _argv_str(self):
        return '%d ms)' % self._delay


class Container(Scalp):
    """
    encapsulated several other scalps used for event handling
    argv #0-1 value :
        - offset in memory for the first encapsulated scalp (MSB first)
    argv #2 value :
        - 0xVV : nb encapsulated scalps or predefined slot
    argv #3 memory type or container number in eeprom:
        - 0xee : eeprom,
        - 0xff : flash,
        - 0xaa : ram,
        - 0x4e : predefined eeprom container (in this case the offset is useless)
    """

    # storage memory type
    RAM = 0xaa
    EEPROM = 0xee
    FLASH = 0xff
    PRE_DEF = 0x4e

    defines = {
        'RAM_STORAGE': '0xaa',
        'EEPROM_STORAGE': '0xee',
        'FLASH_STORAGE': '0xff',
        'PRE_DEF_STORAGE': '0x4e',
    }

    def _argv_str(self):
        addr = 'addr=0x%02x%02x' % (self.argv[0], self.argv[1])
        nb_scalps = 'nb=%d' % self.argv[2]
        container_type = 'unknown type'
        for cont_type, cont_val in self.defines.items():
            cont_val = int(cont_val, 16)
            if self.argv[3] == cont_val:
                container_type = cont_type.lower()
                break

        if 'pre_def' in container_type:
            return container_type + ' #%d' % nb_scalps
        else:
            return '%s, %s, %s' % (container_type, addr, nb_scalps)
