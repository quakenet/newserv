
.PHONY: all
all: newsearch.so

newsearch.so: newsearch.o ns-not.o ns-and.o ns-or.o ns-eq.o ns-match.o ns-hostmask.o ns-realname.o ns-modes.o ns-nick.o ns-ident.o ns-regex.o ns-host.o ns-channel.o ns-lt.o ns-gt.o ns-timestamp.o ns-country.o
	ld -shared -Bdynamic $(LIBPCRE) -o $@ $^
