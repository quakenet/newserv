CFLAGS+=$(INCPGSQL)

.PHONY: all
all: pqsql.so

pqsql.so: pqsql.o
	ld -shared -Bdynamic ${LIBPGSQL} -o $@ $^

