include ../build.mk
include build.mk

CSDIRS=database chancmds usercmds authcmds authtracker newsearch achievements achcmds

.PHONY: all dirs $(CSDIRS) clean distclean
all: chanserv.so chanserv_protect.so chanserv_grep.so chanserv_relay.so chanserv_flags.so chanserv_cleanupdb.so dirs

dirs: $(CSDIRS)
	ln -sf */*.so .

$(CSDIRS):
	cd $@ && $(MAKE) $(MFLAGS) all

clean:
	rm -f */*.o */*.d */*.so *.o *.d *.so

distclean:
	rm -f */.autobuild.mk .autobuild.mk

chanserv.so: chanservdb_updates.o chanserv.o chanservuser.o chanservnetevents.o chanservprivs.o chanservlog.o chanservstdcmds.o \
		chanservdump.o chanservschedule.o chanservcrypto.o authlib.o q9snprintf.o

chanserv_protect.so: chanserv_protect.o

chanserv_grep.so: chanserv_grep.o

chanserv_relay.so: chanserv_relay.o

chanserv_flags.so: chanserv_flags.o

chanserv_cleanupdb.so: chanserv_cleanupdb.o
