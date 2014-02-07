include ../build.mk

.PHONY: all

default: all

ifeq (${USE_VALGRIND},1)
CFLAGS+=-DUSE_NSMALLOC_VALGRIND=1
endif

all: events-${EVENT_ENGINE}.o main.o schedule.o hooks.o error.o modules.o config.o schedulealloc.o nsmalloc.o
