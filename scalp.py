#!/usr/bin/env python3

"""
generate the .c and .h files from scalp descriptions
"""

from scalp_frame import Scalp

import sys
import importlib
import inspect


class Scalps(object):
    descritions = [
        'scalp_basic',
        'scalp_common',
        'scalp_reconf',
        'scalp_cpu',
        'scalp_dna',
        'scalp_log',
        'scalp_route',
    ]

    def __init__(self):
        """create the scalps of the known modules"""
        self._scalps = {}
        for desc in self.descritions:
            mod = importlib.import_module(desc)
            self._update(mod)

    def _update(self, mod):
        """extract the scalps of the module"""
        # filter classes of interest
        klass = [k for k in mod.__dict__.values() if inspect.isclass(k)]
        klass = [k for k in klass if issubclass(k, Scalp) and k != Scalp]

        # ensure the classes are always listed in the same order
        klass.sort(key=lambda x: x.__name__)

        # set the command id of the new scalps
        cmde_id = len(self._scalps)
        for k in klass:
            k._cmde = cmde_id
            cmde_id += 1
            self._scalps.update({k.__name__: k})

    def append(self, scalp_file):
        """append the scalps of the given module"""
        self.descriptions.append(scalp_file)
        mod = importlib.import_module(scalp_file)
        self._update(mod)

    def scalp(self, dest, orig, t_id=None, cmde=None, stat=None, *argv):
        """instanciate a scalp given its parameters"""
        cmdes = {f._cmde: f for f in self._scalps.values()}

        # create the scalp associated to the command value
        if cmde in cmdes:
            return cmdes[cmde](dest, orig, t_id, stat, *argv)

        # no recognized command so create an anonymous scalp
        return Scalp()

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
                h.write('# define SCALP_%s_%s\t%s\n'
                        % (frm_name[:4], k.strip(), v.strip())
                        )

            h.write('\n')

    def _scalp_description(self, frm):
        """extract scalp description from scalp object doc"""
        h = self._h_file

        # extract command name in uppercase
        h.write('\tSCALP_%s = 0x%02x,\n' % (frm.__name__.upper(), frm._cmde))

        # if a doc is provided, use it
        if frm.__doc__:
            # split the multi-line doc
            lines = frm.__doc__.splitlines()

            # print every line except empty ones
            for l in lines:
                l = l.strip()
                if len(l):
                    h.write('\t// %s\n' % l)

        else:
            h.write('\t// description missing, please add one !!!!\n')

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
        h.write('# define SCALP_NB_ARGS\t%d\n' % Scalp.nb_args)
        h.write('\n')
        h.write('// fields offsets\n')
        h.write('# define SCALP_DEST_OFFSET\t0\n')
        h.write('# define SCALP_ORIG_OFFSET\t1\n')
        h.write('# define SCALP_T_ID_OFFSET\t2\n')
        h.write('# define SCALP_CMDE_OFFSET\t3\n')
        h.write('# define SCALP_STAT_OFFSET\t4\n')
        h.write('# define SCALP_ARGV_OFFSET\t5\n')
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
        h.write('\tu8 dest;\t\t\t\t// message destination\n')
        h.write('\tu8 orig;\t\t\t\t// message origin\n')
        h.write('\tu8 t_id;\t\t\t\t// transaction identifier\n')
        h.write('\tenum scalp_cmde cmde;\t\t// message command\n')
        h.write('\tunion {\n')
        h.write('\t\tu8 status;\t\t\t// status field\n')
        h.write('\t\tstruct {\t\t\t// and its sub-parts\n')
        h.write('\t\t\tu8 error:1;\t\t// error flag\n')
        h.write('\t\t\tu8 resp:1;\t\t// response flag\n')
        h.write('\t\t\tu8 time_out:1;\t// time-out flag\n')
        h.write('\t\t\tu8 len:3;\t// length / number of arguments\n')
        h.write('\t\t};\n')
        h.write('\t};\n')
        h.write('\tu8 argv[SCALP_NB_ARGS];\t// msg command argument(s) if any\n')
        h.write('};\n')
        h.write('\n')
        h.write('\n')
        for i in range(Scalp.nb_args + 1):
            h.write(
                'extern u8 scalp_set_%d('
                'struct scalp* fr, u8 dest, u8 orig, enum scalp_cmde cmde, u8 len' % i)
            for j in range(i):
                h.write(', u8 argv%d' % j)
            h.write(');\n')
            h.write('\n')
        h.write('\n')
        h.write('#endif\t// __%s__\n' % def_name)

    def _generate_c(self):
        c = self._c_file

        c.write('#include "scalp.h"\n')
        c.write('\n')
        c.write('\n')
        for i in range(Scalp.nb_args + 1):
            c.write(
                'u8 scalp_set_%d('
                'struct scalp* fr, u8 dest, u8 orig, enum scalp_cmde cmde, u8 len' % i)
            for j in range(i):
                c.write(', u8 argv%d' % j)
            c.write(')\n')
            c.write('{\n')
            c.write('\tfr->dest = dest;\n')
            c.write('\tfr->orig = orig;\n')
            c.write('\tfr->cmde = cmde;\n')
            c.write('\tfr->status = 0;\n')
            c.write('\tfr->len = len;\n')
            for j in range(i):
                c.write('\tfr->argv[%d] = argv%d;\n' % (j, j))
            c.write('\n')
            c.write('\treturn OK;\n')
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
    dest = 0x00
    orig = 0x01
    t_id = None
    cmde = 0x01
    status = Scalp.RESP | Scalp.TIME_OUT
    args = (7, 8, 9, 10, 11)

    print('small test:')
    f = desc.scalp(dest, orig, t_id, cmde, status, *args)
    print('\t%s' % f)


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
