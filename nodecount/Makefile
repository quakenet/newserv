CFLAGS=-g -Wall

.PHONY: all
all: nodecount.so

nodecount.so: nodecount.o
	ld -shared -Bdynamic -o $@ $^
