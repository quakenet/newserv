
.PHONY: all
all: localuser.so localuserchannel.so localuserstats.so

localuser.so: localuser.o
	ld -shared -Bdynamic -o $@ $^

localuserchannel.so: localuserchannel.o
	ld -shared -Bdynamic -o $@ $^

localuserstats.so: localuserstats.o
	ld -shared -Bdynamic -o $@ $^

