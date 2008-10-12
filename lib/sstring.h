/* sstring.h - Declaration of "static strings" functions */

#ifndef __SSTRING_H 
#define __SSTRING_H

#define SSTRING_COMPAT

#ifdef COMPILING_SSTRING

#ifdef SSTRING_NEW

/* this is here for compatibility reasons with old sstring */
/* this should be used when old sstring is removed entirely */
#ifdef SSTRING_COMPAT

typedef struct sstring {
  char *content;
  short length;
  short alloc;
  struct sstring *next;
#ifdef USE_VALGRIND
  void *block;
#endif
  unsigned long refcount;
  char __content[];
} sstring;

#define sstring_content(x) (x)->__content

#else /* SSTRING_COMPAT */

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

#define sstring_content(x) (x)->content

#endif /* SSTRING_COMPAT */

#else /* SSTRING_NEW */

/* this is the old format */

/* Internal version of structure */
typedef struct {
  char *content;
  union {
    struct {
      short length;
      short alloc;
    } l;
    void *next;
  } u;
} sstring;

#endif /* SSTRING_NEW */

/* Internal defines */

/* SSTRING_MAXLEN is the internal version of SSTRING_MAX which includes
 * space for the trailing NUL */
#define SSTRING_MAXLEN      (SSTRING_MAX + 1)
#define SSTRING_SLACK       8

/* new sstring defines */
#define SSTRING_ALLOC       16384

#define SSTRING_HASHSIZE    85243

/* old sstring defines */
#define SSTRING_STRUCTALLOC 4096
#define SSTRING_DATAALLOC   4096

#else

typedef struct sstring {
  char *content;
  short length;
} sstring;

#endif /* COMPILING_SSTRING */

/* Externally visibly max string length */
#define SSTRING_MAX	512

sstring *getsstring(const char *, int);
void freesstring(sstring *);
int sstringcompare(sstring *ss1, sstring *ss2);
void initsstring();
void finisstring();

#endif /* __SSTRING_H */
