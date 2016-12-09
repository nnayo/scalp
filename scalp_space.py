"""
set of scalps for the basic component
"""

from scalp_frame import Scalp


class MpuAcc(Scalp):
    """
    acceleration data
    argv #0 - #1 : MSB - LSB X acceleration
    argv #2 - #3 : MSB - LSB Y acceleration
    argv #4 - #5 : MSB - LSB Z acceleration
    """

    def __str__(self):
        dest = self._dest_str()
        orig = self._orig_str()
        t_id = self._t_id_str()
        status = self._status_str()
        acc_x = (self.argv[0] << 8) + self.argv[1]

        return 'MpuAcc(%s, %s, %s, %s, 0x%04x, 0x%04x, 0x%04x)' % (
                    dest,
                    orig,
                    t_id,
                    status,
                    acc_x,
                    acc_y,
                    acc_z
                )



class MpuGyr(Scalp):
    """
    rotation data
    argv #0 - #1 : MSB - LSB X gyro
    argv #2 - #3 : MSB - LSB Y gyro
    argv #4 - #5 : MSB - LSB Z gyro
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
