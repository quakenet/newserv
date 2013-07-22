INCPATH=../../
include ../../build.mk
include ../build.mk

.PHONY: all
all: chanservdb.so

chanservdb.so: chanservdb.o chanservdb_alloc.o chanservdb_hash.o chanservdb_messages.o
        
chanservdb_messages.o: chanservdb_messages.c ../chanserv_messages.h
