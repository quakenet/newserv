#include "stringbuf.h"

int sbaddchar(StringBuf *buf, char c) {
  if(buf->len >= buf->capacity - 1)
    return 0;

  buf->buf[buf->len++] = c;

  return 1;
}

int sbaddstr(StringBuf *buf, char *c) {
  int remaining = buf->capacity - buf->len - 1;
  char *p;

  for(p=c;*p;p++) {
    if(remaining <= 0)
      return 0;

    remaining--;

    buf->buf[buf->len++] = *p;
  }

  return 1;
}

int sbterminate(StringBuf *buf) {
  if(buf->capacity - buf->len > 0) {
    buf->buf[buf->len] = '\0';
    return 1;
  }
  return 0;
}
