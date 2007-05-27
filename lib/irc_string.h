/* irc_string.h */

#ifndef __IRC_STRING_H
#define __IRC_STRING_H

#include <limits.h>
#include <stdlib.h>

extern const char ToLowerTab_8859_1[];

#define ToLower(c)        (ToLowerTab_8859_1[(c) - CHAR_MIN])

int match2strings(const char *patrn, const char *strng);
int match2patterns(const char *patrn, const char *strng);
unsigned long crc32(const char *s);
unsigned long crc32i(const char *s);
int ircd_strcmp(const char *s1, const char *s2);
int ircd_strncmp(const char *s1, const char *s2, size_t len);
char *delchars(char *string, const char *badchars);
const char *IPlongtostr(unsigned long IP);
const char *longtoduration(unsigned long interval, int format);
int durationtolong(const char *string);

int match(const char *, const char *);
int mmatch(const char *, const char *);
char *collapse(char *mask);

int protectedatoi(char *buf, int *value);

#endif
