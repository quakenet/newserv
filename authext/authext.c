#include "authext.h"
#include "../core/nsmalloc.h"
#include "../core/error.h"
#include "../lib/sstring.h"
#include "../lib/irc_string.h"

#include <string.h>

#define ALLOCUNIT 100

#define authnamehash(x)   ((x)%AUTHNAMEHASHSIZE)

authname *freeauthnames;
authname *authnametable[AUTHNAMEHASHSIZE];

sstring *authnameextnames[MAXAUTHNAMEEXTS];

void _init() {
  freeauthnames=NULL;
  memset(authnametable,0,sizeof(authnametable));
}

void _fini() {
  nsfreeall(POOL_AUTHEXT);
}

authname *newauthname() {
  authname *anp;
  int i;

  if (freeauthnames==NULL) {
    freeauthnames=(authname *)nsmalloc(POOL_AUTHEXT, ALLOCUNIT*sizeof(authname));
    for (i=0;i<(ALLOCUNIT-1);i++) {
      freeauthnames[i].next=&(freeauthnames[i+1]);
    }
    freeauthnames[ALLOCUNIT-1].next=NULL;
  }

  anp=freeauthnames;
  freeauthnames=anp->next;

  return anp;
}

void freeauthname (authname *anp) {
  anp->next=freeauthnames;
  freeauthnames=anp;
}

int registerauthnameext(const char *name) {
  int i;

  if (findauthnameext(name)!=-1) {
    Error("nick",ERR_WARNING,"Tried to register duplicate authname extension %s",name);
    return -1;
  }

  for (i=0;i<MAXAUTHNAMEEXTS;i++) {
    if (authnameextnames[i]==NULL) {
      authnameextnames[i]=getsstring(name,100);
      return i;
    }
  }

  Error("nick",ERR_WARNING,"Tried to register too many authname extensions: %s",name);
  return -1;
}

int findauthnameext(const char *name) {
  int i;

  for (i=0;i<MAXAUTHNAMEEXTS;i++) {
    if (authnameextnames[i]!=NULL && !ircd_strcmp(name,authnameextnames[i]->content)) {
      return i;
    }
  }

  return -1;
}

void releaseauthnameext(int index) {
  int i;
  authname *anp;

  freesstring(authnameextnames[index]);
  authnameextnames[index]=NULL;

  for (i=0;i<AUTHNAMEHASHSIZE;i++) {
    for (anp=authnametable[i];anp;anp=anp->next) {
      anp->exts[index]=NULL;
    }
  }
}

authname *findauthname(unsigned long userid) {
  authname *anp;

  if(!userid)
    return NULL;

  for (anp=authnametable[authnamehash(userid)];anp;anp=(authname *)anp->next)
    if (userid==anp->userid)
      return anp;

  return NULL;
}

authname *findorcreateauthname(unsigned long userid) {
  authname *anp;
  unsigned int thehash=authnamehash(userid);

  if(!userid)
    return NULL;

  for (anp=authnametable[thehash];anp;anp=(authname *)anp->next)
    if (userid==anp->userid)
      return anp;

  anp=newauthname();
  anp->userid=userid;
  anp->usercount=0;
  anp->marker=0;
  anp->nicks=NULL;
  memset(anp->exts, 0, MAXAUTHNAMEEXTS * sizeof(void *));
  anp->next=(struct authname *)authnametable[thehash];
  authnametable[thehash]=anp;

  return anp;
}

void releaseauthname(authname *anp) {
  authname **manp;
  int i;
  if (anp->usercount==0) {
    for(i=0;i<MAXAUTHNAMEEXTS;i++)
      if(anp->exts[i]!=NULL)
        return;

    for(manp=&(authnametable[authnamehash(anp->userid)]);*manp;manp=(authname **)&((*manp)->next)) {
      if ((*manp)==anp) {
        (*manp)=(authname *)anp->next;
        freeauthname(anp);
        return;
      }
    }
    Error("nick",ERR_ERROR,"Unable to remove authname %lu from hashtable",anp->userid);
  }
}

unsigned int nextauthnamemarker() {
  int i;
  authname *anp;
  static unsigned int authnamemarker=0;

  authnamemarker++;
  if (!authnamemarker) {
    /* If we wrapped to zero, zap the marker on all records */
    for (i=0;i<AUTHNAMEHASHSIZE;i++)
      for (anp=authnametable[i];anp;anp=anp->next)
        anp->marker=0;
    authnamemarker++;
  }

  return authnamemarker;
}
