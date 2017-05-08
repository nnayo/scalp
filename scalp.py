#!/usr/bin/env python3

"""
generate the .c and .h files from scalp descriptions
"""

import sys
import importlib
import inspect


class ScalpsError(Exception):
    """scalp exception"""
    pass


class Scalps(object):
    """scalp collector"""

    descritions = [
        'scalp_basic',
        'scalp_common',
        'scalp_reconf',
        'scalp_cpu',
        'scalp_dna',
        'scalp_log',
        'scalp_route',
        'scalp_mpu',
        'scalp_servo',
        'scalp_minut',
    ]

    def __init__(self, extension_path='.'):
        """create the scalps of the known modules"""
        self._scalps = {}

        sys.path.append(extension_path)
        self.scalp_class = \
                importlib.import_module('scalp_frame').__dict__['Scalp']

        for desc in self.descritions:
            mod = importlib.import_module(desc)
            self._update(mod)

    def _update(self, mod):
        """extract the scalps of the module"""
        # filter classes of interest
        klass = [k for k in mod.__dict__.values() if inspect.isclass(k)]
        klass = [k for k in klass if issubclass(k, self.scalp_class)
                                     and k != self.scalp_class]

        # ensure the classes are always listed in the same order
        klass.sort(key=lambda x: x.__name__)

        # set the command id of the new scalps
        cmde_id = len(self._scalps)
        for k in klass:
            k.cmde = cmde_id
            cmde_id += 1
            self._scalps.update({k.__name__: k})

            if cmde_id == 32:
                raise ScalpsError('too many scalps')

    def append(self, scalp_file):
        """append the scalps of the given module"""
        self.descriptions.append(scalp_file)
        mod = importlib.import_module(scalp_file)
        self._update(mod)

    def scalp(self, dest, orig, t_id=None, cmde=None, stat=None, *argv):
        """instanciate a scalp given its parameters"""
        cmdes = {f.cmde: f for f in self._scalps.values()}

        # create the scalp associated to the command value
        if cmde in cmdes:
            return cmdes[cmde](dest, orig, t_id, stat, *argv)

        # no recognized command so create an anonymous scalp
        return Scalp()

    def __getattr__(self, key):
        """instanciate a class from the registered"""
        if key in self._scalps:
            return self._scalps[key]

        raise AttributeError('__getattr__ : %r not in %r' % (key, self._scalps))

    def _scalp_defines(self, frm):
        h = self._h_file

        # retrieve defines from class
        if 'defines' in dir(frm):
            frm_name = frm.__name__.upper()
            h.write('// %s\n' % frm_name)

            # print every define with it value (sort by value)
            sorted_vals = [k for k in frm.defines.items()]
            sorted_vals.sort(key=lambda x: x[1])
            for k,v  in sorted_vals:
                h.write('# define SCALP_%s_%s        %s\n'
                        % (frm_name[:4], k.strip(), v.strip())
                        )

            h.write('\n')

    def _scalp_description(self, frm):
        """extract scalp description from scalp object doc"""
        h = self._h_file

        # extract command name in uppercase
        h.write('        SCALP_%s = 0x%02x,\n' % (frm.__name__.upper(), frm.cmde))

        # if a doc is provided, use it
        if frm.__doc__:
            # split the multi-line doc
            lines = frm.__doc__.splitlines()

            # print every line except empty ones
            for l in lines:
                l = l.strip()
                if len(l):
                    h.write('        // %s\n' % l)

        else:
            h.write('        // description missing, please add one !!!!\n')

        h.write('\n')

    def _generate_h(self):
        h = self._h_file

        def_name = 'SCALP_H'

        h.write('#ifndef __%s__\n' % def_name)
        h.write('# define __%s__\n' % def_name)
        h.write('\n')
        h.write('# include "type_def.h"\n')
        h.write('\n')
        h.write('\n')
        h.write('// --------------------------------------------\n')
        h.write('// public defines\n')
        h.write('//\n')
        h.write('\n')
        h.write('// number of arguments for each scalp\n')
        h.write('# define SCALP_NB_ARGS        %d\n' % self.scalp_class.nb_args)
        h.write('\n')
        h.write('// fields offsets\n')
        h.write('# define SCALP_DEST_OFFSET        0\n')
        h.write('# define SCALP_ORIG_OFFSET        1\n')
        h.write('# define SCALP_T_ID_OFFSET        2\n')
        h.write('# define SCALP_CMDE_OFFSET        3\n')
        h.write('# define SCALP_STAT_OFFSET        4\n')
        h.write('# define SCALP_ARGV_OFFSET        5\n')
        h.write('\n')
        # if some commands needs particular defines
        for fr in self._scalps.values():
            # extract scalp defines
            self._scalp_defines(fr)

        h.write('\n')
        h.write('// --------------------------------------------\n')
        h.write('// public types\n')
        h.write('//\n')
        h.write('\n')
        h.write('enum scalp_cmde {\n')
        sorted_keys = [f for f in self._scalps.keys()]
        sorted_keys.sort()
        for fr in sorted_keys:
            # extract scalp description
            self._scalp_description(self._scalps[fr])
        h.write('};\n')
        h.write('\n')
        h.write('\n')

        h.write('// scalp format (header + arguments)\n')
        h.write('struct scalp {\n')
        h.write('        u8 dest;                       // message destination\n')
        h.write('        u8 orig;                       // message origin\n')
        h.write('        u8 t_id;                       // transaction identifier\n')
        h.write('        enum scalp_cmde cmde;          // message command\n')
        h.write('        union {\n')
        h.write('                u8 status;             // status field\n')
        h.write('                struct {               // and its sub-parts\n')
        h.write('                        u8 len:3;      // length / number of arguments (LSB)\n')
        h.write('                        u8 reserved:2; // reserved field\n')
        h.write('                        u8 time_out:1; // time-out flag\n')
        h.write('                        u8 resp:1;     // response flag\n')
        h.write('                        u8 error:1;    // error flag (MSB)\n')
        h.write('                };\n')
        h.write('        };\n')
        h.write('        u8 argv[SCALP_NB_ARGS];        // msg command argument(s) if any\n')
        h.write('};\n')
        h.write('\n')
        h.write('\n')
        for i in range(self.scalp_class.nb_args + 1):
            h.write(
                'extern u8 scalp_set_%d('
                'struct scalp* fr, u8 dest, u8 orig, enum scalp_cmde cmde' % i)
            for j in range(i):
                h.write(', u8 argv%d' % j)
            h.write(');\n')
            h.write('\n')
        h.write('\n')
        h.write('#endif        // __%s__\n' % def_name)

    def _generate_c(self):
        c = self._c_file

        c.write('#include "scalp.h"\n')
        c.write('\n')
        c.write('\n')
        for i in range(self.scalp_class.nb_args + 1):
            c.write(
                'u8 scalp_set_%d('
                'struct scalp* fr, u8 dest, u8 orig, enum scalp_cmde cmde' % i)
            for j in range(i):
                c.write(', u8 argv%d' % j)
            c.write(')\n')
            c.write('{\n')
            c.write('        fr->dest = dest;\n')
            c.write('        fr->orig = orig;\n')
            c.write('        fr->cmde = cmde;\n')
            c.write('        fr->status = 0;\n')
            c.write('        fr->len = %d;\n' % i)
            for j in range(i):
                c.write('        fr->argv[%d] = argv%d;\n' % (j, j))
            c.write('\n')
            c.write('        return OK;\n')
            c.write('}\n')
            c.write('\n')
            c.write('\n')

    def generate(self):
        """populate the given c and h files"""
        self._c_file = open('scalp.c', 'w+')
        self._h_file = open('scalp.h', 'w+')

        self._generate_h()
        self._generate_c()

        self._c_file.close()
        self._h_file.close()


def test(desc):
    """very basic display test"""
    import scalp_frame

    dest = 0x00
    orig = 0x01
    t_id = None
    cmde = 0x01
    status = scalp_frame.Scalp.RESP | scalp_frame.Scalp.TIME_OUT
    args = (7, 8, 9, 10, 11)

    print('small test:')
    sclp = desc.scalp(dest, orig, t_id, cmde, status, *args)
    print('        %s' % sclp.to_colored_string())
    print('        %r' % sclp)


def usage():
    """
    generate .c and .h files from the description given in:
     - scalp_basic.py
    and optionnaly from given specific scalp definition file(s)
    built on the same model as the mandatory files (use them as an example)

    usage:
        ./scalp.py [optional_scalp_definition.py*]

    test:
        ./scalp.py -t
    """

    print(usage.__doc__)

if __name__ == '__main__':
    frm = Scalps()

    if len(sys.argv) == 2 and sys.argv[1] == '-t':
        test(frm)
        sys.exit(-2)

    if len(sys.argv) < 1:
        usage()
        sys.exit(-1)

    if len(sys.argv) > 1:
        for a in argv[1:]:
            frm.append(a)

    # C code generation
    frm.generate()
