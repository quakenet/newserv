#ifndef __GEOIP_H
#define __GEOIP_H

struct country;

/* "code" -> GB/FR */
/* "name" -> United Kingdom/France */

struct country *geoip_lookup_code(const char *);
struct country *geoip_lookup_nick(nick *);

struct country *geoip_next(struct country *);

const char *geoip_code(struct country *c);
const char *geoip_name(struct country *c);
int geoip_total(struct country *c);

#endif
