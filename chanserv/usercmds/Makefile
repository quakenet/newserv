INCPATH=../../
include ../../build.mk
include ../build.mk

.PHONY: all
all: .autobuild.mk chanserv_usercmds.so

.autobuild.mk: *.c
	../mkcommandlist.pl chanserv_usercmds.so

sinclude .autobuild.mk
