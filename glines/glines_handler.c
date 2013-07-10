#include <stdio.h>
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
#include "glines.h"

/*
  <prefix> GL <target> [!][+|-|>|<]<mask> [<expiration>] [<lastmod>] [<lifetime>] [:<reason>]
*/

int handleglinemsg(void *source, int cargc, char **cargv) {
  char *sender = (char *)source;
  char *target;
  char *mask;
  char *reason;
  char *creator;
  time_t lifetime = 0, expire = 0, lastmod = 0;
  unsigned int flags = 0;
  int numlen; 
  long creatornum;
  gline *agline;
  nick *np;

  /**
   * Valid GL tokens have X params:
   * 2: Local modification to a local gline - ignored by services
   * 5: 1.3.x GL Token
   * 6: 1.4.x GL Token - adds lifetime parameter
   */
  switch ( cargc ) {
    case 2:
      /* local modification which we reject later */
      break;
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

  /**
   * Gline creator can be either a nick or a server
   * 2 long numeric = server
   * 5 long number = client
   */
  numlen = strlen(source);
  switch (numlen) {
    case 2:
      creatornum = numerictolong(sender, 2);
      if (creatornum < 0) {
        Error("gline", ERR_WARNING, "Failed to resolve numeric (%s) to server when adding G-Line from network!", sender);
        creator = "unknown";
      } else 
        if (serverlist[(int)creatornum].name==NULL) {
          Error("gline", ERR_WARNING, "Received gline from non-existant server");
          return CMD_ERROR; 
        } else {
          creator = serverlist[(int)creatornum].name->content, HOSTLEN;
        }
      break;
    case 5:
      creatornum = numerictolong(sender, 5);
      if (!(np = getnickbynumeric(creatornum))) {
        Error("gline", ERR_WARNING, "Failed to resolve numeric (%s) to nick when adding G-Line from network!", sender);
        creator = "unknown";
      } else
        creator = np->nick;
      break;
    default:
      Error("gline", ERR_WARNING, "Invalid numeric '%s' in G-Line message.", sender);
      return CMD_ERROR;
  }

  /* 1st param is target */
  target = cargv[0];

  /* anything other than a global G-Line is irrelevant */
  if (strcmp(target, "*")) {
    Error("gline", ERR_WARNING, "Received local g-line from %s - do they realise we're a service?", sender);
    return CMD_ERROR;
  }

  /* 2nd param is mask */
  mask = cargv[1];

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
      Error("gline", ERR_WARNING, "Received local modification from %s - do they realise we're a service?", sender);
      return CMD_ERROR;
    case '!':
      Error("gline", ERR_WARNING, "Received 'forced gline' token from %s", sender);
      return CMD_ERROR;
  } 

  if (flags & GLINE_ACTIVATE) {
    /* activate gline */
    expire = abs_expire(atoi(cargv[2]));
    switch (cargc) {
      case 4:
        /* asuka U:d, no lastmod */
        reason = cargv[3];
        break;
      case 5:
        /*asuka lastmod */
        lastmod = atoi(cargv[3]);
        lifetime = gline_max(lastmod,expire); /* set lifetime = lastmod */ 
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

    Error("debuggline", ERR_WARNING, "GL Received: Creator %s, Mask %s, Reason %s, Expire %lu, Lastmod %lu, Lifetime %lu", creator, mask, reason, expire, lastmod, lifetime);   

    if ( (agline=gline_find(mask)) ) {
      if (agline->flags & GLINE_ACTIVE ) { 
        Error("debuggline", ERR_WARNING, "Duplicate Gline recieved for %s - old lastmod %lu, expire %lu, lifetime %lu, reason %s, creator %s", mask, agline->lastmod, agline->expire, agline->lifetime, agline->reason->content, agline->creator->content  );
      } else {
        /* we're reactivating a gline - check lastmod then assume the new gline is authoritive */
        if ( lastmod > agline->lastmod ) {
          agline->lastmod = lastmod;
          agline->expire = expire;
          agline->lifetime = lifetime;
          agline->creator = getsstring(creator, 255);
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
      gline_add( creator, mask, reason, expire, lastmod, lifetime); 
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
       expire = abs_expire(atoi(cargv[2]));
       switch (cargc) {
         case 4:
           /* asuka U:d, no lastmod */
           reason = cargv[3];
           break;
         case 5:
           /*asuka lastmod */
           lastmod = atoi(cargv[3]);
           lifetime = gline_max(lastmod,expire); /* set lifetime = lastmod */
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

      if ((agline = gline_add( creator, mask, reason, expire, lastmod, lifetime)))
        gline_deactivate(agline, lastmod, 0);
      else
        Error("gline", ERR_WARNING, "gline_add failed");
      return CMD_ERROR;
    }
  } else {
    /* modification - only snircd 1.4.x */
    if ((agline = gline_find(mask))) {
      expire = abs_expire(atoi(cargv[2]));
      lastmod = atoi(cargv[3]);
      lifetime = atoi(cargv[4]);
      reason = cargv[5];

      if ( lastmod > agline->lastmod ) {
        agline->lastmod = lastmod;
        agline->expire = expire;
        agline->lifetime = lifetime;
        agline->creator = getsstring(creator, 255);
        freesstring(agline->reason);
        agline->reason = getsstring(reason, 255);
      } else {
        Error("debuggline", ERR_WARNING, "received a gline modification with a lower lastmod");
        /* @@@TODO resend our gline ? */
      }
      return CMD_OK;
    } else {
      Error("gline", ERR_WARNING, "Received modification for gline that does not exist for mask %s", mask);
      return CMD_ERROR;
    }
  }
  return CMD_OK;
}


void handleglinestats(int hooknum, void *arg) {
  if((long)arg > 10) {
    char message[100];
    int glcount = 0;
    gline *gl;

    for(gl=glinelist;gl;gl=gl->next) {
      glcount++;
    }

    snprintf(message, sizeof(message), "Glines  :%7d glines", glcount);
    triggerhook(HOOK_CORE_STATSREPLY, message);
  }
}

