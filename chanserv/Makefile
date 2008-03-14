
CSDIRS=database chancmds usercmds authcmds authtracker newsearch

.PHONY: all dirs clean
all: chanserv.so chanserv_protect.so chanserv_grep.so chanserv_chansearch.so chanserv_relay.so dirs

dirs:
	for i in $(CSDIRS) ; do $(MAKE) -C $$i all ; done
	ln -sf */*.so .

clean:
	rm -f */*.o */*.so

chanserv.so: chanservdb_updates.o chanserv.o chanservuser.o chanservnetevents.o chanservprivs.o chanservlog.o chanservstdcmds.o \
		chanservdump.o chanservschedule.o chanservcrypto.o authlib.o q9snprintf.o
	ld -shared -Bdynamic ${LIBPGSQL} -o $@ $^

chanserv_protect.so: chanserv_protect.o
	ld -shared -Bdynamic -o $@ $^

chanserv_grep.so: chanservgrep.o
	ld -shared -Bdynamic ${LIBPCRE} -o $@ $^

chanserv_chansearch.so: chanserv-chansearch.o
	ld -shared -Bdynamic -o $@ $^

chanserv_relay.so: chanserv_relay.o
	ld -shared -Bdynamic -o $@ $^

