CFLAGS+=$(INCMYSQL) $(INCPCRE)

.PHONY: all
all: regexgline.so

regexgline.so: regexgline.o
	ld -shared -Bdynamic ${LIBMYSQL} ${LIBPCRE} -o $@ $^
