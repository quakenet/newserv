/* proxyscanalloc.c */

#include "proxyscan.h"

#include <stdlib.h>

#define ALLOCUNIT 1024

scan *freescans;
cachehost *freecachehosts;
pendingscan *freependingscans;
foundproxy *freefoundproxies;

void *mallocs=NULL;

void *smalloc(size_t size) {
  void **mem;

  /* Get the memory we want, with an extra four bytes for our pointer */
  mem=(void **)malloc(size+sizeof(void *));

  /* Set the first word to point at the last chunk we got */
  *mem=mallocs;

  /* Now set the "last chunk" pointer to the address of this one */
  mallocs=(void *)mem;

  /* Return the rest of the memory to the caller */
  return (void *)(mem+1);
}

void sfreeall() {
  void *vp,**vp2;
  
  vp=mallocs;

  while (vp!=NULL) {
    vp2=(void **)vp;
    vp=*vp2;
    free((void *)vp2);
  }
}

scan *getscan() {
  int i;
  scan *sp;
  
  if (freescans==NULL) {
    /* Eep.  Allocate more. */
    freescans=(scan *)smalloc(ALLOCUNIT*sizeof(scan));
    for (i=0;i<(ALLOCUNIT-1);i++) {
      freescans[i].next=&(freescans[i+1]);
    }
    freescans[ALLOCUNIT-1].next=NULL;
  }
  
  sp=freescans;
  freescans=sp->next;
  
  return sp;
}

void freescan(scan *sp) {
  sp->next=freescans;
  freescans=sp;
}

cachehost *getcachehost() {
  int i;
  cachehost *chp;
  
  if (freecachehosts==NULL) {
    freecachehosts=(cachehost *)smalloc(ALLOCUNIT*sizeof(cachehost));
    for (i=0;i<(ALLOCUNIT-1);i++) {
      freecachehosts[i].next=&(freecachehosts[i+1]);
    }
    freecachehosts[ALLOCUNIT-1].next=NULL;
  }
  
  chp=freecachehosts;
  freecachehosts=chp->next;
  
  return chp;
}

void freecachehost(cachehost *chp) {
  chp->next=freecachehosts;
  freecachehosts=chp;
} 

pendingscan *getpendingscan() {
  int i;
  pendingscan *psp;

  if (!freependingscans) {
    freependingscans=(pendingscan *)smalloc(ALLOCUNIT * sizeof(pendingscan));
    for (i=0;i<(ALLOCUNIT-1);i++)
      freependingscans[i].next = freependingscans+i+1;
    freependingscans[ALLOCUNIT-1].next=NULL;
  }

  psp=freependingscans;
  freependingscans=psp->next;

  return psp;
}

void freependingscan(pendingscan *psp) {
  psp->next=freependingscans;
  freependingscans=psp;
}

foundproxy *getfoundproxy() {
  int i;
  foundproxy *fpp;

  if (!freefoundproxies) {
    freefoundproxies=(foundproxy *)smalloc(ALLOCUNIT * sizeof(foundproxy));
    for (i=0;i<(ALLOCUNIT-1);i++)
      freefoundproxies[i].next = freefoundproxies+i+1;
    freefoundproxies[ALLOCUNIT-1].next=NULL;
  }

  fpp=freefoundproxies;
  freefoundproxies=fpp->next;

  return fpp;
}

void freefoundproxy(foundproxy *fpp) {
  fpp->next=freefoundproxies;
  freefoundproxies=fpp;
}

