
.PHONY: all
all: nickrate.so

nickrate.so: nickrate.o
	ld -shared -Bdynamic -o $@ $^
