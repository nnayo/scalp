#!/usr/bin/env python3

"""
generate the .c and .h files from frame descriptions
"""

from scalp_frame import Frame

import sys
import importlib
import inspect


class Frames(object):
    descritions = [
        'scalp_basic',
    ]

    def __init__(self):
        """create the frames of the known modules"""
        self._frames = {}
        for desc in self.descritions:
            mod = importlib.import_module(desc)
            self._update(mod)

    def _update(self, mod):
        """extract the frames of the module"""
        # filter classes of interest
        klass = [k for k in mod.__dict__.values() if inspect.isclass(k)]
        klass = [k for k in klass if issubclass(k, Frame) and k != Frame]

        # ensure the classes are always listed in the same order
        klass.sort(key=lambda x: x.__name__)

        # set the command id of the new frames
        cmde_id = len(self._frames)
        for k in klass:
            k._cmde = cmde_id
            cmde_id += 1
            self._frames.update({k.__name__: k})

    def append(self, frame_file):
        """append the frames of the given module"""
        self.descriptions.append(frame_file)
        mod = importlib.import_module(frame_file)
        self._update(mod)

    def frame(self, dest, orig, t_id=None, cmde=None, stat=None, *argv):
        """instanciate a frame given its parameters"""
        cmdes = {f._cmde: f for f in self._frames.values()}

        # create the frame associated to the command value
        if cmde in cmdes:
            return cmdes[cmde](dest, orig, t_id, stat, *argv)

        # no recognized command so create an anonymous frame
        return Frame()

    def _frame_defines(self, doc):
        h = self._h_file

        # retrieve defines from class
        if 'defines' in dir(doc):
            h.write('// %s\n' % doc.__name__.upper())

            # print every define with its values
            for d, v in doc.defines.items():
                h.write('# define %s\t%s\n' % (d.strip(), v.strip()))

            h.write('\n')

    def _frame_description(self, doc):
        """extract frame description from frame object doc"""
        h = self._h_file

        # extract command name in uppercase
        h.write('\tFR_%s = 0x%02x,\n' % (doc.__name__.upper(), doc.cmde))

        # if a doc is provided, use it
        if doc.__doc__:
            # split the multi-line doc
            lines = doc.__doc__.splitlines()

            # print every line except empty ones
            for l in lines:
                l = l.strip()
                if len(l):
                    h.write('\t// %s\n' % l)

        else:
            h.write('\t// description missing, please one !!!!\n')

        h.write('\n')

    def _generate_h(self):
        h = self._h_file

        h.write('#ifndef __FRAMES_H__\n')
        h.write('# define __FRAMES_H__\n')
        h.write('\n')
        h.write('# include "type_def.h"\n')
        h.write('\n')
        h.write('\n')
        h.write('// --------------------------------------------\n')
        h.write('// public defines\n')
        h.write('//\n')
        h.write('\n')
        h.write('// number of arguments for each frame\n')
        h.write('# define FRAME_NB_ARGS\t%d\n' % Frame.nb_args)
        h.write('\n')
        h.write('// fields offsets\n')
        h.write('# define FRAME_DEST_OFFSET\t0\n')
        h.write('# define FRAME_ORIG_OFFSET\t1\n')
        h.write('# define FRAME_T_ID_OFFSET\t2\n')
        h.write('# define FRAME_CMDE_OFFSET\t3\n')
        h.write('# define FRAME_STAT_OFFSET\t4\n')
        h.write('# define FRAME_ARGV_OFFSET\t5\n')
        h.write('\n')
        # if some commands needs particular defines
        for dc in Frame.get_derived_class().values():
            # extract frame defines
            self._frame_defines(dc)

        h.write('\n')
        h.write('// --------------------------------------------\n')
        h.write('// public types\n')
        h.write('//\n')
        h.write('\n')
        h.write('typedef enum {\n')
        for dc in Frame.get_derived_class().values():
            # extract frame description
            self._frame_description(dc)
        h.write('} fr_cmdes_t;\n')
        h.write('\n')
        h.write('\n')

        h.write('// frame format (header + arguments)\n')
        h.write('typedef struct {\n')
        h.write('\tu8 dest;\t\t\t\t// message destination\n')
        h.write('\tu8 orig;\t\t\t\t// message origin\n')
        h.write('\tu8 t_id;\t\t\t\t// transaction identifier\n')
        h.write('\tfr_cmdes_t cmde;\t\t// message command\n')
        h.write('\tunion {\n')
        h.write('\t\tu8 status;\t\t\t// status field\n')
        h.write('\t\tstruct {\t\t\t// and its sub-parts\n')
        h.write('\t\t\tu8 error:1;\t\t// error flag\n')
        h.write('\t\t\tu8 resp:1;\t\t// response flag\n')
        h.write('\t\t\tu8 time_out:1;\t// time-out flag\n')
        h.write('\t\t\tu8 eth:1;\t\t// eth nat flag\n')
        h.write('\t\t\tu8 serial:1;\t// serial nat flag\n')
        h.write('\t\t\tu8 len:3;\t// length / number of arguments\n')
        h.write('\t\t};\n')
        h.write('\t};\n')
        h.write('\tu8 argv[FRAME_NB_ARGS];\t// msg command argument(s) if any\n')
        h.write('} frame_t;\n')
        h.write('\n')
        h.write('\n')
        for i in range(Frame.nb_args + 1):
            h.write(
                'extern u8 frame_set_%d('
                'frame_t* fr, u8 dest, u8 orig, fr_cmdes_t cmde, u8 len' % i)
            for j in range(i):
                h.write(', u8 argv%d' % j)
            h.write(');\n')
            h.write('\n')
        h.write('\n')
        h.write('#endif\t// __FRAMES_H__\n')

    def _generate_c(self):
        c = self._c_file

        c.write('#include "fr_cmdes.h"\n')
        c.write('\n')
        c.write('\n')
        for i in range(Frame.nb_args + 1):
            c.write(
                'u8 frame_set_%d('
                'frame_t* fr, u8 dest, u8 orig, fr_cmdes_t cmde, u8 len' % i)
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

    def generate(self, c_file, h_file):
        """populate the given c and h files"""
        self._c_file = open(c_file, 'w+')
        self._h_file = open(h_file, 'w+')

        self.generate_c()
        self.generate_h()

        self._c_file.close()
        self._h_file.close()


def test(desc):
    """very basic display test"""
    dest = 0x00
    orig = 0x01
    t_id = None
    cmde = 0x01
    status = Frame.RESP | Frame.TIME_OUT
    args = (7, 8, 9, 10, 11)

    print('small test:')
    f = desc.frame(dest, orig, t_id, cmde, status, *args)
    print('\t%s' % f)


def usage():
    """
    generate .c and .h files from the description given in:
     - scalp_basic.py
    and optionnaly from given specific frame definition file(s)
    built on the same model as the mandatory files (use them as an example)

    usage:
        ./scalp_frame.py <C file name> <H file name> [optional_frame.py*]

    test:
        ./scalp_frame.py -t
    """

    print(usage.__doc__)

if __name__ == '__main__':
    frm = Frames()

    if len(sys.argv) == 2 and sys.argv[1] == '-t':
        test(frm)
        sys.exit(-2)

    if len(sys.argv) < 3:
        usage()
        sys.exit(-1)

    for a in argv[3:]:
        frm.append(a)

    # C code generation
    frm.generate(sys.argv[1], sys.argv[2])
