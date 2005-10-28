/* parser.c */

#include "parser.h"
#include "../lib/sstring.h"
#include "../lib/irc_string.h"
#include "../core/error.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

/* Local functions */
int insertcommand(Command *c, CommandTree *ct, int depth);
int deletecommand(sstring *cmdname, CommandTree *ct, int depth, CommandHandler handler);
Command *findcommand(CommandTree *ct, const char *command, int depth);
int countcommandtree(CommandTree *ct);

/* newcommandtree:
 *
 * This creates a new command tree.
 *
 * Uses malloc() as it's a pretty rare event.
 */

CommandTree *newcommandtree() {
  CommandTree *nct;
  
  nct=(void *)malloc(sizeof(CommandTree));
  memset(nct,0,sizeof(CommandTree));

  return nct;
}

/* destroycommandtree:
 *
 * This frees a tree and all it's subtrees
 */
 
void destroycommandtree(CommandTree *ct) {
  int i;
  
  for (i=0;i<26;i++) {
    if (ct->next[i]) {
      destroycommandtree((CommandTree *)ct->next[i]);
    }
  }
  
  free(ct);
}

/* countcommandtree:
 *
 * This returns the number of commands registered in
 * a particular (sub-)tree.  This will come in handy
 * later for deleting commands etc.
 */ 

int countcommandtree(CommandTree *ct) {
  int i;
  int sum;
  
  if (ct->cmd!=NULL) {
    sum=1;
  } else {
    sum=0;
  }
  
  for(i=0;i<26;i++)
    if (ct->next[i])
      sum+=countcommandtree((CommandTree *)ct->next[i]);
  
  return sum;
}

/* 
 * addcommandhelptotree:
 *
 * This installs a specific command in a tree, with a help paramater.
 *
 * This function builds the Command structure in addition to 
 * installing it in the tree
 */
 
Command *addcommandhelptotree(CommandTree *ct, const char *cmdname, int level, int maxparams, CommandHandler handler, const char *help) {
  Command *nc, *c;
  int i;
  
  /* Generate the struct.. */
  nc=(void *)malloc(sizeof(Command));
  nc->command=getsstring(cmdname, MAX_COMMAND_LEN);
  nc->level=level;
  nc->maxparams=maxparams;
  nc->handler=handler;
  nc->ext=NULL;
  nc->next=NULL;
  if (help) {
    int len=strlen(help);
    nc->help=(char *)malloc(len+1);
    if(nc->help) {
      strncpy(nc->help, help, len);
      nc->help[len] = '\0';
    }
  } else {
    nc->help=NULL;
  }

  /* Sanity check the string */
  for (i=0;i<nc->command->length;i++) {
    nc->command->content[i]=toupper(nc->command->content[i]);
    if (nc->command->content[i]<'A' || nc->command->content[i]>'Z') {
      /* Someone tried to register an invalid command name */
      freesstring(nc->command);
      if(nc->help)
        free(nc->help);
      free(nc);
      return NULL;    
    }
  }

  if ((c=findcommandintree(ct,cmdname,1))!=NULL) {
    /* Found something already.  Append our entry to the end */
    while (c->next!=NULL) 
      c=(Command *)c->next; 
    c->next=(struct Command *)nc;
  } else if (insertcommand(nc,ct,0)) {
    /* Erk, that didn't work.. */
    freesstring(nc->command);
    if(nc->help)
      free(nc->help);
    free(nc);
    return NULL;
  }
  
  return nc;
}

/*
 * insertcommand: internal recursive function to do actual command insertion
 */

int insertcommand(Command *c, CommandTree *ct, int depth) {
  int nextcharindex=c->command->content[depth]-'A';
  
  if ((c->command->length==depth) || (countcommandtree(ct)==0)) {
    if(ct->cmd!=NULL) {
      int oldcharindex;
      /* There is already another command at this level */
      if(ct->final[0]=='\0') {
        /* It's a conflict with us, shouldn't happen */
        return 1;      
      }
      oldcharindex=ct->final[0]-'A';
      if (ct->next[oldcharindex] != NULL) {
        /* Shouldn't happen */
        Error("parser",ERR_ERROR,"Unexpected command subtree conflicts with final value");
      } else {
        ct->next[oldcharindex]=(struct CommandTree *)newcommandtree();
      }
      insertcommand(ct->cmd,(CommandTree *)ct->next[oldcharindex],depth+1);
    }
    ct->cmd=c;
    ct->final=&(c->command->content[depth]);
    return 0;
  } else {
    if ((ct->cmd!=NULL) && (ct->final[0]!='\0')) {
      int oldcharindex=ct->final[0]-'A';
      /* Someone marked this node as final, we have to undo that since we're now here too */
      if (ct->next[oldcharindex] != NULL) {
        Error("parser",ERR_ERROR,"Unexpected command subtree conflicts with final value");
        /* Shouldn't happen */
      } else {
        ct->next[oldcharindex]=(struct CommandTree *)newcommandtree();
      }
      insertcommand(ct->cmd,(CommandTree *)ct->next[oldcharindex],depth+1);
      ct->cmd=NULL;
      ct->final="";
    }    
        
    if (ct->next[nextcharindex]==NULL) {
      ct->next[nextcharindex]=(struct CommandTree *)newcommandtree();
    }
    return insertcommand(c,(CommandTree *)ct->next[nextcharindex],depth+1);
  }
}

int deletecommandfromtree(CommandTree *ct, const char *cmdname, CommandHandler handler) {
  int i;
  sstring *tmpstr;
  
  tmpstr=getsstring(cmdname,MAX_COMMAND_LEN);
  
  /* Sanity check input string */
  for (i=0;i<tmpstr->length;i++) {
    tmpstr->content[i]=toupper(tmpstr->content[i]);
    if (tmpstr->content[i]<'A' || tmpstr->content[i]>'Z') {
      /* Someone tried to delete an invalid command name */
      freesstring(tmpstr);
      return 1;
    }
  }
  i=deletecommand(tmpstr,ct,0,handler);
  freesstring(tmpstr);
  return i;
}

int deletecommand(sstring *cmdname, CommandTree *ct, int depth, CommandHandler handler) {
  Command **ch, *c;
  int nextcharindex=(cmdname->content[depth])-'A';
  int i;
  
  if (depth==cmdname->length) {
    /* Hit the end of the string.. the command should be in this node */
    if ((ct->cmd==NULL) ||
        (ct->cmd->command->length != cmdname->length) ||
        (strncmp(ct->cmd->command->content,cmdname->content,cmdname->length))) {
      return 1;
    }
    for(ch=&(ct->cmd);*ch;ch=(Command **)&((*ch)->next)) {
      if ((*ch)->handler==handler) {
        c=*ch;
        (*ch)=(Command *)((*ch)->next);
        freesstring(c->command);
        if(c->help)
          free(c->help);
        free(c);
        return 0;
      }
    }
    return 1;
  } else if ((ct->final) && (ct->final[0]==cmdname->content[depth])) {
    /* We have a potentially matching final string here, double check */
    if ((ct->cmd->command->length != cmdname->length) ||
        (strncmp(ct->cmd->command->content,cmdname->content,cmdname->length))) {
      return 1;
    }
    /* Find the command in the potential chain and remove it */
    for(ch=&(ct->cmd);*ch;ch=(Command **)&((*ch)->next)) {
      if ((*ch)->handler==handler) {
        c=*ch;
        (*ch)=(Command *)((*ch)->next);
        freesstring(c->command);
        if(c->help)
          free(c->help);
        free(c);
        return 0;
      }
    }
    return 1;
  } else {
    /* We're going to have to recurse.. */
    if (ct->next[nextcharindex]==NULL) {
      return 1;
    } else {
      i=deletecommand(cmdname,(CommandTree *)ct->next[nextcharindex],depth+1,handler);
      if (countcommandtree((CommandTree *)ct->next[nextcharindex])==0) {
        free(ct->next[nextcharindex]);
        ct->next[nextcharindex]=NULL;
      }
      return i;
    }
  }
}

/* 
 * findcommandintree: Takes a command string and returns the relevant
 *                    structure, if found.
 */
 
Command *findcommandintree(CommandTree *ct, const char *command, int strictcheck) {
  Command *c;
  
  c=findcommand(ct,command,0);
  
  if (c==NULL)
    return NULL;
    
  if (strictcheck && 
      (ircd_strncmp(command,c->command->content,c->command->length) ||
       (c->command->length != strlen(command))))
    return NULL;

  return c;  
}

/* 
 * findcommand: Internal recursive function to find a command
 */
 
Command *findcommand(CommandTree *ct, const char *command, int depth) {
  int nextchar=toupper(command[depth]);
  
  /* If we've run out of string, we return whatever we find in this node,
   * be it NULL or otherwise. */
  if (nextchar=='\0') {
    return ct->cmd;
  }
  
  if ((ct->cmd!=NULL) && ct->final[0]==nextchar) {
    return ct->cmd;
  }
  
  if (nextchar<'A' || nextchar>'Z') {
    return NULL;
  }
  
  if (ct->next[nextchar-'A']==NULL) {
    return NULL;
  } else {
    return findcommand((CommandTree *)ct->next[nextchar-'A'], command, depth+1);
  }
}

/*
 * getcommandlist: Returns the contents of a CommandTree in a user-supplied Command * array
 */
 
int getcommandlist(CommandTree *ct, Command **commandlist, int maxcommands) {
  int i=0,count=0;
  
  if (maxcommands<0) {
    return 0;
  }
  
  if (ct->cmd) {
    commandlist[count++]=ct->cmd;
  }
  
  for (i=0;i<26;i++) {
    if(ct->next[i]) {
      count+=getcommandlist(ct->next[i], &commandlist[count], maxcommands-count);
    }
  }
  
  return count;
}
