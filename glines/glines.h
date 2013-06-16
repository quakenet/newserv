#ifndef __GLINES_H
#define __GLINES_H

void glinebynick(nick *, int, const char *);
void glinebyhost(const char *, const char *, int, const char *);
void unglinebyhost(const char *, const char *, int, const char *);

#endif
