#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "../lib/sstring.h"

#define MAXARGS 10

struct bufs {
  char *buf;
  int capacity;
  int len;
};

static int addchar(struct bufs *buf, char c) {
  if(buf->len >= buf->capacity - 1)
    return 0;

  buf->buf[buf->len++] = c;

  return 1;
}

static int addstr(struct bufs *buf, char *c) {
  int remaining = buf->capacity - buf->len - 1;
  char *p;

  for(p=c;*p;p++) {
    if(remaining-- <= 0)
      return 0;

    buf->buf[buf->len++] = *p;
  }

  return 1;
}

void q9vsnprintf(char *buf, size_t size, const char *format, const char *args, va_list ap) {
  struct bufs b;
  const char *p;
  char *c;
  char convbuf[MAXARGS][512];

  if(size == 0)
    return;

  {
    int argno = 0;
    int d, i;
    char *s;
    sstring *ss;
    double f;

    for(i=0;i<MAXARGS;i++)
      convbuf[i][0] = '\0';

    while(*args) {
      switch(*args++) {
        case 's':
          s = va_arg(ap, char *);
          snprintf(convbuf[argno++], sizeof(convbuf[0]), "%s", s);
          break;
        case 'S':
          ss = va_arg(ap, sstring *);
          snprintf(convbuf[argno++], sizeof(convbuf[0]), "%s", s?"(null)":ss->content);
          break;
        case 'd':
          d = va_arg(ap, int);
          snprintf(convbuf[argno++], sizeof(convbuf[0]), "%d", d);
          break;
        case 'g':
          f = va_arg(ap, double);
          snprintf(convbuf[argno++], sizeof(convbuf[0]), "%.1f", f);
          break;
      }
    }
  }

  b.buf = buf;
  b.capacity = size;
  b.len = 0;

  for(p=format;*p;p++) {
    if(*p != '$') {
      if(!addchar(&b, *p))
        break;
      continue;
    }
    p++;
    if(*p == '\0')
      break;
    if(*p == '$') {
      if(!addchar(&b, *p))
        break;
      continue;
    }

    c = NULL;
    switch(*p) {
/*
      case 'C':
        c = botname; break;
      case 'N':
        c = network; break;
*/
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
        c = convbuf[*p - '0']; break;
      default:
        c = "(bad format specifier)";
    }
    if(c)
      if(!addstr(&b, c))
        break;
  }

  buf[b.len] = '\0';

  /* not required */
  /*
  buf[size-1] = '\0';
  */
}

void q9snprintf(char *buf, size_t size, const char *format, const char *args, ...) {
  va_list ap;

  va_start(ap, args);
  q9vsnprintf(buf, size, format, args, ap);
  va_end(ap);
}
