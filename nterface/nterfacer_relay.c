/*
  nterfacer relay4
  Copyright (C) 2004-2005 Chris Porter.

  v1.13
    - found a load of stuff on froo's box
  v1.12
    - added nickprefix
  v1.11
    - made sure stats buffer was checked (no security problem, just helps pauline)
  v1.10
    - added stats support
  v1.06
    - found \r bug, and another format string problem...
  v1.05
    - oops, fixed quit bug
    - oops, forgot about rehash hook
  v1.04
    - added S support, though it's gross
  v1.03
    - fixed audit found format string vunerability
  v1.02
    - split O username and password from .h file into config file
    - added rehash support
  v1.01
    - item->schedule was not unset and checked upon timeout
*/

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "../lib/irc_string.h"
#include "../lib/strlfunc.h"
#include "../core/schedule.h"
#include "../irc/irc.h"
#include "../core/config.h"
#include "library.h"
#include "nterfacer_relay.h"

struct service_node *node;
sstring *ousername, *opassword;
regex onickname;
int number = 1;
char started = 0;
char nickprefix[NICKLEN - 5 + 1];

int stats_handler(struct rline *ri, int argc, char **argv);

void _init(void) {
  if(!load_config())
    return;
  
  node = register_service("R");
  MemCheck(node);

  register_handler(node, "relay", 4, relay_handler);
  register_handler(node, "stats", 2, stats_handler);

  registerhook(HOOK_NICK_LOSTNICK, &relay_quits);
  registerhook(HOOK_IRC_DISCON, &relay_disconnect);
  registerhook(HOOK_CORE_REHASH, &relay_rehash);

  started = 1;
}

int load_config(void) {
  /* eww, I don't know how to make this any nicer! */
  sstring *ou, *op, *on, *opr;
  regex newregex;

  memset(&newregex, 0, sizeof(newregex));

  opr = getcopyconfigitem("nterfacer", "nickprefix", DEFAULT_NICK_PREFIX, sizeof(nickprefix) - 1); /* the terminator is included in nickprefix */
  strlcpy(nickprefix, (opr&&opr->content)?opr->content:DEFAULT_NICK_PREFIX, sizeof(nickprefix));
  freesstring(opr); /* works fine with NULL */

  ou = getcopyconfigitem("nterfacer", "serviceusername", "nterfacer", 100);
  op = getcopyconfigitem("nterfacer", "servicepassword", "setme", 100);
  on = getcopyconfigitem("nterfacer", "servicenickname", "^(O|S)$", 100);

  if(on) {
    int erroroffset;
    const char *rerror;
    newregex.phrase = pcre_compile(on->content, PCRE_CASELESS, &rerror, &erroroffset, NULL);
    if(newregex.phrase) {
      newregex.hint = pcre_study(newregex.phrase, 0, &rerror);
      if(rerror) {
        pcre_free(newregex.phrase);
        newregex.phrase = NULL;
      }
    }
  }

  if(!ou || !op || !on || !newregex.phrase) {
    freesstring(ou);
    freesstring(op);
    freesstring(on);
    if(newregex.phrase) {
      pcre_free(newregex.phrase);
      if(newregex.hint)
        pcre_free(newregex.hint);
    }
    MemError();
    return 0;
  }

  if(started) {
    if(ousername)
      freesstring(ousername);
    if(opassword)
      freesstring(opassword);
    if(onickname.phrase) {
      pcre_free(onickname.phrase);
      if(onickname.hint)
        pcre_free(onickname.hint);
    }
  }

  freesstring(on);

  ousername = ou;
  opassword = op;
  memcpy(&onickname, &newregex, sizeof(onickname));
  return 1;
}

void relay_rehash(int hook, void *args) {
  load_config();
}

void _fini(void) {
  struct rld *np, *lp;

  if(!started)
    return;

  if(ousername)
    freesstring(ousername);
  if(opassword)
    freesstring(opassword);
  if(onickname.phrase) {
    pcre_free(onickname.phrase);
    if(onickname.hint)
      pcre_free(onickname.hint);
  }
    
  deregisterhook(HOOK_NICK_LOSTNICK, &relay_quits);
  deregisterhook(HOOK_IRC_DISCON, &relay_disconnect);
  deregisterhook(HOOK_CORE_REHASH, &relay_rehash);

  for(np=list;np;) {
    lp = np->next;
    ri_error(np->rline, RELAY_UNLOADED, "Module was unloaded");
    dispose_rld(np);

    np = lp;
  }

  if(node)
    deregister_service(node);
}

nick *relay_getnick(void) {
  static char ournick[NICKLEN + 1];
  int attempts = 100;

  do {
    snprintf(ournick, sizeof(ournick), "%s%d", nickprefix, number++);
    if(number > 60000)
      number = 1;

    if(!getnickbynick(ournick))
       return registerlocaluser(ournick, "nterf", "cer", "nterfacer relay", "nterfacer", UMODE_SERVICE | UMODE_DEAF | UMODE_OPER | UMODE_INV | UMODE_ACCOUNT, &relay_messages);

    attempts--;
  } while(attempts > 0);

  return NULL;
}

int relay_handler(struct rline *ri, int argc, char **argv) {
  struct rld *np;
  int lines = 0, i;
  nick *dest;
  const char *rerror;
  int erroroffset;

  if(!connected)
    return ri_error(ri, RELAY_NOT_ON_IRC, "Not currently on IRC");

  if(argv[0][0] == '1') {
    lines = positive_atoi(argv[1]);
    if(lines < 1)
      return ri_error(ri, RELAY_PARSE_ERROR, "Parse error");
  } else if(argv[0][0] != '2') {
    return ri_error(ri, RELAY_PARSE_ERROR, "Parse error");
  }

  dest = getnickbynick(argv[2]);
  if(!dest)
    return ri_error(ri, RELAY_NICK_NOT_FOUND, "Nickname not found!");

  for(i=3;i<argc;i++)
    if(strchr(argv[i], '\r'))
      return ri_error(ri, RELAY_INVALID_CHARS, "Invalid character in input");

  np = malloc(sizeof(struct rld));
  MemCheckR(np, ri_error(ri, RELAY_MEMORY_ERROR, "Memory error"));

  np->rline = ri;
  if(!lines) {
     np->termination.pcre.phrase = pcre_compile(argv[1], PCRE_FLAGS, &rerror, &erroroffset, NULL);
    if(!np->termination.pcre.phrase) {
      MemError();
      free(np);
      return ri_error(ri, RELAY_REGEX_ERROR, "Regex compliation error");
    }
    np->termination.pcre.hint = pcre_study(np->termination.pcre.phrase, 0, &rerror);
    if(rerror) {
      pcre_free(np->termination.pcre.phrase);
      free(np);
      return ri_error(ri, RELAY_REGEX_HINT_ERROR, "Regex hint error");
    }

    np->mode = MODE_TAG;
  } else {
    np->mode = MODE_LINES;
    np->termination.remaining_lines = lines;
  }

  if(!(np->nick = relay_getnick())) {
    if(!lines) {
      pcre_free(np->termination.pcre.phrase);
      if(np->termination.pcre.hint)
        pcre_free(np->termination.pcre.hint);
    }
    free(np);
    return ri_error(ri, RELAY_NICK_ERROR, "Unable to get a nickname!");
  }

  np->schedule = scheduleoneshot(time(NULL) + MESSAGE_TIMEOUT, &relay_timeout, np);
  if(!np->schedule) {
    MemError();
    if(!lines) {
      pcre_free(np->termination.pcre.phrase);
      if(np->termination.pcre.hint)
        pcre_free(np->termination.pcre.hint);
    }
    deregisterlocaluser(np->nick, NULL);
    free(np);
    return ri_error(ri, RELAY_MEMORY_ERROR, "Memory error");
  }

  np->dest = dest;

  np->next = list;
  list = np;
  
  if(pcre_exec(onickname.phrase, onickname.hint, dest->nick, strlen(dest->nick), 0, 0, NULL, 0) >= 0) {
    np->mode|=MODE_IS_O;
    sendsecuremessagetouser(np->nick, dest, serverlist[dest->numeric>>18].name->content, "AUTH %s %s", ousername->content, opassword->content);
  }

  for(i=3;i<argc;i++)
    sendmessagetouser(np->nick, dest, "%s", argv[i]);

  return RE_OK;
}

int stats_handler(struct rline *ri, int argc, char **argv) {
  struct rld *np;
  char *server = argv[0], *command = argv[1], *more;
  int serverid = findserver(server);
  char targetnumeric[3];

  if(serverid == -1)
    return ri_error(ri, RELAY_SERVER_NOT_FOUND, "Server not found");

  if(!command || !command[0] || (command[0] == ' ') || (command[0] == '\r') || (command[0] == ':'))
    return ri_error(ri, RELAY_INVALID_COMMAND, "Invalid command");

  if(argc > 2) {
    more = argv[2];
    if(strchr(more, '\r'))
      return ri_error(ri, RELAY_INVALID_CHARS, "Invalid character in input");
  } else {
    more = NULL;
  }

  np = malloc(sizeof(struct rld));
  MemCheckR(np, ri_error(ri, RELAY_MEMORY_ERROR, "Memory error"));

  np->rline = ri;
  np->mode = MODE_STATS;
  np->dest = NULL;

  if(!(np->nick = relay_getnick())) {
    free(np);
    return ri_error(ri, RELAY_NICK_ERROR, "Unable to get a nickname!");
  }

  np->schedule = scheduleoneshot(time(NULL) + MESSAGE_TIMEOUT, &relay_timeout, np);
  if(!np->schedule) {
    MemError();
    deregisterlocaluser(np->nick, NULL);
    free(np);
  }

  np->next = list;
  list = np;

  memcpy(targetnumeric, longtonumeric(serverid, 2), 3);
  targetnumeric[2] = '\0';
  if(more) {
    irc_send("%s R %c %s :%s\r\n", longtonumeric(np->nick->numeric,5), command[0], targetnumeric, more);
  } else {
    irc_send("%s R %c :%s\r\n", longtonumeric(np->nick->numeric,5), command[0], targetnumeric);
  }

  return RE_OK;
}

void relay_messages(nick *target, int messagetype, void **args) {
  struct rld *item, *prev = NULL;
  for(item=list;item;prev=item,item=item->next)
    if(item->nick == target)
      break;

  if(!item)
    return;
   
  switch(messagetype) {
    case LU_PRIVNOTICE:
    case LU_PRIVMSG:
    case LU_SECUREMSG:
      if(!item->dest || (item->dest != (nick *)args[0]))
        return;
     
      if((item->mode & MODE_IS_O) && !(item->mode & MODE_O_AUTH2)) {
        if(item->mode & MODE_O_AUTH1) {
          if(!strncmp((char *)args[1], "Your authlevel is ", 18)) {
            item->mode|=MODE_O_AUTH2;
          } else {
            ri_error(item->rline, RELAY_O_AUTH_ERROR, "Unable to auth with O");
            dispose_rld_prev(item, prev);
          }
        } else {
          if(!strcmp((char *)args[1], "Auth successful.")) {
            item->mode|=MODE_O_AUTH1;
          } else {
            ri_error(item->rline, RELAY_O_AUTH_ERROR, "Unable to auth with O");
            dispose_rld_prev(item, prev);
          }
        }
      } else if(ri_append(item->rline, "%s", (char *)args[1]) == BF_OVER) {
        ri_error(item->rline, BF_OVER, "Buffer overflow");
        dispose_rld_prev(item, prev);
      } else {
        if(((item->mode & MODE_LINES) && (!--item->termination.remaining_lines)) || ((item->mode & MODE_TAG) && (pcre_exec(item->termination.pcre.phrase, item->termination.pcre.hint, (char *)args[1], strlen((char *)args[1]), 0, 0, NULL, 0) >= 0))) {
          ri_final(item->rline);
          dispose_rld_prev(item, prev);
        }
      }
      break;

    case LU_KILLED:
      ri_error(item->rline, RELAY_KILLED, "Killed");
      if(prev) {
        prev->next = item->next;
      } else {
        list = item->next;
      }
      dispose_rld_dontquit(item, 1);
      break;

    case LU_STATS:
      if(item->mode != MODE_STATS)
        return;
      if(ri_append(item->rline, "%s", (char *)args[2]) == BF_OVER) {
        ri_error(item->rline, BF_OVER, "Buffer overflow");
        dispose_rld_prev(item, prev);
      }
      break;

    case LU_STATS_END:
      if(item->mode != MODE_STATS)
        return;
      ri_final(item->rline);
      dispose_rld_prev(item, prev);
      break;
  }
}

void dispose_rld_prev(struct rld *item, struct rld *prev) {
  if(prev) {
    prev->next = item->next;
  } else {
    list = item->next;
  }
  dispose_rld(item);
}

void dispose_rld_dontquit(struct rld *item, int dontquit) {
  if(item->schedule)
    deleteschedule(item->schedule, &relay_timeout, item);
  if(!dontquit)
    deregisterlocaluser(item->nick, NULL);
  if(item->mode & MODE_TAG) {
    pcre_free(item->termination.pcre.phrase);
    if(item->termination.pcre.hint)
      pcre_free(item->termination.pcre.hint);
  }
  free(item);
}

void relay_timeout(void *arg) {
  struct rld *item = (struct rld *)arg, *lp = NULL, *np;
  
  item->schedule = NULL;
  
  ri_error(item->rline, RELAY_TIMEOUT, "Timed out");

  for(np=list;np;lp=np,np=np->next) {
    if(np == item) {
      dispose_rld_prev(item, lp);
      break;
    }
  }
}

void relay_quits(int hook, void *args) {
  nick *np = (nick *)args;
  struct rld *cp, *lp;
  
  for(lp=NULL,cp=list;cp;) {
    if(cp->dest == np) {
      ri_error(cp->rline, RELAY_TARGET_LEFT, "Target left QuakeNet");
      if(lp) {
        lp->next = cp->next;
        dispose_rld(cp);
        cp = lp->next;
      } else {
        list = cp->next;
        dispose_rld(cp);
        cp = list;
      }
    } else {
      lp = cp;
      cp = cp->next;
    }
  }
}

void relay_disconnect(int hook, void *args) {
  struct rld *np, *lp = NULL;
  while(list) {
    ri_error(list->rline, RELAY_DISCONNECTED, "Disconnected from IRC");
    dispose_rld_prev(list, NULL);

    np = lp;
  }
}
