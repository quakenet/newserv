/* sstring.h - Declaration of "static strings" functions */

#ifndef __SSTRING_H
#define __SSTRING_H

typedef struct sstring {
  short length;
  char content[];
} sstring;

/* Externally visibly max string length */
#define SSTRING_MAX    512

sstring *getsstring(const char *, int);
void freesstring(sstring *);
int sstringcompare(sstring *ss1, sstring *ss2);

#endif /* __SSTRING_H */
