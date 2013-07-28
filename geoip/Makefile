include ../build.mk

GEOIPDIRS=libGeoIP
LDFLAGS+=-lc

.PHONY: all clean distclean $(GEOIPDIRS) dirs

all: dirs geoip.so

libGeoIP/libgeoip.a: dirs

geoip.so: geoip.o libGeoIP/libgeoip.a

dirs: $(GEOIPDIRS)

$(GEOIPDIRS):
	cd $@ && $(MAKE) $(MFLAGS) all

clean:
	rm -f */*.o */*.so *.o *.so */*.a
	rm -rf .deps */.deps
