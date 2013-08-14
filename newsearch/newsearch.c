#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>

#include "../irc/irc_config.h"
#include "../lib/irc_string.h"
#include "../parser/parser.h"
#include "../control/control.h"
#include "../lib/splitline.h"
#include "../lib/version.h"
#include "../lib/stringbuf.h"
#include "../lib/strlfunc.h"
#include "../lib/array.h"
#include "newsearch.h"
#include "parser.h"

MODULE_VERSION("");

CommandTree *searchCmdTree;
searchList *globalterms = NULL;

int do_nicksearch(void *source, int cargc, char **cargv);
int do_chansearch(void *source, int cargc, char **cargv);
int do_usersearch(void *source, int cargc, char **cargv);
int do_whowassearch(void *source, int cargc, char **cargv);

void printnick_channels(searchCtx *, nick *, nick *);
void printchannel(searchCtx *, nick *, chanindex *);
void printchannel_topic(searchCtx *, nick *, chanindex *);
void printchannel_services(searchCtx *, nick *, chanindex *);

UserDisplayFunc defaultuserfn = printuser;
NickDisplayFunc defaultnickfn = printnick;
ChanDisplayFunc defaultchanfn = printchannel;
WhowasDisplayFunc defaultwhowasfn = printwhowas;

searchCmd *reg_nicksearch, *reg_chansearch, *reg_usersearch, *reg_whowassearch;
void displaycommandhelp(nick *, Command *);
void displaystrerror(replyFunc, nick *, const char *);

searchCmd *registersearchcommand(char *name, int level, CommandHandler cmd, void *defaultdisplayfunc) {
  searchCmd *acmd;
  searchList *sl;

  registercontrolhelpfunccmd(name, NO_OPER,4, cmd, &displaycommandhelp);  
  acmd=(struct searchCmd *)malloc(sizeof(struct searchCmd));
  if (!acmd) {
    Error("newsearch", ERR_ERROR, "malloc failed: registersearchcommand");
    return NULL;
  }
  acmd->handler = cmd;

  acmd->name = getsstring( name, NSMAX_COMMAND_LEN); 
  acmd->outputtree = newcommandtree();
  acmd->searchtree = newcommandtree();

  addcommandtotree(searchCmdTree, name, 0, 0, (CommandHandler)acmd);

  for (sl=globalterms; sl; sl=sl->next) {
    addcommandexttotree(acmd->searchtree, sl->name->content, 0, 1, (CommandHandler)sl->cmd, sl->help);
  }

  return acmd;
}

void deregistersearchcommand(searchCmd *scmd) {
  deregistercontrolcmd(scmd->name->content, (CommandHandler)scmd->handler);
  destroycommandtree(scmd->outputtree);
  destroycommandtree(scmd->searchtree);
  deletecommandfromtree(searchCmdTree, scmd->name->content, (CommandHandler) scmd);
  freesstring(scmd->name);
  free(scmd);
}

void displaycommandhelp(nick *np, Command *cmd) {
  int i,n,j,m;
  Command *cmdlist[100], *acmdlist[100];
  searchCmd *acmd;

  n=getcommandlist(searchCmdTree,cmdlist,100);
  for(i=0;i<n;i++) {
    /* note: we may want to only deregister a command if we've already registered it, for now, try de-registering new commands anyway */
    if ( ((searchCmd *)cmdlist[i]->handler)->handler != cmd->handler )
      continue;
    acmd = ((searchCmd *)(cmdlist[i]->handler)); 

    controlreply(np, "Usage: [flags] <criteria>\n");
    controlreply(np, "Flags:\n");
    controlreply(np, " -l int    : Limit number of rows of results\n");
    controlreply(np, " -d string : a valid output format for the results\n"); 
    controlreply(np, " -s subset : ipmask subset of network to search (only for node search)\n");
    controlreply(np, " \n");
    controlreply(np, "Available Output Formats:\n");
  
    m=getcommandlist(acmd->outputtree,acmdlist,100);
    for(j=0;j<m;j++) {
      if ( controlpermitted( acmdlist[j]->level, np) ) {
        char *help=(char *)acmdlist[j]->ext;
        if ( help && help[0] != '\0')
          controlreply(np, "%-10s: %s\n", acmdlist[j]->command->content, help);
        else 
          controlreply(np, "%s\n", acmdlist[j]->command->content ); 
      }
    }

    controlreply(np, " \n");
    controlreply(np, "Available Global Commands and Operators:\n" );
    m=getcommandlist(acmd->searchtree,acmdlist,100);
    for(j=0;j<m;j++) {
      if ( acmdlist[j]->maxparams) {
        char *help=(char *)acmdlist[j]->ext;
        if ( help && help[0] != '\0')
          controlreply(np, "%-10s: %s\n", acmdlist[j]->command->content, help );
        else
          controlreply(np, "%s\n", acmdlist[j]->command->content );
      }
    }

    controlreply(np, " \n");
    controlreply(np, "Available Commands and Operators for %s:\n", acmd->name->content);

    m=getcommandlist(acmd->searchtree,acmdlist,100);
    for(j=0;j<m;j++) {
      if ( !acmdlist[j]->maxparams && controlpermitted( acmdlist[j]->level, np) ) {
        char *help=(char *)acmdlist[j]->ext;
        if ( help && help[0] != '\0')
          controlreply(np, "%-10s: %s\n", acmdlist[j]->command->content, help );
        else
          controlreply(np, "%s\n", acmdlist[j]->command->content );
      }
    }
  }
}

void regdisp( searchCmd *cmd, const char *name, void *handler, int level, char *help) {
  addcommandexttotree(cmd->outputtree, name, level, 0, (CommandHandler) handler, (char *)help);
} 

void unregdisp( searchCmd *cmd, const char *name, void *handler ) {
  deletecommandfromtree(cmd->outputtree, name, (CommandHandler) handler);
}

void *findcommandinlist( searchList *sl, char *name){
  while (sl) {
    if(strcmp(sl->name->content,name) == 0 ) {
      return sl;
    }
    sl=sl->next;
  }
  return NULL;
} 

const char *parseError;
/* used for *_free functions that need to warn users of certain things
   i.e. hitting too many users in a (kill) or (gline) */
nick *senderNSExtern;

void _init() {
  searchCmdTree=newcommandtree();

  reg_nicksearch = (searchCmd *)registersearchcommand("nicksearch",NO_OPER,&do_nicksearch, printnick);
  reg_chansearch = (searchCmd *)registersearchcommand("chansearch",NO_OPER,&do_chansearch, printchannel);
  reg_usersearch = (searchCmd *)registersearchcommand("usersearch",NO_OPER,&do_usersearch, printuser);
  reg_whowassearch = (searchCmd *)registersearchcommand("whowassearch",NO_OPER,&do_whowassearch, printwhowas);

  /* Boolean operations */
  registerglobalsearchterm("and",and_parse, "usage: (and (X) (X))" );
  registerglobalsearchterm("not",not_parse, "usage: (not (X))");
  registerglobalsearchterm("or",or_parse, "usage: (or (X) (X))" );

  registerglobalsearchterm("eq",eq_parse, "usage: (eq (X) Y)");

  registerglobalsearchterm("lt",lt_parse, "usage: (lt (X) int)");
  registerglobalsearchterm("gt",gt_parse, "usage: (gt (X) int)");
 
  /* String operations */
  registerglobalsearchterm("match",match_parse, "usage: (match (X) string)");
  registerglobalsearchterm("regex",regex_parse, "usage: (regex (X) string)");
  registerglobalsearchterm("length",length_parse, "usage: (length string)");
  
  /* Nickname operations */
  registersearchterm(reg_nicksearch, "hostmask",hostmask_parse, 0, "The user's nick!user@host; \"hostmask real\" returns nick!user@host\rreal");
  registersearchterm(reg_whowassearch, "hostmask",hostmask_parse, 0, "The user's nick!user@host; \"hostmask real\" returns nick!user@host\rreal");

  registersearchterm(reg_nicksearch, "realname",realname_parse, 0, "User's current realname");     /* nick only */
  registersearchterm(reg_whowassearch, "realname",realname_parse, 0, "User's current realname");     /* nick only */

  registersearchterm(reg_nicksearch, "away",away_parse, 0, "User's current away message");
  registersearchterm(reg_whowassearch, "away",away_parse, 0, "User's current away message");
  registersearchterm(reg_nicksearch, "authname",authname_parse, 0, "User's current authname or false");
  registersearchterm(reg_whowassearch, "authname",authname_parse, 0, "User's current authname or false");
  registersearchterm(reg_nicksearch, "authts",authts_parse, 0, "User's Auth timestamp");
  registersearchterm(reg_whowassearch, "authts",authts_parse, 0, "User's Auth timestamp");
  registersearchterm(reg_nicksearch, "ident",ident_parse, 0, "User's current ident");
  registersearchterm(reg_whowassearch, "ident",ident_parse, 0, "User's current ident");
  registersearchterm(reg_nicksearch, "host",host_parse, 0, "User's host, allow \"host real\" to match real host");
  registersearchterm(reg_whowassearch, "host",host_parse, 0, "User's host, allow \"host real\" to match real host");             
  registersearchterm(reg_nicksearch, "channel",channel_parse, 0, "Valid Channel Name to match users against");       /* nick only */
  registersearchterm(reg_nicksearch, "timestamp",timestamp_parse, 0, "User's Timestamp");
  registersearchterm(reg_whowassearch, "timestamp",timestamp_parse, 0, "User's Timestamp");
  registersearchterm(reg_nicksearch, "country",country_parse, 0, "2 letter country code (data source is geoip)");
  registersearchterm(reg_nicksearch, "ip",ip_parse, 0, "User's IP - ipv4 or ipv6 format as appropriate. Note: not 6to4");
  registersearchterm(reg_whowassearch, "ip",ip_parse, 0, "User's IP - ipv4 or ipv6 format as appropriate. Note: not 6to4");
  registersearchterm(reg_nicksearch, "channels",channels_parse, 0, "Channel Count");     /* nick only */
  registersearchterm(reg_nicksearch, "server",server_parse, 0, "Server Name. Either (server string) or (match (server) string)");
  registersearchterm(reg_whowassearch, "server",server_parse, 0, "Server Name. Either (server string) or (match (server) string)");
  registersearchterm(reg_whowassearch, "server",server_parse, 0, "Server Name. Either (server string) or (match (server) string)");
  registersearchterm(reg_nicksearch, "authid",authid_parse, 0, "User's Auth ID");
  registersearchterm(reg_whowassearch, "authid",authid_parse, 0, "User's Auth ID");
  registersearchterm(reg_nicksearch, "cidr",cidr_parse, 0, "CIDR matching");
  registersearchterm(reg_whowassearch, "cidr",cidr_parse, 0, "CIDR matching");
  registersearchterm(reg_nicksearch, "ipvsix",ipv6_parse, 0, "IPv6 user");
  registersearchterm(reg_whowassearch, "ipvsix",ipv6_parse, 0, "IPv6 user");
  registersearchterm(reg_nicksearch, "message",message_parse, 0, "Last message");

  /* Whowas operations */
  registersearchterm(reg_whowassearch, "quit",quit_parse, 0, "User quit");
  registersearchterm(reg_whowassearch, "killed",killed_parse, 0, "User was killed");
  registersearchterm(reg_whowassearch, "renamed",renamed_parse, 0, "User changed nick");
  registersearchterm(reg_whowassearch, "age",age_parse, 0, "Whowas record age in seconds");
  registersearchterm(reg_whowassearch, "newnick",newnick_parse, 0, "New nick (for rename whowas records)");
  registersearchterm(reg_whowassearch, "reason",reason_parse, 0, "Quit/kill reason");

  /* Channel operations */
  registersearchterm(reg_chansearch, "exists",exists_parse, 0, "Returns if channel exists on network. Note: newserv may store data on empty channels");         /* channel only */
  registersearchterm(reg_chansearch, "services",services_parse, 0, "");     /* channel only */
  registersearchterm(reg_chansearch, "size",size_parse, 0, "Channel user count");             /* channel only */
  registersearchterm(reg_chansearch, "name",name_parse, 0, "Channel Name");             /* channel only */
  registersearchterm(reg_chansearch, "topic",topic_parse, 0, "Channel topic");           /* channel only */
  registersearchterm(reg_chansearch, "oppct",oppct_parse, 0, "Percentage Opped");           /* channel only */
  registersearchterm(reg_chansearch, "cumodecount",cumodecount_parse, 0, "Count of users with given channel modes");           /* channel only */
  registersearchterm(reg_chansearch, "cumodepct",cumodepct_parse, 0, "Percentage of users with given channel modes");           /* channel only */
  registersearchterm(reg_chansearch, "uniquehostpct",hostpct_parse, 0, "uniquehost percent"); /* channel only */
  registersearchterm(reg_chansearch, "authedpct",authedpct_parse, 0, "Percentage of authed users");   /* channel only */
  registersearchterm(reg_chansearch, "kick",kick_parse, 0, "KICK users channels in newsearch result. Note: evaluation order");             /* channel only */

  /* Nickname / channel operations */
  registersearchterm(reg_chansearch, "modes",modes_parse, 0, "Channel Modes");
  registersearchterm(reg_nicksearch, "modes",modes_parse, 0, "User Modes");
  registersearchterm(reg_whowassearch, "modes",modes_parse, 0, "User Modes");
  registersearchterm(reg_chansearch, "nick",nick_parse, 0, "Nickname");
  registersearchterm(reg_nicksearch, "nick",nick_parse, 0, "Nickname");
  registersearchterm(reg_whowassearch, "nick",nick_parse, 0, "Nickname");

  /* Kill / gline parameters */
  registersearchterm(reg_chansearch,"kill",kill_parse, 0, "KILL users in newsearch result. Note: evaluation order");
  registersearchterm(reg_chansearch,"gline",gline_parse, 0, "GLINE users in newsearch result. Note: evaluation order");
  registersearchterm(reg_nicksearch,"kill",kill_parse, 0, "KILL users in newsearch result. Note: evaluation order");
  registersearchterm(reg_nicksearch,"gline",gline_parse, 0, "GLINE users in newsearch result. Note: evaluation order");
  registersearchterm(reg_whowassearch,"gline",gline_parse, 0, "GLINE users in newsearch result. Note: evaluation order");

  /* Iteration functionality */
  registerglobalsearchterm("any",any_parse, "usage: any (generatorfn x) (fn ... (var x) ...)");
  registerglobalsearchterm("all",all_parse, "usage: all (generatorfn x) (fn ... (var x) ...)");
  registerglobalsearchterm("var",var_parse, "usage: var variable");
  
  /* Iterable functions */
  registersearchterm(reg_nicksearch, "channeliter",channeliter_parse, 0, "Channel Iterable function - usage: (any (channeliter x) (match (var x) #twilight*))");         /* nick only */
  registersearchterm(reg_chansearch, "nickiter",nickiter_parse, 0, "Nick Iterable function - usage: (any (nickiter x) (match (var x) nickname*))");         /* channel only */

  /* Functions that work on strings?! */
  registersearchterm(reg_nicksearch, "cumodes", cumodes_parse, 0, "usage: (cumodes (var x) <modes>)");
  registersearchterm(reg_chansearch, "cumodes", cumodes_parse, 0, "usage: (cumodes (var x) <modes>)");
    
  /* Notice functionality */
  registersearchterm(reg_chansearch,"notice",notice_parse, 0, "NOTICE users in newsearch result. Note: evaluation order");
  registersearchterm(reg_nicksearch,"notice",notice_parse, 0, "NOTICE users in newsearch result. Note: evaluation order");
 
  /* Nick output filters */
  regdisp(reg_nicksearch,"default",printnick, 0, "");
  regdisp(reg_nicksearch,"channels",printnick_channels, 0, "include channels in output");
    
  /* Channel output filters */
  regdisp(reg_chansearch,"default",printchannel, 0, "");
  regdisp(reg_chansearch,"topic",printchannel_topic, 0, "display channel topics");
  regdisp(reg_chansearch,"services",printchannel_services, 0, "display services on channels");

  /* Nick output filters */
  regdisp(reg_usersearch,"default",printuser, 0, "");
 
}

void _fini() {
  searchList *sl, *psl;
  int i,n;
  Command *cmdlist[100];

  sl=globalterms;
  while (sl) {
    psl = sl;
    sl=sl->next;

    n=getcommandlist(searchCmdTree,cmdlist,100);
    for(i=0;i<n;i++) {
      deregistersearchterm( (searchCmd *)cmdlist[i]->handler, psl->name->content, psl->cmd);
    }

    freesstring(psl->name);
    if (psl->help) 
      free (psl->help);
    free(psl);
  }

  deregistersearchcommand( reg_nicksearch );
  deregistersearchcommand( reg_chansearch );
  deregistersearchcommand( reg_usersearch );
  destroycommandtree( searchCmdTree );
}

void registerglobalsearchterm(char *term, parseFunc parsefunc, char *help) {
  int i,n;
  Command *cmdlist[100];
  searchList *sl = malloc(sizeof(searchList));
  if (!sl) {
    Error("newsearch", ERR_ERROR, "malloc failed: registerglobalsearchterm");
    return;
  }

  sl->cmd = parsefunc;
  sl->name = getsstring(term, NSMAX_COMMAND_LEN);
  sl->next = NULL;
  if (help) {
    int len=strlen(help);
    sl->help=(char *)malloc(len+1);
    if(!sl->help) {
      Error("newsearch", ERR_ERROR, "malloc failed: registerglobalsearchterm");
      return;
    }
      strncpy(sl->help, help, len);
      sl->help[len] = '\0';
  } else {
    sl->help=NULL;
  }


  if ( globalterms != NULL ) {
    sl->next = globalterms;
  }
  globalterms = sl;

  n=getcommandlist(searchCmdTree,cmdlist,100);
  for(i=0;i<n;i++) {
    /* maxparams is set to 1 to indicate a global term */
    /* access level is set to 0 for all global terms */
    addcommandexttotree( ((searchCmd *)cmdlist[i]->handler)->searchtree,term, 0, 1, (CommandHandler) parsefunc, help);
  }
}

void deregisterglobalsearchterm(char *term, parseFunc parsefunc) {
  int i,n;
  Command *cmdlist[100];
  searchList *sl, *psl=NULL;

  for (sl=globalterms; sl; sl=sl->next) {
    if ( strcmp( sl->name->content, term) == 0 ) {
      break;
    }
    psl = sl;
  }

  if (sl) {
    if( psl ) {
      psl->next = sl->next;
    } 

    n=getcommandlist(searchCmdTree,cmdlist,100);
    for(i=0;i<n;i++) {
      deletecommandfromtree( ((searchCmd *)cmdlist[i]->handler)->searchtree, term, (CommandHandler) parsefunc);
    }
    freesstring(sl->name);
    free(sl);
  }
}

void registersearchterm(searchCmd *cmd, char *term, parseFunc parsefunc, int level, char *help) {
  /* NOTE: global terms are added to the tree elsewhere as we set maxparams to 1 to indicate global */
  addcommandexttotree(cmd->searchtree, term, level, 0, (CommandHandler) parsefunc, help);
}

void deregistersearchterm(searchCmd *cmd, char *term, parseFunc parsefunc) {
  /* NOTE: global terms are removed from the tree within deregisterglobalsearchterm */
  deletecommandfromtree(cmd->searchtree, term, (CommandHandler) parsefunc);
}

static void controlwallwrapper(int level, char *format, ...) __attribute__ ((format (printf, 2,	 3)));
static void controlwallwrapper(int level, char *format, ...) {
  char buf[1024];
  va_list ap;

  va_start(ap, format);
  vsnprintf(buf, sizeof(buf), format, ap);
  controlwall(NO_OPER, level, "%s", buf);
  va_end(ap);
}

int parseopts(int cargc, char **cargv, int *arg, int *limit, void **subset, void *display, CommandTree *sl, replyFunc reply, void *sender) {
  char *ch;
  Command *cmd;
  struct irc_in_addr sin; unsigned char bits;
 
  if (*cargv[0] == '-') {
    /* options */
    (*arg)++;
    
    for (ch=cargv[0]+1;*ch;ch++) {
      switch(*ch) {
      case 'l':
	if (cargc<*arg) {
	  reply(sender,"Error: -l switch requires an argument (for help, see help <searchcmd>)");
	  return CMD_ERROR;
	}
	*limit=strtoul(cargv[(*arg)++],NULL,10);
	break;
	
      case 'd':
        if (cargc<*arg) {
          reply(sender,"Error: -d switch requires an argument (for help, see help <searchcmd>)");
          return CMD_ERROR;
        }
        cmd=findcommandintree(sl, cargv[*arg],1);
        if (!cmd) {
          reply(sender,"Error: unknown output format %s (for help, see help <searchcmd>)",cargv[*arg]);
          return CMD_ERROR;
        }
        if ( !controlpermitted( cmd->level, sender) ) {
          reply(sender,"Error: Access Denied for output format %s (for help, see help <searchcmd>)", cargv[*arg]);
          return CMD_ERROR;
        }
        *((void **)display)=(void *)cmd->handler;
        (*arg)++;
        break;

      case 's':
        if (subset == NULL) {
          reply(sender,"Error: -s switch not supported for this search.");
          return CMD_ERROR;
        }
        if (cargc<*arg) {
          reply(sender,"Error: -s switch requires an argument (for help, see help <searchcmd>)");
          return CMD_ERROR;
        }
        if (ipmask_parse(cargv[*arg], &sin, &bits) == 0) {
          reply(sender, "Error: Invalid CIDR mask supplied (for help, see help <searchcmd>)");
          return CMD_ERROR;
        }
        *subset = (void *)refnode(iptree, &sin, bits);
        (*arg)++;
        break;
        
      default:
	reply(sender,"Unrecognised flag -%c. (for help, see help <searchcmd>)",*ch);
        return CMD_ERROR;
      }
    }
  }

  return CMD_OK;
}

void newsearch_ctxinit(searchCtx *ctx, searchParseFunc searchfn, replyFunc replyfn, wallFunc wallfn, void *arg, searchCmd *cmd, nick *np, void *displayfn, int limit, array *targets) {
  memset(ctx, 0, sizeof(searchCtx));
  
  ctx->reply = replyfn;
  ctx->wall = wallfn;
  ctx->parser = searchfn;
  ctx->arg = arg;
  ctx->searchcmd = cmd;
  ctx->sender = np;
  ctx->limit = limit;
  ctx->targets = targets;
  ctx->displayfn = displayfn;
}

int do_nicksearch_real(replyFunc reply, wallFunc wall, void *source, int cargc, char **cargv) {
  nick *sender = source;
  int limit=500;
  int arg=0;
  NickDisplayFunc display=defaultnickfn;
  int ret;
  parsertree *tree;

  if (cargc<1) {
    reply( sender, "Usage: [flags] <criteria>");
    reply( sender, "For help, see help nicksearch");
    return CMD_OK;
  }
 
  ret = parseopts(cargc, cargv, &arg, &limit, NULL, (void *)&display, reg_nicksearch->outputtree, reply, sender);
  if(ret != CMD_OK)
    return ret;

  if (arg>=cargc) {
    reply(sender,"No search terms - aborting.");
    return CMD_ERROR;
  }

  if (arg<(cargc-1)) {
    rejoinline(cargv[arg],cargc-arg);
  }

  tree = parse_string(reg_nicksearch, cargv[arg]);
  if(!tree) {
    displaystrerror(reply, sender, cargv[arg]);
    return CMD_ERROR;
  }

  ast_nicksearch(tree->root, reply, sender, wall, display, NULL, NULL, limit, NULL);

  parse_free(tree);

  return CMD_OK;
}

int do_nicksearch(void *source, int cargc, char **cargv) {
  return do_nicksearch_real(controlreply, controlwallwrapper, source, cargc, cargv);
}

void nicksearch_exe(struct searchNode *search, searchCtx *ctx) {
  int i, j, k;
  int matches = 0;
  unsigned int cmarker;
  unsigned int tchans=0,uchans=0;
  struct channel **cs;
  nick *np, *sender = ctx->sender;
  senderNSExtern = sender;
  NickDisplayFunc display = ctx->displayfn;
  int limit = ctx->limit;

  /* Get a marker value to mark "seen" channels for unique count */
  cmarker=nextchanmarker();
  
  /* The top-level node needs to return a BOOL */
  search=coerceNode(ctx, search, RETURNTYPE_BOOL);
  
  for (i=0;i<NICKHASHSIZE;i++) {
    for (np=nicktable[i], k = 0;ctx->targets ? (k < ctx->targets->cursi) : (np != NULL);np=np->next, k++) {
      if (ctx->targets) {
        np = ((nick **)ctx->targets->content)[k];
        if (!np)
          break;
      }

      if ((search->exe)(ctx, search, np)) {
        /* Add total channels */
        tchans += np->channels->cursi;
        
        /* Check channels for uniqueness */
        cs=(channel **)np->channels->content;
        for (j=0;j<np->channels->cursi;j++) {
          if (cs[j]->index->marker != cmarker) {
            cs[j]->index->marker=cmarker;
            uchans++;
          }
        }
          
	if (matches<limit)
	  display(ctx, sender, np);
	  
	if (matches==limit)
	  ctx->reply(sender, "--- More than %d matches, skipping the rest",limit);
	matches++;
      }
    }

    if (ctx->targets)
      break;
  }

  ctx->reply(sender,"--- End of list: %d matches; users were on %u channels (%u unique, %.1f average clones)", 
                matches, tchans, uchans, (float)tchans/uchans);
}

int do_whowassearch_real(replyFunc reply, wallFunc wall, void *source, int cargc, char **cargv) {
  nick *sender = source;
  int limit=500;
  int arg=0;
  WhowasDisplayFunc display=defaultwhowasfn;
  int ret;
  parsertree *tree;

  if (cargc<1) {
    reply( sender, "Usage: [flags] <criteria>");
    reply( sender, "For help, see help whowassearch");
    return CMD_OK;
  }

  ret = parseopts(cargc, cargv, &arg, &limit, NULL, (void *)&display, reg_whowassearch->outputtree, reply, sender);
  if(ret != CMD_OK)
    return ret;

  if (arg>=cargc) {
    reply(sender,"No search terms - aborting.");
    return CMD_ERROR;
  }

  if (arg<(cargc-1)) {
    rejoinline(cargv[arg],cargc-arg);
  }

  tree = parse_string(reg_whowassearch, cargv[arg]);
  if(!tree) {
    displaystrerror(reply, sender, cargv[arg]);
    return CMD_ERROR;
  }

  ast_whowassearch(tree->root, reply, sender, wall, display, NULL, NULL, limit, NULL);

  parse_free(tree);

  return CMD_OK;
}

int do_whowassearch(void *source, int cargc, char **cargv) {
  return do_whowassearch_real(controlreply, controlwallwrapper, source, cargc, cargv);
}

void whowassearch_exe(struct searchNode *search, searchCtx *ctx) {
  int i, matches = 0;
  whowas *ww;
  nick *sender = ctx->sender;
  senderNSExtern = sender;
  WhowasDisplayFunc display = ctx->displayfn;
  int limit = ctx->limit;

  assert(!ctx->targets);

  /* The top-level node needs to return a BOOL */
  search=coerceNode(ctx, search, RETURNTYPE_BOOL);

  for (i = whowasoffset; i < whowasoffset + WW_MAXENTRIES; i++) {
    ww = &whowasrecs[i % WW_MAXENTRIES];

    if (ww->type == WHOWAS_UNUSED)
      continue;

    /* Note: We're passing the nick to the filter function. The original
     * whowas record is in the nick's ->next field. */
    if ((search->exe)(ctx, search, &ww->nick)) {
      if (matches<limit)
        display(ctx, sender, ww);

      if (matches==limit)
        ctx->reply(sender, "--- More than %d matches, skipping the rest",limit);
      matches++;
    }
  }

  ctx->reply(sender,"--- End of list: %d matches", matches);
}  

int do_chansearch_real(replyFunc reply, wallFunc wall, void *source, int cargc, char **cargv) {
  nick *sender = source;
  int limit=500;
  int arg=0;
  ChanDisplayFunc display=defaultchanfn;
  int ret;
  parsertree *tree;

  if (cargc<1) {
    reply( sender, "Usage: [flags] <criteria>");
    reply( sender, "For help, see help chansearch");
    return CMD_OK;
  }
  
  ret = parseopts(cargc, cargv, &arg, &limit, NULL, (void *)&display, reg_chansearch->outputtree, reply, sender);
  if(ret != CMD_OK)
    return ret;

  if (arg>=cargc) {
    reply(sender,"No search terms - aborting.");
    return CMD_ERROR;
  }

  if (arg<(cargc-1)) {
    rejoinline(cargv[arg],cargc-arg);
  }

  tree = parse_string(reg_chansearch, cargv[arg]);
  if(!tree) {
    displaystrerror(reply, sender, cargv[arg]);
    return CMD_ERROR;
  }

  ast_chansearch(tree->root, reply, sender, wall, display, NULL, NULL, limit, NULL);

  parse_free(tree);

  return CMD_OK;
}

int do_chansearch(void *source, int cargc, char **cargv) {
  return do_chansearch_real(controlreply, controlwallwrapper, source, cargc, cargv);
}

void chansearch_exe(struct searchNode *search, searchCtx *ctx) {  
  int i;
  chanindex *cip;
  int matches = 0;
  nick *sender = ctx->sender;
  senderNSExtern = sender;
  ChanDisplayFunc display = ctx->displayfn;
  int limit = ctx->limit;

  assert(!ctx->targets);  

  search=coerceNode(ctx, search, RETURNTYPE_BOOL);
  
  for (i=0;i<CHANNELHASHSIZE;i++) {
    for (cip=chantable[i];cip;cip=cip->next) {
      if ((search->exe)(ctx, search, cip)) {
	if (matches<limit)
	  display(ctx, sender, cip);
	if (matches==limit)
	  ctx->reply(sender, "--- More than %d matches, skipping the rest",limit);
	matches++;
      }
    }
  }

  ctx->reply(sender,"--- End of list: %d matches", matches);
}

int do_usersearch_real(replyFunc reply, wallFunc wall, void *source, int cargc, char **cargv) {
  nick *sender = source;
  int limit=500;
  int arg=0;
  UserDisplayFunc display=defaultuserfn;
  int ret;
  parsertree *tree;

  if (cargc<1) {
    reply( sender, "Usage: [flags] <criteria>");
    reply( sender, "For help, see help usersearch");
    return CMD_OK;
  }
 
  ret = parseopts(cargc, cargv, &arg, &limit, NULL, (void *)&display, reg_usersearch->outputtree, reply, sender);
  if(ret != CMD_OK)
    return ret;

  if (arg>=cargc) {
    reply(sender,"No search terms - aborting.");
    return CMD_ERROR;
  }

  if (arg<(cargc-1)) {
    rejoinline(cargv[arg],cargc-arg);
  }

  tree = parse_string(reg_usersearch, cargv[arg]);
  if(!tree) {
    displaystrerror(reply, sender, cargv[arg]);
    return CMD_ERROR;
  }

  ast_usersearch(tree->root, reply, sender, wall, display, NULL, NULL, limit, NULL);

  parse_free(tree);

  return CMD_OK;
}

int do_usersearch(void *source, int cargc, char **cargv) {
  return do_usersearch_real(controlreply, controlwallwrapper, source, cargc, cargv);
}

void usersearch_exe(struct searchNode *search, searchCtx *ctx) {  
  int i;
  authname *aup;
  int matches = 0;
  nick *sender = ctx->sender;
  int limit = ctx->limit;
  UserDisplayFunc display = ctx->displayfn;
  senderNSExtern = sender;

  assert(!ctx->targets);

  search=coerceNode(ctx, search, RETURNTYPE_BOOL);
  
  for (i=0;i<AUTHNAMEHASHSIZE;i++) {
    for (aup=authnametable[i];aup;aup=aup->next) {
      if ((search->exe)(ctx, search, aup)) {
	if (matches<limit)
	  display(ctx, sender, aup);
	if (matches==limit)
	  ctx->reply(sender, "--- More than %d matches, skipping the rest",limit);
	matches++;
      }
    }
  }

  ctx->reply(sender,"--- End of list: %d matches", matches);
}

/* Free a coerce node */
void free_coerce(searchCtx *ctx, struct searchNode *thenode) {
  struct coercedata *cd=thenode->localdata;
  
  cd->child->free(ctx, cd->child);
  free(thenode->localdata);
  free(thenode);
}

/* Free a coerce node with a stringbuf allocated */
void free_coercestring(searchCtx *ctx, struct searchNode *thenode) {
  free(((struct coercedata *)thenode->localdata)->u.stringbuf);
  free_coerce(ctx, thenode);
}

/* exe_tostr_null: return the constant string */
void *exe_tostr_null(searchCtx *ctx, struct searchNode *thenode, void *theinput) {
  struct coercedata *cd=thenode->localdata;
  
  return cd->u.stringbuf;
}

/* exe_val_null: return the constant value */
void *exe_val_null(searchCtx *ctx, struct searchNode *thenode, void *theinput) {
  struct coercedata *cd=thenode->localdata;
  
  return (void *)cd->u.val;
}

/* Lots of very dull type conversion functions */
void *exe_inttostr(searchCtx *ctx, struct searchNode *thenode, void *theinput) {
  struct coercedata *cd=thenode->localdata;
  
  sprintf(cd->u.stringbuf, "%lu", (unsigned long)(cd->child->exe)(ctx, cd->child, theinput));
  
  return cd->u.stringbuf;
}

void *exe_booltostr(searchCtx *ctx, struct searchNode *thenode, void *theinput) {
  struct coercedata *cd=thenode->localdata;
  
  if ((cd->child->exe)(ctx, cd->child, theinput)) {
    sprintf(cd->u.stringbuf,"1");
  } else {
    cd->u.stringbuf[0]='\0';
  }
  
  return cd->u.stringbuf;
}

void *exe_strtoint(searchCtx *ctx, struct searchNode *thenode, void *theinput) {
  struct coercedata *cd=thenode->localdata;
  
  return (void *)strtoul((cd->child->exe)(ctx,cd->child,theinput),NULL,10);
}

void *exe_booltoint(searchCtx *ctx, struct searchNode *thenode, void *theinput) {
  struct coercedata *cd=thenode->localdata;
  
  /* Don't need to do anything */
  return (cd->child->exe)(ctx, cd->child, theinput); 
}

void *exe_strtobool(searchCtx *ctx, struct searchNode *thenode, void *theinput) {
  struct coercedata *cd=thenode->localdata;
  char *ch=(cd->child->exe)(ctx, cd->child, theinput);
  
  if (!ch || *ch=='\0' || (*ch=='0' && ch[1]=='\0')) {
    return (void *)0;
  } else { 
    return (void *)1;
  }
}

void *exe_inttobool(searchCtx *ctx, struct searchNode *thenode, void *theinput) {
  struct coercedata *cd=thenode->localdata;
  
  if ((cd->child->exe)(ctx, cd->child, theinput)) {
    return (void *)1;
  } else {
    return (void *)0;
  }
}

struct searchNode *coerceNode(searchCtx *ctx, struct searchNode *thenode, int type) {
  struct searchNode *anode;
  struct coercedata *cd;

  /* You can't coerce a NULL */
  if (!thenode)
    return NULL;
  
  /* No effort required to coerce to the same type */
  if (type==(thenode->returntype & RETURNTYPE_TYPE))
    return thenode;
  
  anode=(struct searchNode *)malloc(sizeof(struct searchNode));
  anode->localdata=cd=(struct coercedata *)malloc(sizeof(struct coercedata));
  cd->child=thenode;
  anode->returntype=type; /* We'll return what they want, always */
  anode->free=free_coerce;
  
  switch(type) {
    case RETURNTYPE_STRING:
      /* For a string we'll need a buffer */
      /* A 64-bit number prints out to 20 digits, this leaves some slack */
      cd->u.stringbuf=malloc(25); 
      anode->free=free_coercestring;
      
      switch(thenode->returntype & RETURNTYPE_TYPE) {
        default:
        case RETURNTYPE_INT:
          if (thenode->returntype & RETURNTYPE_CONST) {
            /* Constant node: sort it out now */
            sprintf(cd->u.stringbuf, "%lu", (unsigned long)thenode->exe(ctx, thenode, NULL));
            anode->exe=exe_tostr_null;
            anode->returntype |= RETURNTYPE_CONST;
          } else {
            /* Variable data */
            anode->exe=exe_inttostr;
          }
          break;
        
        case RETURNTYPE_BOOL:
          if (thenode->returntype & RETURNTYPE_CONST) {
            /* Constant bool value */
            if (thenode->exe(ctx, thenode,NULL)) {
              /* True! */
              sprintf(cd->u.stringbuf, "1");
            } else {
              cd->u.stringbuf[0] = '\0';
            }
            anode->exe=exe_tostr_null;
            anode->returntype |= RETURNTYPE_CONST;
          } else {
            /* Variable bool value */
            anode->exe=exe_booltostr;
          }            
          break;
      }
      break;
    
    case RETURNTYPE_INT:
      /* we want an int */
      switch (thenode->returntype & RETURNTYPE_TYPE) {
        case RETURNTYPE_STRING:
          if (thenode->returntype & RETURNTYPE_CONST) {
            cd->u.val=strtoul((thenode->exe)(ctx, thenode, NULL), NULL, 10);
            anode->exe=exe_val_null;
            anode->returntype |= RETURNTYPE_CONST;
          } else {
            anode->exe=exe_strtoint;
          }
          break;
        
        default:
        case RETURNTYPE_BOOL:
          if (thenode->returntype & RETURNTYPE_CONST) {
            if ((thenode->exe)(ctx, thenode,NULL))
              cd->u.val=1;
            else
              cd->u.val=0;
            
            anode->exe=exe_val_null;
            anode->returntype |= RETURNTYPE_CONST;
          } else {
            anode->exe=exe_booltoint;
          }
          break;
      }      
      break;
    
    default:
    case RETURNTYPE_BOOL:
      /* we want a bool */
      switch (thenode->returntype & RETURNTYPE_TYPE) {
        case RETURNTYPE_STRING:
          if (thenode->returntype & RETURNTYPE_CONST) {
            char *rv=(char *)((thenode->exe)(ctx, thenode, NULL));
            if (!rv || *rv=='\0' || (*rv=='0' && rv[1]=='\0'))
              cd->u.val=0;
            else
              cd->u.val=1;
            
            anode->exe=exe_val_null;
            anode->returntype |= RETURNTYPE_CONST;
          } else {
            anode->exe=exe_strtobool;
          }
          break;
        
        default:
        case RETURNTYPE_INT:
          if (thenode->returntype & RETURNTYPE_CONST) {
            if ((thenode->exe)(ctx, thenode,NULL))
              cd->u.val=1;
            else
              cd->u.val=0;
            
            anode->exe=exe_val_null;
            anode->returntype |= RETURNTYPE_CONST;
          } else {
            anode->exe=exe_inttobool;
          }
          break;
      }
      break;
  }
  
  return anode;
}

/* Literals always return constant strings... */
void *literal_exe(searchCtx *ctx, struct searchNode *thenode, void *theinput) {
  if (thenode->localdata) 
    return ((sstring *)thenode->localdata)->content;
  else
    return "";
}

void literal_free(searchCtx *ctx, struct searchNode *thenode) {
  freesstring(thenode->localdata);
  free(thenode);
}

void nssnprintf(char *buf, size_t size, const char *format, nick *np) {
  StringBuf b;
  const char *p;
  char *c;
  char hostbuf[512];

  if(size == 0)
    return;

  b.buf = buf;
  b.capacity = size;
  b.len = 0;

  for(p=format;*p;p++) {
    if(*p != '%') {
      if(!sbaddchar(&b, *p))
        break;
      continue;
    }
    p++;
    if(*p == '\0')
      break;
    if(*p == '%') {
      if(!sbaddchar(&b, *p))
        break;
      continue;
    }

    c = NULL;
    switch(*p) {
      case 'n':
        c = np->nick; break;
      case 'i':
        c = np->ident; break;
      case 'h':
        c = np->host->name->content; break;
      case 'I':
        snprintf(hostbuf, sizeof(hostbuf), "%s", IPtostr(np->ipaddress));
        c = hostbuf;
        break;
      case 'u':
        snprintf(hostbuf, sizeof(hostbuf), "%s!%s@%s", np->nick, np->ident, IPtostr(np->ipaddress));
        c = hostbuf;
        break;
      default:
        c = "(bad format specifier)";
    }
    if(c)
      if(!sbaddstr(&b, c))
        break;
  }

  sbterminate(&b);

  /* not required */
  /*
  buf[size-1] = '\0';
  */
}

static char *var_tochar(searchCtx *ctx, char *arg, searchNode **variable) {
  *variable = ctx->parser(ctx, arg);
  if (!(*variable = coerceNode(ctx, *variable, RETURNTYPE_STRING)))
    return NULL;

  if(!((*variable)->returntype & RETURNTYPE_CONST)) {
    parseError = "only constant variables allowed";
    ((*variable)->free)(ctx, *variable);
    return NULL;
  }
  
  return (char *)((*variable)->exe)(ctx, *variable, NULL);
}

void free_val_null(searchCtx *ctx, struct searchNode *thenode) {
}

struct searchVariable *var_register(searchCtx *ctx, char *arg, int type) {
  searchNode *variable;
  struct searchVariable *us;
  char *var;
  int i;
  
  if(ctx->lastvar >= MAX_VARIABLES) {
    parseError = "Maximum number of variables reached";
    return NULL;
  }
  
  us = &ctx->vars[ctx->lastvar];
  
  var = var_tochar(ctx, arg, &variable);
  if(!var)
    return NULL;
  
  strlcpy(us->name, var, sizeof(us->name));
  (variable->free)(ctx, variable);
  
  for(i=0;i<ctx->lastvar;i++) {
    if(!strcmp(us->name, ctx->vars[i].name)) {
      parseError = "variable name already in use";
      return NULL;
    }
  }
  
  ctx->lastvar++;
  us->data.returntype = type;
  us->data.localdata = &us->cdata;
  us->data.exe = exe_val_null;
  us->data.free = free_val_null;
  
  us->cdata.child = NULL;
  return us;
}

searchNode *var_get(searchCtx *ctx, char *arg) {
  searchNode *variable, *found = NULL;
  int i;
  char *var = var_tochar(ctx, arg, &variable);
  if(!var)
    return NULL;

  for(i=0;i<ctx->lastvar;i++) {
    if(!strcmp(var, ctx->vars[i].name)) {
      found = &ctx->vars[i].data;
      break;
    }
  }
  (variable->free)(ctx, variable);
  
  if(!found)
    parseError = "variable not found";
  return found;
}

void var_setstr(struct searchVariable *v, char *data) {
  v->cdata.u.stringbuf = data;
}

void displaystrerror(replyFunc reply, nick *np, const char *input) {
  char buf[515];

  if((parseStrErrorPos >= 0) && (parseStrErrorPos < sizeof(buf) - 3)) {
    int i;

    for(i=0;i<parseStrErrorPos;i++)
      buf[i] = ' ';

    buf[i++] = '^';
    buf[i] = '\0';

    reply(np, "%s", input);
    reply(np, "%s", buf);
  }

  reply(np, "Parse error: %s", parseStrError);
}

struct searchNode *argtoconststr(char *command, searchCtx *ctx, char *arg, char **p) {
  struct searchNode *c;
  static char errorbuf[512];
  
  c = ctx->parser(ctx, arg);
  if (!(c = coerceNode(ctx, c, RETURNTYPE_STRING))) {
    snprintf(errorbuf, sizeof(errorbuf), "%s: unable to coerce argument to string", command);
    parseError = errorbuf;
    return NULL;
  }
  
  if (!(c->returntype & RETURNTYPE_CONST)) {
    snprintf(errorbuf, sizeof(errorbuf), "%s: constant argument required", command);
    parseError = errorbuf;
    (c->free)(ctx, c);
    return NULL;
  }
  
  if(p)
    *p = (char *)(c->exe)(ctx, c, NULL);
    
  return c;
}
