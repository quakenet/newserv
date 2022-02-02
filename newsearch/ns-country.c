/*
 * COUNTRY functionality
 */

#include "newsearch.h"

#include <stdio.h>
#include <stdlib.h>

#include "../irc/irc_config.h"
#include "../lib/irc_string.h"
#include "../geoip/geoip.h"
#include "../core/modules.h"

void *country_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput);
void country_free(searchCtx *ctx, struct searchNode *thenode);

static int ext;

struct searchNode *country_parse(searchCtx *ctx, int argc, char **argv) {
  struct searchNode *thenode, *countrysn;
  GeoIP_LookupCode l;
  long target;
  char *p;
  
  if (argc<1) {
    parseError = "country: usage: country <country 2 character ISO code>";
    return NULL;
  }

  ext = findnickext("geoip");
  if(ext == -1) {
    parseError = "country: Geoip module not loaded.";
    return NULL;
  }
  
  l = ndlsym("geoip", "geoip_lookupcode");
  if(!l) {
    parseError = "unable to lookup geoip function pointer";
    return NULL;
  }
  
  if (!(countrysn=argtoconststr("country", ctx, argv[0], &p)))
    return NULL;
    
  target = l(p);
  countrysn->free(ctx, countrysn);
  
  if(target == -1) {
    parseError = "country: invalid country.";
    return NULL;
  }

  if (!(thenode=(struct searchNode *)malloc(sizeof (struct searchNode)))) {
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }

  thenode->returntype = RETURNTYPE_BOOL;
  thenode->localdata = (void *)target;
  thenode->exe = country_exe;
  thenode->free = country_free;

  return thenode;
}

void *country_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput) {
  nick *np = (nick *)theinput;
  long country = (long)thenode->localdata, rc = (long)np->exts[ext];

  if(country == rc)
    return (void *)1;

  return (void *)0;
}

void country_free(searchCtx *ctx, struct searchNode *thenode) {
  free(thenode);
}
