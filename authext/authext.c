#include "authext.h"
#include "../core/nsmalloc.h"
#include "../core/error.h"
#include "../lib/sstring.h"
#include "../lib/irc_string.h"
#include "../nick/nick.h"
#include "../core/hooks.h"
#include "../lib/strlfunc.h"
#include "../lib/version.h"

#include <string.h>
#include <stdio.h>

MODULE_VERSION("")

#define ALLOCUNIT 100

#define authnamehash(x)   ((x)%AUTHNAMEHASHSIZE)
#define authnamehashbyname(x) (crc32i(x)%AUTHNAMEHASHSIZE)

authname *freeauthnames;
authname *authnametable[AUTHNAMEHASHSIZE];

/* internal access only */
static authname *authnametablebyname[AUTHNAMEHASHSIZE];

static struct {
  sstring *name;
  int persistent;
} authnameexts[MAXAUTHNAMEEXTS];

static void authextstats(int hooknum, void *arg);

void _init(void) {
  freeauthnames=NULL;
  memset(authnametable,0,sizeof(authnametable));
  memset(authnametablebyname,0,sizeof(authnametablebyname));
  registerhook(HOOK_CORE_STATSREQUEST, &authextstats);
}

void _fini(void) {
  deregisterhook(HOOK_CORE_STATSREQUEST, &authextstats);
  nsfreeall(POOL_AUTHEXT);
}

authname *newauthname(void) {
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

int registerauthnameext(const char *name, int persistent) {
  int i;

  if (findauthnameext(name)!=-1) {
    Error("nick",ERR_WARNING,"Tried to register duplicate authname extension %s",name);
    return -1;
  }

  for (i=0;i<MAXAUTHNAMEEXTS;i++) {
    if (authnameexts[i].name==NULL) {
      authnameexts[i].name=getsstring(name,100);
      authnameexts[i].persistent=persistent;
      return i;
    }
  }

  Error("nick",ERR_WARNING,"Tried to register too many authname extensions: %s",name);
  return -1;
}

int findauthnameext(const char *name) {
  int i;

  for (i=0;i<MAXAUTHNAMEEXTS;i++) {
    if (authnameexts[i].name!=NULL && !ircd_strcmp(name,authnameexts[i].name->content)) {
      return i;
    }
  }

  return -1;
}

void releaseauthnameext(int index) {
  int i;
  authname *anp;

  freesstring(authnameexts[index].name);
  authnameexts[index].name=NULL;

  for (i=0;i<AUTHNAMEHASHSIZE;i++) {
    for (anp=authnametable[i];anp;anp=anp->next) {
      anp->exts[index]=NULL;
    }
  }

  /* the contents of authnametablebyname should be identical */
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

authname *findauthnamebyname(const char *name) {
  authname *anp;

  if(!name)
    return NULL;

  for (anp=authnametablebyname[authnamehashbyname(name)];anp;anp=(authname *)anp->nextbyname)
    if (!ircd_strcmp(anp->name, name))
      return anp;

  return NULL;
}

authname *findorcreateauthname(unsigned long userid, const char *name) {
  authname *anp;
  unsigned int thehash=authnamehash(userid), secondhash = authnamehashbyname(name);

  if(!userid || !name)
    return NULL;

  for (anp=authnametable[thehash];anp;anp=(authname *)anp->next)
    if (userid==anp->userid)
      return anp;

  anp=newauthname();
  anp->userid=userid;
  strlcpy(anp->name, name, sizeof(anp->name));
  anp->usercount=0;
  anp->marker=0;
  anp->flags=0;
  anp->nicks=NULL;
  memset(anp->exts, 0, MAXAUTHNAMEEXTS * sizeof(void *));
  anp->next=(struct authname *)authnametable[thehash];
  authnametable[thehash]=anp;

  anp->namebucket=secondhash;
  anp->nextbyname=(struct authname *)authnametablebyname[secondhash];
  authnametablebyname[secondhash]=anp;

  return anp;
}

void releaseauthname(authname *anp) {
  authname **manp;
  int i, found;
  if (anp->usercount==0) {
    anp->nicks = NULL;

    for(i=0;i<MAXAUTHNAMEEXTS;i++)
      if(authnameexts[i].persistent && anp->exts[i]!=NULL)
        return;

    triggerhook(HOOK_AUTH_LOSTAUTHNAME, (void *)anp);
    found = 0;
    for(manp=&(authnametable[authnamehash(anp->userid)]);*manp;manp=(authname **)&((*manp)->next)) {
      if ((*manp)==anp) {
        (*manp)=(authname *)anp->next;
        found = 1;
        break;
      }
    }
    if(!found) {
      Error("nick",ERR_ERROR,"Unable to remove authname %lu from hashtable",anp->userid);
      return;
    }

    for(manp=&(authnametablebyname[anp->namebucket]);*manp;manp=(authname **)&((*manp)->nextbyname)) {
      if ((*manp)==anp) {
        (*manp)=(authname *)anp->nextbyname;
        freeauthname(anp);
        return;
      }
    }

    Error("nick",ERR_STOP,"Unable to remove authname %lu from byname hashtable, TABLES ARE INCONSISTENT -- DYING",anp->userid);
  }
}

unsigned int nextauthnamemarker(void) {
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

authname *getauthbyname(const char *name) {
  authname *a = findauthnamebyname(name);
  if(!a || !a->nicks)
    return NULL;

  return a;
}

static char *genstats(authname **hashtable, authname *(nextfn)(authname *)) {
  int i,curchain,maxchain=0,total=0,buckets=0;
  authname *ap;
  static char buf[100];

  for (i=0;i<AUTHNAMEHASHSIZE;i++) {
    if (hashtable[i]!=NULL) {
      buckets++;
      curchain=0;
      for (ap=hashtable[i];ap;ap=nextfn(ap)) {
        total++;
        curchain++;
      }
      if (curchain>maxchain) {
        maxchain=curchain;
      }
    }
  }

  snprintf(buf, sizeof(buf), "%6d authexts (HASH: %6d/%6d, chain %3d)",total,buckets,AUTHNAMEHASHSIZE,maxchain);
  return buf;
}

static authname *nextbynext(authname *in) {
  return in->next;
}

static authname *nextbyname(authname *in) {
  return in->nextbyname;
}

static void authextstats(int hooknum, void *arg) {
  long level=(long)arg;
  char buf[100];

  if (level>5) {
    /* Full stats */
    snprintf(buf,sizeof(buf),"Authext : by id:   %s", genstats(authnametable, nextbynext));
    triggerhook(HOOK_CORE_STATSREPLY,buf);

    snprintf(buf,sizeof(buf),"Authext : by name: %s", genstats(authnametablebyname, nextbyname));
    triggerhook(HOOK_CORE_STATSREPLY,buf);
  }
}

