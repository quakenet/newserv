/*
 * SERVER functionality
 */

#include "newsearch.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../irc/irc_config.h"
#include "../lib/irc_string.h"
#include "../core/modules.h"
#include "../server/server.h"

void *server_exe_bool(searchCtx *ctx, struct searchNode *thenode, void *theinput);
void *server_exe_str(searchCtx *ctx, struct searchNode *thenode, void *theinput);
void server_free(searchCtx *ctx, struct searchNode *thenode);

static int ext;

struct searchNode *server_parse(searchCtx *ctx, int argc, char **argv) {
  struct searchNode *thenode;
  int i;
  long numeric;

  if (!(thenode=(struct searchNode *)malloc(sizeof (struct searchNode)))) {
    parseError = "malloc: could not allocate memory for this search.";
    return NULL;
  }

  if (argc>0) {
    struct searchNode *servername;
    char *p;
    
    if (!(servername=argtoconststr("server", ctx, argv[0], &p))) {
      free(thenode);
      return NULL;
    }
     
    numeric = -1;
    for(i=0;i<MAXSERVERS;i++) {
      sstring *n = serverlist[i].name;
      if(n && !strcmp(n->content, p)) {
        numeric = i;
        break;
      }
    }

    (servername->free)(ctx, servername);
    
    if(numeric == -1) {
      parseError = "server: server not found.";
      free(thenode);
      return NULL;
    }

    thenode->returntype = RETURNTYPE_BOOL;
    thenode->localdata = (void *)numeric;

    thenode->exe = server_exe_bool;
  } else {
    thenode->returntype = RETURNTYPE_STRING;
    thenode->localdata = NULL;

    thenode->exe = server_exe_str;
  }

  thenode->free = server_free;

  return thenode;
}

void *server_exe_bool(searchCtx *ctx, struct searchNode *thenode, void *theinput) {
  nick *np = (nick *)theinput;
  long server = (long)thenode->localdata;

  if(homeserver(np->numeric) == server)
    return (void *)1;

  return (void *)0;
}

void *server_exe_str(searchCtx *ctx, struct searchNode *thenode, void *theinput) {
  nick *np = (nick *)theinput;
  sstring *n = serverlist[homeserver(np->numeric)].name;

  if(!n)
    return "";

  return n->content;
}

void server_free(searchCtx *ctx, struct searchNode *thenode) {
  free(thenode);
}
