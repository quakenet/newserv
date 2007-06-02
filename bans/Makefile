
.PHONY: all
all: bans.so

bans.so: bans.o
	ld -shared -Bdynamic -o $@ $^
