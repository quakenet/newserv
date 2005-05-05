/* sstring.h - Declaration of "static strings" functions */

#ifndef __SSTRING_H 
#define __SSTRING_H

#ifdef COMPILING_SSTRING
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

/* Internal defines */

#define SSTRING_MAXLEN      513
#define SSTRING_SLACK       8
#define SSTRING_STRUCTALLOC 4096
#define SSTRING_DATAALLOC   4096

#else
/* External (simpler) version */

typedef struct {
  char *content;
  short length;
} sstring;

#endif /* COMPILING_SSTRING */

sstring *getsstring(const char *, int);
void freesstring(sstring *);
int sstringcompare(sstring *ss1, sstring *ss2);
void initsstring();

#endif /* __SSTRING_H */
