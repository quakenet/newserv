
.PHONY: all
all: fishjoin.so

fishjoin.so: fishjoin.o
	ld -shared -Bdynamic -o $@ $^
