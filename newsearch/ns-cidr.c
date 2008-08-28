/*
 * CIDR functionality
 */

#include "newsearch.h"

#include <stdio.h>
#include <stdlib.h>

#include "../irc/irc_config.h"
#include "../lib/irc_string.h"
#include "../lib/irc_ipv6.h"

struct cidr_localdata {
  unsigned int ip;
  unsigned int mask;
};

void *cidr_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput);
void cidr_free(searchCtx *ctx, struct searchNode *thenode);

struct searchNode *cidr_parse(searchCtx *ctx, int argc, char **argv) {
  struct searchNode *thenode;
  struct cidr_localdata *c;
  unsigned char mask;
  struct irc_in_addr ip;

  if((argc != 1) || !ipmask_parse(argv[0], &ip, &mask)) {
    parseError = "usage: cidr ip/mask";
    return NULL;
  }

  if(!irc_in_addr_is_ipv4(&ip)) {
    parseError = "cidr: sorry, no IPv6 yet";
    return NULL;
  }

  /* ??? */
  mask-=(128-32);
  if(mask > 32) {
    parseError = "cidr: bad mask supplied";
    return NULL;
  }

  if (!(thenode=(struct searchNode *)malloc(sizeof (struct searchNode)))) {
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }

  thenode->returntype = RETURNTYPE_BOOL;
  if (!(c = malloc(sizeof(struct cidr_localdata)))) {
    /* couldn't malloc() memory for thenode->localdata, so free thenode to avoid leakage */
    parseError = "malloc: could not allocate memory for this search.";
    free(thenode);
    return NULL;
  }

  if(!mask) {
    c->mask = 0;
  } else if(mask < 32) {
    c->mask = 0xffffffff << (32 - mask);
  }

  c->ip = irc_in_addr_v4_to_int(&ip) & c->mask;

  thenode->localdata = (void *)c;
  thenode->free = cidr_free;
  thenode->exe = cidr_exe;

  return thenode;
}

void *cidr_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput) {
  nick *np = (nick *)theinput;
  struct cidr_localdata *c = thenode->localdata;
  unsigned int ip;
  struct irc_in_addr *sin;

  sin = &np->ipnode->prefix->sin;
  if(!irc_in_addr_is_ipv4(sin))
    return (void *)0;

  ip = irc_in_addr_v4_to_int(sin);
  if((ip & c->mask) == c->ip)
    return (void *)1;

  return (void *)0;
}

void cidr_free(searchCtx *ctx, struct searchNode *thenode) {
  free(thenode->localdata);
  free(thenode);
}
