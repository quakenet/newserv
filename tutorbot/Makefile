
.PHONY: all
all: tutorbot.so

tutorbot.so: tutorbot.o
	ld -shared -Bdynamic -o $@ $^
