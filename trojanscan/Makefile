include ../build.mk

CFLAGS+=$(INCMARIADB) $(INCPCRE)
LDFLAGS+=$(LIBMARIADB) $(LIBPCRE)

.PHONY: all
all: trojanscan.so

trojanscan.so: trojanscan.o
