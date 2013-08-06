/*
 * CIDR functionality
 */

#include "newsearch.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../irc/irc_config.h"
#include "../lib/irc_string.h"
#include "../lib/irc_ipv6.h"

struct cidr_localdata {
  struct irc_in_addr ip;
  unsigned char bits;
};

void *cidr_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput);
void cidr_free(searchCtx *ctx, struct searchNode *thenode);

struct searchNode *cidr_parse(searchCtx *ctx, int argc, char **argv) {
  struct searchNode *thenode, *convsn;
  struct cidr_localdata *c;
  struct irc_in_addr ip;
  unsigned char bits;
  char *p;
  int ret;
  
  if(argc != 1) {
    parseError = "usage: cidr ip/mask";
    return NULL;
  }

  if (!(convsn=argtoconststr("cidr", ctx, argv[0], &p)))
    return NULL;
  
  ret = ipmask_parse(p, &ip, &bits);
  convsn->free(ctx, convsn);
  
  if(!ret) {
    parseError = "usage: cidr ip/mask";
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

  memcpy(&c->ip, &ip, sizeof(struct irc_in_addr));
  c->bits = bits;

  thenode->localdata = (void *)c;
  thenode->free = cidr_free;
  thenode->exe = cidr_exe;

  return thenode;
}

void *cidr_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput) {
  nick *np = (nick *)theinput;
  struct cidr_localdata *c = thenode->localdata;

  if(!ipmask_check(&np->ipaddress, &c->ip, c->bits))
    return (void *)0;

  return (void *)1;
}

void cidr_free(searchCtx *ctx, struct searchNode *thenode) {
  free(thenode->localdata);
  free(thenode);
}
