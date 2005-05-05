
.PHONY: all
all: horse.so

horse.so: horse.o
	ld -shared -Bdynamic -o $@ $^
