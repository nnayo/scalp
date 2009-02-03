SRC = test_basic.c ../../test/test.c basic.c dispatcher_stub.c fifo.c
OBJ = test_basic.o ../../test/test.o basic.o dispatcher_stub.o fifo.o

INCS		= -I. -I/usr/include -I../../test
#CFLAGS		= -g -Wall -I. -I/usr/include -shared -lc -fPIC
CFLAGS		= -g -Wall $(INCS) -fshort-enums


all: test_basic

test_basic: $(OBJ)
	gcc $(CFLAGS) $(OBJ) -o test_basic

clean:
	rm -f $(OBJ)
	rm -f test_basic

very_clean: clean
	rm -f *.*~
	rm -f *~
	rm -f *.bak

test: test_basic
	./test_basic

test_summary: test_basic
	./test_basic -s


depend:
	makedepend -Y -o.o $(INCS) $(SRC)


# DO NOT DELETE

test_basic.o: /usr/include/string.h /usr/include/features.h
test_basic.o: /usr/include/sys/cdefs.h /usr/include/bits/wordsize.h
test_basic.o: /usr/include/gnu/stubs.h /usr/include/gnu/stubs-32.h
test_basic.o: ../../test/test.h type_def.h ./inttypes.h /usr/include/stdint.h
test_basic.o: /usr/include/bits/wchar.h dispatcher_stub.h common/fr_cmdes.h
test_basic.o: common/basic.h utils/time.h
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
basic.o: common/basic.h common/fr_cmdes.h common/dispatcher.h type_def.h
basic.o: ./inttypes.h /usr/include/stdint.h /usr/include/features.h
basic.o: /usr/include/sys/cdefs.h /usr/include/bits/wordsize.h
basic.o: /usr/include/gnu/stubs.h /usr/include/gnu/stubs-32.h
basic.o: /usr/include/bits/wchar.h ./avr/eeprom.h ./avr/pgmspace.h
basic.o: /usr/include/string.h utils/time.h utils/pt.h utils/lc.h
basic.o: utils/lc-addrlabels.h utils/fifo.h
dispatcher_stub.o: dispatcher_stub.h type_def.h ./inttypes.h
dispatcher_stub.o: /usr/include/stdint.h /usr/include/features.h
dispatcher_stub.o: /usr/include/sys/cdefs.h /usr/include/bits/wordsize.h
dispatcher_stub.o: /usr/include/gnu/stubs.h /usr/include/gnu/stubs-32.h
dispatcher_stub.o: /usr/include/bits/wchar.h common/fr_cmdes.h
dispatcher_stub.o: /usr/include/string.h ../../test/test.h
fifo.o: utils/fifo.h type_def.h ./inttypes.h /usr/include/stdint.h
fifo.o: /usr/include/features.h /usr/include/sys/cdefs.h
fifo.o: /usr/include/bits/wordsize.h /usr/include/gnu/stubs.h
fifo.o: /usr/include/gnu/stubs-32.h /usr/include/bits/wchar.h
fifo.o: /usr/include/string.h
