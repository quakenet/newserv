
.PHONY: all
all: channel.so  

channel.so: channel.o channelalloc.o channelhandlers.o chanuserhash.o channelbans.o
	ld -shared -Bdynamic -o $@ $^
