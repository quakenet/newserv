#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include "../lib/sstring.h"
#include "../lib/stringbuf.h"
#include "../lib/ccassert.h"
#include "../core/error.h"
#include "chanserv_messages.h"

#define MAXARGS 10
#define CONVBUF 512

void q9strftime(char *buf, size_t size, time_t t) {
  strftime(buf, size, Q9_FORMAT_TIME, gmtime(&t));
}

void q9vsnprintf(char *buf, size_t size, const char *format, const char *args, va_list ap) {
  StringBuf b;
  const char *p;
  char *c;

  char convbuf[MAXARGS][CONVBUF];

  CCASSERT(CONVBUF > TIMELEN);

  if(size == 0)
    return;

  {
    int argno = 0, i;

    int d;
    char *s;
    double g;
    unsigned int u;
    time_t t;

    for(i=0;i<MAXARGS;i++)
      convbuf[i][0] = '\0';

    for(;*args;args++) {
      char *cb = convbuf[argno++];

      switch(*args) {
        case 's':
          s = va_arg(ap, char *);
          snprintf(cb, CONVBUF, "%s", s);
          break;
        case 'd':
          d = va_arg(ap, int);
          snprintf(cb, CONVBUF, "%d", d);
          break;
        case 'u':
          u = va_arg(ap, unsigned int);
          snprintf(cb, CONVBUF, "%u", u);
          break;
        case 'g':
          g = va_arg(ap, double);
          snprintf(cb, CONVBUF, "%.1f", g);
          break;
        case 'T':
          t = va_arg(ap, time_t);
          q9strftime(cb, CONVBUF, t);
          break;
        default:
          /* calls exit(0) */
          Error("chanserv", ERR_STOP, "Bad format specifier '%c' supplied in q9vsnprintf, format: '%s'", *args, format);
      }
    }
  }

  sbinit(&b, buf, size);

  for(p=format;*p;p++) {
    if(*p != '$') {
      if(!sbaddchar(&b, *p))
        break;
      continue;
    }
    p++;
    if(*p == '\0')
      break;
    if(*p == '$') {
      if(!sbaddchar(&b, *p))
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
      if(!sbaddstr(&b, c))
        break;
  }

  sbterminate(&b);

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
