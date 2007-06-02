/* channelalloc.c */

#include <stdlib.h>
#include "channel.h"
#include <assert.h>
#include "../core/nsmalloc.h"

#define ALLOCUNIT  100

channel *freechans;
chanuserhash *freehash;

void initchannelalloc() {
  freechans=NULL;
  freehash=NULL;
}

/* channel records don't have next pointers.  
 * Overload the index pointer for chaining free channels */

channel *newchan() {
  int i;
  channel *cp;
  
  if (freechans==NULL) {
    freechans=(channel *)nsmalloc(POOL_CHANNEL,ALLOCUNIT*sizeof(channel));
    for (i=0;i<(ALLOCUNIT-1);i++) {
      freechans[i].index=(struct chanindex *)&(freechans[i+1]);
    }
    freechans[ALLOCUNIT-1].index=NULL;
  }
  
  cp=freechans;
  freechans=(channel *)cp->index;
  
  return cp;
}

void freechan(channel *cp) {
  cp->index=(struct chanindex *)freechans;
  freechans=cp;
}

/*
 * Free hash for all!
 *
 * Since these things don't naturally have a "next" pointer
 * we abuse the "content" pointer to build our list of free
 * structures.
 *
 * Hence some somewhat bizarre casts...
 */

chanuserhash *newchanuserhash(int hashsize) {
  int i;
  chanuserhash *cuhp;
  
  if (freehash==NULL) {
    freehash=(chanuserhash *)nsmalloc(POOL_CHANNEL,ALLOCUNIT*sizeof(chanuserhash));
    for (i=0;i<(ALLOCUNIT-1);i++) {
      freehash[i].content=(unsigned long *)&(freehash[i+1]);
    }    
    freehash[ALLOCUNIT-1].content=NULL;
  }
  
  cuhp=freehash;
  freehash=(chanuserhash *)cuhp->content;
  
  /* Don't use nsmalloc() here since we will free this in freechanuserhash() */
  cuhp->content=(unsigned long *)malloc(hashsize*sizeof(unsigned long));
  for (i=0;i<hashsize;i++) {
    cuhp->content[i]=nouser;
  }
  
  cuhp->hashsize=hashsize;
  cuhp->totalusers=0;
  
  return cuhp;
}

void freechanuserhash(chanuserhash *cuhp) { 
  free(cuhp->content);
  cuhp->content=(unsigned long *)freehash; 
  freehash=cuhp;
}
