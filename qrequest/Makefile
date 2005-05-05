
.PHONY: all
all: qrequest.so

qrequest.so: qrequest.o
	ld -shared -Bdynamic -o $@ $^
