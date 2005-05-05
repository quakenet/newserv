
.PHONY: all
all: chanstats.so  

chanstats.so: chanstatsalloc.o chanstats.o
	ld -shared -Bdynamic -o $@ $^
