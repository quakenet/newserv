#ifndef __GLINES_H
#define __GLINES_H

#define MAXGLINEUSERS        100

#define GLINE_IGNORE_TRUST   1
#define GLINE_ALWAYS_NICK    2
#define GLINE_ALWAYS_USER    4
#define GLINE_NO_LIMIT       8
#define GLINE_SIMULATE       16

typedef struct gline_params {
  int duration;
  const char *reason;
} gline_params;

typedef void (*gline_callback)(const char *, int, void *);

void glinesetmask(const char *mask, int duration, const char *reason);
void glineunsetmask(const char *mask);

int glinesuggestbyip(const char *, struct irc_in_addr *, unsigned char, int, gline_callback callback, void *uarg);
int glinesuggestbynick(nick *, int, gline_callback callback, void *uarg);
int glinebyip(const char *, struct irc_in_addr *, unsigned char, int, const char *, int);
int glinebynick(nick *, int, const char *, int);

#endif
