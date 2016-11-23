"""
each frame are composed of :
    - destination field     offset + 0
    - origin field         offset + 1
    - transaction id     offset + 2
    - state         offset + 3
    - command field     offset + 4
    - argument #0 field     offset + 5
    - argument #1 field     offset + 6
    - argument #2 field     offset + 7
    - argument #3 field     offset + 8
    - argument #4 field     offset + 9
    - argument #5 field     offset + 10
"""

class FrameError(Exception):
    """basic exception for frame class"""
    pass


# Frame base class
#
class Frame(object):
    """base for all frame classes"""
    # I2C addresses
    I2C_BROADCAST_ADDR = 0x00
    I2C_SELF_ADDR = 0x01

    # fake transaction ID
    T_ID = None

    # status bits
    ERROR = 0x80
    RESP = 0x40
    TIME_OUT = 0x20
    ETH = 0x10
    SERIAL = 0x08
    CMD = 0x00
    CMD_MASK = 0xf8
    LEN_MASK = 0x07

    nb_args = 6  # config parameter
    t_id = 0     # auto-incremented for each instanciation
    _cmde = None  # to be set to set the frame id

    def __init__(self,
                 dest=None, orig=None, t_id=None, status=None,
                 *argv):
        """
        create an empty frame with completely unsignfull content by default
        """
        self.dest = dest
        self.orig = orig
        if t_id is None:
            self.t_id = Frame.t_id
            Frame.t_id += 1
        else:
            self.t_id = t_id
        self.stat = status
        if len(argv) > Frame.nb_args:
            raise FrameError('too many args: %d > %d' % (len(argv), Frame.nb_args))
        self.argv = argv[0]

        self._argv_decode()

    def __len__(self):
        """
        return the frame length in bytes
        """
        return 5 + Frame.nb_args

    def response_set(self):
        self.stat |= Frame.RESP

    def response_unset(self):
        self.stat &= ~Frame.RESP

    def error_set(self):
        self.stat |= Frame.ERROR

    def error_unset(self):
        self.stat &= ~Frame.ERROR

    def time_out_set(self):
        self.stat |= Frame.TIME_OUT

    def time_out_unset(self):
        self.stat &= ~Frame.TIME_OUT

    def from_bytes(self, list):
        """
        help decoding a byte sequence
        """
        self.dest = list[0]
        self.orig = list[1]
        self.t_id = list[2]
        self.cmde = list[3]
        self.stat = list[4]
        self.argv = list[5:]

    def to_bytes(self):
        """
        return the frame encoded as a tuple
        """
        ret = (self.dest, self.orig, self.t_id, self.cmde, self.stat)
        for i in range(len(self.argv)):
            ret += (self.argv[i], )

    def from_string(self, s):
        """
        help decoding a string
        """
        self.dest = ord(s[0])
        self.orig = ord(s[1])
        self.t_id = ord(s[2])
        self.cmde = ord(s[3])
        self.stat = ord(s[4])
        self.argv = []
        for i in range(Frame.nb_args):
            self.argv.append(ord(s[5 + i]))

    def to_string(self):
        """
        return the frame encoded as a string
        """
        st = ''
        try:
            for c in self.to_bytes():
                st += chr(c)
        except:
            raise FrameError('%r' % self)

        return st

    def to_dict(self):
        """
        return the frame encoded as a dict
        """
        d = {}
        d['dest'] = self.dest
        d['orig'] = self.orig
        d['t_id'] = self.t_id
        d['cmde'] = self.cmde
        d['stat'] = self.stat
        for i in range(len(self.argv)):
            d['argv%d' % i] = self.argv[i]

        return d

    @staticmethod
    def _addr_str(addr):
        if addr == Frame.I2C_BROADCAST_ADDR:
            return 'bcst'
        if addr == Frame.I2C_SELF_ADDR:
            return 'self'
        if addr is None:
            return 'None'

        return '0x%02x' % addr

    def _dest_str(self):
        return self._addr_str(self.dest)

    def _orig_str(self):
        return self._addr_str(self.orig)

    def _t_id_str(self):
        return '0x%02x' % self.t_id

    def _status_str(self):
        st = ''

        cmd = self.stat & Frame.CMD_MASK
        if cmd & Frame.RESP:
            st += 'r'
        else:
            st += 'c'
        if cmd & Frame.ERROR:
            st += 'e'
        else:
            st += '_'
        if cmd & Frame.TIME_OUT:
            st += 't'
        else:
            st += '_'
        if cmd & Frame.SERIAL:
            st += 's'
        else:
            st += '_'
        if cmd & Frame.ETH:
            st += 'n'
        else:
            st += '_'

        st += '%d' % (self.stat & Frame.LEN_MASK)

        return st

    def _argv_str_decode(self):
        """
        can be overloaded for specific argument decoding
        """
        st = '[0x'
        for i in range(len(self.argv)):
            st += '%02x ' % self.argv[i]
        st = st[:-1]
        st += ']'
        return st

    def _argv_decode(self):
        pass

    def _argv_encode(self):
        return None

#    def __str__(self):
#        # '\033[0m' default
#        # '\033[1m' bold
#        # '\033[2m' dim
#        # '\033[22m' normal brightness
#        # foreground
#        # '\033[30m' black
#        # '\033[31m' red
#        # '\033[32m' green
#        # '\033[33m' orange
#        # '\033[34m' blue
#        # '\033[35m' violet
#        # '\033[36m' cyan
#        # '\033[37m' white
#        # background
#        # '\033[40m' black
#        # '\033[41m' red
#        # '\033[42m' green
#        # '\033[43m' orange
#        # '\033[44m' blue
#        # '\033[45m' violet
#        # '\033[46m' cyan
#        # '\033[47m' white
#
#        st = '\033[35m%s\033[32m {' % self._cmde_decode()
#        st += 'dest: %s, ' % self._dest_str()
#        st += 'orig: %s, ' % self.__orig_str()
#        st += 't_id: 0x%02x, ' % self._t_id_str()
#        st += 'stat: %s, ' % self._status_str()
#        st += 'argv: %s' % self._argv_str()
#        st += '}'
#
#        return st
#
#    def __repr__(self):
#        try:
#            st = '%s(' % self._cmde_decode()
#            st += '0x%02x, ' % self.dest
#            st += '0x%02x, ' % self.orig
#            st += '0x%02x, ' % self.t_id
#            st += '0x%02x, ' % self.stat
#            st += '['
#            for i in range(len(self.argv)):
#                st += '0x%02x, ' % self.argv[i]
#            st += '])'
#
#        except TypeError:
#            st = 'invalid frame'
#
#        return st
#
    def __getitem__(self, key):
        """
        retrieve a field value by its name or its index
        """
        if type(key) is str:
            if key == 'dest':
                return self.dest
            if key == 'orig':
                return self.orig
            if key == 't_id':
                return self.t_id
            if key == 'cmde':
                return self.cmde
            if key == 'stat':
                return self.stat
            if key[0:4] == 'argv':
                return self.argv[int(key[4:])]

            raise KeyError

        if type(key) is int:
            if key == 0:
                return self.dest
            if key == 1:
                return self.orig
            if key == 2:
                return self.t_id
            if key == 3:
                return self.cmde
            if key == 4:
                return self.stat
            if key >= 5:
                try:
                    # handle two complement
                    return (256 + self.argv[key - 5]) & 0xff
                except:
                    return 0xff

            raise IndexError

        raise TypeError

    def __setitem__(self, key, value):
        """
        modify a field value by its name or its index
        """
        if type(key) is str:
            if key == 'dest':
                self.dest = value
                return
            if key == 'orig':
                self.orig = value
                return
            if key == 't_id':
                self.t_id = value
                return
            if key == 'cmde':
                self.cmde = value
                return
            if key == 'stat':
                self.stat = value
                return
            if key[0:4] == 'argv':
                self.argv[int(key[4:])] = value
                return

            raise KeyError

        if type(key) is int:
            if key == 0:
                self.dest = value
                return
            if key == 1:
                self.orig = value
                return
            if key == 2:
                self.t_id = value
                return
            if key == 3:
                self.cmde = value
                return
            if key == 4:
                self.stat = value
                return
            if key >= 5:
                self.argv[key - 5] = value
                return

            raise IndexError

        raise TypeError
