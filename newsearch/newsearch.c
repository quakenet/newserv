
#include <stdio.h>
#include "newsearch.h"

#include "../irc/irc_config.h"
#include "../lib/irc_string.h"
#include "../parser/parser.h"
#include "../control/control.h"
#include "../lib/splitline.h"
#include "../lib/version.h"

MODULE_VERSION("");

CommandTree *searchTree;
CommandTree *chanOutputTree;
CommandTree *nickOutputTree;

int do_nicksearch(void *source, int cargc, char **cargv);
int do_chansearch(void *source, int cargc, char **cargv);
struct searchNode *search_parse(int type, char *input);

void printnick(nick *, nick *);
void printnick_channels(nick *, nick *);
void printchannel(nick *, chanindex *);
void printchannel_topic(nick *, chanindex *);
void printchannel_services(nick *, chanindex *);

void registersearchterm(char *term, parseFunc parsefunc);
void deregistersearchterm(char *term, parseFunc parsefunc);

void regchandisp(const char *name, ChanDisplayFunc handler) {
  addcommandtotree(chanOutputTree, name, 0, 0, (CommandHandler)handler);
}

void unregchandisp(const char *name, ChanDisplayFunc handler) {
  deletecommandfromtree(chanOutputTree, name, (CommandHandler)handler);
}

void regnickdisp(const char *name, NickDisplayFunc handler) {
  addcommandtotree(nickOutputTree, name, 0, 0, (CommandHandler)handler);
}

void unregnickdisp(const char *name, NickDisplayFunc handler) {
  deletecommandfromtree(nickOutputTree, name, (CommandHandler)handler);
}

const char *parseError;
/* used for *_free functions that need to warn users of certain things
   i.e. hitting too many users in a (kill) or (gline) */
nick *senderNSExtern;

void _init() {
  searchTree=newcommandtree();
  chanOutputTree=newcommandtree();
  nickOutputTree=newcommandtree();

  /* Boolean operations */
  registersearchterm("and",and_parse);
  registersearchterm("not",not_parse);
  registersearchterm("or",or_parse);

  registersearchterm("eq",eq_parse);

  registersearchterm("lt",lt_parse);
  registersearchterm("gt",gt_parse);
 
  /* String operations */
  registersearchterm("match",match_parse);
  registersearchterm("regex",regex_parse);
  registersearchterm("length",length_parse);
  
  /* Nickname operations */
  registersearchterm("hostmask",hostmask_parse);
  registersearchterm("realname",realname_parse);
  registersearchterm("authname",authname_parse);
  registersearchterm("authts",authts_parse);
  registersearchterm("ident",ident_parse);
  registersearchterm("host",host_parse);
  registersearchterm("channel",channel_parse);
  registersearchterm("timestamp",timestamp_parse);
  registersearchterm("country",country_parse);
  registersearchterm("ip",ip_parse);
  registersearchterm("channels",channels_parse);
  registersearchterm("server",server_parse);

  /* Channel operations */
  registersearchterm("exists",exists_parse);
  registersearchterm("services",services_parse);
  registersearchterm("size",size_parse);
  registersearchterm("name",name_parse);
  registersearchterm("topic",topic_parse);
  registersearchterm("oppct",oppct_parse);
  registersearchterm("uniquehostpct",hostpct_parse);
  registersearchterm("authedpct",authedpct_parse);
  registersearchterm("kick",kick_parse);

  /* Nickname / channel operations */
  registersearchterm("modes",modes_parse);
  registersearchterm("nick",nick_parse);

  /* Kill / gline parameters */
  registersearchterm("kill",kill_parse);
  registersearchterm("gline",gline_parse);
  
  /* Nick output filters */
  regnickdisp("default",printnick);
  regnickdisp("channels",printnick_channels);
    
  /* Channel output filters */
  regchandisp("default",printchannel);
  regchandisp("topic",printchannel_topic);
  regchandisp("services",printchannel_services);

  registercontrolhelpcmd("nicksearch",NO_OPER,4,do_nicksearch, "Usage: nicksearch <criteria>\nSearches for nicknames with the given criteria.");
  registercontrolhelpcmd("chansearch",NO_OPER,4,do_chansearch, "Usage: chansearch <criteria>\nSearches for channels with the given criteria.");
}

void _fini() {
  destroycommandtree(searchTree);
  destroycommandtree(chanOutputTree);
  destroycommandtree(nickOutputTree);
  deregistercontrolcmd("nicksearch", do_nicksearch);
  deregistercontrolcmd("chansearch", do_chansearch);
}

void registersearchterm(char *term, parseFunc parsefunc) {
  addcommandtotree(searchTree, term, 0, 0, (CommandHandler) parsefunc);
}

void deregistersearchterm(char *term, parseFunc parsefunc) {
  deletecommandfromtree(searchTree, term, (CommandHandler) parsefunc);
}

int do_nicksearch(void *source, int cargc, char **cargv) {
  nick *sender = senderNSExtern = source, *np;
  int i,j;
  struct searchNode *search;
  int limit=500,matches=0;
  char *ch;
  int arg=0;
  struct Command *cmd;
  NickDisplayFunc display=printnick;
  unsigned int cmarker;
  unsigned int tchans=0,uchans=0;
  struct channel **cs;
  
  if (cargc<1)
    return CMD_USAGE;
  
  if (*cargv[0] == '-') {
    /* options */
    arg++;
    
    for (ch=cargv[0]+1;*ch;ch++) {
      switch(*ch) {
      case 'l':
	if (cargc<arg) {
	  controlreply(sender,"Error: -l switch requires an argument");
	  return CMD_USAGE;
	}
	limit=strtoul(cargv[arg++],NULL,10);
	break;
	
      case 'd':
        if (cargc<arg) {
          controlreply(sender,"Error: -d switch requires an argument");
          return CMD_USAGE;
        }
        cmd=findcommandintree(nickOutputTree, cargv[arg], 1);
        if (!cmd) {
          controlreply(sender,"Error: unknown output format %s",cargv[arg]);
          return CMD_USAGE;
        }
        display=(NickDisplayFunc)cmd->handler;
        arg++;
        break;
        
      default:
	controlreply(sender,"Unrecognised flag -%c.",*ch);
      }
    }
  }

  if (arg>=cargc) {
    controlreply(sender,"No search terms - aborting.");
    return CMD_ERROR;
  }

  if (arg<(cargc-1)) {
    rejoinline(cargv[arg],cargc-arg);
  }
  
  if (!(search = search_parse(SEARCHTYPE_NICK, cargv[arg]))) {
    controlreply(sender,"Parse error: %s",parseError);
    return CMD_ERROR;
  }
  
  /* Get a marker value to mark "seen" channels for unique count */
  cmarker=nextchanmarker();
  
  /* The top-level node needs to return a BOOL */
  search=coerceNode(search, RETURNTYPE_BOOL);
  
  for (i=0;i<NICKHASHSIZE;i++) {
    for (np=nicktable[i];np;np=np->next) {
      if ((search->exe)(search, np)) {
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
	  display(sender, np);
	  
	if (matches==limit)
	  controlreply(sender, "--- More than %d matches, skipping the rest",limit);
	matches++;
      }
    }
  }

  (search->free)(search);

  controlreply(sender,"--- End of list: %d matches; users were on %u channels (%u unique, %.1f average clones)", 
                matches, tchans, uchans, (float)tchans/uchans);

  return CMD_OK;
}  

int do_chansearch(void *source, int cargc, char **cargv) {
  nick *sender = senderNSExtern = source;
  chanindex *cip;
  int i;
  struct searchNode *search;
  int limit=500,matches=0;
  char *ch;
  int arg=0;
  struct Command *cmd;
  ChanDisplayFunc display=printchannel;

  if (cargc<1)
    return CMD_USAGE;
  
  if (*cargv[0] == '-') {
    /* options */
    arg++;
    
    for (ch=cargv[0]+1;*ch;ch++) {
      switch(*ch) {
      case 'l':
	if (cargc<arg) {
	  controlreply(sender,"Error: -l switch requires an argument");
	  return CMD_USAGE;
	}
	limit=strtoul(cargv[arg++],NULL,10);
	break;

      case 'd':
        if (cargc<arg) {
          controlreply(sender,"Error: -d switch requires an argument");
          return CMD_USAGE;
        }
        cmd=findcommandintree(chanOutputTree, cargv[arg], 1);
        if (!cmd) {
          controlreply(sender,"Error: unknown output format %s",cargv[arg]);
          return CMD_USAGE;
        }
        display=(ChanDisplayFunc)cmd->handler;
        arg++;
        break;
	
      default:
	controlreply(sender,"Unrecognised flag -%c.",*ch);
      }
    }
  }

  if (arg>=cargc) {
    controlreply(sender,"No search terms - aborting.");
    return CMD_ERROR;
  }

  if (arg<(cargc-1)) {
    rejoinline(cargv[arg],cargc-arg);
  }
  
  if (!(search = search_parse(SEARCHTYPE_CHANNEL, cargv[arg]))) {
    controlreply(sender,"Parse error: %s",parseError);
    return CMD_ERROR;
  }

  search=coerceNode(search, RETURNTYPE_BOOL);
  
  for (i=0;i<CHANNELHASHSIZE;i++) {
    for (cip=chantable[i];cip;cip=cip->next) {
      if ((search->exe)(search, cip)) {
	if (matches<limit)
	  display(sender, cip);
	if (matches==limit)
	  controlreply(sender, "--- More than %d matches, skipping the rest",limit);
	matches++;
      }
    }
  }

  (search->free)(search);

  controlreply(sender,"--- End of list: %d matches", matches);

  return CMD_OK;
}

struct coercedata {
  struct searchNode *child;
  union {
    char *stringbuf;
    unsigned long val;
  } u;
};

/* Free a coerce node */
void free_coerce(struct searchNode *thenode) {
  struct coercedata *cd=thenode->localdata;
  
  cd->child->free(cd->child);
  free(thenode->localdata);
  free(thenode);
}

/* Free a coerce node with a stringbuf allocated */
void free_coercestring(struct searchNode *thenode) {
  free(((struct coercedata *)thenode->localdata)->u.stringbuf);
  free_coerce(thenode);
}

/* exe_tostr_null: return the constant string */
void *exe_tostr_null(struct searchNode *thenode, void *theinput) {
  struct coercedata *cd=thenode->localdata;
  
  return cd->u.stringbuf;
}

/* exe_val_null: return the constant value */
void *exe_val_null(struct searchNode *thenode, void *theinput) {
  struct coercedata *cd=thenode->localdata;
  
  return (void *)cd->u.val;
}

/* Lots of very dull type conversion functions */
void *exe_inttostr(struct searchNode *thenode, void *theinput) {
  struct coercedata *cd=thenode->localdata;
  
  sprintf(cd->u.stringbuf, "%lu", (unsigned long)(cd->child->exe)(cd->child, theinput));
  
  return cd->u.stringbuf;
}

void *exe_booltostr(struct searchNode *thenode, void *theinput) {
  struct coercedata *cd=thenode->localdata;
  
  if ((cd->child->exe)(cd->child, theinput)) {
    sprintf(cd->u.stringbuf,"1");
  } else {
    cd->u.stringbuf[0]='\0';
  }
  
  return cd->u.stringbuf;
}

void *exe_strtoint(struct searchNode *thenode, void *theinput) {
  struct coercedata *cd=thenode->localdata;
  
  return (void *)strtoul((cd->child->exe)(cd->child,theinput),NULL,10);
}

void *exe_booltoint(struct searchNode *thenode, void *theinput) {
  struct coercedata *cd=thenode->localdata;
  
  /* Don't need to do anything */
  return (cd->child->exe)(cd->child, theinput); 
}

void *exe_strtobool(struct searchNode *thenode, void *theinput) {
  struct coercedata *cd=thenode->localdata;
  char *ch=(cd->child->exe)(cd->child, theinput);
  
  if (!ch || *ch=='\0' || (*ch=='0' && ch[1]=='\0')) {
    return (void *)0;
  } else { 
    return (void *)1;
  }
}

void *exe_inttobool(struct searchNode *thenode, void *theinput) {
  struct coercedata *cd=thenode->localdata;
  
  if ((cd->child->exe)(cd->child, theinput)) {
    return (void *)1;
  } else {
    return (void *)0;
  }
}

struct searchNode *coerceNode(struct searchNode *thenode, int type) {
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
            sprintf(cd->u.stringbuf, "%lu", (unsigned long)thenode->exe(thenode, NULL));
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
            if (thenode->exe(thenode,NULL)) {
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
            cd->u.val=strtoul((thenode->exe)(thenode, NULL), NULL, 10);
            anode->exe=exe_val_null;
            anode->returntype |= RETURNTYPE_CONST;
          } else {
            anode->exe=exe_strtoint;
          }
          break;
        
        default:
        case RETURNTYPE_BOOL:
          if (thenode->returntype & RETURNTYPE_CONST) {
            if ((thenode->exe)(thenode,NULL))
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
            char *rv=(char *)((thenode->exe)(thenode, NULL));
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
            if ((thenode->exe)(thenode,NULL))
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
void *literal_exe(struct searchNode *thenode, void *theinput) {
  if (thenode->localdata) 
    return ((sstring *)thenode->localdata)->content;
  else
    return "";
}

void literal_free(struct searchNode *thenode) {
  freesstring(thenode->localdata);
  free(thenode);
}

/* search_parse:
 *  Given an input string, return a searchNode.
 */

struct searchNode *search_parse(int type, char *input) {
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
    
    if (!(cmd=findcommandintree(searchTree,argvector[0],1))) {
      parseError = "Unknown command";
      return NULL;
    } else {
      return ((parseFunc)cmd->handler)(type, j, argvector+1);
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

struct bufs {
  char *buf;
  int capacity;
  int len;
};

static int addchar(struct bufs *buf, char c) {
  if(buf->len >= buf->capacity - 1)
    return 0;

  buf->buf[buf->len++] = c;

  return 1;
}

static int addstr(struct bufs *buf, char *c) {
  int remaining = buf->capacity - buf->len - 1;
  char *p;

  for(p=c;*p;p++) {
    if(remaining-- <= 0)
      return 0;

    buf->buf[buf->len++] = *p;
  }

  return 1;
}

void nssnprintf(char *buf, size_t size, const char *format, nick *np) {
  struct bufs b;
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
      if(!addchar(&b, *p))
        break;
      continue;
    }
    p++;
    if(*p == '\0')
      break;
    if(*p == '%') {
      if(!addchar(&b, *p))
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
      if(!addstr(&b, c))
        break;
  }

  buf[b.len] = '\0';

  /* not required */
  /*
  buf[size-1] = '\0';
  */
}

