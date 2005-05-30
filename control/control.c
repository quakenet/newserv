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
#include "../channel/channel.h"
#include "../lib/flags.h"
#include "../core/schedule.h"
#include "../lib/base64.h"
#include "../core/modules.h"
#include "control.h"

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

nick *mynick;

nick *statsnick;

CommandTree *controlcmds; 

void handlemessages(nick *target, int messagetype, void **args);
int controlstatus(void *sender, int cargc, char **cargv);
void controlconnect(void *arg);
int controlwhois(void *sender, int cargc, char **cargv);
int controlchannel(void *sender, int cargc, char **cargv);
int relink(void *sender, int cargc, char **cargv);
int die(void *sender, int cargc, char **cargv);
int controlinsmod(void *sender, int cargc, char **cargv);
int controlrmmod(void *sender, int cargc, char **cargv);
int controllsmod(void *sender, int cargc, char **cargv);
int controlrehash(void *sender, int cargc, char **cargv);
int controlshowcommands(void *sender, int cargc, char **cargv);
int controlreload(void *sender, int cargc, char **cargv);

void _init() {
  controlcmds=newcommandtree();
    
  addcommandtotree(controlcmds,"status",10,1,&controlstatus);
  addcommandtotree(controlcmds,"whois",10,1,&controlwhois);
  addcommandtotree(controlcmds,"channel",10,1,&controlchannel);
  addcommandtotree(controlcmds,"relink",10,1,&relink);
  addcommandtotree(controlcmds,"die",10,1,&die);
  addcommandtotree(controlcmds,"insmod",10,1,&controlinsmod);
  addcommandtotree(controlcmds,"rmmod",10,1,&controlrmmod);
  addcommandtotree(controlcmds,"lsmod",10,1,&controllsmod);
  addcommandtotree(controlcmds,"rehash",10,1,&controlrehash);
  addcommandtotree(controlcmds,"showcommands",10,0,&controlshowcommands);
  addcommandtotree(controlcmds,"reload",10,1,&controlreload);
  
  scheduleoneshot(time(NULL)+1,&controlconnect,NULL);
}

void registercontrolcmd(const char *name, int level, int maxparams, CommandHandler handler) {
  addcommandtotree(controlcmds,name,level,maxparams,handler);
}

int deregistercontrolcmd(const char *name, CommandHandler handler) {
  return deletecommandfromtree(controlcmds, name, handler);
}
  
void controlconnect(void *arg) {
  sstring *cnick, *myident, *myhost, *myrealname, *myauthname;
  channel *cp;

  cnick=getcopyconfigitem("control","nick","C",NICKLEN);
  myident=getcopyconfigitem("control","ident","control",NICKLEN);
  myhost=getcopyconfigitem("control","hostname",myserver->content,HOSTLEN);
  myrealname=getcopyconfigitem("control","realname","NewServ Control Service",REALLEN);
  myauthname=getcopyconfigitem("control","authname","C",ACCOUNTLEN);

  mynick=registerlocaluser(cnick->content,myident->content,myhost->content,myrealname->content,myauthname->content,UMODE_SERVICE|UMODE_DEAF|UMODE_OPER|UMODE_ACCOUNT,&handlemessages);
  cp=findchannel("#twilightzone");
  if (!cp) {
    localcreatechannel(mynick,"#twilightzone");
  } else {
    localjoinchannel(mynick,cp);
    localgetops(mynick,cp);
  }
  
  freesstring(cnick);
  freesstring(myident);
  freesstring(myhost);
  freesstring(myrealname);
  freesstring(myauthname);
}

void handlestats(int hooknum, void *arg) {
  sendmessagetouser(mynick,statsnick,"%s",(char *)arg);
}

int controlstatus(void *sender, int cargc, char **cargv) {
  unsigned int level=5;

  statsnick=(nick *)sender;
  
  if (cargc>0) {
    level=strtoul(cargv[0],NULL,10);
  }
  
  registerhook(HOOK_CORE_STATSREPLY,&handlestats);
  triggerhook(HOOK_CORE_STATSREQUEST,(void *)level);
  deregisterhook(HOOK_CORE_STATSREPLY,&handlestats);
  return CMD_OK;
}

int controlrehash(void *sender, int cargc, char **cargv) {
  nick *np=(nick *)sender;
  
  controlreply(np,"Rehashing the config file.");
  
  rehashconfig();
  triggerhook(HOOK_CORE_REHASH,(void *)0);
  return CMD_OK;
}
  
int controlwhois(void *sender, int cargc, char **cargv) {
  nick *target;
  channel **channels;
  char buf[BUFSIZE];
  int i;
  
  if (cargc<1) {
    sendmessagetouser(mynick,(nick *)sender,"Usage: whois <user>");
    return CMD_ERROR;
  }
  
  if (cargv[0][0]=='#') {
    if (!(target=getnickbynumericstr(cargv[0]+1))) {
      sendmessagetouser(mynick,sender,"Sorry, couldn't find numeric %s",cargv[0]+1);
      return CMD_ERROR;
    }
  } else {
    if ((target=getnickbynick(cargv[0]))==NULL) {
      sendmessagetouser(mynick,(nick *)sender,"Sorry, couldn't find that user");
      return CMD_ERROR;
    }
  }
  
  sendmessagetouser(mynick,(nick *)sender,"Nick      : %s",target->nick);
  sendmessagetouser(mynick,(nick *)sender,"Numeric   : %s",longtonumeric(target->numeric,5));
  sendmessagetouser(mynick,(nick *)sender,"User@Host : %s@%s (%d user(s) on this host)",target->ident,target->host->name->content,target->host->clonecount);
  if (IsSetHost(target)) {
    if (target->shident) {
      sendmessagetouser(mynick,(nick *)sender,"Fakehost  : %s@%s",target->shident->content, target->sethost->content);
    } else {
      sendmessagetouser(mynick,(nick *)sender,"Fakehost  : %s",target->sethost->content);
    }
  }
  sendmessagetouser(mynick,(nick *)sender,"Timestamp : %lu",target->timestamp);
  sendmessagetouser(mynick,(nick *)sender,"IP address: %d.%d.%d.%d",(target->ipaddress)>>24,(target->ipaddress>>16)&255, (target->ipaddress>>8)&255,target->ipaddress&255);
  sendmessagetouser(mynick,(nick *)sender,"Realname  : %s (%d user(s) have this realname)",target->realname->name->content,target->realname->usercount);
  if (target->umodes) {
    sendmessagetouser(mynick,(nick *)sender,"Umode(s)  : %s",printflags(target->umodes,umodeflags));
  }
  if (IsAccount(target)) {
    sendmessagetouser(mynick,(nick *)sender,"Account   : %s",target->authname);
    if (target->accountts) 
      sendmessagetouser(mynick,(nick *)sender,"AccountTS : %ld",target->accountts);
  }
  if (target->channels->cursi==0) {
    sendmessagetouser(mynick,(nick *)sender,"Channels  : none");
  } else if (target->channels->cursi>50) {
    sendmessagetouser(mynick,(nick *)sender,"Channels  : - (total: %d)",target->channels->cursi);
  } else {
    buf[0]='\0';
    channels=(channel **)target->channels->content;
    for (i=0;i<=target->channels->cursi;i++) {
      if (!((i==target->channels->cursi) || ((70-strlen(buf))<channels[i]->index->name->length && strlen(buf)>0))) {
        strcat(buf,channels[i]->index->name->content);
        strcat(buf," ");
      } else {
        if (strlen(buf)==0) {
          break;
        } else {
          sendmessagetouser(mynick,(nick *)sender,"Channels  : %s",buf);
          buf[0]='\0';
          i--;
        }
      }
    }
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
    sendmessagetouser(mynick,(nick *)sender,"Usage: rmmod <modulename>");
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

int controllsmod(void *sender, int cargc, char **cargv) {
  int i=0;
  char *ptr;

  if (cargc < 1) { /* list all loaded modules */
    ptr = lsmod(i);
    sendmessagetouser(mynick,(nick *)sender,"Module");
    while (ptr != NULL) {
      sendmessagetouser(mynick,(nick *)sender,"%s", ptr);
      ptr = lsmod(++i);
    }
  } else {
    ptr = lsmod(getindex(cargv[0]));
    sendmessagetouser(mynick,(nick *)sender,"Module \"%s\" %s", cargv[0], (ptr ? "is loaded." : "is NOT loaded."));
  }
  return CMD_OK;
}

int controlreload(void *sender, int cargc, char **cargv) {
  if (cargc<1) {
    sendmessagetouser(mynick,(nick *)sender,"Usage: reload <modulename>");
    return CMD_ERROR;
  }
    
  controlrmmod(sender, cargc, cargv);
  return controlinsmod(sender, cargc, cargv);
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

int die(void *sender, int cargc, char **cargv) {
  if (cargc<1) {
    sendmessagetouser(mynick,(nick *)sender,"You must give a reason.");
    return CMD_ERROR;
  }
  
  irc_send("%s SQ %s 0 :%s",mynumeric->content,myserver->content,cargv[0]);
  irc_disconnected();
  
  exit(0);
}

int controlchannel(void *sender, int cargc, char **cargv) {
  channel *cp;
  nick *np;
  chanban *cbp;
  char buf[BUFSIZE];
  char buf2[12];
  int i,j;
  
  if (cargc<1) {
    sendmessagetouser(mynick,(nick *)sender,"Usage: channel #chan");
    return CMD_ERROR;
  }
  
  if ((cp=findchannel(cargv[0]))==NULL) {
    sendmessagetouser(mynick,(nick *)sender,"Couldn't find channel: %s",cargv[0]);
    return CMD_ERROR;
  }
  
  if (IsLimit(cp)) {
    sprintf(buf2,"%d",cp->limit);
  }
  
  sendmessagetouser(mynick,(nick *)sender,"Channel : %s",cp->index->name->content);
  if (cp->topic) {
    sendmessagetouser(mynick,(nick *)sender,"Topic   : %s",cp->topic->content);
    sendmessagetouser(mynick,(nick *)sender,"T-time  : %ld [%s]",cp->topictime,ctime(&(cp->topictime)));
  }
  sendmessagetouser(mynick,(nick *)sender,"Mode(s) : %s %s%s%s",printflags(cp->flags,cmodeflags),IsLimit(cp)?buf2:"",
    IsLimit(cp)?" ":"",IsKey(cp)?cp->key->content:"");
  sendmessagetouser(mynick,(nick *)sender,"Users   : %d (hash size %d, utilisation %.1f%%); %d unique hosts",
    cp->users->totalusers,cp->users->hashsize,((float)(100*cp->users->totalusers)/cp->users->hashsize),
    countuniquehosts(cp));
  i=0;
  memset(buf,' ',90);
  buf[72]='\0';
  for (j=0;j<=cp->users->hashsize;j++) {
    if (i==4 || j==cp->users->hashsize) {
      if(i>0) {
        sendmessagetouser(mynick,(nick *)sender,"Users   : %s",buf);
      }
      i=0;
      memset(buf,' ',72);
      if (j==cp->users->hashsize) 
        break;
    }
    if (cp->users->content[j]!=nouser) {      
      np=getnickbynumeric(cp->users->content[j]);
      sprintf(&buf[i*18],"%c%c%-15s ",cp->users->content[j]&CUMODE_VOICE?'+':' ',
        cp->users->content[j]&CUMODE_OP?'@':' ', np?np->nick:"!BUG-NONICK!");
      i++;
      if (i<4) 
        buf[i*18]=' ';
    }
  }
    
  for (cbp=cp->bans;cbp;cbp=cbp->next) {
    sendmessagetouser(mynick,(nick *)sender,"Ban     : %s",bantostringdebug(cbp));
  }
  return CMD_OK;
}

int controlshowcommands(void *sender, int cargc, char **cargv) {
  nick *np=(nick *)sender;
  Command *cmdlist[100];
  int i,n;
    
  n=getcommandlist(controlcmds,cmdlist,100);
  
  controlreply(np,"The following commands are registered at present:");
  
  for(i=0;i<n;i++) {
    controlreply(np,"%s (level %d)",cmdlist[i]->command->content,cmdlist[i]->level);
  }

  controlreply(np,"End of list.");
  return CMD_OK;
}
  
void handlemessages(nick *target, int messagetype, void **args) {
  Command *cmd;
  char *cargv[50];
  int cargc;
  nick *sender;
     
  switch(messagetype) {
    case LU_PRIVMSG:
    case LU_SECUREMSG:
      /* If it's a message, first arg is nick and second is message */
      sender=(nick *)args[0];

      Error("control",ERR_INFO,"From: %s!%s@%s: %s",sender->nick,sender->ident,sender->host->name->content, (char *)args[1]);

      /* Split the line into params */
      cargc=splitline((char *)args[1],cargv,50,0);
      
      if (!cargc) {
        /* Blank line */
        return;
      } 
       	
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
      mynick=NULL;
      break;
      
    default:
      break;
  }
}

void controlreply(nick *target, char *message, ... ) {
  char buf[512];
  va_list va;
    
  if (mynick==NULL) {
    return;
  }
  
  va_start(va,message);
  vsnprintf(buf,512,message,va);
  va_end(va);
  
  sendmessagetouser(mynick,target,"%s",buf);
}

void controlchanmsg(channel *cp, char *message, ...) {
  char buf[512];
  va_list va;
  
  if (mynick==NULL) {
    return;
  }
  
  va_start(va,message);
  vsnprintf(buf,512,message,va);
  va_end(va);
  
  sendmessagetochannel(mynick,cp,"%s",buf);
}

void controlnotice(nick *target, char *message, ... ) {
  char buf[512];
  va_list va;
    
  if (mynick==NULL) {
    return;
  }
  
  va_start(va,message);
  vsnprintf(buf,512,message,va);
  va_end(va);
  
  sendnoticetouser(mynick,target,"%s",buf);
}

/* Send a notice to all opers, O(n) and a notice otherwise we'll get infinite loops with services */
void controlnoticeopers(char *format, ...) {
  int i;
  nick *np;
  char broadcast[BUFSIZE];
  va_list va;
  
  va_start(va, format);
  vsnprintf(broadcast, sizeof(broadcast), format, va);
  va_end(va);

  for(i=0;i<NICKHASHSIZE;i++)
    for(np=nicktable[i];np;np=np->next)
      if (IsOper(np))
        controlnotice(np, "%s", broadcast);
}
