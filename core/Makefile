
default: all

events.c:
	rm -f events.c
	ln -s events-poll.c events.c

kqueue:
	rm -f events.c
	ln -s events-kqueue.c events.c

epoll:
	rm -f events.c
	ln -s events-epoll.c events.c

.PHONY: all
all: events.c main.o events.o schedule.o hooks.o error.o modules.o config.o schedulealloc.o

