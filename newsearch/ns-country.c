/*
 * COUNTRY functionality
 */

#include "newsearch.h"

#include <stdlib.h>

#include "../geoip/geoip.h"

void *country_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput);
void country_free(searchCtx *ctx, struct searchNode *thenode);

struct searchNode *country_parse(searchCtx *ctx, int argc, char **argv) {
  struct searchNode *thenode;

  if (!(thenode=(struct searchNode *)malloc(sizeof (struct searchNode)))) {
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }

  thenode->returntype = RETURNTYPE_STRING;
  thenode->localdata = NULL;
  thenode->exe = country_exe;
  thenode->free = country_free;

  return thenode;
}

void *country_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput) {
  nick *np = (nick *)theinput;

  struct country *c = geoip_lookup_nick(np);
  if (!c) {
    return ""; // will cast to FALSE
  }

  const char *code = geoip_code(c);
  if (!code) {
    return "";
  }

  return (void *)code;
}

void country_free(searchCtx *ctx, struct searchNode *thenode) {
  free(thenode);
}
