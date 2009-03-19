CFLAGS=-std=c99
CFLAGS+=$(shell dar-rts-config --cflags)
LDFLAGS=-nostdlib -static
LIBS=$(shell dar-rts-config --libs)
CC=mips-linux-gnu-gcc
LD=mips-linux-gnu-ld
DARIMG=darimg
DARSIM=darsim

PROG=chatter.img

.PHONY: all
all: $(PROG)

%.img: %.cfg %.mips
	$(DARIMG) -o $@ $<

%.o_mips: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<
%.s_mips: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -S -o $@ $<
%.mips: %.o_mips
	$(LD) $(LDFLAGS) -s -o $@ $^ $(LIBS)

.PHONY: run
run: $(PROG)
	$(DARSIM) $(PROG) --cycles=2000

.PHONY: clean
clean:
	$(RM) *.s_mips *.o_mips *.mips $(PROG)

