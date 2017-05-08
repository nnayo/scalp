"""
set of scalps for the servo component
"""

from scalp_frame import Scalp


class ServoCmd(Scalp):
    """
    open/close servo command
    argv #0 value :
        - 0xc0 : cone
        - 0xae : aero
    argv #1 value :
        - 0x09 : open
        - 0xc1 : close
        - 0x0f : servo turn off
    """

    defines = {
        'CONE': '0xc0',
        'AERO': '0xae',
        'OPEN': '0x09',
        'CLOSE': '0xc1',
        'OFF': '0x0f',
    }

    def _argv_str(self):
        argv0 = {0xc0: 'cone', 0xae: 'aero'}
        if self.argv[0] in argv0:
            argv0_str = argv0[self.argv[0]]
        else:
            argv0_str = '????'
        argv1 = {0x09: 'open', 0xc1: 'close', 0x0f: 'off'}
        if self.argv[1] in argv1:
            argv1_str = argv1[self.argv[1]]
        else:
            argv1_str = '?????'

        return '%s, %s' % (argv0_str, argv1_str)


class ServoInfo(Scalp):
    """
    save/read servo position
    argv #0 value :
        - 0xc0 : cone
        - 0xae : aero
    argv #1 value :
        - 0x5a : save
        - 0x4e : read
    argv #2 value :
        - 0x09 : open position
        - 0xc1 : close position
    argv #3 value :
        - 0xVV : servo position [-100; 100] degrees
    """

    defines = {
        # 'CONE': '0xc0',   # reuse from ServoCmd
        # 'AERO': '0xae',   # reuse from ServoCmd
        # 'OPEN': '0x09',   # reuse from ServoCmd
        # 'CLOSE': '0xc1',  # reuse from ServoCmd
        # 'OFF': '0x0f',    # reuse from ServoCmd
        'SAVE': '0x5a',
        'READ': '0x4e',
    }

    def _argv_str(self):
        argv0 = {0xc0: 'cone', 0xae: 'aero'}
        if self.argv[0] in argv0:
            argv0_str = argv0[self.argv[0]]
        else:
            argv0_str = '????'

        argv1 = {0x5a: 'save ', 0x4e: 'read'}
        if self.argv[1] in argv1:
            argv1_str = argv1[self.argv[1]]
        else:
            argv1_str = '????'

        argv2 = {0x09: 'open ', 0xc1: 'close'}
        if self.argv[2] in argv2:
            argv2_str = argv2[self.argv[2]]
        else:
            argv2_str = '?????'

        return '%s, %s, %s, %+d' % (
                    argv0_str,
                    argv1_str,
                    argv2_str,
                    self.argv[3] - 0x80,
                )

