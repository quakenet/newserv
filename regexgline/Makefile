include ../build.mk

CFLAGS+=$(INCDBAPI) $(INCPCRE)
LDFLAGS+=$(LIBDBAPI) $(LIBPCRE)

.PHONY: all
all: regexgline.so

regexgline.so: regexgline.o
