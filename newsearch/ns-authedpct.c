/*
 * authedpct functionality 
 */

#include "newsearch.h"

#include <stdio.h>
#include <stdlib.h>

void *authedpct_exe(struct searchNode *thenode, void *theinput);
void authedpct_free(struct searchNode *thenode);

struct searchNode *authedpct_parse(int type, int argc, char **argv) {
  struct searchNode *thenode;

  if (type != SEARCHTYPE_CHANNEL) {
    parseError = "authedpct: this function is only valid for channel searches.";
    return NULL;
  }

  if (!(thenode=(struct searchNode *)malloc(sizeof(struct searchNode)))) {
    /* couldn't malloc() memory for thenode, so free localdata to avoid leakage */
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }

  thenode->returntype = RETURNTYPE_INT;
  thenode->localdata = NULL;
  thenode->exe = authedpct_exe;
  thenode->free = authedpct_free;

  return thenode;
}

void *authedpct_exe(struct searchNode *thenode, void *theinput) {
  int i;
  int j=0;
  nick *np;
  chanindex *cip = (chanindex *)theinput;
  struct authedpct_localdata *localdata;

  localdata = thenode->localdata;
  
  if (!cip->channel)
    return (void *)0;
  
  for (i=0;i<cip->channel->users->hashsize;i++) {
    if (cip->channel->users->content[i]==nouser)
      continue;
    
    if ((np=getnickbynumeric(cip->channel->users->content[i])) && IsAccount(np))
      j++;
  }

  return (void *)(long)((j * 100) / cip->channel->users->totalusers);
}  

void authedpct_free(struct searchNode *thenode) {
  free(thenode);
}

