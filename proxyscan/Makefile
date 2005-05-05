
.PHONY: all
all: proxyscan.so  

proxyscan.so: proxyscan.o proxyscanalloc.o proxyscanconnect.o proxyscancache.o proxyscanqueue.o proxyscanhandlers.o proxyscandb.o
	ld -shared -Bdynamic ${LIBMYSQL} -o $@ $^
