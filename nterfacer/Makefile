
.PHONY: all
all: nterfacer.so nterfacer_control.so nterfacer_relay.so nterfacer_chanstats.so nterfacer_country.so nterfacer_spamscan2.so

nterfacer.so: nterfacer.o logging.o esockets.o library.o
	ld -shared -Bdynamic -o $@ $^

nterfacer_control.so: nterfacer_control.o
	ld -shared -Bdynamic -o $@ $^

nterfacer_relay.so: nterfacer_relay.o
	ld -shared -Bdynamic ${LIBPCRE} -o $@ $^

nterfacer_chanstats.so: nterfacer_chanstats.o
	ld -shared -Bdynamic -o $@ $^

nterfacer_country.so: nterfacer_country.o
	ld -shared -Bdynamic -o $@ $^

nterfacer_spamscan2.so: nterfacer_spamscan2.o
	ld -shared -Bdynamic ${LIBPCRE} -o $@ $^
