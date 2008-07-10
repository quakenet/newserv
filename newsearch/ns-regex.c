/*
 * REGEX functionality
 */

#include "newsearch.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pcre.h>

struct regex_localdata {
  struct searchNode *targnode;
  struct searchNode *patnode;
  pcre *pcre;
  pcre_extra *pcre_extra;
};

void *regex_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput);
void regex_free(searchCtx *ctx, struct searchNode *thenode);

struct searchNode *regex_parse(searchCtx *ctx, int argc, char **argv) {
  struct regex_localdata *localdata;
  struct searchNode *thenode;
  struct searchNode *targnode, *patnode;
  const char *err;
  int erroffset;
  pcre *pcre;
  pcre_extra *pcre_extra;

  if (argc<2) {
    parseError="regex: usage: regex source pattern";
    return NULL;
  }

  targnode=ctx->parser(ctx, argv[0]);
  if (!(targnode = coerceNode(ctx,targnode, RETURNTYPE_STRING)))
    return NULL;

  patnode=ctx->parser(ctx, argv[1]);
  if (!(patnode = coerceNode(ctx,patnode, RETURNTYPE_STRING))) {
    (targnode->free)(ctx, targnode);
    return NULL;
  }

  if (!(patnode->returntype & RETURNTYPE_CONST)) {
    parseError="regex: only constant regexes allowed";
    (targnode->free)(ctx, targnode);
    (patnode->free)(ctx, patnode);
    return NULL;
  }

  if (!(pcre=pcre_compile((char *)(patnode->exe)(ctx, patnode,NULL),
			  PCRE_CASELESS, &err, &erroffset, NULL))) {
    parseError=err;
    return NULL;
  }

  pcre_extra=pcre_study(pcre, 0, &err);

  if (!(localdata=(struct regex_localdata *)malloc(sizeof (struct regex_localdata)))) {
    parseError = "malloc: could not allocate memory for this search.";
    /* make sure we pcre_free() if we're not going to use them */
    if (pcre_extra)
      pcre_free(pcre_extra);

    if (pcre)
      pcre_free(pcre);
    return NULL;
  }
    
  localdata->targnode=targnode;
  localdata->patnode=patnode;
  localdata->pcre=pcre;
  localdata->pcre_extra=pcre_extra;

  if (!(thenode = (struct searchNode *)malloc(sizeof (struct searchNode)))) {
    /* couldn't malloc() memory for thenode, so free the pcre's and localdata to avoid leakage */
    parseError = "malloc: could not allocate memory for this search.";
    if (localdata->pcre_extra)
      pcre_free(localdata->pcre_extra);

    if (localdata->pcre)
      pcre_free(localdata->pcre);

    free(localdata);
    return NULL;
  }

  thenode->returntype = RETURNTYPE_BOOL;
  thenode->localdata = localdata;
  thenode->exe = regex_exe;
  thenode->free = regex_free;

  return thenode;
}

void *regex_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput) {
  struct regex_localdata *localdata;
  char *target;

  localdata = thenode->localdata;
  
  target  = (char *)((localdata->targnode->exe)(ctx, localdata->targnode,theinput));

  if (pcre_exec(localdata->pcre, localdata->pcre_extra,target,strlen(target),0,
		0,NULL,0)) {
    /* didn't match */
    return (void *)0;
  } else {
    return (void *)1;
  }
}

void regex_free(searchCtx *ctx, struct searchNode *thenode) {
  struct regex_localdata *localdata;

  localdata=thenode->localdata;

  if (localdata->pcre_extra)
    pcre_free(localdata->pcre_extra);

  if (localdata->pcre)
    pcre_free(localdata->pcre);

  (localdata->patnode->free)(ctx, localdata->patnode);
  (localdata->targnode->free)(ctx, localdata->targnode);
  free(localdata);
  free(thenode);
}

