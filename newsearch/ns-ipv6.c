/*
 * ipv6 functionality 
 */

#include "newsearch.h"

#include <stdio.h>
#include <stdlib.h>

void *ipv6_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput);
void ipv6_free(searchCtx *ctx, struct searchNode *thenode);

struct searchNode *ipv6_parse(searchCtx *ctx, int argc, char **argv) {
  struct searchNode *thenode;

  if (!(thenode=(struct searchNode *)malloc(sizeof (struct searchNode)))) {
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }

  thenode->returntype = RETURNTYPE_BOOL;
  thenode->localdata = NULL;
  thenode->exe = ipv6_exe;
  thenode->free = ipv6_free;

  return thenode;
}

void *ipv6_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput) {
  nick *np = (nick *)theinput;

  if (irc_in_addr_is_ipv4(&np->p_ipaddr))
    return (void *)0;
  
  return (void *)1;
}

void ipv6_free(searchCtx *ctx, struct searchNode *thenode) {
  free(thenode);
}

