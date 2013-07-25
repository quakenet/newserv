INCPATH=../../
NOC99=1
include ../../build.mk

CFLAGS+=-DGEOIPDATADIR=\".\" -w

.PHONY: all
all: libgeoip.a

libgeoip.a: GeoIP.o regionName.o timeZone.o
	$(AR) rcs $@ $^
