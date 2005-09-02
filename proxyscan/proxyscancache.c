/*
 * proxyscancache.c:
 *  This file deals with the cache of known hosts, clean or otherwise.
 */

#include "proxyscan.h"
#include <time.h>
#include <stdio.h>
#include "../core/error.h"
#include <string.h>

#define HOSTCACHEHASHSIZE     50000

cachehost *cachetable[HOSTCACHEHASHSIZE];
time_t cleanscaninterval;
time_t dirtyscaninterval;

void cachehostinit(time_t ri) {
  cleanscaninterval=ri;
  dirtyscaninterval=ri*7;
  memset(cachetable,0,sizeof(cachetable));
}

cachehost *addcleanhost(unsigned long IP, time_t timestamp) {
  cachehost *chp;
  int hash;

  hash=(IP%HOSTCACHEHASHSIZE);

  chp=getcachehost();
  chp->IP=IP;
  chp->lastscan=timestamp;
  chp->proxies=NULL;
  chp->glineid=0;
  chp->next=cachetable[hash];
  
  cachetable[hash]=chp;
  
  return chp;
}

void delcachehost(cachehost *chp) {
  cachehost **chh;
  foundproxy *fpp, *nfpp;
  int hash;

  hash=(chp->IP%HOSTCACHEHASHSIZE);

  for (chh=&(cachetable[hash]);*chh;chh=&((*chh)->next)) {
    if (*chh==chp) {
      *chh=chp->next;
      break;
    }
  }
  
  for (fpp=chp->proxies;fpp;fpp=nfpp) {
    nfpp=fpp->next;
    freefoundproxy(fpp);
  }
  freecachehost(chp);
}

/*
 * Returns a cachehost * for the named IP
 */

cachehost *findcachehost(unsigned long IP) {
  int hash;
  cachehost *chp;

  hash=(IP%HOSTCACHEHASHSIZE);

  for (chp=cachetable[hash];chp;chp=chp->next) {
    if (chp->IP==IP) {
      /* match */
      if(chp->lastscan < (time(NULL)-(chp->proxies ? dirtyscaninterval : cleanscaninterval))) {
	/* Needs rescan; delete and return 1 */
	delcachehost(chp);
	return NULL;
      } else {
        /* valid: return it */
	return chp;
      }
    }
  }

  /* Not found: return NULL */
  return NULL;
}

/* 
 * dumpcleanhosts:
 *  Dumps all cached hosts to a savefile.  Expires hosts as it goes along
 */

void dumpcachehosts(void *arg) {
  int i;
  FILE *fp;
  cachehost *chp,*nchp;
  time_t now=time(NULL);
  foundproxy *fpp;

  if ((fp=fopen("cleanhosts","w"))==NULL) {
    Error("proxyscan",ERR_ERROR,"Unable to open cleanhosts file for writing!");
    return;
  }

  for(i=0;i<HOSTCACHEHASHSIZE;i++) {
    for(chp=cachetable[i];chp;chp=nchp) {
      nchp=chp->next;
      if (chp->proxies) {
        if (chp->lastscan < (now-dirtyscaninterval)) {
          delcachehost(chp);
          continue;
        }
        
        for (fpp=chp->proxies;fpp;fpp=fpp->next) 
          fprintf(fp, "%lu %lu %u %i %u\n",chp->IP,chp->lastscan,chp->glineid,fpp->type,fpp->port);
      } else {
        if (chp->lastscan < (now-cleanscaninterval)) {
          /* Needs rescan anyway, so delete it */
	  delcachehost(chp);
	  continue;
        }
        fprintf(fp,"%lu %lu\n",chp->IP,chp->lastscan);
      }
    }
  }

  fclose(fp);
}

/*
 * loadcleanhosts:
 *  Loads clean hosts in from database. 
 */

void loadcachehosts() {
  FILE *fp;
  unsigned long IP,timestamp,glineid,ptype,pport;
  char buf[512];
  cachehost *chp=NULL;
  foundproxy *fpp;
  int res;
  
  if ((fp=fopen("cleanhosts","r"))==NULL) {
    Error("proxyscan",ERR_ERROR,"Unable to open cleanhosts file for reading!");
    return;
  }

  while (!feof(fp)) {
    fgets(buf,512,fp);
    if (feof(fp)) {
      break;
    }

    res=sscanf(buf,"%lu %lu %lu %lu %lu",&IP,&timestamp,&glineid,&ptype,&pport);

    if (res<2)
      continue;

    if (!chp || (chp->IP != IP))
      chp=addcleanhost(IP, timestamp);
      
    if (res==5) {
      chp->glineid=glineid;
      fpp=getfoundproxy();
      fpp->type=ptype;
      fpp->port=pport;
      fpp->next=chp->proxies;
      chp->proxies=fpp;
    }
  }
}

/*
 * cleancount:
 *  Returns the number of "clean" host entries present 
 */

unsigned int cleancount() {
  int i;
  unsigned int total=0;
  cachehost *chp;

  for(i=0;i<HOSTCACHEHASHSIZE;i++) {
    for (chp=cachetable[i];chp;chp=chp->next) {
      if (!chp->proxies)
        total++;
    }
  }

  return total;
}

unsigned int dirtycount() {
  int i;
  unsigned int total=0;
  cachehost *chp;

  for(i=0;i<HOSTCACHEHASHSIZE;i++) {
    for (chp=cachetable[i];chp;chp=chp->next) {
      if (chp->proxies)
        total++;
    }
  }

  return total;
}

/* 
 * scanall:
 *  Scans all hosts on the network for a given proxy, and updates the cache accordingly
 */

void scanall(int type, int port) {
  int i;
  cachehost *chp, *nchp;
  nick *np;
  unsigned int hostmarker;

  hostmarker=nexthostmarker();

  for (i=0;i<HOSTCACHEHASHSIZE;i++)
    for (chp=cachetable[i];chp;chp=chp->next)
      chp->marker=0;

  for (i=0;i<NICKHASHSIZE;i++) {
    for (np=nicktable[i];np;np=np->next) {
      if (np->host->marker==hostmarker)
	continue;

      np->host->marker=hostmarker;

      if ((chp=findcachehost(np->ipaddress)))
	chp->marker=1;

      queuescan(np->ipaddress, type, port, SCLASS_NORMAL, 0);
    }
  }

  for (i=0;i<HOSTCACHEHASHSIZE;i++) {
    for (chp=cachetable[i];chp;chp=nchp) {
      nchp=chp->next;
      if (!chp->proxies && !chp->marker)
	delcachehost(chp);
    }
  }
}
