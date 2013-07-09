#include <stdlib.h>
#include <assert.h>
#include "../core/nsmalloc.h"
#include <stdio.h>
#include <string.h>

#include "glines.h"

#define ALLOCUNIT  100

gline *glinelist;

gline *newgline() {
  gline *gl = nsmalloc(POOL_GLINE,ALLOCUNIT*sizeof(gline));
  if(!gl)
    return NULL;

  gl->next=NULL;
  
  gl->nick=NULL;
  gl->user=NULL;
  gl->host=NULL;
  gl->reason=NULL;
  gl->creator=NULL;

  gl->expires=0;
  gl->lastmod=0;
  gl->lifetime=0;

  gl->flags=0;

  gl->next=NULL;
  
  return gl;
}

void freegline (gline *gl) {
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
  gline *gl2;
  if ( gl == glinelist) {
    glinelist = gl->next;
  } else {
    gl2 = glinelist;
    while (gl2) {
      if ( gl2->next == gl) {
        gl2->next = gl->next;
        break;
      }
      gl2 = gl2->next;
    }
  }
  freegline(gl);
}
