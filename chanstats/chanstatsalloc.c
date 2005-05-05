/* Allocators for chanstats */

#include "chanstats.h"

#include <stdlib.h>

#define ALLOCUNIT 100

static void *malloclist;
static chanstats *freecs;

void *cstsmalloc(size_t size) {
  void **mem;

  /* Get the memory we want, with an extra four bytes for our pointer */
  mem=(void **)malloc(size+sizeof(void *));

  /* Set the first word to point at the last chunk we got */
  *mem=malloclist;

  /* Now set the "last chunk" pointer to the address of this one */
  malloclist=(void *)mem;

  /* Return the rest of the memory to the caller */
  return (void *)(mem+1);
}

void cstsfreeall() {
  void *vp,**vp2;

  vp=malloclist;

  while (vp!=NULL) {
    vp2=(void **)vp;
    vp=*vp2;
    free((void *)vp2);
  }
}

void initchanstatsalloc() {
  malloclist=NULL;
  freecs=NULL;
}

chanstats *getchanstats() {
  chanstats *csp;
  int i;
   
  if (freecs==NULL) {
    freecs=(chanstats *)cstsmalloc(ALLOCUNIT*sizeof(chanstats));
    for(i=0;i<(ALLOCUNIT-1);i++) {
      freecs[i].index=(chanindex *)&(freecs[i+1]);
    }
    freecs[ALLOCUNIT-1].index=NULL;
  }
  
  csp=freecs;
  freecs=(chanstats *)csp->index;
  
  return csp;
}

void freechanstats(chanstats *csp) {
  csp->index=(chanindex *)freecs;
  freecs=csp;
}
