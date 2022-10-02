include ../build.mk

CFLAGS+=$(INCGEOIP)
LDFLAGS+=$(LIBGEOIP)

.PHONY: all
all: geoip.so

geoip.so: geoip.o
