
.PHONY: all
all: control.so control-lite.so

control-lite.so: control-lite.o
	ld -shared -Bdynamic -o $@ $^

control.so: control.o
	ld -shared -Bdynamic -o $@ $^
