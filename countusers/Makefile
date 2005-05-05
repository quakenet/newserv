
.PHONY: all
all: countusers.so

countusers.so: countusers.o
	ld -shared -Bdynamic -o $@ $^
