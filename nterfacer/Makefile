include ../build.mk

CFLAGS+=$(INCPCRE)
LDFLAGS+=$(LIBPCRE)

.PHONY: all
all: nterfacer.so nterfacer_control.so nterfacer_relay.so nterfacer_chanstats.so nterfacer_country.so

nterfacer.so: nterfacer.o logging.o esockets.o library.o

nterfacer_control.so: nterfacer_control.o

nterfacer_relay.so: nterfacer_relay.o

nterfacer_chanstats.so: nterfacer_chanstats.o

nterfacer_country.so: nterfacer_country.o
