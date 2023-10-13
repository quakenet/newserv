include ../build.mk

CFLAGS+=$(INCMAXMINDDB)
LDFLAGS+=$(LIBMAXMINDDB)

.PHONY: all
all: geoip.so

geoip.so: geoip.o
