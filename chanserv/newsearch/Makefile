
.PHONY: all
all: chanserv_newsearch.so

chanserv_newsearch.so: cs-newsearch.o formats.o 
	ld -shared -Bdynamic -o $@ $^
        
