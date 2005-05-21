
.PHONY: all
all: nterfaced.so nterface_shared.so nterfacer.so nterfacer_control.so nterfacer_relay.so

nterfaced.so: nterfaced.o requests.o nterfaced_uds.o nterfaced_service_link.o
	ld -shared -Bdynamic -o $@ $^

nterface_shared.so: logging.o library.o esockets.o
	ld -shared -Bdynamic -o $@ $^

nterfacer.so: nterfacer.o
	ld -shared -Bdynamic -o $@ $^

nterfacer_control.so: nterfacer_control.o
	ld -shared -Bdynamic -o $@ $^

nterfacer_relay.so: nterfacer_relay.o
	ld -shared -Bdynamic ${LIBPCRE} -o $@ $^

