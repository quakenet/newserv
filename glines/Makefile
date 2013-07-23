include ../build.mk

.PHONY: all
all: glines.so glines_commands.so glines_store.so

glines.so: glines.o glines_alloc.o glines_formats.o glines_buf.o glines_handler.o glines_util.o

glines_commands.so: glines_commands.o

glines_store.so: glines_store.o
