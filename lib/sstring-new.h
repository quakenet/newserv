/* sstring.h - Declaration of "static strings" functions */

#ifndef __SSTRING_H 
#define __SSTRING_H

/* Externally visibly max string length */
#define SSTRING_MAX	512

/* you can actually change USE_VALGRIND here without recompiling! */
typedef struct sstring {
  short length;
  short alloc;
  struct sstring *next;
#ifdef USE_VALGRIND
  void *block;
#endif
  unsigned long refcount;
  char content[];
} sstring;

#ifdef COMPILING_SSTRING

/* Internal defines */

/* SSTRING_MAXLEN is the internal version of SSTRING_MAX which includes
 * space for the trailing NUL */
#define SSTRING_MAXLEN      (SSTRING_MAX + 1)
#define SSTRING_SLACK       8
#define SSTRING_ALLOC       16384

#define SSTRING_HASHSIZE    85243

#endif /* COMPILING_SSTRING */

sstring *getsstring(const char *, int);
void freesstring(sstring *);
int sstringcompare(sstring *ss1, sstring *ss2);
void initsstring();
void finisstring();

#endif /* __SSTRING_H */
