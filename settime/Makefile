
.PHONY: all
all: settime.so

settime.so: settime.o
	ld -shared -Bdynamic -o $@ $^
