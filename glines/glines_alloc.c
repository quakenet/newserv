#include <string.h>
#include "../core/nsmalloc.h"
#include "glines.h"

gline *glinelist;

gline *newgline() {
  gline *gl = nsmalloc(POOL_GLINE, sizeof(gline));

  if (!gl)
    return NULL;

  memset(gl, 0, sizeof(gline));

  return gl;
}

void freegline(gline *gl) {
  freesstring(gl->nick);
  freesstring(gl->user);
  freesstring(gl->host);
  freesstring(gl->reason);
  freesstring(gl->creator);

  nsfree(POOL_GLINE, gl);
}

void removegline(gline *gl) {
  gline **pnext;

  for (pnext = &glinelist; *pnext; pnext = &((*pnext)->next)) {
    if (*pnext == gl) {
      *pnext = gl->next;
      break;
    }
  }

  freegline(gl);
}
