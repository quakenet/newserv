/*
 * ipv6 functionality 
 */

#include "patriciasearch.h"

#include <stdio.h>
#include <stdlib.h>

void *ps_ipv6_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput);
void ps_ipv6_free(searchCtx *ctx, struct searchNode *thenode);

struct searchNode *ps_ipv6_parse(searchCtx *ctx, int argc, char **argv) {
  struct searchNode *thenode;

  if (!(thenode=(struct searchNode *)malloc(sizeof (struct searchNode)))) {
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }

  thenode->returntype = RETURNTYPE_BOOL;
  thenode->localdata = NULL;
  thenode->exe = ps_ipv6_exe;
  thenode->free = ps_ipv6_free;

  return thenode;
}

void *ps_ipv6_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput) {
  patricia_node_t *node = (patricia_node_t *)theinput;

  if (irc_in_addr_is_ipv4(&node->prefix->sin))
    return (void *)0;
  
  return (void *)1;
}

void ps_ipv6_free(searchCtx *ctx, struct searchNode *thenode) {
  free(thenode);
}

