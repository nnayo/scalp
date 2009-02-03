SRC = test_reconf.c ../../test/test.c reconf.c dispatcher_stub.c
OBJ = test_reconf.o ../../test/test.o reconf.o dispatcher_stub.o

INCS		= -I. -I/usr/include -I../../test
#CFLAGS		= -g -Wall -I. -I/usr/include -shared -lc -fPIC
CFLAGS		= -g -Wall $(INCS) -fshort-enums


all: test_reconf

test_reconf: $(OBJ)
	gcc $(CFLAGS) $(OBJ) -o test_reconf

clean:
	rm -f $(OBJ)
	rm -f test_reconf

very_clean: clean
	rm -f *.*~
	rm -f *~
	rm -f *.bak

test: test_reconf
	./test_reconf

test_summary: test_reconf
	./test_reconf -s


depend:
	makedepend -Y -o.o $(INCS) $(SRC)


# DO NOT DELETE

test_reconf.o: /usr/include/string.h /usr/include/features.h
test_reconf.o: /usr/include/sys/cdefs.h /usr/include/bits/wordsize.h
test_reconf.o: /usr/include/gnu/stubs.h /usr/include/gnu/stubs-32.h
test_reconf.o: ../../test/test.h type_def.h ./inttypes.h
test_reconf.o: /usr/include/stdint.h /usr/include/bits/wchar.h
test_reconf.o: dispatcher_stub.h common/fr_cmdes.h common/reconf.h
test_reconf.o: utils/time.h avr/io.h
../../test/test.o: ../../test/test.h type_def.h ./inttypes.h
../../test/test.o: /usr/include/stdint.h /usr/include/features.h
../../test/test.o: /usr/include/sys/cdefs.h /usr/include/bits/wordsize.h
../../test/test.o: /usr/include/gnu/stubs.h /usr/include/gnu/stubs-32.h
../../test/test.o: /usr/include/bits/wchar.h /usr/include/stdlib.h
../../test/test.o: /usr/include/sys/types.h /usr/include/bits/types.h
../../test/test.o: /usr/include/bits/typesizes.h /usr/include/time.h
../../test/test.o: /usr/include/endian.h /usr/include/bits/endian.h
../../test/test.o: /usr/include/sys/select.h /usr/include/bits/select.h
../../test/test.o: /usr/include/bits/sigset.h /usr/include/bits/time.h
../../test/test.o: /usr/include/sys/sysmacros.h
../../test/test.o: /usr/include/bits/pthreadtypes.h /usr/include/alloca.h
../../test/test.o: /usr/include/stdio.h /usr/include/libio.h
../../test/test.o: /usr/include/_G_config.h /usr/include/wchar.h
../../test/test.o: /usr/include/bits/stdio_lim.h
../../test/test.o: /usr/include/bits/sys_errlist.h /usr/include/string.h
reconf.o: common/dispatcher.h type_def.h ./inttypes.h /usr/include/stdint.h
reconf.o: /usr/include/features.h /usr/include/sys/cdefs.h
reconf.o: /usr/include/bits/wordsize.h /usr/include/gnu/stubs.h
reconf.o: /usr/include/gnu/stubs-32.h /usr/include/bits/wchar.h
reconf.o: common/fr_cmdes.h utils/pt.h utils/lc.h utils/lc-addrlabels.h
reconf.o: avr/io.h ./avr/eeprom.h
dispatcher_stub.o: dispatcher_stub.h type_def.h ./inttypes.h
dispatcher_stub.o: /usr/include/stdint.h /usr/include/features.h
dispatcher_stub.o: /usr/include/sys/cdefs.h /usr/include/bits/wordsize.h
dispatcher_stub.o: /usr/include/gnu/stubs.h /usr/include/gnu/stubs-32.h
dispatcher_stub.o: /usr/include/bits/wchar.h common/fr_cmdes.h
dispatcher_stub.o: /usr/include/string.h ../../test/test.h
