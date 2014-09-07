include ../build.mk

CFLAGS+=$(INCLUA) $(INCDBAPI)
LDFLAGS+=$(LIBLUA) $(LIBDBAPI)

.PHONY: all

all: a4stats_db.so nterfacer_a4stats.so

a4stats_db.so: a4stats_db.o

nterfacer_a4stats.so: nterfacer_a4stats.o
