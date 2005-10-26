.PHONY: all
all: chanfix.so

chanfix.so: chanfix.o
	ld -shared -Bdynamic -o $@ $^
