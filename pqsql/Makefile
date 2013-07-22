include ../build.mk

CFLAGS+=$(INCPGSQL)
LDFLAGS+=$(LIBPGSQL)

.PHONY: all
all: pqsql.so pqsql-dbapi2.so

pqsql.so: pqsql.o

pqsql-dbapi2.so: pqsql-dbapi2.o

pqsql-dbapi2.o: pqsql-dbapi2.c ../dbapi2/dbapi2-adapter.inc
