#include <stdlib.h>
#include <assert.h>
#include "../core/nsmalloc.h"
#include <stdio.h>
#include <string.h>

#include "gline.h"

#define ALLOCUNIT  100

gline *gline_freelist;

gline *newgline() {
  gline *gl;
  int i;

  if( gline_freelist ==NULL ) {
    gline_freelist=(gline *)nsmalloc(POOL_GLINE,ALLOCUNIT*sizeof(gline));

    for (i=0;i<(ALLOCUNIT-1);i++) {
      gline_freelist[i].next=(gline *)&(gline_freelist[i+1]);
    }
    gline_freelist[ALLOCUNIT-1].next=NULL;
  }

  gl=gline_freelist;
  gline_freelist=(gline *)gl->next;

  gl->next=NULL;
  gl->nextbynode=NULL;
  gl->nextbynonnode=NULL;
  
  gl->glineid=0;
  gl->numeric=0;

  gl->nick=NULL;
  gl->user=NULL;
  gl->host=NULL;
  gl->reason=NULL;
  gl->creator=NULL;

  gl->node=NULL;

  gl->expires=0;
  gl->lastmod=0;
  gl->lifetime=0;

  gl->flags=0;

  return gl;
}

void removeglinefromlists( gline *gl) {
  gline *gl2;

  if (gl->flags & GLINE_IPMASK) {
    if ( gl == gl->node->exts[gl_nodeext]) {
      gl->node->exts[gl_nodeext] = gl->nextbynode;
    } else {
      gl2 = gl->node->exts[gl_nodeext];
      while (gl2) {
        if ( gl2->nextbynode == gl) {
          gl2->nextbynode = gl->nextbynode; 
          break;
        }
        gl2 = gl2->nextbynode;
      }
    }
  } else {
    if ( gl == glinelistnonnode) {
      glinelistnonnode = gl->nextbynonnode;
    } else {
      gl2 = glinelistnonnode;
      while (gl2) {
        if ( gl2->nextbynonnode == gl) {
          gl2->nextbynonnode = gl->nextbynonnode;
          break;
        }
        gl2 = gl2->nextbynonnode;
      }
    }
  }

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
}

void freegline (gline *gl) {
  gl->next=(gline *)gline_freelist;

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

  if (gl->node) { 
    derefnode(iptree, gl->node);
  }
  gline_freelist=gl;
}

void removegline( gline *gl) { 
  removeglinefromlists(gl);
  freegline(gl);
}


