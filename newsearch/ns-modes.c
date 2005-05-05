/*
 * MODES functionality
 */

#include "newsearch.h"

#include <stdio.h>
#include <stdlib.h>

#include "../nick/nick.h"
#include "../channel/channel.h"
#include "../lib/flags.h"

struct modes_localdata {
  int         type;
  flag_t      setmodes;
  flag_t      clearmodes;
}; 

void *modes_exe(struct searchNode *thenode, int type, void *theinput);
void modes_free(struct searchNode *thenode);

struct searchNode *modes_parse(int type, int argc, char **argv) {
  struct modes_localdata *localdata;
  struct searchNode *thenode;
  const flag *flaglist;

  if (argc!=1) {
    parseError="modes: usage: modes (mode string)";
    return NULL;
  }
    
  switch (type) {
  case SEARCHTYPE_CHANNEL:
    flaglist=cmodeflags;
    break;

  case SEARCHTYPE_NICK:
    flaglist=umodeflags;
    break;

  default:
    parseError="modes: unsupported search type";
    return NULL;
  }

  localdata=(struct modes_localdata *)malloc(sizeof(struct modes_localdata));
  
  localdata->type=type;
  localdata->setmodes=0;
  localdata->clearmodes = ~0;
  
  setflags(&(localdata->setmodes), 0xFFFF, argv[0], flaglist, REJECT_NONE);
  setflags(&(localdata->clearmodes), 0xFFFF, argv[0], flaglist, REJECT_NONE);

  localdata->clearmodes = ~(localdata->clearmodes);
  
  thenode=(struct searchNode *)malloc(sizeof(struct searchNode));
  
  thenode->returntype  = RETURNTYPE_BOOL;
  thenode->localdata   = (void *)localdata;
  thenode->exe         = modes_exe;
  thenode->free        = modes_free;

  return thenode;
}

void *modes_exe(struct searchNode *thenode, int type, void *value) {
  struct modes_localdata *localdata;
  nick *np;
  chanindex *cip;
  flag_t flags;
  
  localdata = (struct modes_localdata *)thenode->localdata;

  switch (localdata->type) {
  case SEARCHTYPE_CHANNEL:
    cip=(chanindex *)value;
    if (!cip->channel)
      return NULL;
    flags=cip->channel->flags;
    break;

  case SEARCHTYPE_NICK:
    np=(nick *)value;
    flags=np->umodes;
    break;

  default:
    return NULL;
  }

  if (~flags & (localdata->setmodes))
    return falseval(type);

  if (flags & (localdata->clearmodes))
    return falseval(type);
  
  return trueval(type);
}

void modes_free(struct searchNode *thenode) {
  free (thenode->localdata);
  free (thenode);
}
