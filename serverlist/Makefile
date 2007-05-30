CFLAGS=-g -Wall

.PHONY: all
all: serverlist.so

serverlist.so: serverlist.o
	ld -shared -Bdynamic -o $@ $^
