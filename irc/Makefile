
.PHONY: all
all: irc.so  

irc.so: irc.o
	ld -shared -Bdynamic -o $@ $^
