
.PHONY: all
all: localuser.so localuserchannel.so

localuser.so: localuser.o
	ld -shared -Bdynamic -o $@ $^

localuserchannel.so: localuserchannel.o
	ld -shared -Bdynamic -o $@ $^

