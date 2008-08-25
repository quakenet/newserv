/*
 * CUMODES functionality
 */

#include "newsearch.h"

#include <stdio.h>
#include <stdlib.h>

struct cumodes_localdata {
  long setmodes;
  long clearmodes;
  struct searchNode *xnode;
}; 

#define CU_OP    0x01
#define CU_VOICE 0x02
#define CU_ALL   0x03

const flag cumodelist[] = {
  { 'o', 1 },
  { 'v', 2 },
  { '\0', 0 }
};

void *cumodes_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput);
void cumodes_free(searchCtx *ctx, struct searchNode *thenode);

struct searchNode *cumodes_parse(searchCtx *ctx, int argc, char **argv) {
  struct cumodes_localdata *localdata;
  struct searchNode *thenode;
  flag_t fset, fclear;

  if (argc!=2) {
    parseError="cumodes: usage: cumodes (string) (mode string)";
    return NULL;
  }
    
  if (!(localdata=(struct cumodes_localdata *)malloc(sizeof(struct cumodes_localdata)))) {
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }

  localdata->xnode = ctx->parser(ctx, argv[0]);
  if (!(localdata->xnode = coerceNode(ctx, localdata->xnode, RETURNTYPE_STRING))) {
    free(localdata);
    return NULL;
  }

  fset=0;
  fclear=~0;
  
  setflags(&(fset), CU_ALL, argv[1], cumodelist, REJECT_NONE);
  setflags(&(fclear), CU_ALL, argv[1], cumodelist, REJECT_NONE);

  localdata->setmodes=0;
  localdata->clearmodes=~0;

  if(fset & CU_OP)
    localdata->setmodes|=CUMODE_OP;
  if(fset & CU_VOICE)
    localdata->setmodes|=CUMODE_VOICE;
  if(!(fclear & CU_OP))
    localdata->clearmodes&=~CUMODE_OP;
  if(!(fclear & CU_VOICE))
    localdata->clearmodes&=~CUMODE_VOICE;

  localdata->clearmodes = ~(localdata->clearmodes);
  
  if (!(thenode=(struct searchNode *)malloc(sizeof(struct searchNode)))) {
    /* couldn't malloc() memory for thenode, so free localdata to avoid leakage */
    parseError = "malloc: could not allocate memory for this search.";
    (localdata->xnode->free)(ctx, localdata->xnode);
    free(localdata);
    return NULL;
  }
  
  thenode->returntype  = RETURNTYPE_BOOL;
  thenode->localdata   = (void *)localdata;
  thenode->exe         = cumodes_exe;
  thenode->free        = cumodes_free;

  return thenode;
}

void *cumodes_exe(searchCtx *ctx, struct searchNode *thenode, void *value) {
  struct cumodes_localdata *localdata;
  nick *np = (nick *)value;
  channel *cp;
  char *channame;
  unsigned long *lp, flags;

  localdata = (struct cumodes_localdata *)thenode->localdata;

  /* MEGA SLOW */
  channame = (char *)(localdata->xnode->exe) (ctx, localdata->xnode, value);
  cp = findchannel(channame);
  if(!cp)
    return (void *)0;

  lp = getnumerichandlefromchanhash(cp->users,np->numeric);
  if(!lp)
    return (void *)0;

  flags = *lp;

  if (~flags & (localdata->setmodes))
    return (void *)0;

  if (flags & (localdata->clearmodes))
    return (void *)0;
  
  return (void *)1;
}

void cumodes_free(searchCtx *ctx, struct searchNode *thenode) {
  struct cumodes_localdata *localdata = (struct cumodes_localdata *)thenode->localdata;
  (localdata->xnode->free)(ctx, localdata->xnode);
  free (thenode->localdata);
  free (thenode);
}
