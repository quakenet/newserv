
.PHONY: all
all: newsearch.so

newsearch.so: newsearch.o ns-not.o ns-and.o ns-or.o ns-eq.o ns-match.o ns-hostmask.o ns-realname.o ns-modes.o ns-nick.o ns-ident.o ns-regex.o ns-host.o ns-channel.o ns-lt.o ns-gt.o ns-timestamp.o ns-country.o ns-authname.o ns-ip.o ns-kill.o ns-gline.o ns-exists.o ns-services.o ns-size.o ns-name.o ns-topic.o ns-oppct.o ns-hostpct.o ns-authedpct.o ns-length.o ns-kick.o ns-authts.o
	ld -shared -Bdynamic $(LIBPCRE) -o $@ $^
