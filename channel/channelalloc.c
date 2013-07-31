/* channelalloc.c */

#include "channel.h"
#include "../core/nsmalloc.h"

channel *newchan() {
  return nsmalloc(POOL_CHANNEL, sizeof(channel));
}

void freechan(channel *cp) {
  nsfree(POOL_CHANNEL, cp);
}

chanuserhash *newchanuserhash(int hashsize) {
  int i;
  chanuserhash *cuhp = nsmalloc(POOL_CHANNEL, sizeof(chanuserhash));

  if (!cuhp)
    return NULL;

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
  nsfree(POOL_CHANNEL, cuhp);
}
