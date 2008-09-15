#ifndef __STRINGBUF_H
#define __STRINGBUF_H

#include <stdlib.h>

typedef struct StringBuf {
  char *buf;
  int capacity;
  int len;
} StringBuf;

int sbaddchar(StringBuf *buf, char c);
int sbaddstr(StringBuf *buf, char *c);
int sbterminate(StringBuf *buf);
void sbinit(StringBuf *buf, char *c, int capacity);
int sbaddstrlen(StringBuf *buf, char *c, size_t len);

#endif
