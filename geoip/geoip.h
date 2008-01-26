#define COUNTRY_MIN 0
#define COUNTRY_MAX 246

extern int geoip_totals[COUNTRY_MAX + 1];

int geoip_lookupcode(char *code);
typedef int (*GeoIP_LookupCode)(char *);

