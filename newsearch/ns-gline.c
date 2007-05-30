/*
 * GLINE functionality 
 */

#include "newsearch.h"

#include <stdio.h>
#include <stdlib.h>

#include "../control/control.h" /* controlreply() */
#include "../irc/irc.h" /* irc_send() */
#include "../lib/irc_string.h" /* IPtostr(), longtoduration(), durationtolong() */

/* used for *_free functions that need to warn users of certain things
   i.e. hitting too many users in a (kill) or (gline) - declared in newsearch.c */
extern const struct nick *senderNSExtern;

void *gline_exe(struct searchNode *thenode, int type, void *theinput);
void gline_free(struct searchNode *thenode);

struct gline_localdata {
  unsigned int marker;
  unsigned int duration;
  int count;
};

struct searchNode *gline_parse(int type, int argc, char **argv) {
  struct gline_localdata *localdata;
  struct searchNode *thenode;

  if (!(localdata = (struct gline_localdata *) malloc(sizeof(struct gline_localdata)))) {
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }
  localdata->count = 0;
  localdata->marker = nextnickmarker();

  /* gline durations */
  if (argc<1)
    localdata->duration = NSGLINE_DURATION;
  else {
    localdata->duration = durationtolong(argv[0]);
    /* error checking on gline duration */
    if (localdata->duration == 0)
      localdata->duration = NSGLINE_DURATION;
  }

  if (!(thenode=(struct searchNode *)malloc(sizeof (struct searchNode)))) {
    /* couldn't malloc() memory for thenode, so free localdata to avoid leakage */
    parseError = "malloc: could not allocate memory for this search.";
    free(localdata);
    return NULL;
  }

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
            irc_send("%s GL * +*@%s %u :You (%s!%s@%s) have been glined for violating our terms of service.", 
              mynumeric->content, IPtostr(np->ipaddress), localdata->duration, np->nick, np->ident, IPtostr(np->ipaddress));
          else
            irc_send("%s GL * +%s@%s %u :You (%s!%s@%s) have been glined for violating our terms of service.", 
              mynumeric->content, np->ident, IPtostr(np->ipaddress), localdata->duration, np->nick, np->ident, IPtostr(np->ipaddress));
        }
        else
            safe++;
      }
    }
  }
  if (safe)
    controlreply(senderNSExtern, "Warning: your pattern matched privileged users (%d in total) - these have not been touched.", safe);
  /* notify opers of the action */
  controlwall(NO_OPER, NL_GLINES, "%s/%s glined %d %s via nicksearch for %s [%d untouched].", senderNSExtern->nick, senderNSExtern->authname, (localdata->count - safe), 
    (localdata->count - safe) != 1 ? "users" : "user", longtoduration(localdata->duration, 1), safe);
  free(localdata);
  free(thenode);
}
