.PHONY: all
all: trojanscan.so

trojanscan.so: trojanscan.o
	ld -shared -Bdynamic ${LIBMYSQL} ${LIBPCRE} -o $@ $^
