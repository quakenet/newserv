include ../build.mk

RBLDIRS=newsearch

.PHONY: all dirs $(RBLDIRS) clean distclean
all: rbl.so rbl_commands.so rbl_zonefile.so dirs

rbl.so: rbl.o

rbl_commands.so: rbl_commands.o

rbl_zonefile.so: rbl_zonefile.o

dirs: $(RBLDIRS)
	ln -sf */*.so .

$(RBLDIRS):
	cd $@ && $(MAKE) $(MFLAGS) all

clean:
	rm -f */*.o */*.so *.o *.so
	rm -rf */.deps .deps

distclean:

