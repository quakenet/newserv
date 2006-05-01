/* nickhelpers.c */

#include "nick.h"
#include "../lib/flags.h"
#include "../lib/irc_string.h"
#include "../irc/irc_config.h"
#include "../core/error.h"
#include "../lib/sstring.h"

#include <string.h>

#define hosthash(x)       ((crc32i(x))%HOSTHASHSIZE)
#define realnamehash(x)   ((crc32(x))%REALNAMEHASHSIZE)

unsigned int hostmarker;
unsigned int realnamemarker;
unsigned int nickmarker;

host *hosttable[HOSTHASHSIZE];
realname *realnametable[REALNAMEHASHSIZE];

void initnickhelpers() {
  memset(hosttable,0,sizeof(hosttable));
  memset(realnametable,0,sizeof(realnametable));
}

host *findhost(const char *hostname) {
  host *hp;
  for (hp=hosttable[hosthash(hostname)];hp;hp=(host *)hp->next) {
    if (!ircd_strcmp(hostname,hp->name->content))
      return hp;
  }    
  return NULL;
}

host *findorcreatehost(const char *hostname) {
  host *hp;
  unsigned long thehash=hosthash(hostname);
  
  for (hp=hosttable[thehash];hp;hp=(host *)hp->next)
    if (!ircd_strcmp(hostname,hp->name->content)) {
      hp->clonecount++;
      return hp;
    }
  
  hp=newhost();
  hp->name=getsstring(hostname,HOSTLEN);
  hp->clonecount=1;
  hp->marker=0;
  hp->nicks=NULL;
  hp->next=(struct host *)hosttable[thehash];
  hosttable[thehash]=hp;
  
  return hp;
}

void releasehost(host *hp) {
  host **mhp;
  if (--(hp->clonecount)==0) {
    for(mhp=&(hosttable[hosthash(hp->name->content)]);*mhp;mhp=(host **)&((*mhp)->next)) {
      if ((*mhp)==hp) {
        (*mhp)=(host *)hp->next;
        freesstring(hp->name);
        freehost(hp);
        return;
      }
    }
    Error("nick",ERR_ERROR,"Unable to remove host %s from hashtable",hp->name->content);
  }
}

realname *findrealname(const char *name) {
  realname *rnp;

  for (rnp=realnametable[realnamehash(name)];rnp;rnp=(realname *)rnp->next)
    if (!strcmp(name,rnp->name->content))
      return rnp;
      
  return NULL;
}

realname *findorcreaterealname(const char *name) {
  realname *rnp;
  unsigned int thehash=realnamehash(name);

  for (rnp=realnametable[thehash];rnp;rnp=(realname *)rnp->next)
    if (!strcmp(name,rnp->name->content)) {
      rnp->usercount++;
      return rnp;
    }
  
  rnp=newrealname();
  rnp->name=getsstring(name,REALLEN);
  rnp->usercount=1;
  rnp->marker=0;
  rnp->nicks=NULL;
  rnp->next=(struct realname *)realnametable[thehash];
  realnametable[thehash]=rnp;
  
  return rnp;
}

void releaserealname(realname *rnp) {
  realname **mrnp;
  if (--(rnp->usercount)==0) {
    for(mrnp=&(realnametable[realnamehash(rnp->name->content)]);*mrnp;mrnp=(realname **)&((*mrnp)->next)) {
      if ((*mrnp)==rnp) {
        (*mrnp)=(realname *)rnp->next;
        freesstring(rnp->name);
        freerealname(rnp);
        return;
      }
    }
    Error("nick",ERR_ERROR,"Unable to remove realname %s from hashtable",rnp->name->content);
  }
}

unsigned int nexthostmarker() {	
  int i;
  host *hp;
  
  hostmarker++;
  if (!hostmarker) {
    /* If we wrapped to zero, zap the marker on all hosts */
    for (i=0;i<HOSTHASHSIZE;i++)
      for (hp=hosttable[i];hp;hp=hp->next)
        hp->marker=0;
    hostmarker++;
  }
  
  return hostmarker;
}

unsigned int nextrealnamemarker() {
  int i;
  realname *rnp;
  
  realnamemarker++;
  if (!realnamemarker) {
    /* If we wrapped to zero, zap the marker on all records */
    for (i=0;i<REALNAMEHASHSIZE;i++)
      for (rnp=realnametable[i];rnp;rnp=rnp->next) 
        rnp->marker=0;
    realnamemarker++;
  }
  
  return realnamemarker;
}

unsigned int nextnickmarker() {
  int i;
  nick *np;
  
  nickmarker++;
  
  if (!nickmarker) {
    /* If we wrapped to zero, zap the marker on all records */
    for (i=0;i<NICKHASHSIZE;i++)
      for (np=nicktable[i];np;np=np->next)
        np->marker=0;
    nickmarker++;
  }
  
  return nickmarker;
}
