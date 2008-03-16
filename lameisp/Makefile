CFLAGS+=$(INCMYSQL)

.PHONY: all
all: lameisp.so

lameisp.so: lameisp.o
	ld -shared -Bdynamic ${LIBMYSQL} -o $@ $^
