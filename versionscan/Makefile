.PHONY: all
all: versionscan.so

versionscan.so: versionscan.o
	ld -shared -Bdynamic ${LIBMYSQL} ${LIBPCRE} -o $@ $^
