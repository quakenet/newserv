
#include "chanstats.h"
#include "../parser/parser.h"
#include "../nick/nick.h"
#include "../channel/channel.h"
#include "../control/control.h"
#include "../lib/flags.h"
#include "../lib/irc_string.h"

#define  MAXTERMS    10
#define  MAXMATCHES  500

CommandTree *searchfilters;

typedef struct {
  CommandHandler  searchfunc;
  int             params;
  int             invert;
  char          **args;
} filterterm;

void cs_describe(nick *sender, chanindex *cip);
int numcompare(char *cmp, unsigned int num); 
int cs_exists(void *chan, int cargc, char **cargv);
int cs_nick(void *chan, int cargc, char **cargv);
int cs_size(void *chan, int cargc, char **cargv);
int cs_namelen(void *chan, int cargc, char **cargv);
int cs_keyed(void *chan, int cargc, char **cargv);
int cs_secret(void *chan, int cargc, char **cargv);
int cs_invite(void *chan, int cargc, char **cargv);
int cs_moderated(void *chan, int cargc, char **cargv);
int cs_modes(void *chan, int cargc, char **cargv);
int cs_name(void *chan, int cargc, char **cargv);

void initchansearch() {
  searchfilters=newcommandtree();
  
  regchansearchfunc("name", 1, cs_name);
  regchansearchfunc("exists", 0, cs_exists);
  regchansearchfunc("nick", 1, cs_nick);
  regchansearchfunc("size", 1, cs_size);
  regchansearchfunc("namelen", 1, cs_namelen);
  regchansearchfunc("keyed", 0, cs_keyed);
  regchansearchfunc("secret", 0, cs_secret);
  regchansearchfunc("invite", 0, cs_invite);
  regchansearchfunc("moderated", 0, cs_moderated);
  regchansearchfunc("modes", 1, cs_modes);
  
  registercontrolhelpcmd("chansearch",NO_OPER,19,&dochansearch,
    "Usage: chansearch <search terms>\n"
    " Valid search terms are: name, exists, size, nick, namelen, keyed, secret, invite\n"
    " Terms can be inverted with !");
}

void finichansearch() {
  deregistercontrolcmd("chansearch",&dochansearch);
  destroycommandtree(searchfilters);
}

void regchansearchfunc(const char *name, int args, CommandHandler handler) {
  addcommandtotree(searchfilters, name, 0, args, handler);
}

void unregchansearchfunc(const char *name, CommandHandler handler) {
  deletecommandfromtree(searchfilters, name, handler);
}

int dochansearch(void *source, int cargc, char **cargv) {
  nick *sender=(nick *)source;
  filterterm terms[MAXTERMS];
  int i,j;
  int numterms=0;
  Command *mc;
  chanindex *cip;
  int matched=0;
  int offset;
  int res;
    
  if (cargc<1)
    return CMD_USAGE;
  
  for (i=0;i<cargc;) {
    if (cargv[i][0]=='!') {
      offset=1;
      terms[numterms].invert=1;
    } else {
      offset=0;
      terms[numterms].invert=0;
    }
    
    if ((mc=findcommandintree(searchfilters, &cargv[i][offset], 1))==NULL) {
      controlreply(sender,"Unrecognised search term: %s",cargv[i]);
      return CMD_ERROR;    
    }
    if ((cargc-i-1) < mc->maxparams) {
      controlreply(sender,"Not enough arguments supplied for %s",cargv[i]);
      return CMD_ERROR;
    }
    terms[numterms].searchfunc=mc->handler;
    terms[numterms].params=mc->maxparams;
    terms[numterms].args=cargv+i+1;
    i+=(mc->maxparams+1);
    numterms++;
    
    if (numterms==MAXTERMS)
      break;
  }

  controlreply(sender,"The following channels match your criteria:");
  
  for(i=0;i<CHANNELHASHSIZE;i++) {
    for(cip=chantable[i];cip;cip=cip->next) {
      for(j=0;j<numterms;j++) {
        res=(terms[j].searchfunc)((void *)cip,terms[j].params,terms[j].args);
        if (res==0 && terms[j].invert)
          break;
        if (res==1 && !terms[j].invert)
          break;
      }
      if (j==numterms) {
        if (matched == MAXMATCHES) {
          controlreply(sender,"--- More than %d matches, truncating list.",MAXMATCHES);
        }
        if (matched < MAXMATCHES) {
          cs_describe(sender, cip);
        }
        matched++;
      }
    }
  }
  
  if (matched==0) {
    controlreply(sender,"--- No matches found.");
  } else {
    controlreply(sender,"--- End of list: %d match(es).",matched);
  }
  
  return CMD_OK;
}

int cs_name(void *chan, int cargc, char **cargv) {
  chanindex *cip=(chanindex *)chan;
  
  return !match2strings(cargv[0],cip->name->content);
}

int cs_exists(void *chan, int cargc, char **cargv) {
  chanindex *cip=(chanindex *)chan;
  
  return (cip->channel==NULL);
}

int cs_nick(void *chan, int cargc, char **cargv) {
  chanindex *cip=(chanindex *)chan;
  nick *np;
  
  if (cip->channel==NULL) {
    return 1;
  }

  if ((np=getnickbynick(cargv[0]))==NULL) {
    return 1;
  }
  
  if ((getnumerichandlefromchanhash(cip->channel->users, np->numeric))==NULL) {
    return 1;
  }
  
  return 0;
}

int cs_size(void *chan, int cargc, char **cargv) {
  chanindex *cip=(chanindex *)chan;
  
  if (cip->channel==NULL) {
    return 1;
  }
  
  return numcompare(cargv[0],cip->channel->users->totalusers); 
}

int cs_namelen(void *chan, int cargc, char **cargv) {
  chanindex *cip=(chanindex *)chan;
  
  return numcompare(cargv[0],cip->name->length);
}

int cs_keyed(void *chan, int cargc, char **cargv) {
  chanindex *cip=(chanindex *)chan;
  
  if (cip->channel==NULL) {
    return 1;
  }
  
  return !IsKey(cip->channel);
}

int cs_secret(void *chan, int cargc, char **cargv) {
  chanindex *cip=(chanindex *)chan;
  
  if (cip->channel==NULL) {
    return 1;
  }
  
  return !IsSecret(cip->channel);
}

int cs_invite(void *chan, int cargc, char **cargv) {
  chanindex *cip=(chanindex *)chan;
  
  if (cip->channel==NULL) {
    return 1;
  }
  
  return !IsInviteOnly(cip->channel);
}

int cs_moderated(void *chan, int cargc, char **cargv) {
  chanindex *cip=(chanindex *)chan;
  
  if (cip->channel==NULL) {
    return 1;
  }
  
  return !IsModerated(cip->channel);
}

int cs_modes(void *chan, int cargc, char **cargv) {
  flag_t flags=0;
  chanindex *cip=(chanindex *)chan;
  
  if (cip->channel==NULL) {
    return 1;
  }
  
  setflags(&flags, CHANMODE_ALL, cargv[0], cmodeflags, REJECT_NONE);

  if ((cip->channel->flags & flags)!=flags) {
    return 1;
  }
  
  flags=CHANMODE_ALL;
  setflags(&flags, CHANMODE_ALL, cargv[0], cmodeflags, REJECT_NONE);
 
  if ((cip->channel->flags & ~flags)) {
    return 1;
  }
  
  return 0;  
}

int numcompare(char *cmp, unsigned int num) {
  int sz=strtol(&cmp[1],NULL,10);

  switch(cmp[0]) {
    case '>': if (num > sz)
                return 0;     
              break;
              
    case '<': if (num < sz)
                return 0;
              break;
    
    case '=': if (num == sz)
                return 0;
              break;
    
    default : return 1;
  }
  
  return 1;
}
  
void cs_describe(nick *sender, chanindex *cip) {
  int i;
  int op,voice,peon;
  int oper,service;
  nick *np;
  chanuserhash *cuhp;
  
  op=voice=peon=oper=service=0;
  
  if (cip->channel==NULL) {
    controlreply(sender,"[      Channel currently empty       ] %s",cip->name->content);
  } else {
    cuhp=cip->channel->users;
    for (i=0;i<cuhp->hashsize;i++) {
      if (cuhp->content[i]!=nouser) {
        if (cuhp->content[i]&CUMODE_OP) {
          op++;
        } else if (cuhp->content[i]&CUMODE_VOICE) {
          voice++;
        } else {
          peon++;
        }
        if ((np=getnickbynumeric(cuhp->content[i]&CU_NUMERICMASK))!=NULL) {
          if (IsOper(np)) {
            oper++;
          }
          if (IsService(np)) {
            service++;
          }
        }
      }
    }
    controlreply(sender,"[ %4dU %4d@ %4d+ %4d %4d* %4dk ] %s",cuhp->totalusers,op,voice,peon,oper,service,cip->name->content);
  }
}
