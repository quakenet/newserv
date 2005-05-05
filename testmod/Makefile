
.PHONY: all
all: testmod.so  

testmod.so: testmod.o
	ld -shared -Bdynamic -o $@ $^
