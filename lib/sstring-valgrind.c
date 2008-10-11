/* sstring.h - Declaration of "static strings" functions */

#define COMPILING_SSTRING
#include "sstring.h"

#include "../core/hooks.h"
#include "../core/nsmalloc.h"
#include "../core/error.h"

#include <stdio.h>

#include <assert.h>
#include <stdlib.h>
#define __USE_GNU
#include <string.h>

void initsstring() {
}

void finisstring() {
  sstringlist *s, *sn;

  /* here we deliberately don't free the pointers so valgrind can tell us where they were allocated, in theory */

  for(s=head;s;s=sn) {
    sn = s->next;
    s->next = NULL;
    s->prev = NULL;

    Error("sstring", ERR_WARNING, "sstring of length %d still allocated: %s", s->s->u.l.length, s->s->content);
  }

  head = NULL;
}

sstring *getsstring(const char *inputstr, int maxlen) {
  sstringlist *s;
  size_t len;
  char *p;

  if(!inputstr)
    return NULL;

  for(p=(char *)inputstr;*p&&maxlen;maxlen--,p++)
    ; /* empty */

  len = p - inputstr;
  s=(sstringlist *)malloc(sizeof(sstringlist) + sizeof(sstring));

  s->s->u.l.length = len;
  s->s->content=(char *)malloc(len + 1);

  memcpy(s->s->content, inputstr, len);
  s->s->content[len] = '\0';

  s->next = head;
  s->prev = NULL;
  if(head)
    head->prev = s;
  head = s;

  return s->s;
}

void freesstring(sstring *inval) {
  sstringlist *s;
  if(!inval)
    return;

  s = (sstringlist *)inval - 1;

  if(s->prev) {
    s->prev->next = s->next;
    if(s->next)
      s->next->prev = s->prev;
  } else {
    head = s->next;
    if(head)
      head->prev = NULL;
  }

  free(inval->content);
  free(s);
}
