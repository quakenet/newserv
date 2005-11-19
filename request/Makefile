
.PHONY: all
all: request.so

request.so: lrequest.o request.o request_block.o sqrequest.o user.o 
	ld -shared -Bdynamic -o $@ $^
