/*
 * CONCAT functionality
 */

#include "newsearch.h"
#include "../lib/irc_string.h"

#include <stdio.h>
#include <stdlib.h>

#define BUFLEN 1024

struct concat_localdata {
  char buf[BUFLEN];
  int count;
  struct searchNode **nodes;
};

void concat_free(searchCtx *ctx, struct searchNode *thenode);
void *concat_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput);

struct searchNode *concat_parse(searchCtx *ctx, int argc, char **argv) {
  struct concat_localdata *localdata;
  struct searchNode *thenode;
  int i;

  if (!(localdata = (struct concat_localdata *)malloc(sizeof(struct concat_localdata)))) {
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }
  if (!(localdata->nodes = (struct searchNode **)malloc(sizeof(struct searchNode *) * argc))) {
    /* couldn't malloc() memory for localdata->nodes, so free localdata to avoid leakage */
    parseError = "malloc: could not allocate memory for this search.";
    free(localdata);
    return NULL;
  }
  localdata->count = 0;

  if (!(thenode = (struct searchNode *)malloc(sizeof(struct searchNode)))) {
    /* couldn't malloc() memory for thenode, so free localdata and localdata->nodes to avoid leakage */
    parseError = "malloc: could not allocate memory for this search.";
    free(localdata->nodes);
    free(localdata);
    return NULL;
  }

  thenode->localdata  = localdata;
  thenode->returntype = RETURNTYPE_STRING;
  thenode->exe  = concat_exe;
  thenode->free = concat_free;

  for (i=0;i<argc;i++) {
    /* Parse the node.. */
    localdata->nodes[i] = ctx->parser(ctx, argv[i]);

    /* Subsequent nodes get coerced to match the type of the first node */
    if (i)
      localdata->nodes[i]=coerceNode(ctx,localdata->nodes[i],RETURNTYPE_STRING);

    /* If a node didn't parse, give up */
    if (!localdata->nodes[i]) {
      concat_free(ctx, thenode);
      return NULL;
    }

    localdata->count++;
  }

  return thenode;
}

void concat_free(searchCtx *ctx, struct searchNode *thenode) {
  struct concat_localdata *localdata;
  int i;

  localdata=thenode->localdata;

  for (i=0;i<localdata->count;i++) {
    if (localdata->nodes[i])
      (localdata->nodes[i]->free)(ctx, localdata->nodes[i]);
  }

  free(localdata->nodes);
  free(localdata);
  free(thenode);
}

void *concat_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput) {
  struct concat_localdata *localdata;
  char buf2[BUFLEN];

  localdata=thenode->localdata;
  localdata->buf[0] = '\0';

  for (int i=0;i<localdata->count;i++) {
    char *buf = (char *)(localdata->nodes[i]->exe)(ctx, localdata->nodes[i], theinput);
    /* HACK */
    snprintf(buf2, BUFLEN, "%s%s", localdata->buf, buf);
    snprintf(localdata->buf, BUFLEN, "%s", buf2);
  }

  return localdata->buf;
}

