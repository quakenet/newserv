/*
 * HOSTMASK functionality
 */

#include "newsearch.h"

#include <stdio.h>
#include <stdlib.h>

#include "../irc/irc_config.h"
#include "../lib/irc_string.h"

void *hostmask_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput);
void *hostmask_exe_rn(searchCtx *ctx, struct searchNode *thenode, void *theinput);
void *hostmask_exe_rh(searchCtx *ctx, struct searchNode *thenode, void *theinput);
void *hostmask_exe_rn_rh(searchCtx *ctx, struct searchNode *thenode, void *theinput);
void hostmask_free(searchCtx *ctx, struct searchNode *thenode);

struct searchNode *hostmask_parse(searchCtx *ctx, int argc, char **argv) {
  struct searchNode *thenode;
  int realhost = 0, realname = 0, i;
  
  for(i=0;i<argc;i++) {
    if(!ircd_strcmp(argv[i], "realhost")) {
      realhost = 1;
    } else if(!ircd_strcmp(argv[i], "realname")) {
      realname = 1;
    } else {
      parseError = "bad argument: use realhost and/or realname";
      return NULL;
    }
  }

  if (!(thenode=(struct searchNode *)malloc(sizeof (struct searchNode)))) {
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }

  thenode->returntype = RETURNTYPE_STRING;
  if (!(thenode->localdata = (void *)malloc(HOSTLEN+USERLEN+NICKLEN+REALLEN+10))) {
    /* couldn't malloc() memory for thenode->localdata, so free thenode to avoid leakage */
    parseError = "malloc: could not allocate memory for this search.";
    free(thenode);
    return NULL;
  }
  thenode->free = hostmask_free;

  if(realname) {
    if(realhost) {
      thenode->exe = hostmask_exe_rn_rh;
    } else {
      thenode->exe = hostmask_exe_rn;
    }
  } else {
    if(realhost) {
      thenode->exe = hostmask_exe_rh;
    } else {
      thenode->exe = hostmask_exe;
    }
  }

  return thenode;
}

void *hostmask_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput) {
  nick *np = (nick *)theinput;
  char *buf = thenode->localdata;

  return visiblehostmask(np, buf);
}

void *hostmask_exe_rh(searchCtx *ctx, struct searchNode *thenode, void *theinput) {
  nick *np = (nick *)theinput;
  char *buf = thenode->localdata;

  sprintf(buf,"%s!%s@%s",np->nick,np->ident,np->host->name->content);

  return buf;
}

void *hostmask_exe_rn_rh(searchCtx *ctx, struct searchNode *thenode, void *theinput) {
  nick *np = (nick *)theinput;
  char *buf = thenode->localdata;

  sprintf(buf,"%s!%s@%s\r%s",np->nick,np->ident,np->host->name->content,np->realname->name->content);

  return buf;
}

void *hostmask_exe_rn(searchCtx *ctx, struct searchNode *thenode, void *theinput) {
  nick *np = (nick *)theinput;
  char *buf = thenode->localdata;

  sprintf(buf,"%s\r%s",visiblehostmask(np, buf),np->realname->name->content);

  return buf;
}

void hostmask_free(searchCtx *ctx, struct searchNode *thenode) {
  free(thenode->localdata);
  free(thenode);
}

