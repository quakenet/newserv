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

  for (pnext = &glinelist; *pnext; pnext = &((*pnext)->next)) {
    if (*pnext == gl) {
      *pnext = gl->next;
      break;
    }
  }

  freegline(gl);
}
