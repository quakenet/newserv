
.PHONY: all
all: authhash.so

authhash.so: authhash.o
	ld -shared -Bdynamic -o $@ $^
