/*
 * CUMODECOUNT functionality
 */

#include "newsearch.h"

#include <stdio.h>
#include <stdlib.h>

struct cumodecount_localdata {
  long setmodes;
  long clearmodes;
  struct searchNode *xnode;
}; 

#define CU_OP    0x01
#define CU_VOICE 0x02
#define CU_ALL   0x03

const flag cumodecountlist[] = {
  { 'o', 1 },
  { 'v', 2 },
  { '\0', 0 }
};

void *cumodecount_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput);
void cumodecount_free(searchCtx *ctx, struct searchNode *thenode);

struct searchNode *cumodecount_parse(searchCtx *ctx, int argc, char **argv) {
  struct cumodecount_localdata *localdata;
  struct searchNode *thenode, *flagstr;
  flag_t fset, fclear;
  char *p;

  if (argc!=1) {
    parseError="cumodes: usage: cumodecount (mode string)";
    return NULL;
  }

  if (!(localdata=(struct cumodecount_localdata *)malloc(sizeof(struct cumodecount_localdata)))) {
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }

  fset=0;
  fclear=~0;

  if (!(flagstr=argtoconststr("cumodecount", ctx, argv[0], &p))) {
    localdata->xnode->free(ctx, localdata->xnode);
    free(localdata);
    return NULL;
  }

  setflags(&(fset), CU_ALL, p, cumodecountlist, REJECT_NONE);
  setflags(&(fclear), CU_ALL, p, cumodecountlist, REJECT_NONE);
  flagstr->free(ctx, flagstr);

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
    free(localdata);
    return NULL;
  }

  thenode->returntype  = RETURNTYPE_INT;
  thenode->localdata   = (void *)localdata;
  thenode->exe         = cumodecount_exe;
  thenode->free        = cumodecount_free;

  return thenode;
}

void *cumodecount_exe(searchCtx *ctx, struct searchNode *thenode, void *value) {
  struct cumodecount_localdata *localdata;
  chanindex *cip = (chanindex *)value;
  int i, count;
  unsigned long flags;

  if(!cip->channel || !cip->channel->users)
    return (void *)0;

  localdata = (struct cumodecount_localdata *)thenode->localdata;
  count = 0;

  for (i=0;i<cip->channel->users->hashsize;i++) {
    if (cip->channel->users->content[i]!=nouser) {
      flags = cip->channel->users->content[i];

      if (~flags & (localdata->setmodes))
        continue;
      else if (flags & (localdata->clearmodes))
        continue;

      count++;
    }
  }

  return (void *)(long)count;
}

void cumodecount_free(searchCtx *ctx, struct searchNode *thenode) {
  free (thenode->localdata);
  free (thenode);
}
