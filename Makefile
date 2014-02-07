INCPATH = ./

include build.mk

CLEANDIRS = chanserv geoip newsearch trusts

OBJS  = core/hooks.o core/main.o core/schedule.o core/events-${EVENT_ENGINE}.o lib/sstring.o
OBJS += lib/array.o lib/splitline.o parser/parser.o lib/base64.o
OBJS += core/error.o core/modules.o core/config.o lib/flags.o lib/irc_string.o
OBJS += core/schedulealloc.o core/nsmalloc.o lib/sha1.o lib/md5.o
OBJS += lib/strlfunc.o lib/irc_ipv6.o lib/sha2.o lib/rijndael.o
OBJS += lib/hmac.o lib/prng.o lib/stringbuf.o lib/cbc.o

.PHONY: all $(DIRS) clean distclean

all: $(DIRS) newserv

newserv: $(OBJS)
	$(CC) $(CFLAGS) -Wl,--export-dynamic $(LDFLAGS) -o $@ $^ $(LIBDL) -lm

$(DIRS):
	cd $@ && $(MAKE) $(MFLAGS) all

clean: 
	for i in $(CLEANDIRS) ; do $(MAKE) -C $$i $(MFLAGS) clean ; done
	rm -f newserv .settings.mk
	for i in $(WORKSPACES); do \
		rm -f $$i/*/*.o $$i/*/*.so; \
		rm -Rf $$i/*/.deps; \
	done
	rm -rf modules

install:
	mkdir -p modules
	rm -f modules/*.so
	cd modules; for i in $(WORKSPACES); do \
		ln -s ../$$i/*/*.so ./; \
	done
	cd modules; ../depmod.pl

distclean:	clean
	for i in $(CLEANDIRS) ; do $(MAKE) -C $$i $(MFLAGS) distclean; done
	rm -f build.mk .configure.log config.h
