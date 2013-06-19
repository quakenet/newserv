#include "versionscan.h"
#include "../lib/version.h"
#include "../glines/glines.h"

MODULE_VERSION("")

CommandTree* versionscan_commands;
nick* versionscan_nick;
int versionscannext;
int versionscan_mode;
vspattern* vspatterns;
vsauthdata* vsauths;
vsstatistic* vsstats;
unsigned long hcount=0;
unsigned long wcount=0;
unsigned long kcount=0;
unsigned long gcount=0;
schedule *vsconnect;

void versionscan_addstat(char* reply) {
  unsigned int replylen;
  unsigned long replycrc;
  vsstatistic* v, *pv;
  
  replylen = strlen(reply);
  replycrc = crc32i(reply);
  
  pv=NULL;
  for (v=vsstats; v; v=v->next) {
    if (v->replylen==replylen && v->replycrc==replycrc) {
      v->count++;
      return;
    }
    pv=v;
  }
  if (!pv) {
    vsstats=(vsstatistic*)malloc(sizeof(vsstatistic));
    vsstats->reply=(char*)malloc(replylen + 1);
    strcpy(vsstats->reply, reply);
    vsstats->replylen = replylen;
    vsstats->replycrc = replycrc;
    vsstats->count=1;
    vsstats->next=NULL;
  }
  else {
    pv->next=(vsstatistic*)malloc(sizeof(vsstatistic));
    pv->next->reply=(char*)malloc(replylen + 1);
    strcpy(pv->next->reply, reply);
    pv->next->replylen = replylen;
    pv->next->replycrc = replycrc;
    pv->next->count=1;
    pv->next->next=NULL;
  }
}

unsigned char versionscan_getlevelbyauth(char* auth) {
  vsauthdata* v;
  
  for (v=vsauths; v; v=v->next) {
    if (!ircd_strcmp(v->account, auth)) {
      return v->flags;
    }
  }
  return 0;
}

vsauthdata* versionscan_getauthbyauth(char* auth) {
  vsauthdata* v;
  
  for (v=vsauths; v; v=v->next) {
    if (!ircd_strcmp(v->account, auth)) {
      return v;
    }
  }
  return 0;
}

int IsVersionscanStaff(nick* np) {
  unsigned char level;
  
  if (!IsAccount(np)) {
    return 0;
  }
  level=versionscan_getlevelbyauth(np->authname);
  if (level & (VS_STAFF | VS_GLINE | VS_ADMIN)) {
    return 1;
  }
  return 0;
}

int IsVersionscanGlineAccess(nick* np) {
  unsigned char level;
  
  if (!IsAccount(np)) {
    return 0;
  }
  level=versionscan_getlevelbyauth(np->authname);
  if (level & (VS_GLINE | VS_ADMIN)) {
    return 1;
  }
  return 0;
}

int IsVersionscanAdmin(nick* np) {
  unsigned char level;
  
  if (!IsAccount(np)) {
    return 0;
  }
  level=versionscan_getlevelbyauth(np->authname);
  if (level & VS_ADMIN) {
    return 1;
  }
  return 0;
}

const char* versionscan_flagstochar(unsigned char flags) {
  static char outstring[50];
  int pos=0;
  
  outstring[pos++]='+';
  if (flags & VS_ADMIN) { outstring[pos++]='a'; }
  if (flags & VS_GLINE) { outstring[pos++]='g'; }
  if (flags & VS_STAFF) { outstring[pos++]='s'; }
  outstring[pos]='\0';
  
  return outstring;
}

void versionscan_addpattern(char* pattern, char* data, unsigned char action) {
  vspattern* v;
  
  if (!vspatterns) {
    vspatterns=(vspattern*)malloc(sizeof(vspattern));
    strncpy(vspatterns->pattern, pattern, VSPATTERNLEN);
    strncpy(vspatterns->data, data, VSDATALEN);
    vspatterns->action=action;
    vspatterns->next=0;
    vspatterns->hitcount=0;
    return;
  }
  
  for (v=vspatterns; v; v=v->next) {
    if (!v->next) {
      v->next=(vspattern*)malloc(sizeof(vspattern));
      strncpy(v->next->pattern, pattern, VSPATTERNLEN);
      strncpy(v->next->data, data, VSDATALEN);
      v->next->action=action;
      v->next->next=0;
      v->next->hitcount=0;
      return;
    }
  }
}

void versionscan_delpattern(char* pattern) {
  vspattern* v, *pv;
  
  pv=0;
  for (v=vspatterns; v; v=v->next) {
    if (!ircd_strcmp(v->pattern, pattern)) {
      if (pv) {
        pv->next=v->next;
        free(v);
      }
      else {
        vspatterns=v->next;
        free(v);
      }
      return;
    }
    pv=v;
  }
}

vspattern* versionscan_getpattern(char* pattern) {
  vspattern* v;
  
  for (v=vspatterns; v; v=v->next) {
    if (!ircd_strcmp(v->pattern, pattern)) {
      return v;
    }
  }
  return 0;
}

int versionscan_whois(void* sender, int cargc, char** cargv) {
  nick* np=(nick*)sender;
  nick* target;
  vsauthdata* v;
  
  if (cargc < 1) {
    sendnoticetouser(versionscan_nick, np, "Syntax: whois <nickname>");
    return CMD_ERROR;
  }
  if (!(target=getnickbynick(cargv[0]))) {
    sendnoticetouser(versionscan_nick, np, "No such nick.");
    return CMD_ERROR;
  }
  if (!IsAccount(target)) {
    sendnoticetouser(versionscan_nick, np, "%s is not authed with the network.", target->nick);
    return CMD_ERROR;
  }
  if (!(v=versionscan_getauthbyauth(target->authname))) {
    sendnoticetouser(versionscan_nick, np, "User %s is not in my database.", target->nick);
    return CMD_ERROR;
  }
  sendnoticetouser(versionscan_nick, np, "%s is authed as %s, with flags: %s", target->nick, target->authname, versionscan_flagstochar(v->flags));
  return CMD_OK;
}

int versionscan_showcommands(void* sender, int cargc, char** cargv) {
  nick* np=(nick*)sender;
  Command* cmdlist[150];
  int i, j;
  
  sendnoticetouser(versionscan_nick, np, "The following commands are registered at present:");
  j=getcommandlist(versionscan_commands, cmdlist, 150);
  for (i=0; i<j; i++) {
    if (cmdlist[i]->level & (VS_STAFF | VS_GLINE | VS_ADMIN)) {
      sendnoticetouser(versionscan_nick, np, "%s (%s)", cmdlist[i]->command->content, versionscan_flagstochar(cmdlist[i]->level));
    }
    else {
      sendnoticetouser(versionscan_nick, np, "%s", cmdlist[i]->command->content);
    }
  }
  sendnoticetouser(versionscan_nick, np, "End of list.");
  
  return CMD_OK;
}

int versionscan_help(void* sender, int cargc, char** cargv) {
  nick* np=(nick*)sender;
  
  if (cargc < 1) {
    sendnoticetouser(versionscan_nick, np, "Syntax: help <command>");
    return CMD_ERROR;
  }
  
  if (!strcasecmp(cargv[0], "help")) {
    sendnoticetouser(versionscan_nick, np, "Syntax: help <command>");
    sendnoticetouser(versionscan_nick, np, "Gives help on commands.");
    return CMD_OK;
  }
  if (!strcasecmp(cargv[0], "hello")) {
    sendnoticetouser(versionscan_nick, np, "Syntax: hello");
    sendnoticetouser(versionscan_nick, np, "Creates the first account on the bot.");
    return CMD_OK;
  }
  if (!strcasecmp(cargv[0], "scan")) {
    sendnoticetouser(versionscan_nick, np, "Syntax: scan <target>");
    sendnoticetouser(versionscan_nick, np, "Sends a version request to the specified target, which may be a nick or a channel.");
    return CMD_OK;
  }
  if (!strcasecmp(cargv[0], "broadcast")) {
    sendnoticetouser(versionscan_nick, np, "Syntax: broadcast [-f]");
    sendnoticetouser(versionscan_nick, np, "Send a network-wide CTCP version.");
    return CMD_OK;
  }
  if (!strcasecmp(cargv[0], "changelev")) {
    sendnoticetouser(versionscan_nick, np, "Syntax: changelev <nick> <level>");
    sendnoticetouser(versionscan_nick, np, "Changes a user's privileges.");
    sendnoticetouser(versionscan_nick, np, "+a -> admin access");
    sendnoticetouser(versionscan_nick, np, "+g -> g-line access");
    sendnoticetouser(versionscan_nick, np, "+s -> staff access");
    return CMD_OK;
  }
  if (!strcasecmp(cargv[0], "mode")) {
    sendnoticetouser(versionscan_nick, np, "Syntax: mode [<mode of operation>]");
    sendnoticetouser(versionscan_nick, np, "Where <mode of operation> is one of:");
    sendnoticetouser(versionscan_nick, np, "idle: do nothing");
    sendnoticetouser(versionscan_nick, np, "scan: scan newly connecting users and those targeted by the 'scan' command");
    sendnoticetouser(versionscan_nick, np, "stat: collect statistics after a network-wide CTCP version request");
    return CMD_OK;
  }
  if (!strcasecmp(cargv[0], "showcommands")) {
    sendnoticetouser(versionscan_nick, np, "Syntax: showcommands");
    sendnoticetouser(versionscan_nick, np, "Displays registered commands.");
    return CMD_OK;
  }
  if (!strcasecmp(cargv[0], "whois")) {
    sendnoticetouser(versionscan_nick, np, "Syntax: whois <nickname>");
    sendnoticetouser(versionscan_nick, np, "Display information about the specified user.");
    return CMD_OK;
  }
  if (!strcasecmp(cargv[0], "statistics")) {
    sendnoticetouser(versionscan_nick, np, "Syntax: statistics [<limit>]");
    sendnoticetouser(versionscan_nick, np, "Display statistics of collected CTCP version replies.");
    return CMD_OK;
  }
  if (!strcasecmp(cargv[0], "listpatterns")) {
    sendnoticetouser(versionscan_nick, np, "Syntax: listpatterns");
    sendnoticetouser(versionscan_nick, np, "Lists CTCP version reply patterns.");
    return CMD_OK;
  }
  if (!strcasecmp(cargv[0], "addpattern")) {
    sendnoticetouser(versionscan_nick, np, "Syntax: addpattern <pattern> <action> <data>");
    sendnoticetouser(versionscan_nick, np, "Adds a CTCP version reply pattern, where action is one of the following:");
    sendnoticetouser(versionscan_nick, np, "%d - warn", VS_WARN);
    sendnoticetouser(versionscan_nick, np, "%d - kill", VS_KILL);
    sendnoticetouser(versionscan_nick, np, "%d - g-line user@host", VS_GLUSER);
    sendnoticetouser(versionscan_nick, np, "%d - g-line *@host", VS_GLHOST);
    return CMD_OK;
  }
  if (!strcasecmp(cargv[0], "delpattern")) {
    sendnoticetouser(versionscan_nick, np, "Syntax: delpattern <pattern>");
    sendnoticetouser(versionscan_nick, np, "Deletes a CTCP version reply pattern.");
    return CMD_OK;
  }
  if (!strcasecmp(cargv[0], "status")) {
    sendnoticetouser(versionscan_nick, np, "Syntax: status");
    sendnoticetouser(versionscan_nick, np, "Gives various bits of information about the bot.");
    return CMD_OK;
  }
  
  return CMD_OK;
}

int versionscan_listpatterns(void* sender, int cargc, char** cargv) {
  nick* np=(nick*)sender;
  vspattern* v;
  
  for (v=vspatterns; v; v=v->next) {
    sendnoticetouser(versionscan_nick, np, "Pattern [%s]:", v->pattern);
    sendnoticetouser(versionscan_nick, np, "Data: %s", v->data);
    sendnoticetouser(versionscan_nick, np, "Action: %s", (v->action == VS_WARN)?"warn":(v->action == VS_KILL)?"kill":(v->action == VS_GLUSER)?"g-line user@host":"g-line *@host");
    sendnoticetouser(versionscan_nick, np, "Hit count: %lu", v->hitcount);
  }  
  sendnoticetouser(versionscan_nick, np, "End of list.");
  return CMD_OK;
}

int versionscan_addpatterncmd(void* sender, int cargc, char** cargv) {
  nick* np=(nick*)sender;
  int action;
  
  if (cargc < 3) {
    sendnoticetouser(versionscan_nick, np, "Syntax: addpattern <pattern> <action> <data>");
    return CMD_ERROR;
  }
  
  action=atoi(cargv[1]);
  if ((action < VS_WARN) || (action > VS_GLHOST)) {
    sendnoticetouser(versionscan_nick, np, "Action must be a number between 1 and 4.");
    return CMD_ERROR;
  }
  
  if (versionscan_getpattern(cargv[0])) {
    sendnoticetouser(versionscan_nick, np, "That pattern already exists.");
    return CMD_ERROR;
  }
  
  if ((action > VS_KILL) && !IsVersionscanGlineAccess(np)) {
    sendnoticetouser(versionscan_nick, np, "You are not allowed to add G-Lines.");
    return CMD_ERROR;
  }
  
  versionscan_addpattern(cargv[0], cargv[2], (unsigned char)action);
  sendnoticetouser(versionscan_nick, np, "Done.");
  return CMD_OK;
}

int versionscan_delpatterncmd(void* sender, int cargc, char** cargv) {
  nick* np=(nick*)sender;
  
  if (cargc < 1) {
    sendnoticetouser(versionscan_nick, np, "Syntax: delpattern <pattern>");
    return CMD_ERROR;
  }
  
  if (!versionscan_getpattern(cargv[0])) {
    sendnoticetouser(versionscan_nick, np, "That pattern does not exist.");
    return CMD_ERROR;
  }
  
  versionscan_delpattern(cargv[0]);
  sendnoticetouser(versionscan_nick, np, "Done.");
  return CMD_OK;
}

int versionscan_status(void* sender, int cargc, char** cargv) {
  nick* np=(nick*)sender;
  vspattern* v;
  int pcount=0; unsigned long chcount=0;
  
  for (v=vspatterns; v; v=v->next) {
    pcount++;
    chcount+=v->hitcount;
  }
  
  sendnoticetouser(versionscan_nick, np, "Patterns:       %d", pcount);
  sendnoticetouser(versionscan_nick, np, "Users hit:      %lu (%lu from current patterns)", hcount, chcount);
  sendnoticetouser(versionscan_nick, np, "Warnings given: %lu", wcount);
  sendnoticetouser(versionscan_nick, np, "Kills sent:     %lu", kcount);
  sendnoticetouser(versionscan_nick, np, "G-Lines set:    %lu", gcount);
  return CMD_OK;
}

int versionscan_hello(void* sender, int cargc, char** cargv) {
  nick* np=(nick*)sender;
  
  if (vsauths) {
    sendnoticetouser(versionscan_nick, np, "The hello command cannot be used after the first user account has been created.");
    return CMD_ERROR;
  }
  
  vsauths=(vsauthdata*)malloc(sizeof(vsauthdata));
  strncpy(vsauths->account, np->authname, ACCOUNTLEN);
  vsauths->flags=VS_STAFF | VS_GLINE | VS_ADMIN;
  vsauths->next=NULL;
  
  sendnoticetouser(versionscan_nick, np, "An account has been created for you with the following flags: %s.", versionscan_flagstochar(vsauths->flags));
  return CMD_OK;
}

int versionscan_changelev(void* sender, int cargc, char** cargv) {
  nick* np=(nick*)sender;
  vsauthdata* v;
  nick* target;
  unsigned char flags=0;
  int i; int plus=1;
  
  if (cargc < 2) {
    sendnoticetouser(versionscan_nick, np, "Syntax: changelev <nick> [+|-]<level>");
    return CMD_ERROR;
  }
  
  if (!(target=getnickbynick(cargv[0]))) {
    sendnoticetouser(versionscan_nick, np, "No such nick.");
    return CMD_ERROR;
  }
  
  if (!IsAccount(target)) {
    sendnoticetouser(versionscan_nick, np, "%s is not authed.", target->nick);
    return CMD_ERROR;
  }
  
  if ((v=versionscan_getauthbyauth(target->authname))) {
    i=0;
    if ((cargv[1][0] == '+') || (cargv[1][0] =='-')) {
      plus=(cargv[1][0] == '+')?1:0;
      i++;
      flags=v->flags;
    }
    for (; cargv[1][i]; i++) {
      switch (cargv[1][i]) {
      case 'a':
        flags=(plus)?flags | VS_ADMIN:flags & (~VS_ADMIN);
        break;
      case 'g':
        flags=(plus)?flags | VS_GLINE:flags & (~VS_GLINE);
        break;
      case 's':
        flags=(plus)?flags | VS_STAFF:flags & (~VS_STAFF);
        break;
      default:
        sendnoticetouser(versionscan_nick, np, "Invalid level '%c'.", cargv[1][i]);
        return CMD_ERROR;
        break;
      }
    }
    if (!flags) {
      vsauthdata* pv, *prevv;
      
      prevv=0;
      for (pv=vsauths; pv; pv++) {
        if (pv == v) {
          if (prevv) {
            prevv->next=pv->next;
            free(pv);
          }
          else {
            vsauths=pv->next;
            free(pv);
          }
        }
        prevv=pv;
      }
    }
    else {
      v->flags=flags;
    }
    sendnoticetouser(versionscan_nick, np, "Done.");
    return CMD_OK;
  }
  else {
    i=0;
    if ((cargv[1][0] == '+') || (cargv[1][0] =='-')) {
      plus=(cargv[1][0] == '+')?1:0;
      i++;
    }
    for (; cargv[1][i]; i++) {
      switch (cargv[1][i]) {
      case 'a':
        flags=(plus)?flags | VS_ADMIN:flags & (~VS_ADMIN);
        break;
      case 'g':
        flags=(plus)?flags | VS_GLINE:flags & (~VS_GLINE);
        break;
      case 's':
        flags=(plus)?flags | VS_STAFF:flags & (~VS_STAFF);
        break;
      default:
        sendnoticetouser(versionscan_nick, np, "Invalid level '%c'.", cargv[1][i]);
        return CMD_ERROR;
        break;
      }
    }
    if (flags) {
      for (v=vsauths; v; v=v->next) {
        if (!v->next) {
          v->next=(vsauthdata*)malloc(sizeof(vsauthdata));
          strncpy(v->next->account, target->authname, ACCOUNTLEN);
          v->next->flags=flags;
          v->next->next=0;
          sendnoticetouser(versionscan_nick, np, "Done.");
          return CMD_OK;
        }
      }
      sendnoticetouser(versionscan_nick, np, "Error adding user to database.");
    }
    else {
      sendnoticetouser(versionscan_nick, np, "No level specified.");
    }
  }
  
  return CMD_ERROR;
}

int versionscan_scan(void* sender, int cargc, char** cargv) {
  nick* np=(nick*)sender;
  nick* n;
  channel* cp;
  
  if (cargc < 1) {
    sendnoticetouser(versionscan_nick, np, "Syntax: scan <target>");
    return CMD_ERROR;
  }
  
  if (versionscan_mode != VS_SCAN) {
    sendnoticetouser(versionscan_nick, np, "Scanning of users is currently disabled.");
    return CMD_ERROR;
  }
  
  if (cargv[0][0] == '#') {
    if ((cp=findchannel(cargv[0]))) {
      sendmessagetochannel(versionscan_nick, cp, "\001VERSION\001");
      sendnoticetouser(versionscan_nick, np, "Done.");
    }
    else {
      sendnoticetouser(versionscan_nick, np, "No such channel.");
      return CMD_ERROR;
    }
  }
  else {
    if ((n=getnickbynick(cargv[0]))) {
      if (IsOper(n)) {
        sendnoticetouser(versionscan_nick, np, "Cannot scan IRC Operators.");
        return CMD_ERROR;
      }
      sendmessagetouser(versionscan_nick, n, "\001VERSION\001");
      sendnoticetouser(versionscan_nick, np, "Done.");
    }
    else {
      sendnoticetouser(versionscan_nick, np, "No such nick.");
      return CMD_ERROR;
    }
  }
  return CMD_OK;
}

int versionscan_modecmd(void* sender, int cargc, char** cargv) {
  nick* np=(nick*)sender;
  int oldmode=versionscan_mode;
  
  if (cargc < 1) {
    sendnoticetouser(versionscan_nick, np, "Currently running in %s mode.", (versionscan_mode == VS_SCAN)?"SCAN":(versionscan_mode == VS_STAT)?"STATISTICS":"IDLE");
    return CMD_OK;
  }
  
  if (!ircd_strcmp(cargv[0], "idle")) {
    versionscan_mode=VS_IDLE;
    sendnoticetouser(versionscan_nick, np, "Now operating in IDLE mode.");
  }
  else if (!ircd_strcmp(cargv[0], "scan")) {
    versionscan_mode=VS_SCAN;
    sendnoticetouser(versionscan_nick, np, "Now operating in SCAN mode.");
  }
  else if (!ircd_strcmp(cargv[0], "stat")) {
    versionscan_mode=VS_STAT;
    sendnoticetouser(versionscan_nick, np, "Now operating in STATISTICS mode.");
  }
  else {
    sendnoticetouser(versionscan_nick, np, "Invalid mode of operation.");
    return CMD_ERROR;
  }
  
  if (oldmode == VS_STAT) {
    vsstatistic* v, *nv;
    
    for (v=vsstats; v;) {
      nv=v->next;
      free(v->reply);
      free(v);
      v=nv;
    }
    vsstats=0;
  }
  
  return CMD_OK;
}

int versionscan_statistics(void* sender, int cargc, char** cargv) {
  nick* np=(nick*)sender;
  vsstatistic* v;
  long rlimit=0, limit=100;
  
  if (versionscan_mode != VS_STAT) {
    sendnoticetouser(versionscan_nick, np, "No statistics are available unless STATISTICS mode of operation is enabled.");
    return CMD_ERROR;
  }
  if (cargc) {
    limit=atoi(cargv[0]);
  }
  if ((limit < 1) || (limit > 500)) {
    sendnoticetouser(versionscan_nick, np, "Invalid results limit. Valid values are 1-500.");
    return CMD_ERROR;
  }
  sendnoticetouser(versionscan_nick, np, "Reply: [Count]:");
  for (v=vsstats; (v && (rlimit < limit)); v=v->next) {
    sendnoticetouser(versionscan_nick, np, "%s [%lu]", v->reply, v->count);
    rlimit++;
  }
  sendnoticetouser(versionscan_nick, np, "End of list - %lu results returned.", rlimit);
  return CMD_OK;
}

int versionscan_statsdump(void* sender, int cargc, char** cargv) {
  nick* np=(nick*)sender;
  vsstatistic* v;
  long rlimit=0;
  FILE *fout;
  
  if (versionscan_mode != VS_STAT) {
    sendnoticetouser(versionscan_nick, np, "No statistics are available unless STATISTICS mode of operation is enabled.");
    return CMD_ERROR;
  }
  if (!(fout=fopen("data/versionscanstats","w"))) {
    sendnoticetouser(versionscan_nick, np, "Unable to open save file.");
    return CMD_ERROR;
  }
  for (v=vsstats; v; v=v->next) {
    fprintf(fout, "%lu:%s\n", v->count, v->reply);
    rlimit++;
  }
  fclose(fout);
  sendnoticetouser(versionscan_nick, np, "%lu results saved.", rlimit);
  return CMD_OK;
}

int versionscan_broadcast(void* sender, int cargc, char** cargv) {
  nick* np=(nick*)sender;
  int force=0;
  
  if (cargc) {
    if (strcmp(cargv[0], "-f")) {
      sendnoticetouser(versionscan_nick, np, "Invalid flag.");
      return CMD_ERROR;
    }
    force=1;
  }
  
  if (versionscan_mode != VS_STAT) {
    if (!force) {
      sendnoticetouser(versionscan_nick, np, "Statistics collection mode is not currently enabled. Use the 'mode' command to change current mode of operation.");
      sendnoticetouser(versionscan_nick, np, "If you really wish to send a network-wide CTCP version whilst running in SCAN or IDLE mode, use the -f flag.");
      return CMD_ERROR;
    }
    sendnoticetouser(versionscan_nick, np, "Forcing network-wide CTCP version.");
  }
  
  irc_send("%s P $* :\001VERSION\001\r\n", longtonumeric(versionscan_nick->numeric, 5));
  sendnoticetouser(versionscan_nick, np, "Done.");
  
  return CMD_OK;
}

void versionscan_newnick(int hooknum, void* arg) {
  nick* np=(nick*)arg;
  
  /* ignore opers or auth'd users, helps cut down on spam during a burst */
  if (!(IsOper(np) || IsAccount(np)) && (versionscan_mode == VS_SCAN)) {
    sendmessagetouser(versionscan_nick, np, "\001VERSION\001");
  }
}

void versionscan_handler(nick* me, int type, void** args) {
  nick* sender;
  Command* cmd;
  char* cargv[50];
  int cargc;
  vspattern* v;
  char* p;

  switch (type) {
  case LU_PRIVMSG:
  case LU_SECUREMSG:
    /* nick */
    sender=args[0];
    
    if (!strncmp("\001VERSION", args[1], 8)) {
      sendnoticetouser(versionscan_nick, sender, "\001VERSION QuakeNet %s v%s.\001", VS_RNDESC, VS_VERSION);
      return;
    }
    
    cargc=splitline((char*)args[1], cargv, 50, 0);
    
    cmd=findcommandintree(versionscan_commands, cargv[0], 1);
    if (!cmd) {
      sendnoticetouser(versionscan_nick, sender, "Unknown command.");
      return;
    }
    
    if ((cmd->level & VS_AUTHED) && !IsAccount(sender)) {
      sendnoticetouser(versionscan_nick, sender, "Sorry, you need to be authed to use this command.");
      return;
    }
    
    if ((cmd->level & VS_OPER) && !IsOper(sender)) {
      sendnoticetouser(versionscan_nick, sender, "Sorry, you need to be opered to use this command.");
      return;
    }
    
    if (((cmd->level & VS_STAFF) && !IsVersionscanStaff(sender)) || 
        ((cmd->level & VS_GLINE) && !IsVersionscanGlineAccess(sender)) || 
        ((cmd->level & VS_ADMIN) && !IsVersionscanAdmin(sender))) {
      sendnoticetouser(versionscan_nick, sender, "Sorry, you do not have access to this command.");
      return;
    }
    
    if (cmd->maxparams < (cargc-1)) {
      /* We need to do some rejoining */
      rejoinline(cargv[cmd->maxparams], cargc-(cmd->maxparams));
      cargc=(cmd->maxparams)+1;
    }
    
    (cmd->handler)((void*)sender, cargc-1, &(cargv[1]));
    break;
  case LU_PRIVNOTICE:
    sender=args[0];
    
    if (strncmp("\001VERSION ", args[1], 9)) {
      break;
    }
    if ((p=strchr((char *)args[1] + 9, '\001'))) {
      *p++='\0';
    }
    if (versionscan_mode == VS_SCAN) {
      if (IsOper(sender)) {
        break;
      }
      for (v=vspatterns; v; v=v->next) {
        if (match2strings(v->pattern, (char *)args[1] + 9)) {
          v->hitcount++;
          hcount++;
          switch (v->action) {
          case VS_WARN:
            sendnoticetouser(versionscan_nick, sender, "%s", v->data);
            wcount++;
            break;
          case VS_KILL:
            killuser(versionscan_nick, sender, "%s", v->data);
            kcount++;
            break;
          case VS_GLUSER:
            glinesetbynick(sender, 3600, v->data, versionscan_nick->nick, GLINE_FORCE_IDENT);
            gcount++;
            break;
          case VS_GLHOST:
            glinesetbynick(sender, 3600, v->data, versionscan_nick->nick, 0);
            gcount++;
            break;
          default:
            /* oh dear, something's fucked */
            break;
          }
         break;
        }
      }
    }
    else if (versionscan_mode == VS_STAT) {
      versionscan_addstat((char *)args[1] + 9);
    }
    break;
  case LU_KILLED:
    versionscan_nick=NULL;
    scheduleoneshot(time(NULL)+1, &versionscan_createfakeuser, NULL);
    break;
  }
}

void versionscan_createfakeuser(void* arg) {
  channel* cp;
  char buf[200];
  
  vsconnect=NULL;
  sprintf(buf, "%s v%s", VS_RNDESC, VS_VERSION);
  versionscan_nick=registerlocaluser(VS_NICK, VS_IDENT, VS_HOST, buf, VS_AUTHNAME, UMODE_ACCOUNT | UMODE_DEAF | UMODE_OPER | UMODE_SERVICE, versionscan_handler);
  if ((cp=findchannel(OPER_CHAN))) {
    localjoinchannel(versionscan_nick, cp);
    localgetops(versionscan_nick, cp);
  } else {
    localcreatechannel(versionscan_nick, OPER_CHAN);
  }
}

void _init() {
  vspatterns=NULL;
  vsauths=NULL;
  vsstats=NULL;
  versionscan_mode=VS_IDLE;
  
  versionscan_commands=newcommandtree();
  
  addcommandtotree(versionscan_commands, "showcommands", VS_AUTHED | VS_STAFF, 0, versionscan_showcommands);
  addcommandtotree(versionscan_commands, "help", VS_AUTHED | VS_STAFF, 1, versionscan_help);
  addcommandtotree(versionscan_commands, "hello", VS_AUTHED | VS_OPER, 0, versionscan_hello);
  addcommandtotree(versionscan_commands, "scan", VS_AUTHED | VS_STAFF, 1, versionscan_scan);
  addcommandtotree(versionscan_commands, "changelev", VS_AUTHED | VS_OPER | VS_ADMIN, 2, versionscan_changelev);
  addcommandtotree(versionscan_commands, "listpatterns", VS_AUTHED | VS_STAFF | VS_OPER, 0, versionscan_listpatterns);
  addcommandtotree(versionscan_commands, "addpattern", VS_AUTHED | VS_STAFF | VS_OPER, 3, versionscan_addpatterncmd);
  addcommandtotree(versionscan_commands, "delpattern", VS_AUTHED | VS_OPER | VS_ADMIN, 1, versionscan_delpatterncmd);
  addcommandtotree(versionscan_commands, "status", VS_AUTHED | VS_OPER | VS_ADMIN, 0, versionscan_status);
  addcommandtotree(versionscan_commands, "mode", VS_AUTHED | VS_OPER | VS_ADMIN, 1, versionscan_modecmd);
  addcommandtotree(versionscan_commands, "statistics", VS_AUTHED | VS_OPER | VS_STAFF, 1, versionscan_statistics);
  addcommandtotree(versionscan_commands, "statsdump", VS_AUTHED | VS_OPER | VS_STAFF, 1, versionscan_statsdump);
  addcommandtotree(versionscan_commands, "broadcast", VS_AUTHED | VS_OPER | VS_ADMIN, 1, versionscan_broadcast);
  addcommandtotree(versionscan_commands, "whois", VS_AUTHED | VS_STAFF, 1, versionscan_whois);
  
  registerhook(HOOK_NICK_NEWNICK, &versionscan_newnick);
  
  vsconnect=scheduleoneshot(time(NULL)+1, &versionscan_createfakeuser, NULL);
}

void _fini() {
  void* p, *np;
  
  deregisterhook(HOOK_NICK_NEWNICK, &versionscan_newnick);
  
  if (vsconnect) {
    deleteschedule(vsconnect, &versionscan_createfakeuser, NULL);
    vsconnect=NULL;
  }
  
  if (versionscan_nick) {
    deregisterlocaluser(versionscan_nick, "Module unloaded.");
    versionscan_nick=NULL;
  }
  
  destroycommandtree(versionscan_commands);
  
  for (p=vspatterns; p;) {
    np=((vspattern*)p)->next;
    free(p);
    p=np;
  }
  for (p=vsauths; p;) {
    np=((vsauthdata*)p)->next;
    free(p);
    p=np;
  }
  for (p=vsstats; p;) {
    np=((vsstatistic*)p)->next;
    free(((vsstatistic*)p)->reply);
    free(p);
    p=np;
  }
}
