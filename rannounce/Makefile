
.PHONY: all
all: rannounce.so

rannounce.so: rannounce.o
	ld -shared -Bdynamic -o $@ $^
