SRC = test_dispatcher.c ../../test/test.c dispatcher.c twi_stub.c
OBJ = test_dispatcher.o ../../test/test.o dispatcher.o twi_stub.o

INCS		= -I. -I/usr/include -I../../test 
#CFLAGS		= -g -Wall -I. -I/usr/include -shared -lc -fPIC
CFLAGS		= -g -Wall $(INCS) -fshort-enums


all: test_dispatcher

test_dispatcher: $(OBJ)
	gcc $(CFLAGS) $(OBJ) -o test_dispatcher

clean:
	rm -f $(OBJ)
	rm -f test_dispatcher

very_clean: clean
	rm -f *.*~
	rm -f *~
	rm -f *.bak

test: test_dispatcher
	./test_dispatcher

test_summary: test_dispatcher
	./test_dispatcher -s


depend:
	makedepend -Y -o.o $(INCS) $(SRC)


# DO NOT DELETE

test_dispatcher.o: /usr/include/string.h /usr/include/features.h
test_dispatcher.o: /usr/include/sys/cdefs.h /usr/include/gnu/stubs.h
test_dispatcher.o: ../../test/test.h twi_stub.h drivers/twi.h nanoK.h
test_dispatcher.o: type_def.h ./inttypes.h /usr/include/stdint.h
test_dispatcher.o: /usr/include/bits/wchar.h /usr/include/bits/wordsize.h
test_dispatcher.o: common/dispatcher.h common/fr_cmdes.h
../../test/test.o: ../../test/test.h type_def.h ./inttypes.h
../../test/test.o: /usr/include/stdint.h /usr/include/features.h
../../test/test.o: /usr/include/sys/cdefs.h /usr/include/gnu/stubs.h
../../test/test.o: /usr/include/bits/wchar.h /usr/include/bits/wordsize.h
../../test/test.o: /usr/include/stdlib.h /usr/include/stdio.h
../../test/test.o: /usr/include/bits/types.h /usr/include/bits/typesizes.h
../../test/test.o: /usr/include/libio.h /usr/include/_G_config.h
../../test/test.o: /usr/include/wchar.h /usr/include/gconv.h
../../test/test.o: /usr/include/bits/stdio_lim.h
../../test/test.o: /usr/include/bits/sys_errlist.h /usr/include/string.h
dispatcher.o: common/dispatcher.h type_def.h ./inttypes.h
dispatcher.o: /usr/include/stdint.h /usr/include/features.h
dispatcher.o: /usr/include/sys/cdefs.h /usr/include/gnu/stubs.h
dispatcher.o: /usr/include/bits/wchar.h /usr/include/bits/wordsize.h
dispatcher.o: common/fr_cmdes.h drivers/twi.h nanoK.h /usr/include/string.h
twi_stub.o: ../../test/test.h nanoK.h drivers/twi.h type_def.h ./inttypes.h
twi_stub.o: /usr/include/stdint.h /usr/include/features.h
twi_stub.o: /usr/include/sys/cdefs.h /usr/include/gnu/stubs.h
twi_stub.o: /usr/include/bits/wchar.h /usr/include/bits/wordsize.h
