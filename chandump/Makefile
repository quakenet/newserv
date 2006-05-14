
.PHONY: all
all: chandump.so  

chandump.so: chandump.o
	ld -shared -Bdynamic -o $@ $^
