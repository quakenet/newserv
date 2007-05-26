CFLAGS=-g

.PHONY: all
all: jupe.so jupe_commands.so

jupe.so: jupe.o
	ld -shared -Bdynamic -o $@ $^

jupe_commands.so: jupe_commands.o
	ld -shared -Bdynamic -o $@ $^
