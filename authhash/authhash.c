/* authhash.c */

#include "../lib/irc_string.h"
#include "../nick/nick.h"
#include "../core/hooks.h"
#include "../core/error.h"
#include "../lib/version.h"
#include "authhash.h"

MODULE_VERSION("");

#define AUTHHASHSIZE	40000
#define authhash(x)	((crc32i(x))%AUTHHASHSIZE)

nick *authtable[AUTHHASHSIZE];

int nextbyauthext = -1;
int nextbyhashext = -1;

#define nextbyhash(x) x->exts[nextbyhashext]
#define rnextbyauth(x) x->exts[nextbyauthext]

void ah_onnick(int hooknum, void *arg);
void ah_onauth(int hooknum, void *arg);
void ah_onquit(int hooknum, void *arg);

void _init() {
  nick *np;
  int i;

  nextbyauthext = registernickext("authnext");
  if(nextbyauthext == -1)
    return;

  nextbyhashext = registernickext("authhash");
  if(nextbyhashext == -1)
    return;

  for(i=0;i<NICKHASHSIZE;i++)
    for(np=nicktable[i];np;np=np->next)
      ah_onauth(HOOK_NICK_NEWNICK, np);

  registerhook(HOOK_NICK_NEWNICK, &ah_onauth);
  registerhook(HOOK_NICK_ACCOUNT, &ah_onauth);
  registerhook(HOOK_NICK_LOSTNICK, &ah_onquit);
}

void _fini() {
  deregisterhook(HOOK_NICK_LOSTNICK, &ah_onquit);
  deregisterhook(HOOK_NICK_ACCOUNT, &ah_onauth);
  deregisterhook(HOOK_NICK_NEWNICK, &ah_onauth);

  if(nextbyhashext != -1)
    releasenickext(nextbyhashext);

  if(nextbyauthext != -1)
    releasenickext(nextbyauthext);
}

void ah_onauth(int hooknum, void *arg) {
  nick *np = (nick *)arg, *existing, *pred;
  long hash;

  if(!np || !IsAccount(np))
    return;

  hash = authhash(np->authname);

  for(pred=NULL,existing=authtable[hash];existing;pred=existing,existing=nextbyhash(existing))
    if(!ircd_strcmp(existing->authname, np->authname))
      break;

  if(!existing) { 
    nextbyhash(np) = authtable[hash]; /* np->nextbyhash = authtable[hash]; */
    authtable[hash] = np;
  } else {
    rnextbyauth(np) = existing; /* np->nextbyauth = existing */
    nextbyhash(np) = nextbyhash(existing); /* np->nextbyhash = existing->nextbyhash */
    nextbyhash(existing) = NULL; /* existing->nextbyhash = NULL */

    if(pred) {
      nextbyhash(pred) = np; /* pred->nextbyhash = np */
    } else {      
      authtable[hash] = np;
    }
  }
}

void ah_onquit(int hooknum, void *arg) {
  nick *np = (nick *)arg, *authpred, *hashpred, *nextauth, *nexthash;
  long hash;

  if(!np || !IsAccount(np))
    return;

  hash = authhash(np->authname);
  nexthash = nextbyhash(np);

  authpred = getnickbyauth(np->authname);
  if(authpred == np) {
    authpred = NULL;

    if(authtable[hash] == np) {
      hashpred = NULL;
    } else {
      for(hashpred=authtable[hash];nextbyhash(hashpred);hashpred=nextbyhash(hashpred))
        if(!ircd_strcmp(((nick *)nextbyhash(hashpred))->authname, np->authname))
          break;
    }
  } else {
    nexthash = nextbyhash(authpred);

    for(authpred=getnickbyauth(np->authname);rnextbyauth(authpred);authpred=rnextbyauth(authpred))
      if(np == rnextbyauth(authpred))
        break;
  }

  nextauth = rnextbyauth(np);
  if(authpred) {
    rnextbyauth(authpred) = nextauth;
  } else {
    if(nextauth && hashpred) {
      nextbyhash(hashpred) = nextauth;
      nextbyhash(nextauth) = nexthash;
    } else if(!nextauth && hashpred) {
      nextbyhash(hashpred) = nexthash;
    } else if(nextauth && !hashpred) {
      authtable[hash] = nextauth;
      nextbyhash(nextauth) = nexthash;
    } else {
      authtable[hash] = nexthash;
    }
  }
}

nick *getnickbyauth(const char *auth) {
  nick *np;

  for(np=authtable[authhash(auth)];np;np=nextbyhash(np))
    if(!ircd_strcmp(np->authname, auth))
      break;

  return np;
}
