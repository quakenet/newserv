/*
 * MODES functionality
 */

#include "newsearch.h"

#include <stdio.h>
#include <stdlib.h>

struct modes_localdata {
  flag_t      setmodes;
  flag_t      clearmodes;
}; 

void *modes_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput);
void modes_free(searchCtx *ctx, struct searchNode *thenode);

struct searchNode *modes_parse(searchCtx *ctx, int argc, char **argv) {
  struct modes_localdata *localdata;
  struct searchNode *thenode, *modestring;
  const flag *flaglist;
  char *p;
  
  if (argc!=1) {
    parseError="modes: usage: modes (mode string)";
    return NULL;
  }
    
  if (ctx->searchcmd == reg_chansearch) {
    flaglist=cmodeflags;
  } else if (ctx->searchcmd == reg_nicksearch) {
    flaglist=umodeflags;
  } else {
    parseError="modes: unsupported search type";
    return NULL;
  }

  if (!(localdata=(struct modes_localdata *)malloc(sizeof(struct modes_localdata)))) {
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }
  
  localdata->setmodes=0;
  localdata->clearmodes = ~0;
  
  if (!(modestring=argtoconststr("modes", ctx, argv[0], &p))) {
    free(localdata);
    return NULL;
  }
  
  setflags(&(localdata->setmodes), 0xFFFF, p, flaglist, REJECT_NONE);
  setflags(&(localdata->clearmodes), 0xFFFF, p, flaglist, REJECT_NONE);
  (modestring->free)(ctx, modestring);
  
  localdata->clearmodes = ~(localdata->clearmodes);
  
  if (!(thenode=(struct searchNode *)malloc(sizeof(struct searchNode)))) {
    /* couldn't malloc() memory for thenode, so free localdata to avoid leakage */
    parseError = "malloc: could not allocate memory for this search.";
    free(localdata);
    return NULL;
  }
  
  thenode->returntype  = RETURNTYPE_BOOL;
  thenode->localdata   = (void *)localdata;
  thenode->exe         = modes_exe;
  thenode->free        = modes_free;

  return thenode;
}

void *modes_exe(searchCtx *ctx, struct searchNode *thenode, void *value) {
  struct modes_localdata *localdata;
  nick *np;
  chanindex *cip;
  flag_t flags;
  
  localdata = (struct modes_localdata *)thenode->localdata;

  if (ctx->searchcmd == reg_chansearch) {
    cip=(chanindex *)value;
    if (!cip->channel)
      return NULL;
    flags=cip->channel->flags;
  } else if (ctx->searchcmd == reg_nicksearch) {
    np=(nick *)value;
    flags=np->umodes;
  } else {
    return NULL;
  }

  if (~flags & (localdata->setmodes))
    return (void *)0;

  if (flags & (localdata->clearmodes))
    return (void *)0;
  
  return (void *)1;
}

void modes_free(searchCtx *ctx, struct searchNode *thenode) {
  free (thenode->localdata);
  free (thenode);
}
