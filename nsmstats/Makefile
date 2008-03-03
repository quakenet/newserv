
.PHONY: all
all: nsmstats.so

nsmstats.so: nsmstats.o
	ld -shared -Bdynamic -o $@ -lm $^
