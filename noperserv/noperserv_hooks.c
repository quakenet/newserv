#include "../control/control.h"
#include "../localuser/localuser.h"
#include "../core/schedule.h"
#include "../core/modules.h"
#include "../lib/splitline.h"
#include "../lib/flags.h"
#include "../lib/irc_string.h"
#include "../lib/strlfunc.h"

#include "noperserv.h"
#include "noperserv_db.h"
#include "noperserv_hooks.h"
#include "noperserv_policy.h"

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

struct storedhook {
  CommandHandler old;
  sstring *name;
  char *oldhelp;
  char *newhelp;
  struct storedhook *next;
} storedhook;

struct storedhook *storedhooks = NULL;

int firsttime = 0;
nick *replynick = NULL;

UserMessageHandler oldhandler;
ControlMsg oldreply;
ControlWall oldwall;

void noperserv_trap_registration(int hooknum, void *arg);
int noperserv_showcommands(void *sender, int cargc, char **cargv);
int noperserv_rmmod(void *sender, int cargc, char **cargv);
int noperserv_reload(void *sender, int cargc, char **cargv);
int noperserv_whois(void *sender, int cargc, char **cargv);
int noperserv_help(void *sender, int cargc, char **cargv);
void noperserv_whois_handler(int hooknum, void *arg);
void noperserv_whois_account_handler(int hooknum, void *arg);
void noperserv_handle_messages(nick *target, int messagetype, void **args);
void noperserv_reply(nick *np, char *format, ...);
void noperserv_wall(flag_t permissionlevel, flag_t noticelevel, char *format, ...);

struct specialsched special;

#define HOOK_CONTROL_WHOISREQUEST_AUTHNAME -1
#define HOOK_CONTROL_WHOISREQUEST_AUTHEDUSER -2

void noperserv_setup_hooks(void) {
  oldreply = controlreply;
  controlreply = &noperserv_reply;
  
  oldwall = controlwall;
  controlwall = &noperserv_wall;

  memset(&special, 0, sizeof(struct specialsched));

  if(!mynick) {
    registerhook(HOOK_CONTROL_REGISTERED, &noperserv_trap_registration);
  } else {
    noperserv_trap_registration(0, (void *)mynick);
  }

  registerhook(HOOK_CONTROL_WHOISREQUEST, &noperserv_whois_handler);
}

int noperserv_hook_command(char *command, CommandHandler newcommand, char *newhelp) {
  struct storedhook *newhook;
  Command *fetchcommand = findcommandintree(controlcmds, command, 1);

  if(!fetchcommand)
    return 1;

  newhook = (struct storedhook *)malloc(sizeof(struct storedhook));
  if(!newhook)
    return 1;

  newhook->name = getsstring(command, strlen(command));
  if(!newhook->name) {
    free(newhook);
    return 1;
  }

  newhook->old = fetchcommand->handler;
  if(newhelp) {
    int len = strlen(newhelp) + 1;
    newhook->newhelp = (char *)malloc(len);
    if(!newhook->newhelp) {
      freesstring(newhook->name);
      free(newhook);
    }
    strlcpy(newhook->newhelp, newhelp, len);
    newhook->oldhelp = fetchcommand->help;
    fetchcommand->help = newhook->newhelp;
  } else {
    newhook->newhelp = NULL;
  }
  
  newhook->next = storedhooks;
  storedhooks = newhook;
  
  fetchcommand->handler = newcommand;

  return 0;
}

void noperserv_unhook_all_commands(void) {
  struct storedhook *nh, *ch = storedhooks;
  Command *fetchcommand;

  while(ch) {
    if(ch->old && (fetchcommand = findcommandintree(controlcmds, ch->name->content, 1))) {
      fetchcommand->handler = ch->old;
      if(ch->newhelp) {
        fetchcommand->help = ch->oldhelp;
        free(ch->newhelp);
      }
    }
    nh = ch->next;
    freesstring(ch->name);
    free(ch);
    ch = nh;
  }
}

void noperserv_cleanup_hooks(void) {
  deregisterhook(HOOK_CONTROL_WHOISREQUEST, &noperserv_whois_handler);
  deregisterhook(HOOK_CONTROL_REGISTERED, &noperserv_trap_registration);
  
  if(firsttime) {
    noperserv_unhook_all_commands();
    firsttime = 0;
  }

  if(oldhandler)
    hooklocaluserhandler(mynick, oldhandler);

  controlwall = oldwall;
  controlreply = oldreply;
}

void noperserv_trap_registration(int hooknum, void *arg) {
  oldhandler = hooklocaluserhandler((nick *)arg, &noperserv_handle_messages);
  if(!oldhandler)
    return;

  if(!firsttime) {
    firsttime = 1;
    noperserv_hook_command("rmmod", &noperserv_rmmod, NULL);
    noperserv_hook_command("reload", &noperserv_reload, NULL);
    noperserv_hook_command("showcommands", &noperserv_showcommands, NULL);
    noperserv_hook_command("whois", &noperserv_whois, "Usage: whois <nickname|#authname|*numeric>\nDisplays lots of information about the specified nickname, auth name or numeric.");
    noperserv_hook_command("help", &noperserv_help, NULL);
  }
}

CommandHandler noperserv_find_hook(char *command) {
  struct storedhook *hh = storedhooks;
  for(;hh;hh=hh->next)
    if(!ircd_strcmp(hh->name->content, command))
      return hh->old;

  return NULL;
}

int noperserv_modules_loaded(char *mask) {
  int i;
  char *ptr;

  for(i=0,ptr=lsmod(i);ptr;ptr=lsmod(++i))
    if(match2strings(mask, ptr))
      return 1;

  return 0;
}

int noperserv_specialmod(nick *np, char *command, ScheduleCallback reloadhandler, int cargc, char **cargv) {
  CommandHandler oldcommand = noperserv_find_hook(command);
  if(cargc < 1) {
    if(oldcommand)
      return oldcommand(np, cargc, cargv);
    return CMD_ERROR;
  }

  if(!strcmp(cargv[0], "noperserv")) {
    if(noperserv_modules_loaded("noperserv_*")) {
      controlreply(np, "NOT UNLOADING. Unload all dependencies first.");
      return CMD_ERROR;
    }
    if(special.schedule) {
      controlreply(np, "Previous attempt at un/reload still in progress.");
      return CMD_OK;
    } else {
      special.modulename = getsstring(cargv[0], strlen(cargv[0]));
      if(!special.modulename) {
        controlreply(np, "Unable to copy module name. Seek cow herd to trample on server.");
        return CMD_ERROR;
      } else {
        special.schedule = scheduleoneshot(time(NULL) + 1, reloadhandler, &special);
        if(!special.schedule) {
          freesstring(special.modulename);
          special.modulename = NULL;
          controlreply(np, "Unable to allocate schedule. Seek cow herd to trample on server.");
          return CMD_ERROR;
        }
        controlreply(np, "Special case un/reload in <1 second, no response will be sent, standby. . .");
        return CMD_OK;
      }
    }
  } else {
    if(oldcommand)
      return oldcommand(np, cargc, cargv);
    return CMD_ERROR;
  }
}

int noperserv_rmmod(void *sender, int cargc, char **cargv) {
  return noperserv_specialmod(sender, "rmmod", &controlspecialrmmod, cargc, cargv);
}

int noperserv_reload(void *sender, int cargc, char **cargv) {
  return noperserv_specialmod(sender, "reload", &controlspecialreloadmod, cargc, cargv);
}

void noperserv_whois_hook(int hooknum, void *arg) {
  controlreply(replynick, "%s", (char *)arg);
}

int noperserv_whois(void *sender, int cargc, char **cargv) {
  no_autheduser *au;
  nick *np = (nick *)sender;
  CommandHandler oldwhois = noperserv_find_hook("whois");

  if(cargc < 1) {
    if(oldwhois)
      return oldwhois(sender, cargc, cargv);
    return CMD_ERROR;
  }

  if(cargv[0][0] != '#') {
    if(cargv[0][0] == '*')
      cargv[0][0] = '#';
    if(oldwhois)
      return oldwhois(sender, cargc, cargv);
    return CMD_ERROR;
  }
  
  au = noperserv_get_autheduser(cargv[0] + 1);
  if(!au) {
    controlreply(np, "Account not registered.");
    return CMD_OK;
  }

  controlreply(np, "Account   : %s", au->authname->content);

  replynick = np;

  registerhook(HOOK_CONTROL_WHOISREPLY, &noperserv_whois_hook);
  noperserv_whois_account_handler(HOOK_CONTROL_WHOISREQUEST_AUTHEDUSER, (void *)au);
  deregisterhook(HOOK_CONTROL_WHOISREPLY, &noperserv_whois_hook);

  controlreply(np, "Flags     : %s", printflags(NOGetAuthLevel(au), no_userflags));

  return CMD_OK;
}

int noperserv_showcommands(void *sender, int cargc, char **cargv) {
  nick *np = (nick *)sender;
  Command *cmdlist[100];
  int i, n;
    
  n = getcommandlist(controlcmds, cmdlist, 100);
  
  controlreply(np, "The following commands are registered at present:");
  
  for(i=0;i<n;i++)
    controlreply(np, "%s (%s)", cmdlist[i]->command->content, printflags(cmdlist[i]->level, no_commandflags));

  controlreply(np, "End of list.");
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
  
  nickbuffer[0] = '\0';
  if(hooknum == HOOK_CONTROL_WHOISREQUEST_AUTHEDUSER) {
    /* we can just read out the authed user linked list */
    no_autheduser *au = (void *)arg;
    no_nicklist *nl = au->nick;
    
    if(nl)
      found = 1;

    for(;nl;nl=nl->next) {
      snprintf(accountspace, sizeof(accountspace), "%s%s", count++?", ":"", nl->nick->nick);
      strlcat(nickbuffer, accountspace, sizeof(nickbuffer));

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
          strlcat(nickbuffer, accountspace, sizeof(nickbuffer));

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

/* Obviously pinched from control.c */
void noperserv_handle_messages(nick *target, int messagetype, void **args) {
  Command *cmd;
  char *cargv[50];
  int cargc;
  nick *sender;

  switch(messagetype) {
    case LU_PRIVMSG: /* override these two commands only */
    case LU_SECUREMSG:
      /* If it's a message, first arg is nick and second is message */
      sender = (nick *)args[0];

      controlwall(NO_DEVELOPER, NL_ALL_COMMANDS, "From: %s!%s@%s%s%s: %s", sender->nick, sender->ident, sender->host->name->content, IsAccount(sender)?"/":"", IsAccount(sender)?sender->authname:"", (char *)args[1]);

      /* Split the line into params */
      cargc = splitline((char *)args[1], cargv, 50, 0);
      
      if(!cargc) /* Blank line */
        return;
       	
      cmd = findcommandintree(controlcmds,cargv[0],1);
      if(!cmd) {
        controlreply(sender, "Unknown command.");
        return;
      }
      
      /* If we were doing "authed user tracking" here we'd put a check in for authlevel */
      /* Here it is! */
      if (!noperserv_policy_command_permitted(cmd->level, sender)) {
        controlreply(sender, "Access denied.");
        return;
      }

      /* Check the maxargs */
      if(cmd->maxparams < (cargc - 1)) {
        /* We need to do some rejoining */
        rejoinline(cargv[cmd->maxparams], cargc - (cmd->maxparams));
        cargc = (cmd->maxparams) + 1;
      }
      
      if((cmd->handler)((void *)sender,cargc-1,&(cargv[1])) == CMD_USAGE)
        controlhelp(sender, cmd);

      break;
    default:
      if(oldhandler)
        oldhandler(target, messagetype, args);
      break;
  }
}

void noperserv_reply(nick *np, char *format, ...) {
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

int noperserv_help(void *sender, int cargc, char **cargv) {
  Command *cmd;
  nick *np = (nick *)sender;

  if(cargc < 1)
    return CMD_USAGE;

  cmd = findcommandintree(controlcmds, cargv[0], 1);
  if(!cmd) {
    controlreply(np, "Unknown command.");
    return CMD_ERROR;
  }
  
  if(!noperserv_policy_command_permitted(cmd->level, np)) {
    controlreply(np, "Access denied.");
    return CMD_ERROR;
  }

  controlhelp(np, cmd);
  return CMD_OK;
}

void noperserv_wall(flag_t permissionlevel, flag_t noticelevel, char *format, ...) {
  char buf[512];
  va_list va;
  no_autheduser *au = authedusers;
  no_nicklist *nl;
  char *flags = printflags(noticelevel, no_noticeflags) + 1;

  va_start(va, format);
  vsnprintf(buf, sizeof(buf), format, va);
  va_end(va);

  Error("noperserv", ERR_INFO, "$%s$ %s", flags, buf);

  for(;au;au=au->next) {
    if(NOGetNoticeLevel(au) & noticelevel) {
      for(nl=au->nick;nl;nl=nl->next)
        if(noperserv_policy_command_permitted(permissionlevel, nl->nick))
          controlreply(nl->nick, "$%s$ %s", flags, buf);
    }
  }
}
