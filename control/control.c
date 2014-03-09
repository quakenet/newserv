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
#include "../lib/version.h"
#include "../lib/irc_string.h"
#include "control.h"
#include "control_policy.h"

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

MODULE_VERSION("");

nick *hooknick;

nick *mynick;

CommandTree *controlcmds;
DestroyExt controldestroyext;
int noperserv_ext;

const flag no_commandflags[] = {
    { 'o', __NO_OPER },
    { 't', __NO_TRUST },
    { 's', __NO_STAFF },
    { 'S', __NO_SEC },
    { 'd', __NO_DEVELOPER },
    { 'L', __NO_LEGACY },
    { 'O', __NO_OPERED },
    { 'r', __NO_AUTHED },
    { 'R', __NO_ACCOUNT },
    { 'Y', __NO_RELAY },
    { '\0', 0 }
  };

const flag no_userflags[] = {
    { 'o', __NO_OPER },
    { 't', __NO_TRUST },
    { 's', __NO_STAFF },
    { 'S', __NO_SEC },
    { 'd', __NO_DEVELOPER },
    { 'Y', __NO_RELAY },
    { '\0', 0 }
  };

const flag no_noticeflags[] = {
    { 'm', NL_MANAGEMENT },   /* hello, password, userflags, noticeflags */
    { 't', NL_TRUSTS },       /* trust stuff... */
    { 'k', NL_KICKKILLS },    /* KICK/KILL commands */
    { 'I', NL_MISC },         /* misc commands */
    { 'g', NL_GLINES },       /* GLINE commands */
    { 'G', NL_GLINES_AUTO },  /* automated gline messages */
    { 'h', NL_HITS },         /* Where a gline or kill is set automatically by the bot */
    { 'c', NL_CLONING },      /* Clone detection */
    { 'C', NL_CLEARCHAN },    /* When someone clearchans */
    { 'f', NL_FAKEUSERS },    /* Fakeuser addition */
    { 'b', NL_BROADCASTS },   /* Broadcast/mbroadcast/sbroadcast */
    { 'o', NL_OPERATIONS },   /* insmod/rmmod/etc */
    { 'O', NL_OPERING },      /* when someone opers */
    { 'n', NL_NOTICES },      /* turn off to receive notices instead of privmsgs */
    { 'A', NL_ALL_COMMANDS }, /* all commands sent */
    { 'a', NL_CLEANUP },      /* automated cleanup notices */
    { '\0', 0 }
  };

void controldestroycmdext(void *ext); 
void handlemessages(nick *target, int messagetype, void **args);
int controlstatus(void *sender, int cargc, char **cargv);
void controlconnect(void *arg);
int controlwhois(void *sender, int cargc, char **cargv);
int controllistusers(void *sender, int cargc, char **cargv);
int controlchannel(void *sender, int cargc, char **cargv);
int relink(void *sender, int cargc, char **cargv);
int die(void *sender, int cargc, char **cargv);
int controlinsmod(void *sender, int cargc, char **cargv);
int controllsmod(void *sender, int cargc, char **cargv);
int controlrehash(void *sender, int cargc, char **cargv);
int controlreload(void *sender, int cargc, char **cargv);
int controlhelpcmd(void *sender, int cargc, char **cargv);
void controlnoticeopers(flag_t permissionlevel, flag_t noticelevel, char *format, ...) __attribute__ ((format (printf, 3, 4)));
void controlnoticeopers(flag_t permissionlevel, flag_t noticelevel, char *format, ...);
int controlcheckpermitted(flag_t level, nick *user);
void handlesignal(int hooknum, void *arg);
void noperserv_oper_detection(int hooknum, void *arg);
void noperserv_whois_handler(int hooknum, void *arg);
void noperserv_whois_account_handler(int hooknum, void *arg);

#define HOOK_CONTROL_WHOISREQUEST_AUTHNAME -1
#define HOOK_CONTROL_WHOISREQUEST_AUTHEDUSER -2

static int init;

void _init() {
  if(!noperserv_load_db())
    return;

  controlcmds=newcommandtree();
  controldestroyext=&controldestroycmdext;

  registercontrolhelpcmd("status",NO_DEVELOPER,1,&controlstatus,"Usage: status ?level?\nDisplays status information, increasing level gives more verbose information.");
  registercontrolhelpcmd("whois",NO_OPERED,1,&controlwhois,"Usage: whois <nickname|#numeric>\nDisplays lots of information about the specified nickname or numeric.");
  registercontrolhelpcmd("listusers", NO_OPERED | NO_ACCOUNT, 2, &controllistusers, "Syntax: LISTUSERS\nLists all accounts.");
  registercontrolhelpcmd("channel",NO_OPER,1,&controlchannel,"Usage: channel <#channel>\nDisplays channel information.");
  registercontrolhelpcmd("relink",NO_DEVELOPER,1,&relink,"Usage: relink\nRelinks service to the network.");
  registercontrolhelpcmd("die",NO_DEVELOPER,1,&die,"Usage: die <reason>\nTerminates the service.");
  registercontrolhelpcmd("insmod",NO_DEVELOPER,1,&controlinsmod,"Usage: insmod <module>\nAdds a module to the running instance.");
  registercontrolhelpcmd("rmmod",NO_DEVELOPER,1,&controlrmmod,"Usage: rmmod <module>\nRemoves a module from the running instance.");
  registercontrolhelpcmd("lsmod",NO_OPER,0,&controllsmod,"Usage: lsmod\nLists currently running modules.");
  registercontrolhelpcmd("rehash",NO_DEVELOPER,1,&controlrehash,"Usage: rehash\nReloads configuration file.");
  registercontrolhelpcmd("showcommands",NO_ACCOUNT,0,&controlshowcommands,"Usage: showcommands\nShows all registered commands.");
  registercontrolhelpcmd("reload",NO_DEVELOPER,1,&controlreload,"Usage: reload <module>\nReloads specified module.");
  registercontrolhelpcmd("help",NO_ANYONE,1,&controlhelpcmd,"Usage: help <command>\nShows help for specified command.");
 
  registerhook(HOOK_CORE_REHASH, &handlesignal);
  registerhook(HOOK_CORE_SIGINT, &handlesignal);
  registerhook(HOOK_NICK_MODEOPER, &noperserv_oper_detection);
  registerhook(HOOK_CONTROL_WHOISREQUEST, &noperserv_whois_handler);

  noperserv_ext = registerauthnameext("noperserv", 1);

  scheduleoneshot(time(NULL)+1,&controlconnect,NULL);

  init = 1;
}

void _fini() {
  if (!init)
    return;

  deleteallschedules(&controlconnect);
  if (mynick) {
    deregisterlocaluser(mynick,"Leaving");
  }
  
  deregistercontrolcmd("status",&controlstatus);
  deregistercontrolcmd("whois",&controlwhois);
  deregistercontrolcmd("listusers", &controllistusers);
  deregistercontrolcmd("channel",&controlchannel);
  deregistercontrolcmd("relink",&relink);
  deregistercontrolcmd("die",&die);
  deregistercontrolcmd("insmod",&controlinsmod);
  deregistercontrolcmd("rmmod",&controlrmmod);
  deregistercontrolcmd("lsmod",&controllsmod);
  deregistercontrolcmd("rehash",&controlrehash);
  deregistercontrolcmd("showcommands",&controlshowcommands);
  deregistercontrolcmd("reload",&controlreload);
  deregistercontrolcmd("help",&controlhelpcmd);
  
  destroycommandtree(controlcmds);

  deregisterhook(HOOK_CORE_REHASH, &handlesignal);
  deregisterhook(HOOK_CORE_SIGINT, &handlesignal);
  deregisterhook(HOOK_NICK_MODEOPER, &noperserv_oper_detection);
  deregisterhook(HOOK_CONTROL_WHOISREQUEST, &noperserv_whois_handler);

  releaseauthnameext(noperserv_ext);

  noperserv_cleanup_db();
}

void registercontrolhelpcmd(const char *name, int level, int maxparams, CommandHandler handler, char *helpstr) {
  cmdhelp *help;
  Command *newcmd;

  newcmd = addcommandtotree(controlcmds,name,level,maxparams,handler);
 
  if (helpstr) {
    /* Allocate a help record */
    help=(cmdhelp *)malloc(sizeof(cmdhelp));
    if(!help) {
      Error("control",ERR_ERROR,"Malloc failed: registercontrolhelpcmd");
      return; 
    }
    memset((void *)help,0,sizeof(cmdhelp));

    /* this isn't an sstring atm, as sstring has a max length (512) */ 
    int len=strlen(helpstr);
    help->helpstr=(char *)malloc(len+1);
    if(help->helpstr) {
      strncpy(help->helpstr, helpstr, len);
      help->helpstr[len] = '\0';
    }
    newcmd->ext=(void *)help;
    newcmd->destroyext=controldestroyext;
  }
}

void registercontrolhelpfunccmd(const char *name, int level, int maxparams, CommandHandler handler, CommandHelp helpcmd) {
  cmdhelp *help;
  Command *newcmd;

  /* Allocate a help record */
  help=(cmdhelp *)malloc(sizeof(cmdhelp));
  if (!help) {
    Error("control",ERR_ERROR,"Malloc failed: registercontrolhelpfunccmd");
    return;
  }
  memset((void *)help,0,sizeof(cmdhelp));

  help->helpcmd=helpcmd;

  newcmd = addcommandexttotree(controlcmds,name,level,maxparams,handler, (void *)help);
  newcmd->destroyext=controldestroyext;
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

  mynick=registerlocaluser(cnick->content,myident->content,myhost->content,myrealname->content,myauthname->content,UMODE_SERVICE|UMODE_DEAF|UMODE_OPER|UMODE_ACCOUNT|UMODE_INV,&handlemessages);
  triggerhook(HOOK_CONTROL_REGISTERED, mynick);
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
  controlreply(hooknick,"%s",(char *)arg);
}

int controlstatus(void *sender, int cargc, char **cargv) {
  unsigned long level=999;
  hooknick=(nick *)sender;
  
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

void handlewhois(int hooknum, void *arg) {
  controlreply(hooknick,"%s",(char *)arg);
}

static int controlwhois_plain(void *sender, int cargc, char **cargv) {
  nick *target;
  channel **channels;
  char buf[BUFSIZE];
  int i;
  
  if (cargc<1)
    return CMD_USAGE;
  
  if (cargv[0][0]=='#') {
    if (!(target=getnickbynumericstr(cargv[0]+1))) {
      controlreply(sender,"Sorry, couldn't find numeric %s",cargv[0]+1);
      return CMD_ERROR;
    }
  } else {
    if ((target=getnickbynick(cargv[0]))==NULL) {
      controlreply((nick *)sender,"Sorry, couldn't find that user");
      return CMD_ERROR;
    }
  }
  
  controlreply((nick *)sender,"Nick      : %s",target->nick);
  controlreply((nick *)sender,"Numeric   : %s",longtonumeric(target->numeric,5));
  controlreply((nick *)sender,"User@Host : %s@%s (%d user(s) on this host)",target->ident,target->host->name->content,target->host->clonecount);
  if (IsSetHost(target)) {
    if (target->shident) {
      controlreply((nick *)sender,"Fakehost  : %s@%s",target->shident->content, target->sethost->content);
    } else {
      controlreply((nick *)sender,"Fakehost  : %s",target->sethost->content);
    }
  }
  controlreply((nick *)sender,"Timestamp : %lu",target->timestamp);

  /* HACK */
  {
    int ext = findnickext("signontracker");

    if(ext >= 0) {
      time_t signedon = (time_t)(target->exts[ext]);
      if(signedon) {
        controlreply((nick *)sender,"Signed on : %lu",signedon);
      } else {
        controlreply((nick *)sender,"Signed on : ???");
      }
    }
  }
  controlreply((nick *)sender,"IP address: %s",IPtostr(target->ipaddress));
  controlreply((nick *)sender,"Realname  : %s (%d user(s) have this realname)",target->realname->name->content,target->realname->usercount);
  if (target->umodes) {
    controlreply((nick *)sender,"Umode(s)  : %s",printflags(target->umodes,umodeflags));
  }
  if (IsOper(target) && target->opername)
    controlreply((nick *)sender,"Opered as : %s",target->opername->content);
  if (IsAccount(target)) {
    controlreply((nick *)sender,"Account   : %s",target->authname);
    if (target->accountts) 
      controlreply((nick *)sender,"AccountTS : %ld",target->accountts);
    if (target->auth)  {
      controlreply((nick *)sender,"UserID    : %ld",target->auth->userid);
      if (target->auth->flags) 
        controlreply((nick *)sender,"AccFlags  : %s",printflags(target->auth->flags,accountflags));
    }
  }

  if (target->away) {
    controlreply((nick *)sender, "Away      : %s",target->away->content);
  }

  hooknick=(nick *)sender;
  registerhook(HOOK_CONTROL_WHOISREPLY,&handlewhois);
  triggerhook(HOOK_CONTROL_WHOISREQUEST,target);
  deregisterhook(HOOK_CONTROL_WHOISREPLY,&handlewhois);

  if (target->channels->cursi==0) {
    controlreply((nick *)sender,"Channels  : none");
  } else if (target->channels->cursi>50) {
    controlreply((nick *)sender,"Channels  : - (total: %d)",target->channels->cursi);
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
          controlreply((nick *)sender,"Channels  : %s",buf);
          buf[0]='\0';
          i--;
        }
      }
    }
  }
  
  return CMD_OK;
}

int controlwhois(void *sender, int cargc, char **cargv) {
  authname *an;
  no_autheduser *au;
  nick *np = (nick *)sender;

  if(cargc < 1)
    return controlwhois_plain(sender, cargc, cargv);

  if(cargv[0][0] != '#') {
    if(cargv[0][0] == '*')
      cargv[0][0] = '#';
    return controlwhois_plain(sender, cargc, cargv);
  }

  an = findauthnamebyname(cargv[0] + 1);
  if(!an) {
    controlreply(np, "Account not registered.");
    return CMD_OK;
  }

  au = noperserv_get_autheduser(an);
  if(!au) {
    controlreply(np, "User does not have a NOperserv account.");
    return CMD_OK;
  }

  controlreply(np, "Account   : %s", au->authname->name);

  hooknick = np;

  registerhook(HOOK_CONTROL_WHOISREPLY, &handlewhois);
  noperserv_whois_account_handler(HOOK_CONTROL_WHOISREQUEST_AUTHEDUSER, (void *)au);
  deregisterhook(HOOK_CONTROL_WHOISREPLY, &handlewhois);

  controlreply(np, "Flags     : %s", printflags(NOGetAuthLevel(au), no_userflags));

  return CMD_OK;
}

void noperserv_whois_handler(int hooknum, void *arg) {
  char message[100];
  nick *np = (nick *)arg;
  no_autheduser *au;
  if(!np)
    return;

  if(IsAccount(np)) {
    au = NOGetAuthedUser(np);
    if(au) {
      snprintf(message, sizeof(message), "Flags     : %s", printflags(NOGetAuthLevel(au), no_userflags));
      noperserv_whois_account_handler(HOOK_CONTROL_WHOISREQUEST_AUTHEDUSER, (void *)au);
    } else {
      snprintf(message, sizeof(message), "Flags     : (user not known)");
      noperserv_whois_account_handler(HOOK_CONTROL_WHOISREQUEST_AUTHNAME, (void *)np->authname);
    }
    triggerhook(HOOK_CONTROL_WHOISREPLY, message);
  }
}

/* mmm, hacky */
void noperserv_whois_account_handler(int hooknum, void *arg) {
  int count = 0, found = 0;
  char nickbuffer[(NICKLEN + 2) * NO_NICKS_PER_WHOIS_LINE - 1]; /* since we don't need space or comma for the first item we're fine NULL wise */
  char accountspace[NICKLEN + 3]; /* space, comma, null */
  char message[1024];
  nick *np;

  nickbuffer[0] = '\0';
  if(hooknum == HOOK_CONTROL_WHOISREQUEST_AUTHEDUSER) {
    /* we can just read out the authed user linked list */
    no_autheduser *au = (void *)arg;

    if(au->authname->nicks)
      found = 1;

    for(np=au->authname->nicks;np;np=np->nextbyauthname) {
      snprintf(accountspace, sizeof(accountspace), "%s%s", count++?", ":"", np->nick);
      strncat(nickbuffer, accountspace, sizeof(nickbuffer));
      nickbuffer[sizeof(nickbuffer) - 1] = '\0';

      if(count >= NO_NICKS_PER_WHOIS_LINE) {
        snprintf(message, sizeof(message), "Authed    : %s", nickbuffer);
        triggerhook(HOOK_CONTROL_WHOISREPLY, message);
        nickbuffer[0] = '\0';
        count = 0;
      }
    }
  } else {
    /* inefficient way */
    char *authname = (char *)arg;
    int i = 0;
    nick *sp;

    for(;i<NICKHASHSIZE;i++)
      for(sp=nicktable[i];sp;sp=sp->next)
        if(IsAccount(sp) && !ircd_strcmp(sp->authname, authname)) {
          found = 1;

          snprintf(accountspace, sizeof(accountspace), "%s%s", count++?", ":"", sp->nick);
          strncat(nickbuffer, accountspace, sizeof(nickbuffer));
          nickbuffer[sizeof(nickbuffer) - 1] = '\0';

          if(count >= NO_NICKS_PER_WHOIS_LINE) {
            snprintf(message, sizeof(message), "Authed    : %s", nickbuffer);
            triggerhook(HOOK_CONTROL_WHOISREPLY, message);
            nickbuffer[0] = '\0';
            count = 0;
          }
        }
  }

  if(!found) {
    snprintf(message, sizeof(message), "Authed    : (no nicks authed)");
    triggerhook(HOOK_CONTROL_WHOISREPLY, message);
  } else if(nickbuffer[0]) {
    snprintf(message, sizeof(message), "Authed    : %s", nickbuffer);
    triggerhook(HOOK_CONTROL_WHOISREPLY, message);
  }
}

int controllistusers(void *sender, int cargc, char **cargv) {
  nick *np = (nick *)sender;
  int i, count = 0;
  authname *anp;
  no_autheduser *au;

  hooknick = np;

  registerhook(HOOK_CONTROL_WHOISREPLY, &handlewhois);

  for (i=0;i<AUTHNAMEHASHSIZE;i++) {
    for (anp=authnametable[i];anp;anp=anp->next) {
      au = noperserv_get_autheduser(anp);
      if(!au)
        continue;

      if (count > 0)
        controlreply(np, "---");

      controlreply(np, "Account   : %s", au->authname->name);
      noperserv_whois_account_handler(HOOK_CONTROL_WHOISREQUEST_AUTHEDUSER, (void *)au);
      controlreply(np, "Flags     : %s", printflags(NOGetAuthLevel(au), no_userflags));

      count++;
    }
  }

  deregisterhook(HOOK_CONTROL_WHOISREPLY, &handlewhois);

  controlreply(np, "--- Found %d users.", count);

  return CMD_OK;
}

int controlinsmod(void *sender, int cargc, char **cargv) {
  if (cargc<1)
    return CMD_USAGE;
  
  switch(insmod(cargv[0])) {
    case -1:
      controlreply((nick *)sender,"Unable to load module %s",cargv[0]);
      return CMD_ERROR;
      
    case 1:
      controlreply((nick *)sender,"Module %s already loaded, or name not valid",cargv[0]);
      return CMD_ERROR;
      
    case 0:
      controlreply((nick *)sender,"Module %s loaded.",cargv[0]);
      return CMD_OK;
    
    default:
      controlreply((nick *)sender,"An unknown error occured.");
      return CMD_ERROR;
  }
}

int controlrmmod(void *sender, int cargc, char **cargv) {
  if (cargc<1)
    return CMD_USAGE;
  
  switch(rmmod(cargv[0], 1)) {
    case 1:
      controlreply((nick *)sender,"Module %s is not loaded.",cargv[0]);
      return CMD_ERROR;
      
    case 0:
      controlreply((nick *)sender,"Module %s unloaded.",cargv[0]);
      return CMD_OK;
    
    default:
      controlreply((nick *)sender,"An unknown error occured.");
      return CMD_ERROR;
  }
}

int controllsmod(void *sender, int cargc, char **cargv) {
  int i=0;
  char *ptr;

  if (cargc < 1) { /* list all loaded modules */
    const char *ver, *buildid;
    time_t t, t2 = time(NULL);
    ptr = lsmod(i, &ver, &buildid, &t);

    /*                                                                    9999d 24h 59m 59s fbf2a4a69ee1-tip */
    controlreply((nick *)sender,"Module                                   Loaded for        Version                        Build id");
    while (ptr != NULL) {
      controlreply((nick *)sender," %-40s %-17s %-30s %s", ptr, longtoduration(t2-t, 2), ver?ver:"", buildid?buildid:"");
      ptr = lsmod(++i, &ver, &buildid, &t);
    }
  } else {
    ptr = lsmod(getindex(cargv[0]), NULL, NULL, NULL);
    controlreply((nick *)sender,"Module \"%s\" %s", cargv[0], (ptr ? "is loaded." : "is NOT loaded."));
  }
  return CMD_OK;
}

int controlreload(void *sender, int cargc, char **cargv) {
  if (cargc<1)
    return CMD_USAGE;

  controlreply((nick *)sender,"Imma gonna try and reload %s",cargv[0]);
  
  safereload(cargv[0]);
  
  return CMD_OK;
}  

int relink(void *sender, int cargc, char **cargv) {
  if (cargc<1) {
    controlreply((nick *)sender,"You must give a reason.");
    return CMD_USAGE;
  }
  
  irc_send("%s SQ %s 0 :%s",mynumeric->content,myserver->content,cargv[0]);
  irc_disconnected();
  
  return CMD_OK;
}

int die(void *sender, int cargc, char **cargv) {
  if (cargc<1 || (strlen(cargv[0]) < 10)) {
    controlreply((nick *)sender,"You must give a reason.");
    return CMD_USAGE;
  }

  controlwall(NO_OPER,NL_OPERATIONS,"DIE from %s: %s",((nick *)sender)->nick, cargv[0]);
  
  newserv_shutdown_pending=1;
  
  return CMD_OK;
}

int controlchannel(void *sender, int cargc, char **cargv) {
  channel *cp;
  nick *np;
  chanban *cbp;
  char buf[BUFSIZE];
  char buf2[12];
  int i,j, ops=0, voice=0;
  char timebuf[30];

  if (cargc<1)
    return CMD_USAGE;
  
  if ((cp=findchannel(cargv[0]))==NULL) {
    controlreply((nick *)sender,"Couldn't find channel: %s",cargv[0]);
    return CMD_ERROR;
  }
  
  if (IsLimit(cp)) {
    sprintf(buf2,"%d",cp->limit);
  }
  
  controlreply((nick *)sender,"Channel : %s",cp->index->name->content);
  strftime(timebuf, 30, "%d/%m/%y %H:%M", localtime(&(cp->timestamp)));
  controlreply((nick *)sender,"C-time  : %ld [%s]",cp->timestamp,timebuf);
  if (cp->topic) {
    controlreply((nick *)sender,"Topic   : %s",cp->topic->content);
    strftime(timebuf, 30, "%d/%m/%y %H:%M", localtime(&(cp->topictime)));
    controlreply((nick *)sender,"T-time  : %ld [%s]",cp->topictime,timebuf);
  } else { 
    controlreply((nick *)sender,"Topic   : (none)");
  }
  controlreply((nick *)sender,"Mode(s) : %s %s%s%s",printflags(cp->flags,cmodeflags),IsLimit(cp)?buf2:"",
    IsLimit(cp)?" ":"",IsKey(cp)?cp->key->content:"");
  controlreply((nick *)sender,"Users   : %d (hash size %d, utilisation %.1f%%); %d unique hosts",
    cp->users->totalusers,cp->users->hashsize,((float)(100*cp->users->totalusers)/cp->users->hashsize),
    countuniquehosts(cp));
  i=0;
  memset(buf,' ',90);
  buf[72]='\0';
  for (j=0;j<=cp->users->hashsize;j++) {
    if (i==4 || j==cp->users->hashsize) {
      if(i>0) {
        controlreply((nick *)sender,"Users   : %s",buf);
      }
      i=0;
      memset(buf,' ',72);
      if (j==cp->users->hashsize) 
        break;
    }
    if (cp->users->content[j]!=nouser) {      
      if (cp->users->content[j]&CUMODE_OP)
        ops++;
      else if (cp->users->content[j]&CUMODE_VOICE) 
        voice++;
      np=getnickbynumeric(cp->users->content[j]);
      sprintf(&buf[i*18],"%c%c%-15s ",cp->users->content[j]&CUMODE_VOICE?'+':' ',
        cp->users->content[j]&CUMODE_OP?'@':' ', np?np->nick:"!BUG-NONICK!");
      i++;
      if (i<4) 
        buf[i*18]=' ';
    }
  }
  controlreply((nick *)sender, "Users   : Opped: %d, Voiced: %d", ops,voice);
  for (cbp=cp->bans;cbp;cbp=cbp->next) {
    controlreply((nick *)sender,"Ban     : %s",bantostringdebug(cbp));
  }
  return CMD_OK;
}

int controlshowcommands(void *sender, int cargc, char **cargv) {
  nick *np = (nick *)sender;
  Command *cmdlist[100];
  int i, n;

  n = getcommandlist(controlcmds, cmdlist, 100);

  controlreply(np, "The following commands are registered at present:");

  for(i=0;i<n;i++)
    if(noperserv_policy_command_permitted(cmdlist[i]->level, np))
      controlreply(np, " %-25s %s", cmdlist[i]->command->content, printflags(cmdlist[i]->level, no_commandflags));

  controlreply(np, "End of list.");
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
      sender = (nick *)args[0];

      controlwall(NO_DEVELOPER, NL_ALL_COMMANDS, "From: %s!%s@%s%s%s: %s", sender->nick, sender->ident, sender->host->name->content, IsAccount(sender)?"/":"", IsAccount(sender)?sender->authname:"", (char *)args[1]);

      /* Split the line into params */
      cargc = splitline((char *)args[1], cargv, 50, 0);
      
      if(!cargc) /* Blank line */
        return;
       	
      cmd = findcommandintree(controlcmds,cargv[0],1);
      if(!cmd || !noperserv_policy_command_permitted(cmd->level, sender)) {
        controlreply(sender, "Unknown command or access denied.");
        return;
      }
      
      /* Check the maxargs */
      if (cmd->maxparams<(cargc-1)) {
        /* We need to do some rejoining */
        rejoinline(cargv[cmd->maxparams],cargc-(cmd->maxparams));
        cargc=(cmd->maxparams)+1;
      }
      
      if((cmd->handler)((void *)sender,cargc-1,&(cargv[1])) == CMD_USAGE)
        controlhelp(sender, cmd);

      break;
      
    case LU_KILLED:
      /* someone killed me?  Bastards */
      scheduleoneshot(time(NULL)+1,&controlconnect,NULL);
      mynick=NULL;
      triggerhook(HOOK_CONTROL_REGISTERED, NULL);

      break;
      
    default:
      break;
  }
}

void controlmessage(nick *target, char *message, ... ) {
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

void controlreply(nick *np, char *format, ...) {
  char buf[512];
  va_list va;
  no_autheduser *au = NOGetAuthedUser(np);

  va_start(va, format);
  vsnprintf(buf, sizeof(buf), format, va);
  va_end(va);

  if(au && !(NOGetNoticeLevel(au) & NL_NOTICES)) {
    controlmessage(np, "%s", buf);
  } else {
    controlnotice(np, "%s", buf);
  }
}

void controlwall(flag_t permissionlevel, flag_t noticelevel, char *format, ...) {
  char buf[512];
  va_list va;
  char *flags = printflags(noticelevel, no_noticeflags) + 1;
  int i;
  authname *anp;
  no_autheduser *au;
  nick *np;

  va_start(va, format);
  vsnprintf(buf, sizeof(buf), format, va);
  va_end(va);

  Error("noperserv", ERR_INFO, "$%s$ %s", flags, buf);

  for (i=0;i<AUTHNAMEHASHSIZE;i++) {
    for (anp=authnametable[i];anp;anp=anp->next) {
      au = noperserv_get_autheduser(anp);
      if(!au)
        continue;
      if((NOGetNoticeLevel(au) & noticelevel) && !(NOGetAuthLevel(au) & __NO_RELAY)) {
        for(np=anp->nicks;np;np=np->nextbyauthname)
          if(noperserv_policy_command_permitted(permissionlevel, np))
            controlreply(np, "$%s$ %s", flags, buf);
      }
    }
  }
}

void controlspecialrmmod(void *arg) {
  struct specialsched *a = (struct specialsched *)arg;
  sstring *froo = a->modulename;
  
  a->schedule = NULL;

  rmmod(froo->content, 1);
  freesstring(froo);
}

void controlspecialreloadmod(void *arg) {
  struct specialsched *a = (struct specialsched *)arg;
  sstring *froo = a->modulename;

  a->schedule = NULL;

  safereload(froo->content);
  freesstring(froo);
}

void controlhelp(nick *np, Command *cmd) {
  char *cp, *sp;
  char *scp;
  cmdhelp *help=(cmdhelp *)cmd->ext;

  if (!help) { 
    controlreply(np, "Sorry, no help for this command.");
    return;
  }
  if ( help->helpcmd ) {
    (help->helpcmd)(np, cmd); 
  } else {
    scp = help->helpstr;
    if (!scp) {
      controlreply(np, "Sorry, no help for this command.");
    } else {
      cp = scp;
      sp = cp;
      int finished = 0;
      for(;;cp++) {
        if(*cp == '\0' || *cp == '\n') {
          if(*cp == '\0') {
            finished = 1;
          } else {
            *cp = '\0';
          }

          if(sp != cp)
            controlreply(np, "%s", sp);

          if(finished)
            break;

          *cp = '\n';

          sp = cp + 1;
        }
      }
    }
  }
}

int controlhelpcmd(void *sender, int cargc, char **cargv) {
  Command *cmd;
  nick *np = (nick *)sender;

  if(cargc < 1)
    return CMD_USAGE;

  cmd = findcommandintree(controlcmds, cargv[0], 1);
  if(!cmd || !noperserv_policy_command_permitted(cmd->level, np)) {
    controlreply(np, "Unknown command or access denied.");
    return CMD_ERROR;
  }

  controlhelp(np, cmd);
  return CMD_OK;
}

void controlnoticeopers(flag_t permissionlevel, flag_t noticelevel, char *format, ...) {
  int i;
  nick *np;
  char broadcast[512];
  va_list va;

  va_start(va, format);
  vsnprintf(broadcast, sizeof(broadcast), format, va);
  va_end(va);

  for(i=0;i<NICKHASHSIZE;i++)
    for(np=nicktable[i];np;np=np->next)
      if (IsOper(np))
        controlnotice(np, "%s", broadcast);
}

void controlnswall(int noticelevel, char *format, ...) {
  char broadcast[512];
  va_list va;

  va_start(va, format);
  vsnprintf(broadcast, sizeof(broadcast), format, va);
  va_end(va);

  controlwall(NO_OPER, noticelevel, "%s", broadcast);
}

int controlpermitted(flag_t level, nick *user) {
  return noperserv_policy_command_permitted(level, user);
}

void handlesignal(int hooknum, void *arg) {
  char *signal, *action;

  if(hooknum == HOOK_CORE_SIGINT) {
    signal = "INT";
    action = "terminating";
  } else {
    long hupped = (long)arg;
    if(!hupped)
      return;

    signal = "HUP";
    action = "rehashing";
  }

  controlwall(NO_OPER, NL_OPERATIONS, "SIG%s received, %s...", signal, action);
}

void controldestroycmdext(void *ext) {
  if ( ((cmdhelp *)ext)->helpstr)
    free( ((cmdhelp *)ext)->helpstr);
  free(ext);
}

char *controlid(nick *np) {
  static char buf[512];

  snprintf(buf, sizeof(buf), "%s!%s@%s/%s", np->nick, np->ident, np->host->name->content, np->authname);

  return buf;
}

void noperserv_oper_detection(int hooknum, void *arg) {
  nick *np = (nick *)arg;

  if(np->umodes & UMODE_OPER) {
    if(np->opername && strcmp(np->opername->content, "-")) {
      controlwall(NO_OPER, NL_OPERING, "%s!%s@%s%s%s just OPERed as %s", np->nick, np->ident, np->host->name->content, IsAccount(np)?"/":"", IsAccount(np)?np->authname:"", np->opername->content);
    } else {
      controlwall(NO_OPER, NL_OPERING, "%s!%s@%s%s%s just OPERed", np->nick, np->ident, np->host->name->content, IsAccount(np)?"/":"", IsAccount(np)?np->authname:"");
    }
  } else {
    controlwall(NO_OPER, NL_OPERING, "%s!%s@%s%s%s just DEOPERed", np->nick, np->ident, np->host->name->content, IsAccount(np)?"/":"", IsAccount(np)?np->authname:"");
  }
}

