/* localuser.c */

#include "../nick/nick.h"
#include "../lib/base64.h"
#include "../lib/sstring.h"
#include "../irc/irc.h"
#include "../irc/irc_config.h"
#include "../core/hooks.h"
#include "../core/error.h"
#include "../lib/version.h"
#include "localuser.h"

#include <string.h>
#include <stdarg.h>
#include <stdio.h>

MODULE_VERSION("");

int currentlocalunum;
UserMessageHandler umhandlers[MAXLOCALUSER+1];

typedef struct pendingkill {
  nick *source, *target;
  sstring *reason;
  struct pendingkill *next;
} pendingkill;

pendingkill *pendingkilllist;

void checklocalkill(int hooknum, void *nick);
void clearpendingkills(int hooknum, void *arg);
void checkpendingkills(int hooknum, void *arg);
void _killuser(nick *source, nick *target, char *reason);

void _init() {
  int i;
  
  for (i=0;i<=MAXLOCALUSER;i++) {
    umhandlers[i]=NULL;
  }
  currentlocalunum=1;
  pendingkilllist=NULL;
  registerhook(HOOK_IRC_SENDBURSTNICKS,&sendnickburst);
  registerhook(HOOK_NICK_KILL,&checklocalkill);
  registerhook(HOOK_NICK_LOSTNICK,&checkpendingkills); /* CHECK ME -> should this hook KILL or LOSTNICK or BOTH */
  registerhook(HOOK_CORE_ENDOFHOOKSQUEUE,&clearpendingkills);
  registerserverhandler("P",&handleprivatemsgcmd,2);
  registerserverhandler("O",&handleprivatenoticecmd, 2);
}

void _fini() {
  pendingkill *pk;

  for (pk=pendingkilllist;pk;pk=pendingkilllist) {
    pendingkilllist = pk->next;
    freesstring(pk->reason);
    free(pk);
  }

  deregisterhook(HOOK_IRC_SENDBURSTNICKS,&sendnickburst);
  deregisterhook(HOOK_NICK_KILL,&checklocalkill);
  deregisterhook(HOOK_NICK_LOSTNICK,&checkpendingkills); /* CHECK ME -> should this hook KILL or LOSTNICK or BOTH */
  deregisterhook(HOOK_CORE_ENDOFHOOKSQUEUE,&clearpendingkills);
  
  deregisterserverhandler("P",&handleprivatemsgcmd);
  deregisterserverhandler("O",&handleprivatenoticecmd);
}

/*
 * registerlocaluserflags:
 *  This function creates a local user, and broadcasts it's existence to the net (if connected).
 */

nick *registerlocaluserflags(char *nickname, char *ident, char *host, char *realname, char *authname, unsigned long authid, flag_t accountflags, flag_t umodes, UserMessageHandler handler) {
  int i;  
  nick *newuser,*np; 
  struct irc_in_addr ipaddress;
 
  i=0;
  currentlocalunum=(currentlocalunum+1)%262142;
  while (servernicks[numerictolong(mynumeric->content,2)][currentlocalunum&MAXLOCALUSER]!=NULL) {
    /* Numeric 262143 on our server is used for "nouser" by the channels module, so cannot be allocated */
    currentlocalunum=(currentlocalunum+1)%262142; 
    if (++i>MAXLOCALUSER) {
      return NULL;
    }
  }
  
  /* This code is very similar to stuff in nick.c... */
  newuser=newnick();
  newuser->numeric=(numerictolong(mynumeric->content,2)<<18)|(currentlocalunum);
  strncpy(newuser->nick,nickname,NICKLEN);
  newuser->nick[NICKLEN]='\0';
  strncpy(newuser->ident,ident,USERLEN);
  newuser->ident[USERLEN]='\0';
  newuser->host=findorcreatehost(host);
  newuser->realname=findorcreaterealname(realname);
  newuser->nextbyhost=newuser->host->nicks;
  newuser->host->nicks=newuser;
  newuser->nextbyrealname=newuser->realname->nicks;
  newuser->realname->nicks=newuser;
  newuser->umodes=umodes;

  memset(&ipaddress, 0, sizeof(ipaddress));
  ((unsigned short *)(ipaddress.in6_16))[5] = 65535;
  ((unsigned short *)(ipaddress.in6_16))[6] = 127;
  ((unsigned char *)(ipaddress.in6_16))[14] = 1;
  ((unsigned char *)(ipaddress.in6_16))[15] = (currentlocalunum%253)+1;

  newuser->ipnode = refnode(iptree, &ipaddress, PATRICIA_MAXBITS);
  node_increment_usercount(newuser->ipnode);

  newuser->timestamp=getnettime();
  newuser->shident=NULL;
  newuser->sethost=NULL;
  newuser->marker=0;
  memset(newuser->exts, 0, MAXNICKEXTS * sizeof(void *));

  if (IsOper(newuser)) {
    newuser->opername = getsstring("-", ACCOUNTLEN);
  } else {
    newuser->opername = NULL;
  }

  newuser->accountts=0;
  newuser->auth=NULL;
  newuser->authname=NULLAUTHNAME;
  if (IsAccount(newuser)) {
    newuser->accountts=newuser->timestamp;
    if (authid) {
      newuser->auth=findorcreateauthname(authid, authname);
      newuser->authname=newuser->auth->name;
      newuser->auth->usercount++;
      newuser->nextbyauthname=newuser->auth->nicks;
      newuser->auth->nicks=newuser;
      newuser->auth->flags=accountflags;
    } else {
      /*
         this is done for three reasons:
         1: so I don't have to change 500 pieces of code
         2: so services can be authed with special reserved ids
         3: saves space over the old authname per user method
       */
      newuser->authname=malloc(strlen(authname) + 1);
      strcpy(newuser->authname,authname);
    }
  }

  if (connected) {
    /* Check for nick collision */
    if ((np=getnickbynick(nickname))!=NULL) {
      /* Make sure we will win the collision */
      newuser->timestamp=(np->timestamp-1);
      killuser(NULL, np, "Nick collision");
    }
    sendnickmsg(newuser);
  }
  
  if (handler!=NULL) {
    umhandlers[(currentlocalunum&MAXLOCALUSER)]=handler;  
  }

  *(gethandlebynumeric(newuser->numeric))=newuser;
  addnicktohash(newuser);
  triggerhook(HOOK_NICK_NEWNICK,newuser);
  
  return newuser;
}

/*
 * renamelocaluser:
 *  This function changes the name of a given local user 
 */

int renamelocaluser(nick *np, char *newnick) {
  nick *np2;
  char ipbuf[25];
  time_t timestamp=getnettime();
  
  if (!np)
    return -1;
  
  if (strlen(newnick) > NICKLEN)
    return -1;
  
  if (homeserver(np->numeric)!=mylongnum) 
    return -1;

  if ((np2=getnickbynick(newnick))) {
    if (np2==np) {
      /* Case only name change */
      strncpy(np->nick,newnick,NICKLEN);
      np->nick[NICKLEN]='\0';
      irc_send("%s N %s %d",iptobase64(ipbuf, &(np->p_ipaddr), sizeof(ipbuf), 1),np->nick,np->timestamp);
      triggerhook(HOOK_NICK_RENAME,np);     
      return 0;
    } else {
      /* Kill other user and drop through */
      timestamp=np2->timestamp-1;
      killuser(NULL, np2, "Nick collision");
    }
  }

  np->timestamp=timestamp;
  removenickfromhash(np);
  strncpy(np->nick,newnick,NICKLEN);
  np->nick[NICKLEN]='\0';
  addnicktohash(np);
  irc_send("%s N %s %d",longtonumeric(np->numeric,5),np->nick,np->timestamp);
  triggerhook(HOOK_NICK_RENAME,np);
  
  return 0;
}

/*
 * deregisterlocaluser:
 *  This function removes the given local user from the network
 */
 
int deregisterlocaluser(nick *np, char *reason) {
  long numeric;
  char reasonstr[512];
  void *harg[2];
  
  if (np==NULL || (homeserver(np->numeric)!=mylongnum)) {
    /* Non-existent user, or user not on this server */
    return -1;
  }
  
  if (reason==NULL || *reason=='\0') {
    sprintf(reasonstr,"Quit");
  } else {
    snprintf(reasonstr,510,"Quit: %s",reason);
  }
  
  harg[0]=np;
  harg[1]=reasonstr;
  
  triggerhook(HOOK_NICK_QUIT, harg);
  
  numeric=np->numeric;
  umhandlers[np->numeric&MAXLOCALUSER]=NULL;
  deletenick(np);
  if (connected) {
    irc_send("%s Q :%s",longtonumeric(numeric,5),reasonstr);
  }

  return 0;
}

/*
 * hooklocaluserhandler:
 *  This function adds a new handler to the hook chain for a local user
 *  THIS RELIES ON MODULES BEING UNLOADED IN THE CORRECT ORDER.
 */
UserMessageHandler hooklocaluserhandler(nick *np, UserMessageHandler newhandler) {
  UserMessageHandler oldhandler = NULL;
  if (np==NULL || (homeserver(np->numeric)!=mylongnum)) {
    /* Non-existent user, or user not on this server */
    return NULL;
  }
  oldhandler = umhandlers[np->numeric&MAXLOCALUSER];
  umhandlers[np->numeric&MAXLOCALUSER]=newhandler;
  return oldhandler;
}

/*
 * sendnickmsg:
 *  Sends details of a given local nick to the network.
 */

void sendnickmsg(nick *np) {
  char numericbuf[6];
  char ipbuf[25], operbuf[ACCOUNTLEN + 5];
  char accountbuf[100];

  strncpy(numericbuf,longtonumeric(np->numeric,5),5);
  numericbuf[5]='\0';

  if(IsOper(np) && (serverlist[myhub].flags & SMODE_OPERNAME)) {
    snprintf(operbuf,sizeof(operbuf)," %s",np->opername?np->opername->content:"-");
  } else {
    operbuf[0] = '\0';
  }

  accountbuf[0]='\0';
  if (IsAccount(np)) {
    if (np->auth) {
      if(np->auth->flags) {
        snprintf(accountbuf,sizeof(accountbuf)," %s:%ld:%lu:%llu",np->authname,np->accountts,np->auth->userid,np->auth->flags);
      } else {
        snprintf(accountbuf,sizeof(accountbuf)," %s:%ld:%lu",np->authname,np->accountts,np->auth->userid);
      }
    } else if(np->authname) {
      snprintf(accountbuf,sizeof(accountbuf)," %s:%ld:0",np->authname,np->accountts);
    }
  }

  irc_send("%s N %s 1 %ld %s %s %s%s%s %s %s :%s",
    mynumeric->content,np->nick,np->timestamp,np->ident,np->host->name->content,
    printflags(np->umodes,umodeflags),operbuf,accountbuf,iptobase64(ipbuf,&(np->p_ipaddr),
    sizeof(ipbuf),1),numericbuf,np->realname->name->content);
}

void sendnickburst(int hooknum, void *arg) {
  /* Send nick messages for all local users */
  nick **nh;
  int i;
  
  nh=servernicks[numerictolong(mynumeric->content,2)];
  for (i=0;i<=MAXLOCALUSER;i++) {
    if (nh[i]!=NULL) {
      sendnickmsg(nh[i]);
    }
  }
}

/* Check for a kill of a local user */
void checklocalkill(int hooknum, void *arg) {
  void **args=arg;
  nick *target=args[0];
  char *reason=args[1];
  long numeric;

  void *myargs[1];
  myargs[0]=reason;

  
  numeric=((nick *)target)->numeric;
  
  if (homeserver(numeric)==mylongnum) {
    if (umhandlers[(numeric)&MAXLOCALUSER]!=NULL) {
      (umhandlers[(numeric)&MAXLOCALUSER])((nick *)target,LU_KILLED,myargs);
    }
  }
} 

int handleprivatemsgcmd(void *source, int cargc, char **cargv) {
  return handlemessageornotice(source, cargc, cargv, 0);
}

int handleprivatenoticecmd(void *source, int cargc, char **cargv) {
  return handlemessageornotice(source, cargc, cargv, 1);
}

/* Handle privmsg or notice command addressed to user */
int handlemessageornotice(void *source, int cargc, char **cargv, int isnotice) {
  nick *sender;
  nick *target;
  char *ch;
  char targetnick[NICKLEN+1];
  int foundat;
  void *nargs[2];
  int i;
  
  /* Should have target and message */
  if (cargc<2) { 
    return CMD_OK;
  }

  if (cargv[0][0]=='#' || cargv[0][0]=='+') {
    /* Channel message/notice */
    return CMD_OK;
  }
  
  if ((sender=getnickbynumericstr((char *)source))==NULL) {
    Error("localuser",ERR_WARNING,"PRIVMSG from non existant user %s",(char *)source);
    return CMD_OK;
  }
  
  /* Check for a "secure" message (foo@bar) */
  foundat=0;
  for (i=0,ch=cargv[0];(i<=NICKLEN) && (*ch);i++,ch++) {
    if (*ch=='@') {
      targetnick[i]='\0';
      foundat=1;
      break;
    } else {
      targetnick[i]=*ch;
    }
  }
   
  if (!foundat) { /* Didn't find an @ sign, assume it's a numeric */
    if ((target=getnickbynumericstr(cargv[0]))==NULL) {
      Error("localuser",ERR_DEBUG,"Couldn't find target for %s",cargv[0]);
      return CMD_OK;
    } 
  } else { /* Did find @, do a lookup by nick */
    if ((target=getnickbynick(targetnick))==NULL) {   
      Error("localuser",ERR_DEBUG,"Couldn't find target for %s",cargv[0]);
      irc_send(":%s 401 %s %s :No such nick",myserver->content,sender->nick,cargv[0]);
      return CMD_OK;
    }
  }
  
  if (homeserver(target->numeric)!=mylongnum) {
    Error("localuser",ERR_WARNING,"Got message/notice for someone not on my server");
    irc_send(":%s 401 %s %s :No such nick",myserver->content,sender->nick,cargv[0]);
    return CMD_OK;
  }
  
  if (foundat && !IsService(target)) {   
    Error("localuser",ERR_DEBUG,"Received secure message for %s, but user is not a service",cargv[0]);
    irc_send(":%s 401 %s %s :No such nick",myserver->content,sender->nick,cargv[0]);
    return CMD_OK;
  }
  
  if (umhandlers[(target->numeric)&MAXLOCALUSER]==NULL) {
    /* No handler anyhow.. */
    return CMD_OK;    
  }
  
  /* Dispatch the message. */  
  nargs[0]=(void *)sender;
  nargs[1]=(void *)cargv[1];
  (umhandlers[(target->numeric)&MAXLOCALUSER])(target,isnotice?LU_PRIVNOTICE:(foundat?LU_SECUREMSG:LU_PRIVMSG),nargs);
  
  return CMD_OK;
}

/* Send message to user */
void sendmessagetouser(nick *source, nick *target, char *format, ... ) {
  char buf[BUFSIZE];
  char senderstr[6];
  va_list va;
  
  longtonumeric2(source->numeric,5,senderstr);
  
  va_start(va,format);
  /* 10 bytes of numeric, 5 bytes of fixed format + terminator = 17 bytes */
  /* So max sendable message is 495 bytes.  Of course, a client won't be able
   * to receive this.. */
   
  vsnprintf(buf,BUFSIZE-17,format,va);
  va_end(va);
  
  if (homeserver(target->numeric)!=mylongnum)
    irc_send("%s P %s :%s",senderstr,longtonumeric(target->numeric,5),buf);
}

/* Send messageto server, we don't check, but noones going to want to put a server pointer in anyway... */
void sendsecuremessagetouser(nick *source, nick *target, char *servername, char *format, ... ) {
  char buf[BUFSIZE];
  char senderstr[6];
  va_list va;
  
  longtonumeric2(source->numeric,5,senderstr);
  
  va_start(va,format);
  /* 10 bytes of numeric, 5 bytes of fixed format + terminator = 17 bytes */
  /* So max sendable message is 495 bytes.  Of course, a client won't be able
   * to receive this.. */
   
  vsnprintf(buf,BUFSIZE-17,format,va);
  va_end(va);
  
  if (homeserver(target->numeric)!=mylongnum)
    irc_send("%s P %s@%s :%s",senderstr,target->nick,servername,buf);
}

/* Send notice to user */
void sendnoticetouser(nick *source, nick *target, char *format, ... ) {
  char buf[BUFSIZE];
  char senderstr[6];
  va_list va;
  
  longtonumeric2(source->numeric,5,senderstr);
  
  va_start(va,format);
  /* 10 bytes of numeric, 5 bytes of fixed format + terminator = 17 bytes */
  /* So max sendable message is 495 bytes.  Of course, a client won't be able
   * to receive this.. */
   
  vsnprintf(buf,BUFSIZE-17,format,va);
  va_end(va);
  
  if (homeserver(target->numeric)!=mylongnum)
    irc_send("%s O %s :%s",senderstr,longtonumeric(target->numeric,5),buf);
}

/* Kill user */
void killuser(nick *source, nick *target, char *format, ... ) {
  char buf[BUFSIZE];
  va_list va;
  pendingkill *pk;

  va_start(va, format);
  vsnprintf(buf, BUFSIZE-17, format, va);
  va_end (va);

  if (hookqueuelength) {
    for (pk = pendingkilllist; pk; pk = pk->next)
      if (pk->target == target)
        return;

    Error("localuser", ERR_DEBUG, "Adding pending kill for %s", target->nick);
    pk = (pendingkill *)malloc(sizeof(pendingkill));
    pk->source = source;
    pk->target = target;
    pk->reason = getsstring(buf, BUFSIZE);
    pk->next = pendingkilllist;
    pendingkilllist = pk;
  } else {
    _killuser(source, target, buf);
  }
}

void sethostuser(nick *target, char *ident, char *host) {
  irc_send("%s SH %s %s %s", mynumeric->content, longtonumeric(target->numeric, 5), ident, host);
}

void _killuser(nick *source, nick *target, char *reason) {
  char senderstr[6];
  char sourcestring[HOSTLEN+NICKLEN+3];

  if (!source) {
    /* If we have a null nick, use the server.. */
    strcpy(senderstr, mynumeric->content);
    strcpy(sourcestring, myserver->content);
  } else {
    strcpy(senderstr, longtonumeric(source->numeric,5));
    sprintf(sourcestring,"%s!%s",source->host->name->content, source->nick);
  }

  irc_send("%s D %s :%s (%s)",senderstr,longtonumeric(target->numeric,5),sourcestring,reason);
  deletenick(target);
}

void clearpendingkills(int hooknum, void *arg) {
  pendingkill *pk;

  pk = pendingkilllist;
  while (pk) {
    pendingkilllist = pk->next;

    if (pk->target) {
      Error("localuser", ERR_DEBUG, "Processing pending kill for %s", pk->target->nick);
      _killuser(pk->source, pk->target, pk->reason->content);
    }

    freesstring(pk->reason);
    free(pk);
    pk = pendingkilllist;
  }
}

void checkpendingkills(int hooknum, void *arg) {
  nick *np = (nick *)arg;
  pendingkill *pk;

  for (pk=pendingkilllist; pk; pk = pk->next) {
    if (pk->source == np) {
      Error("localuser", ERR_INFO, "Pending kill source %s got deleted, NULL'ing source for pending kill", np->nick);
      pk->source = NULL;
    }
    if (pk->target == np) {
      Error("localuser", ERR_INFO, "Pending kill target %s got deleted, NULL'ing target for pending kill", np->nick);
      pk->target = NULL;
    }
  }
}

void sendaccountmessage(nick *np) {
  if (connected) {
    if (np->auth) {
      if (np->auth->flags) {
        irc_send("%s AC %s %s %ld %lu %llu",mynumeric->content, longtonumeric(np->numeric,5), np->authname, np->accountts, np->auth->userid, np->auth->flags);
      } else {
        irc_send("%s AC %s %s %ld %lu",mynumeric->content, longtonumeric(np->numeric,5), np->authname, np->accountts, np->auth->userid);
      }
    } else {
      irc_send("%s AC %s %s %ld 0",mynumeric->content, longtonumeric(np->numeric,5), np->authname, np->accountts);
    }
  }
}

/* Auth user, don't use to set flags after authing */
void localusersetaccount(nick *np, char *accname, unsigned long accid, u_int64_t accountflags, time_t authTS) {
  if (IsAccount(np)) {
    Error("localuser",ERR_WARNING,"Tried to set account on user %s already authed", np->nick);
    return;
  }

  SetAccount(np);
  np->accountts=authTS?authTS:getnettime();

  if (accid) {
    np->auth=findorcreateauthname(accid, accname);
    np->auth->usercount++;
    np->authname=np->auth->name;
    np->nextbyauthname=np->auth->nicks;
    np->auth->nicks=np;
    np->auth->flags=accountflags;
  } else {
    np->auth=NULL;
    np->authname=malloc(strlen(accname) + 1);
    strcpy(np->authname,accname);
  }

  sendaccountmessage(np);

  triggerhook(HOOK_NICK_ACCOUNT, np);
}

void localusersetumodes(nick *np, flag_t newmodes) {
  if (connected) {
    irc_send("%s M %s %s", longtonumeric(np->numeric,5), np->nick, printflagdiff(np->umodes, newmodes, umodeflags));
  }

  np->umodes = newmodes;
}

void localusersetaccountflags(authname *anp, u_int64_t accountflags) {
  void *arg[2];
  nick *np;
  u_int64_t oldflags = anp->flags;

  arg[0] = anp;
  arg[1] = &oldflags;
  anp->flags = accountflags;

  for(np=anp->nicks;np;np=np->next)
    sendaccountmessage(np);

  triggerhook(HOOK_AUTH_FLAGSUPDATED, arg);
}
