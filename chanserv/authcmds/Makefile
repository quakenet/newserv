INCPATH=../../
include ../../build.mk
include ../build.mk

.PHONY: all clean
all: .autobuild.mk chanserv_authcmds.so

.autobuild.mk: *.c
	../mkcommandlist.pl chanserv_authcmds.so

sinclude .autobuild.mk

clean:
	rm -f commandlist.c *.o *.so .autobuild.mk

