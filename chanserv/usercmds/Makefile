INCPATH=../../
include ../../build.mk
include ../build.mk

.PHONY: all clean
all: .autobuild.mk chanserv_usercmds.so

.autobuild.mk: *.c
	../mkcommandlist.pl chanserv_usercmds.so

sinclude .autobuild.mk

clean:
	rm -f commandlist.c *.o *.so .autobuild.mk

