#include <stdlib.h>
#include <assert.h>
#include "../core/nsmalloc.h"
#include <stdio.h>
#include <string.h>

#include "glines.h"

gline *glinelist;

gline *newgline() {
  gline *gl = nsmalloc(POOL_GLINE, sizeof(gline));

  if(!gl)
    return NULL;

  gl->next=NULL;
  
  gl->nick=NULL;
  gl->user=NULL;
  gl->host=NULL;
  gl->reason=NULL;
  gl->creator=NULL;

  gl->expire=0;
  gl->lastmod=0;
  gl->lifetime=0;

  gl->flags=0;

  gl->next=NULL;
  
  return gl;
}

void freegline(gline *gl) {
  if (gl->nick)
    freesstring(gl->nick);
  if (gl->user)
    freesstring(gl->user);
  if (gl->host)
    freesstring(gl->host);
  if (gl->reason)
    freesstring(gl->reason);
  if (gl->creator)
    freesstring(gl->creator);

  nsfree(POOL_GLINE, gl);
}

void removegline(gline *gl) {
  gline **pnext;

  for(pnext=&glinelist;*pnext;pnext=&((*pnext)->next)) {
    if (*pnext == gl) {
      *pnext = gl->next;
      break;
    }
  }

  freegline(gl);
}
