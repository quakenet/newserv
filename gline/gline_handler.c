#include <string.h>

#include "../control/control.h"
#include "../nick/nick.h"
#include "../localuser/localuserchannel.h"
#include "../core/hooks.h"
#include "../server/server.h"
#include "../parser/parser.h"
#include "../core/schedule.h"
#include "../lib/array.h"
#include "../lib/base64.h"
#include "../lib/irc_string.h"
#include "../lib/splitline.h"
#include "gline.h"

/*
  <prefix> GL <target> [!][+|-|>|<]<mask> [<expiration>] [<lastmod>]
        [<lifetime>] [:<reason>]
*/

int handleglinemsg(void* source, int cargc, char** cargv) {
  char* sender = (char*)source;
  char* target;
  char* mask;
  char* reason;
  sstring *creator;
  time_t lifetime = 0, expires = 0, lastmod = 0;
  unsigned int flags = 0;
  int numlen; 
  long creatornum;
  gline *agline;

  nick* np;

  int i;
  for (i = 0; i <cargc; i++) {
    Error("debuggline", ERR_WARNING, "Token %d is - %s", i, cargv[i]);
  }

  /* valid GL tokens have X params */
  switch ( cargc ) {
    case 5:
      /* 1.3.x GL Token */
      break;
    case 6:
      /* 1.4.0 and above GL token */
      break;
    default: 
      Error("gline", ERR_WARNING, "Invalid number of arguments in GL token (%d)", cargc);
      return CMD_ERROR;
  }

  /* gline creator can be either a nick or a server */
  numlen = strlen(source);
  switch (numlen) {
    case 2:
      creatornum = numerictolong(sender, 2);
      if (creatornum < 0) {
        Error("gline", ERR_WARNING, "Failed to resolve numeric to server (%s) when adding G-Line!", sender);
        creator = getsstring("unknown", HOSTLEN);
      } else 
        creator = getsstring(serverlist[(int)creatornum].name->content, HOSTLEN);
      break;
    case 5:
      creatornum = numerictolong(sender, 5);
      if (!(np = getnickbynumeric(creatornum))) {
        Error("gline", ERR_WARNING, "Failed to resolve numeric to nick when adding G-Line!", sender);
        creator = getsstring("unknown", HOSTLEN);
      } else
        creator = getsstring( np->nick, NICKLEN);
      break;
    default:
      Error("gline", ERR_WARNING, "Invalid numeric '%s' in G-Line message.", sender);
  }

  /* 1st param is target */
  target = cargv[0];

  /* 2nd param is mask */
  mask = cargv[1];

  if (*mask == '!') {
    Error("gline", ERR_DEBUG, " forced flag ");
    mask++;
  }

  switch (*mask) {
    case '+':
      flags |= GLINE_ACTIVATE;
      mask++;
      break;
    case '-':
      flags |= GLINE_DEACTIVATE;
      mask++;
      break;
    case '>':
    case '<':
      Error("gline", ERR_WARNING, "Received local modification from %s.", sender);
      return CMD_ERROR;
  } 

  /* anything other than a global G-Line is irrelevant */
  if (strcmp(target, "*")) {
    Error("gline", ERR_WARNING, "Received local g-line from %s - do they realise we're a service?", sender);
    return CMD_ERROR;
  }

  if (flags & GLINE_ACTIVATE) {
    /* activate gline */
    expires = abs_expire(atoi(cargv[2]));
    switch (cargc) {
      case 4:
        /* asuka U:d, no lastmod */
        reason = cargv[3];
        break;
      case 5:
        /*asuka lastmod */
        lastmod = atoi(cargv[3]);
        lifetime = gline_max(lastmod,expires); /* set lifetime = lastmod */ 
        reason = cargv[4];
        break;
      case 6:
        /* snircd */
        lastmod = atoi(cargv[3]);
        lifetime = atoi(cargv[4]); 
        reason = cargv[5];
        break;
      default:
         Error("gline", ERR_WARNING, "Gline Activate with invalid number (%d) of arguments", cargc);
         return CMD_ERROR;
    }

    Error("debuggline", ERR_WARNING, "GL Received: Creator %s, Mask %s, Reason %s, Expire %lu, Lastmod %lu, Lifetime %lu", creator->content, mask, reason, expires, lastmod, lifetime);   
    if ( (agline=gline_find(mask)) ) {
      if (agline->flags & GLINE_ACTIVE ) { 
        Error("debuggline", ERR_WARNING, "Duplicate Gline recieved for %s - old lastmod %lu, expire %lu, lifetime %lu, reason %s, creator %s", mask, agline->lastmod, agline->expires, agline->lifetime, agline->reason->content, agline->creator->content  );
      } else {
        /* we're reactivating a gline - check lastmod then assume the new gline is authoritive */
        if ( lastmod > agline->lastmod ) {
          agline->lastmod = lastmod;
          agline->expires = expires;
          agline->lifetime = lifetime;
          agline->creator = creator;
          freesstring(agline->reason);
          agline->reason = getsstring(reason, 255); 
          agline->flags |= GLINE_ACTIVE;
        } else {
          Error("debuggline", ERR_WARNING, "received a gline with a lower lastmod");
          /* @@@TODO resend our gline ? */
        } 
      }
      /* TODO */
      return CMD_ERROR;
    } else {
      gline_add( creatornum, creator, mask, reason, expires, lastmod, lifetime); 
    } 
  } else if (flags & GLINE_DEACTIVATE) {
    /* deactivate gline */
    if ((agline = gline_find(mask))) {
      switch (cargc) {
        case 2:
          /* asuka U:d, no last mod */
          removegline(agline);
          break;
        case 5:
          /* asuka last mod */
          lastmod = atoi(cargv[3]);
          gline_deactivate(agline, lastmod, 0);
          break;
        case 6:
          /* snircd */
          lastmod = atoi(cargv[3]);
          gline_deactivate(agline, lastmod, 0);
          break;
        default:
          Error("gline", ERR_WARNING, "Gline Deactivate with invalid number (%d) of arguments", cargc);
          return CMD_ERROR;
      }
      return CMD_OK;
    } else {
       Error("gline", ERR_WARNING, "Gline addition - adding deactivated gline - mask not found (%s)", mask);
       expires = abs_expire(atoi(cargv[2]));
       switch (cargc) {
         case 4:
           /* asuka U:d, no lastmod */
           reason = cargv[3];
           break;
         case 5:
           /*asuka lastmod */
           lastmod = atoi(cargv[3]);
           lifetime = gline_max(lastmod,expires); /* set lifetime = lastmod */
           reason = cargv[4];
           break;
         case 6:
           /* snircd */
           lastmod = atoi(cargv[3]);
           lifetime = atoi(cargv[4]);
           reason = cargv[5];
           break;
         default:
           Error("gline", ERR_WARNING, "Gline DeActivate with invalid number (%d) of arguments", cargc);
           return CMD_ERROR;
      }

      agline = gline_add( creatornum, creator, mask, reason, expires, lastmod, lifetime);
      gline_deactivate(agline, lastmod, 0);       
      return CMD_ERROR;
    }
  } else {
    /* modification - only snircd 1.4.x */
    Error("gline", ERR_WARNING, "Not Implemented");
  }

  Error("debuggline", ERR_WARNING, "Creator %s", creator->content);
  return CMD_OK;
}


int gline_deactivate(gline *agline, time_t lastmod, int propagate) {
  agline->flags &= ~GLINE_ACTIVE;
  agline->lastmod = lastmod;
}
