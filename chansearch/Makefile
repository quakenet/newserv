
.PHONY: all
all: chansearch.so

chansearch.so: chansearch.o
	ld -shared -Bdynamic -o $@ $^
