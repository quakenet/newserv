INCPATH=../../
include ../../build.mk
include ../build.mk

.PHONY: all
all: .autobuild.mk chanserv_authcmds.so

.autobuild.mk: *.c
	../mkcommandlist.pl chanserv_authcmds.so

sinclude .autobuild.mk
