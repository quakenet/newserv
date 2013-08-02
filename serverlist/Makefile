include ../build.mk

CFLAGS+=$(INCPCRE)
LDFLAGS+=$(LIBPCRE)

.PHONY: all
all: serverlist.so

serverlist.so: serverlist.o
