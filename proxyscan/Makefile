
.PHONY: all
all: proxyscan.so  

proxyscan.so: proxyscan.o proxyscanalloc.o proxyscanconnect.o proxyscancache.o proxyscanext.o proxyscanqueue.o proxyscanhandlers.o proxyscandb.o
	ld -shared -Bdynamic ${LIBPGSQL} -o $@ $^
