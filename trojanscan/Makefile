include ../build.mk

CFLAGS+=$(INCMYSQL) $(INCPCRE)
LDFLAGS+=$(LIBMYSQL) $(LIBPCRE)

.PHONY: all
all: trojanscan.so

trojanscan.so: trojanscan.o
