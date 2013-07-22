INCPATH=../../
include ../../build.mk
include ../build.mk

.PHONY: all
all: .autobuild.mk chanserv_chancmds.so

.autobuild.mk: *.c
	../mkcommandlist.pl chanserv_chancmds.so

sinclude .autobuild.mk

