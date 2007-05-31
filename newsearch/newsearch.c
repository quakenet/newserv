
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

int do_nicksearch(void *source, int cargc, char **cargv);
struct searchNode *search_parse(int type, char *input);

void registersearchterm(char *term, parseFunc parsefunc);
void deregistersearchterm(char *term, parseFunc parsefunc);

void *trueval(int type);
void *falseval(int type);

const char *parseError;
/* used for *_free functions that need to warn users of certain things
   i.e. hitting too many users in a (kill) or (gline) */
nick *senderNSExtern;

void _init() {
  searchTree=newcommandtree();

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

  /* Nickname operations */
  registersearchterm("hostmask",hostmask_parse);
  registersearchterm("realname",realname_parse);
  registersearchterm("nick",nick_parse);
  registersearchterm("authname",authname_parse);
  registersearchterm("ident",ident_parse);
  registersearchterm("host",host_parse);
  registersearchterm("channel",channel_parse);
  registersearchterm("timestamp",timestamp_parse);
  registersearchterm("country",country_parse);
  registersearchterm("ip",ip_parse);

  /* Nickname / channel operations */
  registersearchterm("modes",modes_parse);

  /* Kill / gline parameters */
  registersearchterm("kill",kill_parse);
  registersearchterm("gline",gline_parse);

  registercontrolhelpcmd("nicksearch",NO_OPER,4,do_nicksearch, "Usage: nicksearch <criteria>\nSearches for nicknames with the given criteria.");
}

void _fini() {
  destroycommandtree(searchTree);
  deregistercontrolcmd("nicksearch", do_nicksearch);
}

void registersearchterm(char *term, parseFunc parsefunc) {
  addcommandtotree(searchTree, term, 0, 0, (CommandHandler) parsefunc);
}

void deregistersearchterm(char *term, parseFunc parsefunc) {
  deletecommandfromtree(searchTree, term, (CommandHandler) parsefunc);
}

void printnick(nick *sender, nick *np) {
  char hostbuf[HOSTLEN+NICKLEN+USERLEN+4];

  controlreply(sender,"%s [%s] (%s) (%s)",visiblehostmask(np,hostbuf),
	       IPtostr(np->p_ipaddr), printflags(np->umodes, umodeflags), np->realname->name->content);
}

int do_nicksearch(void *source, int cargc, char **cargv) {
  nick *sender = senderNSExtern = source, *np;
  int i;
  struct searchNode *search;
  int limit=500,matches=0;
  char *ch;
  int arg=0;

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
  
  for (i=0;i<NICKHASHSIZE;i++) {
    for (np=nicktable[i];np;np=np->next) {
      if ((search->exe)(search, RETURNTYPE_BOOL, np)) {
	if (matches<limit)
	  printnick(sender, np);
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

void *trueval(int type) {
  switch(type) {
  default:
  case RETURNTYPE_INT:
  case RETURNTYPE_BOOL:
    return (void *)1;
    
  case RETURNTYPE_STRING:
    return "1";
  }
}

void *falseval(int type) {
  switch (type) {
  default:
  case RETURNTYPE_INT:
  case RETURNTYPE_BOOL:
    return NULL;
    
  case RETURNTYPE_STRING:
    return "";
  }
}


/*
 * LITERAL node type: used by the top level parse function
 */

struct literal_localdata {
  int      intval;
  int      boolval;
  sstring *stringval;
};

void literal_free(struct searchNode *thenode);
void *literal_exe(struct searchNode *thenode, int type, void *theinput);

/* search_parse:
 *  Given an input string, return a searchNode.
 */

struct searchNode *search_parse(int type, char *input) {
  /* OK, we need to split the input into chunks on spaces and brackets.. */
  char *argvector[100];
  int i,j;
  char *ch;
  struct Command *cmd;
  struct searchNode *thenode;
  struct literal_localdata *localdata;

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
    argvector[0]="";
    for (ch=input;*ch;ch++) {
      if (i==-1) {
	argvector[j]=ch;
	if (*ch=='(') {
	  i=1;
	} else if (*ch != ' ') {
	  i=0;
	}
      } else if (i==0) {
	if (*ch==' ') {
	  *ch='\0';
	  j++;
          if(j >= (sizeof(argvector) / sizeof(*argvector))) {
            parseError = "Too many arguments";
            return NULL;
          }
	  i=-1;
	}
      } else {
	if (*ch=='(') {
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
    if (!(thenode=(struct searchNode *)malloc(sizeof(struct searchNode)))) {
      parseError = "malloc: could not allocate memory for this search.";
      return NULL;
	}
    if (!(localdata=(struct literal_localdata *)malloc(sizeof (struct literal_localdata)))) {
      /* couldn't malloc() memory for localdata, so free thenode to avoid leakage */
      parseError = "malloc: could not allocate memory for this search.";
      free(thenode);
      return NULL;
    }

    localdata->stringval=getsstring(input,512);
    localdata->intval=strtol(input,NULL,10);
    if (input==NULL || *input=='\0') {
      localdata->boolval = 0;
    } else {
      localdata->boolval = 1;
    }

    thenode->localdata  = localdata;
    thenode->returntype = RETURNTYPE_CONST | RETURNTYPE_STRING;
    thenode->exe        = literal_exe;
    thenode->free       = literal_free;

    return thenode;
  }    
}

void *literal_exe(struct searchNode *thenode, int type, void *theinput) {
  struct literal_localdata *localdata;

  localdata=thenode->localdata;
  
  switch (type) {
  case RETURNTYPE_STRING:
    return (void *)(localdata->stringval->content);
    
  default:
  case RETURNTYPE_BOOL:
    return (void *)((long)(localdata->boolval));

  case RETURNTYPE_INT:
    return (void *)((long)(localdata->intval));
  }
}

void literal_free(struct searchNode *thenode) {
  struct literal_localdata *localdata;

  localdata=thenode->localdata;

  freesstring(localdata->stringval);
  free(localdata);
  free(thenode);
}
