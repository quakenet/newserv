
.PHONY: all
all: ticketauth.so

ticketauth.so: ticketauth.o
	ld -shared -Bdynamic -o $@ $^
