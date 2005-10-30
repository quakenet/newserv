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
  
  if (cargc==2) { /* rename */
    /* Nyklon 1017697578 */
    timestamp=strtol(cargv[1],NULL,10);
    np=getnickbynumericstr(sender);
    if (np==NULL) {
      Error("nick",ERR_ERROR,"Rename from non-existent sender %s",sender);
      return CMD_OK;
    }
    np2=getnickbynick(cargv[0]);
    if (np==np2) {
      /* The new and old nickname have the same hash, this means a rename to the same name in 
       * different case, e.g. Flash -> flash.  In this case the timestamp for the change should
       * match the existing timestamp, and we can bypass all the collision checking and hash fettling. */
      if (np->timestamp!=timestamp) {
        Error("nick",ERR_WARNING,"Rename to same nickname with different timestamp (%s(%d) -> %s(%d))",
                            np->nick,np->timestamp,cargv[0],timestamp);
        np->timestamp=timestamp;
      }
      strncpy(np->nick,cargv[0],NICKLEN);
      np->nick[NICKLEN]='\0';
      triggerhook(HOOK_NICK_RENAME,np);        
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
    triggerhook(HOOK_NICK_RENAME,np);
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
    np->ipaddress=numerictolong(cargv[cargc-3],6);
    np->shident=NULL;
    np->sethost=NULL;
    np->umodes=0;
    np->marker=0;
    memset(np->exts, 0, MAXNICKEXTS * sizeof(void *));
    np->authname[0]='\0';
    if(cargc>=9) {
      setflags(&(np->umodes),UMODE_ALL,cargv[5],umodeflags,REJECT_NONE);
      if (IsAccount(np)) {
        if ((accountts=strchr(cargv[6],':'))) {
          *accountts++='\0';
          np->accountts=strtoul(accountts,NULL,10);
        } else {
          np->accountts=0;
        }        
        strncpy(np->authname,cargv[6],ACCOUNTLEN);
        np->authname[ACCOUNTLEN]='\0';
      } 
      if (IsSetHost(np) && (fakehost=strchr(cargv[cargc-4],'@'))) {
	/* valid sethost */
	*fakehost++='\0';
	np->shident=getsstring(cargv[cargc-4],USERLEN);
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
  
  if (cargc>1) {
    harg[1]=(void *)cargv[1];
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
  
  if (cargc<1) {
    Error("nick",ERR_WARNING,"Kill message with too few parameters");
    return CMD_ERROR;  
  }
  np=getnickbynumericstr(cargv[0]);
  if (np) {
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

int handlewhoismsg(void *source, int cargc, char **cargv) {
  nick *sender,*target;
  nick *nicks[2];
  
  if (cargc<2)
    return CMD_OK;
  
  if (strncmp(cargv[0],mynumeric->content,2)) {
    return CMD_OK;
  }
  
  /* Find the sender... */  
  if ((sender=getnickbynumericstr((char *)source))==NULL) {
    Error("localuser",ERR_WARNING,"WHOIS message from non existent numeric %s",(char *)source);
    return CMD_OK;
  }
  
  /* :hub.splidge.netsplit.net 311 moo splidge splidge ground.stbarnab.as * :splidge
     :hub.splidge.netsplit.net 312 moo splidge splidge.netsplit.net :splidge's netsplit leaf
     :hub.splidge.netsplit.net 313 moo splidge :is an IRC Operator
     :hub.splidge.netsplit.net 318 moo splidge :End of /WHOIS list.
  */
  
  /* And the target... */
  if ((target=getnickbynick(cargv[1]))==NULL) {
    irc_send(":%s 401 %s %s :No such nick",myserver->content,sender->nick,cargv[1]);
  } else {
    irc_send(":%s 311 %s %s %s %s * :%s",myserver->content,sender->nick,target->nick,target->ident,
        target->host->name->content, target->realname->name->content);
    nicks[0]=sender; nicks[1]=target;
    triggerhook(HOOK_NICK_WHOISCHANNELS,nicks);
    if (IsOper(sender) || !HIS_SERVER ) {
      irc_send(":%s 312 %s %s %s :%s",myserver->content,sender->nick,target->nick,
          serverlist[homeserver(target->numeric)].name->content,
          serverlist[homeserver(target->numeric)].description->content);
    } else {
      irc_send(":%s 312 %s %s " HIS_SERVERNAME " :" HIS_SERVERDESC,myserver->content,sender->nick,target->nick);
    }
    if (IsOper(target)) {
      irc_send(":%s 313 %s %s :is an IRC Operator",myserver->content,sender->nick,target->nick);
    }
    if (IsAccount(target)) {
      irc_send(":%s 330 %s %s %s :is authed as",myserver->content,sender->nick,target->nick,target->authname);
    }
    if (homeserver(target->numeric)==mylongnum && !IsService(target) && !IsHideIdle(target)) {
      irc_send(":%s 317 %s %s %ld %ld :seconds idle, signon time",myserver->content,sender->nick,target->nick,
         target->timestamp % 3600,target->timestamp);
    }
  }
  
  irc_send(":%s 318 %s %s :End of /WHOIS list.",myserver->content,sender->nick,cargv[1]);
  return CMD_OK;
}  

int handleaccountmsg(void *source, int cargc, char **cargv) {
  nick *target;
  
  if (cargc<2) {
    return CMD_OK;
  }
  
  if ((target=getnickbynumericstr(cargv[0]))==NULL) {
    return CMD_OK;
  }
  
  if (IsAccount(target)) {
    return CMD_OK;
  }
  
  SetAccount(target);
  strncpy(target->authname,cargv[1],ACCOUNTLEN);
  target->authname[ACCOUNTLEN]='\0';

  triggerhook(HOOK_NICK_ACCOUNT, (void *)target);
  
  return CMD_OK;
}

int handlestatsmsg(void *source, int cargc, char **cargv) {
  int sourceserver;
  char *sender=(char *)source;
  char *replytarget;
  char *fromstring;
  nick *np;
  
  if (cargc<2) {
    Error("nick",ERR_WARNING,"STATS request without enough parameters!");
    return CMD_OK;
  }
  
  if (strlen(sender)==5) {
    /* client */
    np=getnickbynumericstr(sender);
    if (!np) {
      Error("nick",ERR_WARNING,"STATS request from unknown client %s",sender);
      return CMD_OK;
    }
    replytarget=np->nick;
  } else {
    Error("nick",ERR_WARNING,"STATS request from odd source %s",sender);
    return CMD_OK;
  }

  /* Reply to stats for ANY server.. including any we are juping */
  sourceserver=numerictolong(cargv[1],2);
  if (serverlist[sourceserver].maxusernum==0) {
    Error("nick",ERR_WARNING,"Stats request for bad server %s",cargv[1]);
    return CMD_OK;
  }
  fromstring=serverlist[sourceserver].name->content;

  switch(cargv[0][0]) {
  case 'u':
    irc_send(":%s 242 %s :Server Up %s",fromstring,replytarget,
	     longtoduration(time(NULL)-starttime, 0));
    irc_send(":%s 250 %s :Highest connection count: 10 (9 clients)",fromstring,replytarget);
    break;

  case 'P':
    irc_send(":%s 217 %s P none 0 :0x2000",fromstring,replytarget);
    break;
    
  }

  irc_send(":%s 219 %s %c :End of /STATS report",fromstring,replytarget,cargv[0][0]);

  return CMD_OK;
}
