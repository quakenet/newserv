#include <stdio.h>
#include <stdarg.h>
#include <string.h>

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

MODULE_VERSION("");

CommandTree *searchCmdTree;
searchList *globalterms = NULL;

int do_nicksearch(void *source, int cargc, char **cargv);
int do_chansearch(void *source, int cargc, char **cargv);
int do_usersearch(void *source, int cargc, char **cargv);

void printnick_channels(searchCtx *, nick *, nick *);
void printchannel(searchCtx *, nick *, chanindex *);
void printchannel_topic(searchCtx *, nick *, chanindex *);
void printchannel_services(searchCtx *, nick *, chanindex *);

UserDisplayFunc defaultuserfn = printuser;
NickDisplayFunc defaultnickfn = printnick;
ChanDisplayFunc defaultchanfn = printchannel;

searchCmd *reg_nicksearch, *reg_chansearch, *reg_usersearch;

searchCmd *registersearchcommand(char *name, int level, CommandHandler cmd, void *defaultdisplayfunc) {
  searchCmd *acmd;
  searchList *sl;

  registercontrolhelpcmd(name, NO_OPER,4, cmd, "Usage: <criteria\nSearches with the given criteria");
  
  acmd=(struct searchCmd *)malloc(sizeof(struct searchCmd));

  acmd->handler = cmd;

  acmd->name = getsstring( name, NSMAX_COMMAND_LEN); 
  acmd->outputtree = newcommandtree();
  acmd->searchtree = newcommandtree();

  addcommandtotree(searchCmdTree, name, 0, 0, (CommandHandler)acmd);

  sl = globalterms;
  while (sl) {
    registersearchterm( acmd, sl->name->content, sl->cmd);
    sl = sl->next;
  }

  return acmd;
}

void deregistersearchcommand(searchCmd *scmd) {
  deregistercontrolcmd(scmd->name->content, (CommandHandler)scmd->handler);
  destroycommandtree(scmd->outputtree);
  destroycommandtree(scmd->searchtree);
  freesstring(scmd->name);
  free(scmd);
}

void regdisp( searchCmd *cmd, const char *name, void *handler) {
  addcommandtotree(cmd->outputtree, name, 0, 0, (CommandHandler) handler);
} 

void unregdisp( searchCmd *cmd, const char *name, void *handler ) {
  deletecommandfromtree(cmd->outputtree, name, (CommandHandler) handler);
}

void *findcommandinlist( searchList *sl, char *name){
  while(sl) {
    if(strcmp(sl->name->content,name) == 0 ) {
      return sl;
    }
    sl = sl->next;
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

  /* Boolean operations */
  registerglobalsearchterm("and",and_parse);
  registerglobalsearchterm("not",not_parse);
  registerglobalsearchterm("or",or_parse);

  registerglobalsearchterm("eq",eq_parse);

  registerglobalsearchterm("lt",lt_parse);
  registerglobalsearchterm("gt",gt_parse);
 
  /* String operations */
  registerglobalsearchterm("match",match_parse);
  registerglobalsearchterm("regex",regex_parse);
  registerglobalsearchterm("length",length_parse);
  
  /* Nickname operations */
  registersearchterm(reg_nicksearch, "hostmask",hostmask_parse);     /* nick only */
  registersearchterm(reg_nicksearch, "realname",realname_parse);     /* nick only */
  registersearchterm(reg_nicksearch, "authname",authname_parse);     /* nick only */
  registersearchterm(reg_nicksearch, "authts",authts_parse);         /* nick only */
  registersearchterm(reg_nicksearch, "ident",ident_parse);           /* nick only */
  registersearchterm(reg_nicksearch, "host",host_parse);             /* nick only */
  registersearchterm(reg_nicksearch, "channel",channel_parse);       /* nick only */
  registersearchterm(reg_nicksearch, "timestamp",timestamp_parse);   /* nick only */
  registersearchterm(reg_nicksearch, "country",country_parse);       /* nick only */
  registersearchterm(reg_nicksearch, "ip",ip_parse);                 /* nick only */
  registersearchterm(reg_nicksearch, "channels",channels_parse);     /* nick only */
  registersearchterm(reg_nicksearch, "server",server_parse);         /* nick only */
  registersearchterm(reg_nicksearch, "authid",authid_parse);         /* nick only */

  /* Channel operations */
  registersearchterm(reg_chansearch, "exists",exists_parse);         /* channel only */
  registersearchterm(reg_chansearch, "services",services_parse);     /* channel only */
  registersearchterm(reg_chansearch, "size",size_parse);             /* channel only */
  registersearchterm(reg_chansearch, "name",name_parse);             /* channel only */
  registersearchterm(reg_chansearch, "topic",topic_parse);           /* channel only */
  registersearchterm(reg_chansearch, "oppct",oppct_parse);           /* channel only */
  registersearchterm(reg_chansearch, "uniquehostpct",hostpct_parse); /* channel only */
  registersearchterm(reg_chansearch, "authedpct",authedpct_parse);   /* channel only */
  registersearchterm(reg_chansearch, "kick",kick_parse);             /* channel only */

  /* Nickname / channel operations */
  registersearchterm(reg_chansearch, "modes",modes_parse);
  registersearchterm(reg_nicksearch, "modes",modes_parse);
  registersearchterm(reg_chansearch, "nick",nick_parse);
  registersearchterm(reg_nicksearch, "nick",nick_parse);

  /* Kill / gline parameters */
  registersearchterm(reg_chansearch,"kill",kill_parse);
  registersearchterm(reg_chansearch,"gline",gline_parse);
  registersearchterm(reg_nicksearch,"kill",kill_parse);
  registersearchterm(reg_nicksearch,"gline",gline_parse);

  /* Iteration functionality */
  registerglobalsearchterm("any",any_parse);
  registerglobalsearchterm("all",all_parse);
  registerglobalsearchterm("var",var_parse);
  
  /* Iterable functions */
  registersearchterm(reg_nicksearch, "channeliter",channeliter_parse);         /* nick only */
  
  /* Notice functionality */
  registersearchterm(reg_chansearch,"notice",notice_parse);
  registersearchterm(reg_nicksearch,"notice",notice_parse);
 
  /* Nick output filters */
  regdisp(reg_nicksearch,"default",printnick);
  regdisp(reg_nicksearch,"channels",printnick_channels);
    
  /* Channel output filters */
  regdisp(reg_chansearch,"default",printchannel);
  regdisp(reg_chansearch,"topic",printchannel_topic);
  regdisp(reg_chansearch,"services",printchannel_services);

  /* Nick output filters */
  regdisp(reg_usersearch,"default",printuser);
 
}

void _fini() {
  searchList *sl, *psl;
  int i,n;
  Command *cmdlist[100];

  sl = globalterms; 
  while (sl) {
    psl = sl;
    sl = sl->next;

    n=getcommandlist(searchCmdTree,cmdlist,100);
    for(i=0;i<n;i++) {
      deregistersearchterm( (searchCmd *)cmdlist[i]->handler, psl->name->content, psl->cmd);
    }

    freesstring(psl->name);
    free(psl);
 
  }

  deregistersearchcommand( reg_nicksearch );
  deregistersearchcommand( reg_chansearch );
  deregistersearchcommand( reg_usersearch );
  destroycommandtree( searchCmdTree );
}

void registerglobalsearchterm(char *term, parseFunc parsefunc) {
  searchList *sl = malloc(sizeof(searchList));
  int i,n;
  Command *cmdlist[100];

  sl->cmd = parsefunc;
  sl->name = getsstring(term, NSMAX_COMMAND_LEN);
  sl->next = NULL;

  if ( globalterms != NULL ) {
    sl->next = globalterms;
  }
  globalterms = sl;

  n=getcommandlist(searchCmdTree,cmdlist,100);
  for(i=0;i<n;i++) {
    registersearchterm( (searchCmd *)cmdlist[i]->handler,term, parsefunc); 
  }
}

void deregisterglobalsearchterm(char *term, parseFunc parsefunc) {
  int i,n;
  Command *cmdlist[100];
  searchList *sl, *psl=NULL;

  sl = globalterms;
  while (sl) {
    if ( strcmp( sl->name->content, term) == 0 ) {
      break;
    }
    psl = sl;
    sl = sl->next;
  }

  if (sl) {
    if( psl ) {
      psl->next = sl->next;
    } 

    n=getcommandlist(searchCmdTree,cmdlist,100);
    for(i=0;i<n;i++) {
      deregistersearchterm( (searchCmd *)cmdlist[i]->handler, term, parsefunc);
    }
    freesstring(sl->name);
    free(sl);
  }
}

void registersearchterm(searchCmd *cmd, char *term, parseFunc parsefunc) {
  addcommandtotree(cmd->searchtree, term, 0, 0, (CommandHandler) parsefunc);
}

void deregistersearchterm(searchCmd *cmd, char *term, parseFunc parsefunc) {
  deletecommandfromtree(cmd->searchtree, term, (CommandHandler) parsefunc);
}

static void controlwallwrapper(int level, char *format, ...) {
  char buf[1024];
  va_list ap;

  va_start(ap, format);
  vsnprintf(buf, sizeof(buf), format, ap);
  controlwall(NO_OPER, level, "%s", buf);
  va_end(ap);
}

int parseopts(int cargc, char **cargv, int *arg, int *limit, void **subset, void **display, CommandTree *sl, replyFunc reply, void *sender) {
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
	  reply(sender,"Error: -l switch requires an argument");
	  return CMD_USAGE;
	}
	*limit=strtoul(cargv[(*arg)++],NULL,10);
	break;
	
      case 'd':
        if (cargc<*arg) {
          reply(sender,"Error: -d switch requires an argument");
          return CMD_USAGE;
        }
        cmd=findcommandintree(sl, cargv[*arg],1);
        if (!cmd) {
          reply(sender,"Error: unknown output format %s",cargv[*arg]);
          return CMD_USAGE;
        }
        *display=(void *)cmd->handler;
        (*arg)++;
        break;

      case 's':
        if (cargc<*arg) {
          reply(sender,"Error: -s switch requires an argument");
          return CMD_USAGE;
        }
        if (ipmask_parse(cargv[*arg], &sin, &bits) == 0) {
          reply(sender, "Error: Invalid CIDR mask supplied");
          return CMD_USAGE;
        }
        *subset = (void *)refnode(iptree, &sin, bits);
        (*arg)++;
        break;
        
      default:
	reply(sender,"Unrecognised flag -%c.",*ch);
      }
    }
  }

  return CMD_OK;
}

void newsearch_ctxinit(searchCtx *ctx, searchParseFunc searchfn, replyFunc replyfn, wallFunc wallfn, void *arg, searchCmd *cmd) {
  memset(ctx, 0, sizeof(searchCtx));
  
  ctx->reply = replyfn;
  ctx->wall = wallfn;
  ctx->parser = searchfn;
  ctx->arg = arg;
  ctx->searchcmd = cmd;
}

int do_nicksearch_real(replyFunc reply, wallFunc wall, void *source, int cargc, char **cargv) {
  nick *sender = source;
  struct searchNode *search;
  int limit=500;
  int arg=0;
  NickDisplayFunc display=defaultnickfn;
  searchCtx ctx;
  int ret;

  if (cargc<1)
    return CMD_USAGE;
  
  ret = parseopts(cargc, cargv, &arg, &limit, NULL, (void **)&display, reg_nicksearch->outputtree, reply, sender);
  if(ret != CMD_OK)
    return ret;

  if (arg>=cargc) {
    reply(sender,"No search terms - aborting.");
    return CMD_ERROR;
  }

  if (arg<(cargc-1)) {
    rejoinline(cargv[arg],cargc-arg);
  }

  newsearch_ctxinit(&ctx, search_parse, reply, wall, NULL, reg_nicksearch);

  if (!(search = ctx.parser(&ctx, cargv[arg]))) {
    reply(sender,"Parse error: %s",parseError);
    return CMD_ERROR;
  }

  nicksearch_exe(search, &ctx, sender, display, limit);

  (search->free)(&ctx, search);

  return CMD_OK;
}

int do_nicksearch(void *source, int cargc, char **cargv) {
  return do_nicksearch_real(controlreply, controlwallwrapper, source, cargc, cargv);
}

void nicksearch_exe(struct searchNode *search, searchCtx *ctx, nick *sender, NickDisplayFunc display, int limit) {
  int i, j;
  int matches = 0;
  unsigned int cmarker;
  unsigned int tchans=0,uchans=0;
  struct channel **cs;
  nick *np;
  senderNSExtern = sender;
  
  /* Get a marker value to mark "seen" channels for unique count */
  cmarker=nextchanmarker();
  
  /* The top-level node needs to return a BOOL */
  search=coerceNode(ctx, search, RETURNTYPE_BOOL);
  
  for (i=0;i<NICKHASHSIZE;i++) {
    for (np=nicktable[i];np;np=np->next) {
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
  }

  ctx->reply(sender,"--- End of list: %d matches; users were on %u channels (%u unique, %.1f average clones)", 
                matches, tchans, uchans, (float)tchans/uchans);
}  

int do_chansearch_real(replyFunc reply, wallFunc wall, void *source, int cargc, char **cargv) {
  nick *sender = source;
  struct searchNode *search;
  int limit=500;
  int arg=0;
  ChanDisplayFunc display=defaultchanfn;
  searchCtx ctx;
  int ret;

  if (cargc<1)
    return CMD_USAGE;
  
  ret = parseopts(cargc, cargv, &arg, &limit, NULL, (void **)&display, reg_chansearch->outputtree, reply, sender);
  if(ret != CMD_OK)
    return ret;

  if (arg>=cargc) {
    reply(sender,"No search terms - aborting.");
    return CMD_ERROR;
  }

  if (arg<(cargc-1)) {
    rejoinline(cargv[arg],cargc-arg);
  }

  newsearch_ctxinit(&ctx, search_parse, reply, wall, NULL, reg_chansearch);
  if (!(search = ctx.parser(&ctx, cargv[arg]))) {
    reply(sender,"Parse error: %s",parseError);
    return CMD_ERROR;
  }

  chansearch_exe(search, &ctx, sender, display, limit);

  (search->free)(&ctx, search);

  return CMD_OK;
}

int do_chansearch(void *source, int cargc, char **cargv) {
  return do_chansearch_real(controlreply, controlwallwrapper, source, cargc, cargv);
}

void chansearch_exe(struct searchNode *search, searchCtx *ctx, nick *sender, ChanDisplayFunc display, int limit) {  
  int i;
  chanindex *cip;
  int matches = 0;
  senderNSExtern = sender;
  
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
  struct searchNode *search;
  int limit=500;
  int arg=0;
  UserDisplayFunc display=defaultuserfn;
  searchCtx ctx;
  int ret;

  if (cargc<1)
    return CMD_USAGE;
  
  ret = parseopts(cargc, cargv, &arg, &limit, NULL, (void **)&display, reg_usersearch->outputtree, reply, sender);
  if(ret != CMD_OK)
    return ret;

  if (arg>=cargc) {
    reply(sender,"No search terms - aborting.");
    return CMD_ERROR;
  }

  if (arg<(cargc-1)) {
    rejoinline(cargv[arg],cargc-arg);
  }

  newsearch_ctxinit(&ctx, search_parse, reply, wall, NULL, reg_usersearch);
  if (!(search = ctx.parser(&ctx, cargv[arg]))) {
    reply(sender,"Parse error: %s",parseError);
    return CMD_ERROR;
  }

  usersearch_exe(search, &ctx, sender, display, limit);

  (search->free)(&ctx, search);

  return CMD_OK;
}

int do_usersearch(void *source, int cargc, char **cargv) {
  return do_usersearch_real(controlreply, controlwallwrapper, source, cargc, cargv);
}

void usersearch_exe(struct searchNode *search, searchCtx *ctx, nick *sender, UserDisplayFunc display, int limit) {  
  int i;
  authname *aup;
  int matches = 0;
  senderNSExtern = sender;
  
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

/* search_parse:
 *  Given an input string, return a searchNode.
 */

struct searchNode *search_parse(searchCtx *ctx, char *input) {
  /* OK, we need to split the input into chunks on spaces and brackets.. */
  char *argvector[100];
  char thestring[500];
  int i,j,q=0,e=0;
  char *ch,*ch2;
  struct Command *cmd;
  struct searchNode *thenode;

  /* If it starts with a bracket, it's a function call.. */
  if (*input=='(') {
    /* Skip past string */
    for (ch=input;*ch;ch++);
    if (*(ch-1) != ')') {
      parseError = "Bracket mismatch!";
      return NULL;
    }
    input++;
    *(ch-1)='\0';

    /* Split further args */
    i=-1; /* i = -1 BoW, 0 = inword, 1 = bracket nest depth */
    j=0;  /* j = current arg */
    e=0;
    q=0;
    argvector[0]="";
    for (ch=input;*ch;ch++) {
      if (i==-1) {
        argvector[j]=ch;
        if (*ch=='(') {
          i=1;
        } else if (*ch != ' ') {
          i=0;
          if (*ch=='\\') {
            e=1;
          } else if (*ch=='\"') {
            q=1;
          }
        }
      } else if (e==1) {
        e=0;
      } else if (q==1) {
        if (*ch=='\"')	
        q=0;
      } else if (i==0) {
        if (*ch=='\\') {
          e=1;
        } else if (*ch=='\"') {
          q=1;
        } else if (*ch==' ') {
          *ch='\0';
          j++;
          if(j >= (sizeof(argvector) / sizeof(*argvector))) {
            parseError = "Too many arguments";
            return NULL;
          }
          i=-1;
        }
      } else {
        if (*ch=='\\') {
          e=1;
        } else if (*ch=='\"') {
          q=1;
        } else if (*ch=='(') {
          i++;
        } else if (*ch==')') {
          i--;
        }
      }
    }
    
    if (i>0) {
      parseError = "Bracket mismatch!";
      return NULL;
    }

    if (*(ch-1) == 0) /* if the last character was a space */
      j--; /* remove an argument */
    
    if (!(cmd=findcommandintree(ctx->searchcmd->searchtree,argvector[0],1))) {
      parseError = "Unknown command";
      return NULL;
    } else {
      return ((parseFunc)cmd->handler)(ctx, j, argvector+1);
    }
  } else {
    /* Literal */
    if (*input=='\"') {
      for (ch=input;*ch;ch++);
      
      if (*(ch-1) != '\"') {
        parseError="Quote mismatch";
        return NULL;
      }

      *(ch-1)='\0';
      input++;
    }
    
    ch2=thestring;
    for (ch=input;*ch;ch++) {
      if (e) {
        e=0;
        *ch2++=*ch;
      } else if (*ch=='\\') {
        e=1;
      } else {
        *ch2++=*ch;
      }
    }
    *ch2='\0';
        
    if (!(thenode=(struct searchNode *)malloc(sizeof(struct searchNode)))) {
      parseError = "malloc: could not allocate memory for this search.";
      return NULL;
    }

    thenode->localdata  = getsstring(thestring,512);
    thenode->returntype = RETURNTYPE_CONST | RETURNTYPE_STRING;
    thenode->exe        = literal_exe;
    thenode->free       = literal_free;

    return thenode;
  }    
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
        snprintf(hostbuf, sizeof(hostbuf), "%s", IPtostr(np->p_ipaddr));
        c = hostbuf;
        break;
      case 'u':
        snprintf(hostbuf, sizeof(hostbuf), "%s!%s@%s", np->nick, np->ident, IPtostr(np->p_ipaddr));
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
