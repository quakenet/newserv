
CSDIRS=database

.PHONY: all dirs clean
all: chanserv.so chanserv_chancmds.so chanserv_usercmds.so chanserv_protect.so chanserv_auth.so chanserv_grep.so \
	chanserv_chansearch.so dirs

dirs:
	for i in $(CSDIRS) ; do $(MAKE) -C $$i all ; done
	ln -sf */*.so .

clean:
	rm -f */*.o */*.so

chanserv.so: chanserv.o chanservuser.o chanservnetevents.o chanservprivs.o chanservlog.o chanservstdcmds.o \
		chanservdump.o chanservschedule.o
	ld -shared -Bdynamic ${LIBPGSQL} -o $@ $^

chanserv_chancmds.so: chancmds.o
	ld -shared -Bdynamic -o $@ $^

chanserv_usercmds.so: usercmds.o
	ld -shared -Bdynamic -o $@ $^

chanserv_protect.so: chanserv_protect.o
	ld -shared -Bdynamic -o $@ $^

chanserv_auth.so: authcmds.o authlib.o
	ld -shared -Bdynamic -o $@ $^

chanserv_grep.so: chanservgrep.o
	ld -shared -Bdynamic ${LIBPCRE} -o $@ $^

chanserv_chansearch.so: chanserv-chansearch.o
	ld -shared -Bdynamic -o $@ $^
