
.PHONY: all
all: geoip.so

geoip.so: geoip.o libGeoIP/GeoIP.o
	ld -shared -Bdynamic -o $@ $^
