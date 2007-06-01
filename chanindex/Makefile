
.PHONY: all
all: chanindex.so

chanindex.so: chanindex.o
	ld -shared -Bdynamic -o $@ $^
