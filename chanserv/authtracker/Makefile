INCPATH=../../
include ../../build.mk
include ../build.mk

.PHONY: all
all: chanserv_authtracker.so

chanserv_authtracker.so: authtracker.o authtracker_hooks.o authtracker_query.o authtracker_db.o chanserv_authtracker.o
        
