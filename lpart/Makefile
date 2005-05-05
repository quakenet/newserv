
.PHONY: all
all: lpart.so

lpart.so: lpart.o
	ld -shared -Bdynamic -o $@ $^
