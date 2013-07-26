CFLAGS+=$(INCDBAPI) $(INCPCRE)
LDFLAGS+=$(LIBDBAPI) $(LIBPCRE)

include ../build.mk

.PHONY: all
all: fakeq.so

fakeq.so: fakeq.o
