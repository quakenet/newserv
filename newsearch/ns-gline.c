/*
 * GLINE functionality 
 */

#include "newsearch.h"

#include <stdio.h>
#include <stdlib.h>

#include "../control/control.h" /* controlreply() */
#include "../irc/irc.h" /* irc_send() */
#include "../lib/irc_string.h" /* IPtostr() */

/* used for *_free functions that need to warn users of certain things
   i.e. hitting too many users in a (kill) or (gline) - declared in newsearch.c */
extern const struct nick *senderNSExtern;

void *gline_exe(struct searchNode *thenode, int type, void *theinput);
void gline_free(struct searchNode *thenode);

struct gline_localdata {
  unsigned int marker;
  int count;
  searchNode **nodes;
};

struct searchNode *gline_parse(int type, int argc, char **argv) {
  struct gline_localdata *localdata;
  struct searchNode *thenode;

  localdata = (struct gline_localdata *) malloc(sizeof(struct gline_localdata));
  localdata->nodes = (struct searchNode **) malloc(sizeof(struct searchNode *) * argc);
  localdata->count = 0;
  localdata->marker = nextnickmarker();

  thenode=(struct searchNode *)malloc(sizeof (struct searchNode));

  thenode->returntype = RETURNTYPE_BOOL;
  thenode->localdata = localdata;
  thenode->exe = gline_exe;
  thenode->free = gline_free;

  return thenode;
}

void *gline_exe(struct searchNode *thenode, int type, void *theinput) {
  struct gline_localdata *localdata;
  nick *np = (nick *)theinput;

  localdata = thenode->localdata;

  np->marker = localdata->marker;
  localdata->count++;

  switch (type) {
    case RETURNTYPE_INT:
    case RETURNTYPE_BOOL:
      return (void *)1;
    case RETURNTYPE_STRING:
     return "1";
  }
  return NULL;
}

void gline_free(struct searchNode *thenode) {
  struct gline_localdata *localdata;
  nick *np, *nnp;
  int i, safe=0;

  localdata = thenode->localdata;

  if (localdata->count > NSMAX_GLINE_LIMIT) {
    /* need to warn the user that they have just tried to twat half the network ... */
    controlreply(senderNSExtern, "Warning: your pattern matches too many users (%d) - nothing done.", localdata->count);
    free(localdata->nodes);
    free(localdata);
    free(thenode);
    return;
  }

  for (i=0;i<NICKHASHSIZE;i++) {
    for (np=nicktable[i];np;np=nnp) {
      nnp = np->next;
      if (np->marker == localdata->marker) {
        if (!IsOper(np) && !IsService(np) && !IsXOper(np)) {
          if (np->ident[0] == '~')
            irc_send("%s GL * +*@%s %u 1 :You (%s!%s@%s) have been glined for violating our terms of service.", 
              mynumeric->content, IPtostr(np->ipaddress), NSGLINE_DURATION, np->nick, np->ident, IPtostr(np->ipaddress));
          else
            irc_send("%s GL * +%s@%s %u 1 :You (%s!%s@%s) have been glined for violating our terms of service.", 
              mynumeric->content, np->ident, IPtostr(np->ipaddress), NSGLINE_DURATION, np->nick, np->ident, IPtostr(np->ipaddress));
        }
        else
            safe++;
      }
    }
  }
  if (safe)
    controlreply(senderNSExtern, "Warning: your pattern matched privileged users (%d in total) - these have not been touched.", safe);
  free(localdata->nodes);
  free(localdata);
  free(thenode);
}
