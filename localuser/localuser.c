/* localuser.c */

#include "../nick/nick.h"
#include "../lib/base64.h"
#include "../irc/irc.h"
#include "../irc/irc_config.h"
#include "../core/hooks.h"
#include "../core/error.h"
#include "../lib/version.h"
#include "localuser.h"

#include <string.h>
#include <stdarg.h>
#include <stdio.h>

MODULE_VERSION("$Id")

int currentlocalunum;
UserMessageHandler umhandlers[MAXLOCALUSER+1];

void checklocalkill(int hooknum, void *nick);

void _init() {
  int i;
  
  for (i=0;i<=MAXLOCALUSER;i++) {
    umhandlers[i]=NULL;
  }
  currentlocalunum=1;
  registerhook(HOOK_IRC_SENDBURSTNICKS,&sendnickburst);
  registerhook(HOOK_NICK_LOSTNICK,&checklocalkill);
  registerserverhandler("P",&handleprivatemsgcmd,2);
  registerserverhandler("O",&handleprivatenoticecmd, 2);
}

/*
 * registerlocaluser:
 *  This function creates a local user, and broadcasts it's existence to the net (if connected).
 */

nick *registerlocaluser(char *nickname, char *ident, char *host, char *realname, char *authname, flag_t umodes, UserMessageHandler handler) {
  int i;  
  nick *newuser,*np; 
  
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
  newuser->ipaddress=(127<<24)+(1<<8)+((currentlocalunum%253)+1); /* Make it look like a valid addr on 127.0.1.0/24 */
  newuser->timestamp=getnettime();
  newuser->shident=NULL;
  newuser->sethost=NULL;
  newuser->marker=0;
  memset(newuser->exts, 0, MAXNICKEXTS * sizeof(void *));

  if (IsAccount(newuser)) {
    strncpy(newuser->authname,authname,ACCOUNTLEN);
  } else {
    newuser->authname[0]='\0';
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
      irc_send("%s N %s %d",longtonumeric(np->numeric,5),np->nick,np->timestamp);
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
  int defaultreason=0;
  long numeric;
  
  if (np==NULL || (homeserver(np->numeric)!=mylongnum)) {
    /* Non-existent user, or user not on this server */
    return -1;
  }
  
  if (reason==NULL || *reason=='\0') {
    defaultreason=1;
  }
  
  numeric=np->numeric;
  umhandlers[np->numeric&MAXLOCALUSER]=NULL;
  deletenick(np);
  if (connected) {
    irc_send("%s Q :Quit: %s",longtonumeric(numeric,5),(defaultreason?"Leaving":reason));
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

  strncpy(numericbuf,longtonumeric(np->numeric,5),5);
  numericbuf[5]='\0';
  
  irc_send("%s N %s 1 %ld %s %s %s%s%s %s %s :%s",
    mynumeric->content,np->nick,np->timestamp,np->ident,np->host->name->content,
    printflags(np->umodes,umodeflags),IsAccount(np)?" ":"",
    np->authname,longtonumeric(np->ipaddress,6),numericbuf,
    np->realname->name->content);
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
void checklocalkill(int hooknum, void *target) {
  long numeric;
  void *args[1];
  
  args[0]=NULL;
  
  numeric=((nick *)target)->numeric;
  
  if (homeserver(numeric)==mylongnum) {
    if (umhandlers[(numeric)&MAXLOCALUSER]!=NULL) {
      (umhandlers[(numeric)&MAXLOCALUSER])((nick *)target,LU_KILLED,args);
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
  char senderstr[6];
  char sourcestring[NICKLEN+USERLEN+3];
  va_list va;

  if (!source) {
    /* If we have a null nick, use the server.. */
    strcpy(senderstr, mynumeric->content);
    strcpy(sourcestring, myserver->content);
  } else {
    strcpy(senderstr, longtonumeric(source->numeric,5));
    sprintf(sourcestring,"%s!%s",source->host->name->content, source->nick);
  }

  va_start(va, format);
  vsnprintf(buf, BUFSIZE-17, format, va);
  va_end (va);

  irc_send("%s D %s :%s (%s)",senderstr,longtonumeric(target->numeric,5),sourcestring,buf);
  deletenick(target);
}

/* Auth user */
void localusersetaccount(nick *np, char *accname) {
  if (IsAccount(np)) {
    Error("localuser",ERR_WARNING,"Tried to set account on user %s already authed", np->nick);
    return;
  }

  SetAccount(np);
  strncpy(np->authname, accname, ACCOUNTLEN);
  np->authname[ACCOUNTLEN]='\0';

  if (connected) {
    irc_send("%s AC %s %s %ld",mynumeric->content, longtonumeric(np->numeric,5), np->authname, getnettime());
  }

  triggerhook(HOOK_NICK_ACCOUNT, np);
}

