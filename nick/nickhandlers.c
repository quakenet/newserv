/* nickhandlers.c */

#include "nick.h"
#include "../lib/flags.h"
#include "../lib/irc_string.h"
#include "../lib/base64.h"
#include "../irc/irc.h"
#include "../irc/irc_config.h"
#include "../core/error.h"
#include "../core/hooks.h"
#include "../lib/sstring.h"
#include "../server/server.h"
#include "../parser/parser.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/*
 * handlenickmsg:
 *  Handle new nicks being introduced to the network.
 *  Handle renames.
 */
 
int handlenickmsg(void *source, int cargc, char **cargv) {
  char *sender=(char *)source;
  time_t timestamp;
  nick *np,*np2;
  nick **nh;
  char *fakehost;
  char *accountts;
  char *accountflags;
  struct irc_in_addr ipaddress;
  char *accountid;
  unsigned long userid;
  
  if (cargc==2) { /* rename */
    char oldnick[NICKLEN+1];
    void *harg[2];

    /* Nyklon 1017697578 */
    timestamp=strtol(cargv[1],NULL,10);
    np=getnickbynumericstr(sender);
    if (np==NULL) {
      Error("nick",ERR_ERROR,"Rename from non-existent sender %s",sender);
      return CMD_OK;
    }

    strncpy(oldnick,np->nick,NICKLEN);
    oldnick[NICKLEN]='\0';
    harg[0]=(void *)np;
    harg[1]=(void *)oldnick;

    np2=getnickbynick(cargv[0]);
    if (np==np2) {
      /* The new and old nickname have the same hash, this means a rename to the same name in 
       * different case, e.g. Flash -> flash.  In this case the timestamp for the change should
       * match the existing timestamp, and we can bypass all the collision checking and hash fettling. */
      if (np->timestamp!=timestamp) {
        Error("nick",ERR_WARNING,"Rename to same nickname with different timestamp (%s(%jd) -> %s(%jd))",
                            np->nick,(intmax_t)np->timestamp,cargv[0], (intmax_t)timestamp);
        np->timestamp=timestamp;
      }
      strncpy(np->nick,cargv[0],NICKLEN);
      np->nick[NICKLEN]='\0';
      triggerhook(HOOK_NICK_RENAME,harg);
      return CMD_OK;
    }
    if (np2!=NULL) {
      /* Nick collision */
      if (ircd_strcmp(np->ident,np2->ident) || (np->host!=np2->host)) {
        /* Different user@host */
        if (np2->timestamp < timestamp) {
          /* The nick attempting to rename got killed.  Guess we don't need to do the rename now :) */
          deletenick(np);
          return CMD_OK;
        } else {
          /* The other nick got killed */
          deletenick(np2);
        }
      } else {
        if (np2->timestamp < timestamp) {
          /* Same user@host: reverse logic.  Whose idea was all this anyway? */
          deletenick(np2);
        } else {
          deletenick(np);
          return CMD_OK;
        }
      }
    }
    /* OK, we've survived the collision hazard.  Change timestamp and rename */
    np->timestamp=timestamp;
    removenickfromhash(np);
    strncpy(np->nick,cargv[0],NICKLEN);
    np->nick[NICKLEN]='\0';
    addnicktohash(np);
    triggerhook(HOOK_NICK_RENAME,harg);
  } else if (cargc>=8) { /* new nick */
    /* Jupiler 2 1016645147 ~Jupiler www.iglobal.be +ir moo [FUTURE CRAP HERE] DV74O] BNBd7 :Jupiler */
    timestamp=strtol(cargv[2],NULL,10);
    np=getnickbynick(cargv[0]);
    if (np!=NULL) {
      /* Nick collision */
      if (ircd_strcmp(np->ident,cargv[3]) || ircd_strcmp(np->host->name->content,cargv[4])) {
        /* Different user@host */
        if (timestamp>np->timestamp) {
          /* New nick is newer.  Ignore this nick message */
          return CMD_OK;
        } else {
          /* New nick is older.  Kill the imposter, and drop through */
          deletenick(np);
        }
      } else {
        if (timestamp>np->timestamp) {
          /* Same user@host, newer timestamp: we're killing off a ghost */
          deletenick(np);
        } else {
          /* This nick is the ghost, so ignore it */
          return CMD_OK;
        }
      }
    }
    
    nh=gethandlebynumeric(numerictolong(cargv[cargc-2],5));
    if (!nh) {
      /* This isn't a valid numeric */      
      Error("nick",ERR_WARNING,"Received NICK with invalid numeric %s from %s.",cargv[cargc-2],sender);
      return CMD_ERROR;
    }

    base64toip(cargv[cargc-3], &ipaddress);

    /* At this stage the nick is cleared to proceed */
    np=newnick();
    strncpy(np->nick,cargv[0],NICKLEN);
    np->nick[NICKLEN]='\0';
    np->numeric=numerictolong(cargv[cargc-2],5);
    strncpy(np->ident,cargv[3],USERLEN);
    np->ident[USERLEN]='\0';
    np->host=findorcreatehost(cargv[4]);
    np->realname=findorcreaterealname(cargv[cargc-1]);
    np->nextbyhost=np->host->nicks;
    np->host->nicks=np;
    np->nextbyrealname=np->realname->nicks;
    np->realname->nicks=np;
    np->timestamp=timestamp;

    base64toip(cargv[cargc-3], &ipaddress);
    np->ipnode = refnode(iptree, &ipaddress, PATRICIA_MAXBITS);
    node_increment_usercount(np->ipnode);

    np->away=NULL;
    np->shident=NULL;
    np->sethost=NULL;
    np->opername=NULL;
    np->umodes=0;
    np->marker=0;
    memset(np->exts, 0, MAXNICKEXTS * sizeof(void *));
    np->authname=NULLAUTHNAME;
    np->auth=NULL;
    np->accountts=0;
    np->cloak_count = 0;
    np->cloak_extra = NULL;
    if(cargc>=9) {
      int sethostarg = 6, opernamearg = 6, accountarg = 6;

      setflags(&(np->umodes),UMODE_ALL,cargv[5],umodeflags,REJECT_NONE);

      if(IsOper(np) && (serverlist[myhub].flags & SMODE_OPERNAME)) {
        accountarg++;
        sethostarg++;

        np->opername=getsstring(cargv[opernamearg],ACCOUNTLEN);
      }

      if (IsAccount(np)) {
        sethostarg++;

        if ((accountts=strchr(cargv[accountarg],':'))) {
          userid=0;
          *accountts++='\0';
          np->accountts=strtoul(accountts,&accountid,10);
          if(accountid) {
            userid=strtoul(accountid + 1,&accountflags,10);
            if(userid) {
              np->auth=findorcreateauthname(userid, cargv[accountarg]);
              np->authname=np->auth->name;
              np->auth->usercount++;
              np->nextbyauthname=np->auth->nicks;
              np->auth->nicks=np;
              if(accountflags)
                np->auth->flags=strtoull(accountflags + 1,NULL,10);
            }
          }
          if(!userid) {
            np->authname=malloc(strlen(cargv[accountarg]) + 1);
            strcpy(np->authname,cargv[accountarg]);
          }
        }        
      } 
      if (IsSetHost(np) && (fakehost=strchr(cargv[sethostarg],'@'))) {
	/* valid sethost */
	*fakehost++='\0';
	np->shident=getsstring(cargv[sethostarg],USERLEN);
	np->sethost=getsstring(fakehost,HOSTLEN);
      }
    }
    
    /* Place this nick in the server nick table.  Note that nh is valid from the numeric check above */
    if (*nh) {
      /* There was a nick there already -- we have a masked numeric collision
       * This shouldn't happen, but if it does the newer nick takes precedence
       * (the two nicks are from the same server, and if the server has reissued
       * the masked numeric it must believe the old user no longer exists).
       */
       Error("nick",ERR_ERROR,"Masked numeric collision for %s [%s vs %s]",cargv[0],cargv[cargc-2],longtonumeric((*nh)->numeric,5));
       deletenick(*nh);
    }
    *nh=np;
    
    /* And the nick hash table */
    addnicktohash(np);      
    
    /* Trigger the hook */
    triggerhook(HOOK_NICK_NEWNICK,np);
  } else {
    Error("nick",ERR_WARNING,"Nick message with weird number of parameters (%d)",cargc);
  }
  
  return CMD_OK;
}

int handlequitmsg(void *source, int cargc, char **cargv) {
  nick *np;
  void *harg[2];
  
  if (cargc>0) {
    harg[1]=(void *)cargv[0];
  } else {
    harg[1]="";
  } 
  
  np=getnickbynumericstr((char *)source);
  if (np) {
    harg[0]=(void *)np;
    triggerhook(HOOK_NICK_QUIT, harg);
    deletenick(np);
  } else {
    Error("nick",ERR_WARNING,"Quit from non-existant numeric %s",(char *)source);
  }
  return CMD_OK; 
}

int handlekillmsg(void *source, int cargc, char **cargv) {
  nick *np;
  void *harg[2];
#warning Fix me to use source

  if (cargc<1) {
    Error("nick",ERR_WARNING,"Kill message with too few parameters");
    return CMD_ERROR;  
  }

  if (cargc>1) {
    harg[1]=(void *)cargv[1];
  } else {
    harg[1]="";
  } 
  
  np=getnickbynumericstr(cargv[0]);
  if (np) {
    harg[0]=(void *)np;
    triggerhook(HOOK_NICK_KILL, harg);
    deletenick(np);
  } else {
    Error("nick",ERR_WARNING,"Kill for non-existant numeric %s",cargv[0]);
  }
  return CMD_OK; 
}

int handleusermodemsg(void *source, int cargc, char **cargv) {
  nick *np;
  flag_t oldflags;
  char *fakehost;
  
  if (cargc<2) {
    Error("nick",ERR_WARNING,"Mode message with too few parameters");
    return CMD_LAST; /* With <2 params the channels module won't want it either */
  }
  
  if (cargv[0][0]=='#') {
    /* Channel mode change, we don't care.
     * We don't bother checking for other channel types here, since & channel mode 
     * changes aren't broadcast and + channels don't have mode changes :) */
    return CMD_OK;
  }
  
  np=getnickbynumericstr((char *)source);
  if (np!=NULL) {
    if (ircd_strcmp(cargv[0],np->nick)) {
      Error("nick",ERR_WARNING,"Attempted mode change on user %s by %s",cargv[0],np->nick);
      return CMD_OK;
    }
    oldflags=np->umodes;
    setflags(&(np->umodes),UMODE_ALL,cargv[1],umodeflags,REJECT_NONE);

    if (strchr(cargv[1],'o')) { /* o always comes on its own when being set */
      if(serverlist[myhub].flags & SMODE_OPERNAME) {
        if((np->umodes & UMODE_OPER)) {
          np->opername = getsstring(cargv[2], ACCOUNTLEN);
        } else {
          freesstring(np->opername);
          np->opername = NULL;
        }
      }
      if((np->umodes ^ oldflags) & UMODE_OPER)
        triggerhook(HOOK_NICK_MODEOPER,np);
    }
    if (strchr(cargv[1],'h')) { /* Have to allow +h twice.. */
      /* +-h: just the freesstring() calls for the -h case */
      freesstring(np->shident); /* freesstring(NULL) is OK */
      freesstring(np->sethost);
      np->shident=NULL;
      np->sethost=NULL;
      if (IsSetHost(np) && cargc>2) { /* +h and mask received */
	if ((fakehost=strchr(cargv[2],'@'))) {
	  /* user@host change */
	  *fakehost++='\0';
	  np->shident=getsstring(cargv[2],USERLEN);
	  np->sethost=getsstring(fakehost,HOSTLEN);
	} else {
	  np->sethost=getsstring(cargv[2],HOSTLEN);
	}
      }
      triggerhook(HOOK_NICK_SETHOST, (void *)np);
    }  
  } else {
    Error("nick",ERR_WARNING,"Usermode change by unknown user %s",(char *)source);
  }
  return CMD_OK;
}

int handleaccountmsg(void *source, int cargc, char **cargv) {
  nick *target;
  unsigned long userid;
  time_t accountts;
  u_int64_t accountflags=0, oldflags;

  if (cargc<4) {
    return CMD_OK;
  }
  
  if ((target=getnickbynumericstr(cargv[0]))==NULL) {
    return CMD_OK;
  }
  
  accountts=strtoul(cargv[2],NULL,10);
  userid=strtoul(cargv[3],NULL,10);
  if(cargc>=5)
    accountflags=strtoull(cargv[4],NULL,10);

  /* allow user flags to change if all fields match */
  if (IsAccount(target)) {
    void *arg[2];

    if (!target->auth || strcmp(target->auth->name,cargv[1]) || (target->auth->userid != userid) || (target->accountts != accountts)) {
      return CMD_OK;
    }

    oldflags = target->auth->flags;
    arg[0] = target->auth;
    arg[1] = &oldflags;
    
    if (cargc>=5)
      target->auth->flags=accountflags;

    triggerhook(HOOK_AUTH_FLAGSUPDATED, (void *)arg);

    return CMD_OK;
  }
  
  SetAccount(target);
  target->accountts=accountts;

  if(!userid) {
    target->auth=NULL;
    target->authname=malloc(strlen(cargv[1]) + 1);
    strcpy(target->authname,cargv[1]);
  } else {
    target->auth=findorcreateauthname(userid, cargv[1]);
    target->auth->usercount++;
    target->authname=target->auth->name;
    target->nextbyauthname = target->auth->nicks;
    target->auth->nicks = target;
    if (cargc>=5)
      target->auth->flags=accountflags;
  }

  triggerhook(HOOK_NICK_ACCOUNT, (void *)target);

  return CMD_OK;
}

int handleprivmsg(void *source, int cargc, char **cargv) {
  nick *sender;
  char *message;
  void *args[3];

  if (cargc<2)
    return CMD_OK;

  if (cargv[0][0]!='$')
    return CMD_OK;

  sender=getnickbynumericstr((char *)source);

  if (!match2strings(cargv[0] + 1,myserver->content))
    return CMD_OK;

  message=cargv[0];

  args[0]=sender;
  args[1]=cargv[0];
  args[2]=cargv[1];

  triggerhook(HOOK_NICK_MASKPRIVMSG, (void *)args);

  return CMD_OK;
}

int handleawaymsg(void *source, int cargc, char **cargv) {
  nick *sender;
    
  /* Check source is a valid user */ 
  if (!(sender=getnickbynumericstr(source))) {
    return CMD_OK;
  }

  /* Done with the old away message either way */
  freesstring(sender->away);
  sender->away=NULL;
  
  /* If we have an arg and it isn't an empty string, this sets a new message */
  if (cargc > 0 && *(cargv[0])) {
    sender->away=getsstring(cargv[0], AWAYLEN);
  }

  return CMD_OK;
}

int handleaddcloak(void *source, int cargc, char **cargv) {
  nick *sender, *target;

  /* Check source is a valid user */
  if (!(sender=getnickbynumericstr(source))) {
    return CMD_OK;
  }

  if (cargc < 1)
    return CMD_OK;

  if (!(target=getnickbynumericstr(cargv[0]))) {
    return CMD_OK;
  }

  addcloaktarget(sender, target);

  return CMD_OK;
}

int handleclearcloak(void *source, int cargc, char **cargv) {
  nick *sender;

  /* Check source is a valid user */
  if (!(sender=getnickbynumericstr(source))) {
    return CMD_OK;
  }

  clearcloaktargets(sender);

  return CMD_OK;
}

