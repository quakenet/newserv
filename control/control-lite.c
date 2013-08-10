/* control.c */

/* 
 * This is the first client module for newserv :)
 *
 * A very simple bot which should give people some ideas for how to
 * implement stuff on this thing 
 */

#include "../irc/irc_config.h" 
#include "../parser/parser.h"
#include "../localuser/localuser.h"
#include "../localuser/localuserchannel.h"
#include "../nick/nick.h"
#include "../lib/sstring.h"
#include "../core/config.h"
#include "../irc/irc.h"
#include "../lib/splitline.h"
#include "../lib/flags.h"
#include "../core/schedule.h"
#include "../lib/base64.h"
#include "../core/modules.h"

#include <stdio.h>
#include <string.h>

nick *mynick;

nick *statsnick;

CommandTree *controlcmds; 

void handlemessages(nick *target, int messagetype, void **args);
int controlstatus(void *sender, int cargc, char **cargv);
void controlconnect(void *arg);
int controlwhois(void *sender, int cargc, char **cargv);
int relink(void *sender, int cargc, char **cargv);
int controlinsmod(void *sender, int cargc, char **cargv);
int controlrmmod(void *sender, int cargc, char **cargv);

void _init(void) {
  controlcmds=newcommandtree();
    
  addcommandtotree(controlcmds,"status",10,0,&controlstatus);
  addcommandtotree(controlcmds,"whois",10,1,&controlwhois);
  addcommandtotree(controlcmds,"relink",10,1,&relink);
  addcommandtotree(controlcmds,"insmod",10,1,&controlinsmod);
  addcommandtotree(controlcmds,"rmmod",10,1,&controlrmmod);
  scheduleoneshot(time(NULL)+1,&controlconnect,NULL);
}

void controlconnect(void *arg) {
  sstring *cnick, *myident, *myhost, *myrealname;

  cnick=getcopyconfigitem("control","nick","C",NICKLEN);
  myident=getcopyconfigitem("control","ident","control",NICKLEN);
  myhost=getcopyconfigitem("control","hostname",myserver->content,HOSTLEN);
  myrealname=getcopyconfigitem("control","realname","NewServ Control Service",REALLEN);

  mynick=registerlocaluser(cnick->content,myident->content,myhost->content,myrealname->content,NULL,UMODE_SERVICE|UMODE_DEAF|UMODE_OPER,&handlemessages);

  freesstring(cnick);
  freesstring(myident);
  freesstring(myhost);
  freesstring(myrealname);
}

void handlestats(int hooknum, void *arg) {
  sendmessagetouser(mynick,statsnick,(char *)arg);
}

int controlstatus(void *sender, int cargc, char **cargv) {
  statsnick=(nick *)sender;
  registerhook(HOOK_CORE_STATSREPLY,&handlestats);
  triggerhook(HOOK_CORE_STATSREQUEST,(void *)6);
  deregisterhook(HOOK_CORE_STATSREPLY,&handlestats);
  return CMD_OK;
}
  
int controlwhois(void *sender, int cargc, char **cargv) {
  nick *target;
  
  if (cargc<1) {
    sendmessagetouser(mynick,(nick *)sender,"Usage: whois <user>");
    return CMD_ERROR;
  }
  
  if ((target=getnickbynick(cargv[0]))==NULL) {
    sendmessagetouser(mynick,(nick *)sender,"Sorry, couldn't find that user");
    return CMD_ERROR;
  }
  
  sendmessagetouser(mynick,(nick *)sender,"Nick      : %s",target->nick);
  sendmessagetouser(mynick,(nick *)sender,"Numeric   : %s",longtonumeric(target->numeric,5));
  sendmessagetouser(mynick,(nick *)sender,"User@Host : %s@%s (%d user(s) on this host)",target->ident,target->host->name->content,target->host->clonecount);
  sendmessagetouser(mynick,(nick *)sender,"IP address: %s",IPtostr(target->ipaddress));
  sendmessagetouser(mynick,(nick *)sender,"Realname  : %s (%d user(s) have this realname)",target->realname->name->content,target->realname->usercount);
  if (target->umodes) {
    sendmessagetouser(mynick,(nick *)sender,"Umode(s)  : %s",printflags(target->umodes,umodeflags));
  }
  if (IsAccount(target)) {
    sendmessagetouser(mynick,(nick *)sender,"Account   : %s",target->authname);
  }
  
  return CMD_OK;
}

int controlinsmod(void *sender, int cargc, char **cargv) {
  if (cargc<1) {
    sendmessagetouser(mynick,(nick *)sender,"Usage: insmod <modulename>");
    return CMD_ERROR;
  }
  
  switch(insmod(cargv[0])) {
    case -1:
      sendmessagetouser(mynick,(nick *)sender,"Unable to load module %s",cargv[0]);
      return CMD_ERROR;
      
    case 1:
      sendmessagetouser(mynick,(nick *)sender,"Module %s already loaded, or name not valid",cargv[0]);
      return CMD_ERROR;
      
    case 0:
      sendmessagetouser(mynick,(nick *)sender,"Module %s loaded.",cargv[0]);
      return CMD_OK;
    
    default:
      sendmessagetouser(mynick,(nick *)sender,"An unknown error occured.");
      return CMD_ERROR;
  }
}

int controlrmmod(void *sender, int cargc, char **cargv) {
  if (cargc<1) {
    sendmessagetouser(mynick,(nick *)sender,"Usage: insmod <modulename>");
    return CMD_ERROR;
  }
  
  switch(rmmod(cargv[0])) {
    case 1:
      sendmessagetouser(mynick,(nick *)sender,"Module %s is not loaded.",cargv[0]);
      return CMD_ERROR;
      
    case 0:
      sendmessagetouser(mynick,(nick *)sender,"Module %s unloaded.",cargv[0]);
      return CMD_OK;
    
    default:
      sendmessagetouser(mynick,(nick *)sender,"An unknown error occured.");
      return CMD_ERROR;
  }
}

int relink(void *sender, int cargc, char **cargv) {
  if (cargc<1) {
    sendmessagetouser(mynick,(nick *)sender,"You must give a reason.");
    return CMD_ERROR;
  }
  
  irc_send("%s SQ %s 0 :%s",mynumeric->content,myserver->content,cargv[0]);
  irc_disconnected();
  
  return CMD_OK;
}
  
void handlemessages(nick *target, int messagetype, void **args) {
  Command *cmd;
  char *cargv[20];
  int cargc;
  nick *sender;
     
  switch(messagetype) {
    case LU_PRIVMSG:
    case LU_SECUREMSG:
      /* If it's a message, first arg is nick and second is message */
      sender=(nick *)args[0];

      /* Split the line into params */
      cargc=splitline((char *)args[1],cargv,20,0);

      cmd=findcommandintree(controlcmds,cargv[0],1);
      if (cmd==NULL) {
        sendmessagetouser(mynick,sender,"Unknown command.");
        return;
      }
      
      if (cmd->level>=10 && !IsOper(sender)) {
        sendmessagetouser(mynick,sender,"You need to be opered to use this command.");
        return;
      }
      
      /* If we were doing "authed user tracking" here we'd put a check in for authlevel */
      
      /* Check the maxargs */
      if (cmd->maxparams<(cargc-1)) {
        /* We need to do some rejoining */
        rejoinline(cargv[cmd->maxparams],cargc-(cmd->maxparams));
        cargc=(cmd->maxparams)+1;
      }
      
      (cmd->handler)((void *)sender,cargc-1,&(cargv[1]));
      break;
      
    case LU_KILLED:
      /* someone killed me?  Bastards */
      scheduleoneshot(time(NULL)+1,&controlconnect,NULL);
      break;
      
    default:
      break;
  }
}
