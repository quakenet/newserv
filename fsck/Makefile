
.PHONY: all
all: fsck.so

fsck.so: fsck.o
	ld -shared -Bdynamic -o $@ $^
