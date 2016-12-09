SRCS = scalp.c basic.c common.c dispatcher.c routing_tables.c cpu.c
	#dna.c log.c nat.c reconf.c
OBJS = scalp.o basic.o common.o dispatcher.o routing_tables.o cpu.o

CFLAGS = \
		 -g -std=c99 \
		 -Wall -Wextra -Werror \
		 -mmcu=atmega328p \
		 -Os -mcall-prologues -fshort-enums \
		 -I. \
		 -I$(TROLL_PROJECTS)/nanoK

CC = avr-gcc
AR = avr-ar

# for dependency autogeneration
# part #1
DEPDIR = .deps
$(shell mkdir -p $(DEPDIR) >/dev/null)
DEPFLAGS = -MT $@ -MMD -MP -MF $(DEPDIR)/$*.Td

COMPILE.c = $(CC) $(DEPFLAGS) $(CFLAGS) -c
POSTCOMPILE = mv -f $(DEPDIR)/$*.Td $(DEPDIR)/$*.d

%.o : %.c
%.o : %.c $(DEPDIR)/%.d
	$(COMPILE.c) $(OUTPUT_OPTION) $<
	@$(POSTCOMPILE)

$(DEPDIR)/%.d: ;
.PRECIOUS: $(DEPDIR)/%.d
# end part #1


.PHONY: all clean

all: libscalp.a

libscalp.a: $(OBJS)
	$(AR) -rs $@ $?

scalp.o: scalp.c scalp.h

scalp.c scalp.h: scalp.py
	./scalp.py

clean:
	rm -fr *.o scalp.c scalp.h libscalp.a

very_clean: clean
	rm -f *~
	rm -fr $(DEPDIR) __pycache__


# for dependency autogeneration
# part #2
-include $(patsubst %,$(DEPDIR)/%.d,$(basename $(SRCS)))
# end part #2

