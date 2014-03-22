INCPATH=../../
include ../../build.mk
include ../build.mk

.PHONY: all clean
all: .autobuild.mk chanserv_chancmds.so

.autobuild.mk: *.c
	../mkcommandlist.pl chanserv_chancmds.so

sinclude .autobuild.mk

clean:
	rm -f commandlist.c *.o *.so .autobuild.mk
