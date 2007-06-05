/*
 * COUNTRY functionality
 */

#include "newsearch.h"

#include <stdio.h>
#include <stdlib.h>

#include "../irc/irc_config.h"
#include "../lib/irc_string.h"

void *country_exe(struct searchNode *thenode, void *theinput);
void country_free(struct searchNode *thenode);

int ext;

struct searchNode *country_parse(int type, int argc, char **argv) {
  struct searchNode *thenode;

  if (type != SEARCHTYPE_NICK) {
    parseError = "country: this function is only valid for nick searches.";
    return NULL;
  }

  ext = findnickext("geoip");
  if(ext == -1) {
    parseError = "country: Geoip module not loaded.";
    return NULL;
  }
  

  if (!(thenode=(struct searchNode *)malloc(sizeof (struct searchNode)))) {
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }

  thenode->returntype = RETURNTYPE_INT;
  thenode->localdata = NULL;
  thenode->exe = country_exe;
  thenode->free = country_free;

  return thenode;
}

void *country_exe(struct searchNode *thenode, void *theinput) {
  nick *np = (nick *)theinput;

  return (void *)np->exts[ext];
}

void country_free(struct searchNode *thenode) {
  free(thenode);
}
