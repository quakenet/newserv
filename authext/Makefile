
.PHONY: all
all: authext.so

authext.so: authext.o
	ld -shared -Bdynamic -o $@ $^
