CC=gcc
CFLAGS=-std=c99 -pedantic -Wno-padded -Os
LDFLAGS=-Os
AR=ar
ARFLAGS=rcs

OBJS = kk_ihex_write.o kk_ihex_read.o log2hex.o
BINPATH = ./
LIBPATH = ./
BINS = $(BINPATH)log2hex
LIB = $(LIBPATH)libkk_ihex.a
TESTFILE = $(LIB)
TESTER = 
#TESTER = valgrind

.PHONY: all clean distclean test

all: $(BINS) $(LIB) install

$(OBJS): kk_ihex.h
$(BINS): | $(BINPATH)
$(LIB): | $(LIBPATH)

log2hex.o kk_ihex_write.o: kk_ihex_write.h

$(LIB): kk_ihex_write.o kk_ihex_read.o
	$(AR) $(ARFLAGS) $@ $+

$(BINPATH)log2hex: log2hex.o $(LIB)
	$(CC) $(LDFLAGS) -o $@ $+

$(sort $(BINPATH) $(LIBPATH)):
	@mkdir -p $@

clean:
	rm -f $(OBJS) $(BINS) $(LIB)

distclean: | clean
	rm -f $(BINS) $(LIB)
	@rmdir $(BINPATH) $(LIBPATH) >/dev/null 2>/dev/null || true

install: 
	cp log2hex.exe '../../PD trunk'