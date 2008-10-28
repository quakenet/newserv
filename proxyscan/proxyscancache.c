/*
 * proxyscancache.c:
 *  This file deals with the cache of known hosts, clean or otherwise.
 */

#include "proxyscan.h"
#include <time.h>
#include <stdio.h>
#include "../core/error.h"
#include <string.h>

time_t cleanscaninterval;
time_t dirtyscaninterval;

void cachehostinit(time_t ri) {
  cleanscaninterval=ri;
  dirtyscaninterval=ri*7;
}

cachehost *addcleanhost(time_t timestamp) {
  cachehost *chp;

  chp=getcachehost();
  chp->lastscan=timestamp;
  chp->proxies=NULL;
  chp->glineid=0;
  
  return chp;
}

void delcachehost(cachehost *chp) {
  foundproxy *fpp, *nfpp;

  for (fpp=chp->proxies;fpp;fpp=nfpp) {
    nfpp=fpp->next;
    freefoundproxy(fpp);
  }
  freecachehost(chp);
}

/*
 * Returns a cachehost * for the named IP
 */

cachehost *findcachehost(patricia_node_t *node) {
  cachehost *chp;

  if( (cachehost *)node->exts[ps_cache_ext] ) {
    chp = (cachehost *)node->exts[ps_cache_ext];
    if(chp->lastscan < (time(NULL)-(chp->proxies ? dirtyscaninterval : cleanscaninterval))) {
      /* Needs rescan; delete and return 1 */
      delcachehost(chp);
      derefnode(iptree,node);
      node->exts[ps_cache_ext] = NULL;
      return NULL;
    } else {
      /* valid: return it */
      return chp;
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
  FILE *fp;
  cachehost *chp;
  time_t now=time(NULL);
  foundproxy *fpp;
  patricia_node_t *node;

  if ((fp=fopen("data/cleanhosts","w"))==NULL) {
    Error("proxyscan",ERR_ERROR,"Unable to open cleanhosts file for writing!");
    return;
  }

  PATRICIA_WALK_CLEAR (iptree->head, node) {
    if (node->exts[ps_cache_ext] ) {
      chp = (cachehost *) node->exts[ps_cache_ext];
      if (chp) { 
        if (chp->proxies) {
          if (chp->lastscan < (now-dirtyscaninterval)) {
            derefnode(iptree,node); 
	    delcachehost(chp);
  	    node->exts[ps_cache_ext] = NULL;
          } else
        
          for (fpp=chp->proxies;fpp;fpp=fpp->next) 
            fprintf(fp, "%s %lu %u %i %u\n",IPtostr(node->prefix->sin),chp->lastscan,chp->glineid,fpp->type,fpp->port);
        } else {
          if (chp->lastscan < (now-cleanscaninterval)) {
            /* Needs rescan anyway, so delete it */
	    derefnode(iptree,node); 
	    delcachehost(chp);
            node->exts[ps_cache_ext] = NULL;          
          } else
          fprintf(fp,"%s %lu\n",IPtostr(node->prefix->sin),chp->lastscan);
        }
      }
    }
  } PATRICIA_WALK_CLEAR_END;

//  patricia_tidy_tree(iptree); 

  fclose(fp);
}

/*
 * loadcleanhosts:
 *  Loads clean hosts in from database. 
 */

void loadcachehosts() {
  FILE *fp;
  unsigned long timestamp,glineid,ptype,pport;
  char buf[512];
  cachehost *chp=NULL;
  foundproxy *fpp;
  char ip[512];
  int res;
  struct irc_in_addr sin;
  unsigned char bits;
  patricia_node_t *node;
  int i=0;

  if ((fp=fopen("data/cleanhosts","r"))==NULL) {
    Error("proxyscan",ERR_ERROR,"Unable to open cleanhosts file for reading!");
    return;
  }


  while (!feof(fp)) {
    fgets(buf,512,fp);
    if (feof(fp)) {
      break;
    }

    res=sscanf(buf,"%s %lu %lu %lu %lu",ip,&timestamp,&glineid,&ptype,&pport);

    if (res<2)
      continue;

    if (0 == ipmask_parse(ip,&sin, &bits)) {
      /* invalid mask */
    } else {
      node = refnode(iptree, &sin, bits);
      if( node ) {
        i++;
        chp=addcleanhost(timestamp);
        node->exts[ps_cache_ext] = chp;
      
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
  }
 
  Error("proxyscan",ERR_INFO, "Loaded %d entries from cache", i); 
}

/*
 * cleancount:
 *  Returns the number of "clean" host entries present 
 */

unsigned int cleancount() {
  unsigned int total=0;
  cachehost *chp;
  patricia_node_t *head, *node;
  head = iptree->head;
  PATRICIA_WALK (head, node) {
    if ( node->exts[ps_cache_ext] ) {
      chp = (cachehost *) node->exts[ps_cache_ext];

      if (!chp->proxies)
        total++;
    }
  } PATRICIA_WALK_END;

  return total;
}

unsigned int dirtycount() {
  unsigned int total=0;
  cachehost *chp;
  patricia_node_t *node;

  PATRICIA_WALK (iptree->head, node) {
    if ( node->exts[ps_cache_ext] ) {
      chp = (cachehost *) node->exts[ps_cache_ext];
      if (chp->proxies)
        total++;
    }
  } PATRICIA_WALK_END;

  return total;
}

/* 
 * scanall:
 *  Scans all hosts on the network for a given proxy, and updates the cache accordingly
 */

void scanall(int type, int port) {
  int i;
  cachehost *chp;
  nick *np;
  unsigned int hostmarker;
  patricia_node_t *node;

  hostmarker=nexthostmarker();

  PATRICIA_WALK (iptree->head, node) {
    if ( node->exts[ps_cache_ext] ) {
      chp = (cachehost *) node->exts[ps_cache_ext];
      chp->marker=0;
    }
  } PATRICIA_WALK_END; 

  for (i=0;i<NICKHASHSIZE;i++) {
    for (np=nicktable[i];np;np=np->next) {
      if (np->host->marker==hostmarker)
	continue;

      np->host->marker=hostmarker;

      if (!irc_in_addr_is_ipv4(&np->p_ipaddr))
        continue;

      if ((chp=findcachehost(np->ipnode)))
	chp->marker=1;

      queuescan(np->ipnode, type, port, SCLASS_NORMAL, 0);
    }
  }
}
