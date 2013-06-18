#ifndef __GLINES_H
#define __GLINES_H

#define MAXGLINEUSERS        100

#define GLINE_IGNORE_TRUST   1
#define GLINE_ALWAYS_USER    2
#define GLINE_NO_LIMIT       3
#define GLINE_SIMULATE       4

int glinebynick(nick *, int, const char *, int);
int glinebyhost(const char *, const char *, int, const char *, int);
void unglinebyhost(const char *, const char *, int, const char *, int);

#endif
