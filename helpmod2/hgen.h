/* new H general functions and procedures */
#ifndef HGEN_H
#define HGEN_H

#include "hdef.h"

int ci_strcmp(const char*, const char*);

/* matches string (const char*) to the regular expression (? and *) (const char*) */
int strregexp(const char *, const char *);

/* prints the time in the common 1h 2m 3s format, has three internal buffers */
const char *helpmod_strtime(int);
/* reads a time string */
int helpmod_read_strtime(const char *);

int hword_count(const char*);

int helpmod_is_lame_line(const char*);

int strislower(const char*);
int strisupper(const char*);

int strisalpha(const char*);
int strnumcount(const char*);

float helpmod_percentage(int, int);

#endif
