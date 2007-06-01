/* channelindex.c */

#include "chanindex.h"
#include "../irc/irc_config.h"
#include "../lib/irc_string.h"
#include "../core/error.h"
#include "../core/nsmalloc.h"

#include <stdio.h>
#include <string.h>

#define ALLOCUNIT	1000

#define channelhash(x)  (crc32i(x)%CHANNELHASHSIZE)

chanindex *chantable[CHANNELHASHSIZE];
sstring *extnames[MAXCHANNELEXTS];
chanindex *freechanindices;

unsigned int channelmarker;

void __init() {
  memset(chantable,0,sizeof(chantable));
  memset(extnames,0,sizeof(extnames));
  channelmarker=0;
  freechanindices=NULL;
}

void __fini() {
  nsfreeall(POOL_CHANINDEX);
}

chanindex *getchanindex() {
  int i;
  chanindex *cip;

  if (freechanindices==NULL) {
    freechanindices=(chanindex *)nsmalloc(POOL_CHANINDEX, ALLOCUNIT*sizeof(chanindex));
    for(i=0;i<ALLOCUNIT-1;i++) {
      freechanindices[i].next=&(freechanindices[i+1]);
    }
    freechanindices[ALLOCUNIT-1].next=NULL;
  }

  cip=freechanindices;
  freechanindices=cip->next;

  return cip;
}

void freechanindex(chanindex *cip) {
  cip->next=freechanindices;
  freechanindices=cip;
}

chanindex *findchanindex(const char *name) {
  chanindex *cip;
  int hash=channelhash(name);
  
  for (cip=chantable[hash];cip;cip=cip->next) {
    if (!ircd_strcmp(cip->name->content,name)) {
      return cip;
    }
  }
  
  return NULL;
}

chanindex *findorcreatechanindex(const char *name) {
  chanindex *cip;
  int hash=channelhash(name);
  int i;

  for (cip=chantable[hash];cip;cip=cip->next) {
    if (!ircd_strcmp(cip->name->content,name)) {
      return cip;
    }
  }

  cip=getchanindex();

  cip->name=getsstring(name,CHANNELLEN);
  cip->channel=NULL;
  cip->marker=0;
  cip->next=chantable[hash];
  chantable[hash]=cip;
  
  for(i=0;i<MAXCHANNELEXTS;i++) {
    cip->exts[i]=NULL;
  }
  
  return cip;
}

void releasechanindex(chanindex *cip) {
  int i;
  chanindex **cih;
  int hash;
  
  /* If any module is still using the channel, do nothing */
  /* Same if the channel is still present on the network */
  
  if (cip->channel!=NULL) {
    return;
  }
  
  for (i=0;i<MAXCHANNELEXTS;i++) {
    if (cip->exts[i]!=NULL) {
      return;
    }
  }
  
  /* Now remove the index record from the index. */
  hash=channelhash(cip->name->content);
  
  for(cih=&(chantable[hash]);*cih;cih=&((*cih)->next)) {
    if (*cih==cip) {
      (*cih)=cip->next;
      freesstring(cip->name);
      freechanindex(cip);
      return;
    }
  }
  
  Error("channel",ERR_ERROR,"Tried to release chanindex record for %s not found in hash",cip->name->content);
}

int registerchanext(const char *name) {
  int i;
  
  if (findchanext(name)!=-1) {
    Error("channel",ERR_WARNING,"Tried to register duplicate extension %s",name);
    return -1;
  }
  
  for (i=0;i<MAXCHANNELEXTS;i++) {
    if (extnames[i]==NULL) {
      extnames[i]=getsstring(name,100);
      return i;
    }
  }
  
  Error("channel",ERR_WARNING,"Tried to register too many extensions: %s",name);
  return -1;
}

int findchanext(const char *name) {
  int i;
  
  for (i=0;i<MAXCHANNELEXTS;i++) {
    if (extnames[i]!=NULL && !ircd_strcmp(name,extnames[i]->content)) {
      return i;
    }
  }
  
  return -1;
}

void releasechanext(int index) {
  int i;
  chanindex *cip,*ncip;
  
  freesstring(extnames[index]);
  extnames[index]=NULL;
  
  for (i=0;i<CHANNELHASHSIZE;i++) {
    for (cip=chantable[i];cip;cip=ncip) {
      /* CAREFUL: deleting items from chains you're walking is bad */
      ncip=cip->next;
      if (cip->exts[index]!=NULL) {
        cip->exts[index]=NULL;
        releasechanindex(cip);
      }
    }
  }
}

unsigned int nextchanmarker() {
  int i;
  chanindex *cip;
  
  channelmarker++;
  if (!channelmarker) {
    /* If we wrapped to zero, zap the marker on all records */
    for (i=0;i<CHANNELHASHSIZE;i++)
      for (cip=chantable[i];cip;cip=cip->next)
	cip->marker=0;
    channelmarker++;
  }
  
  return channelmarker;
}
