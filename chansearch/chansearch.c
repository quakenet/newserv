#include "chansearch.h"
#include "../parser/parser.h"
#include "../nick/nick.h"
#include "../channel/channel.h"
#include "../control/control.h"
#include "../lib/flags.h"
#include "../lib/irc_string.h"
#include "../lib/version.h"

#include <stdio.h>

MODULE_VERSION("");

#define  MAXTERMS    10
#define  MAXMATCHES  500

CommandTree *searchfilters;
CommandTree *outputfilters;

typedef struct modeflags {
  flag_t setflags;
  flag_t clearflags;
} modeflags; 

void cs_describe(nick *sender, chanindex *cip);
void cs_desctopic(nick *sender, chanindex *cip);
void cs_descservices(nick *sender, chanindex *cip);
int cs_exists(void *chan, int cargc, char **cargv);
int cs_services(void *chan, int cargc, char **cargv);
int cs_nick(void *chan, int cargc, char **cargv);
int cs_size(void *chan, int cargc, char **cargv);
int cs_namelen(void *chan, int cargc, char **cargv);
int cs_modes(void *chan, int cargc, char **cargv);
int cs_name(void *chan, int cargc, char **cargv);
int cs_topic(void *source, int cargc, char **cargv);
int cs_oppct(void *source, int cargc, char **cargv);
int cs_hostpct(void *source, int cargc, char **cargv);
int cs_authedpct(void *source, int cargc, char **cargv);

int dochansearch(void *source, int cargc, char **cargv);

void _init() {
  searchfilters=newcommandtree();
  outputfilters=newcommandtree();
  
  regchansearchfunc("name", 1, cs_name);
  regchansearchfunc("exists", 0, cs_exists);
  regchansearchfunc("services", 1, cs_services);
  regchansearchfunc("nick", 1, cs_nick);
  regchansearchfunc("size", 1, cs_size);
  regchansearchfunc("namelen", 1, cs_namelen);
  regchansearchfunc("modes", 1, cs_modes);
  regchansearchfunc("topic", 1, cs_topic);
  regchansearchfunc("oppercent", 1, cs_oppct);
  regchansearchfunc("uniquehostpct", 1, cs_hostpct);
  regchansearchfunc("authedpct", 1, cs_authedpct);
    
  regchansearchdisp("default", cs_describe);
  regchansearchdisp("topic", cs_desctopic);
  regchansearchdisp("services", cs_descservices);
  
  registercontrolhelpcmd("chansearch",NO_OPER,19,&dochansearch,"Usage: chansearch <criteria>\nSearches for channels with specified criteria.\nSend chanstats with no arguments for more information.");
}

void _fini() {
  deregistercontrolcmd("chansearch",&dochansearch);
  destroycommandtree(searchfilters);
  destroycommandtree(outputfilters);
}

void regchansearchfunc(const char *name, int args, CommandHandler handler) {
  addcommandtotree(searchfilters, name, 0, args, handler);
}

void unregchansearchfunc(const char *name, CommandHandler handler) {
  deletecommandfromtree(searchfilters, name, handler);
}

void regchansearchdisp(const char *name, DisplayFunc handler) {
  addcommandtotree(outputfilters, name, 0, 0, (CommandHandler)handler);
}

void unregchansearchdisp(const char *name, DisplayFunc handler) {
  deletecommandfromtree(outputfilters, name, (CommandHandler)handler);
}

int dochansearch(void *source, int cargc, char **cargv) {
  nick *sender=(nick *)source;
  filter terms[MAXTERMS];
  int i,j;
  int numterms=0;
  Command *mc;
  chanindex *cip;
  int matched=0;
  int offset;
  int res;
  char *ch;
  int limit=MAXMATCHES;
  DisplayFunc df=cs_describe; 
  
  i=0;
  if (cargc>0 && cargv[0][0]=='-') {
    /* We have options, parse them */
    i++;
    for (ch=cargv[0];*ch;ch++) {
      switch(*ch) {
        case 'l': limit=strtol(cargv[i++],NULL,10);
                  break;
                 
        case 'd': if ((mc=findcommandintree(outputfilters,cargv[i],1))==NULL) {
                    controlreply(sender, "Invalid display format %s",cargv[i]);
                    return CMD_ERROR;
                  } else {
                    df=(DisplayFunc)(mc->handler);
                  }
                  i++;
                  break;              
      }
      if (i>=cargc) {
        break;
      }
    }
  }

  if (i>=cargc) {
    Command *cmdlist[100];
    int i,n,bufpos,j;
    char buf[200];
    
    controlreply(sender,"Usage: chansearch [options] (search terms)");
    controlreply(sender," Note that all options must be bundled together (-ld 5 default not -l 5 -d default)");
    controlreply(sender," Current valid options:");
    controlreply(sender,"  -l <number>  Set maximum number of results to return");
    controlreply(sender,"  -d <format>  Set output display format");
    
    /* Get list of display formats */
    
    bufpos=0;
    
    n=getcommandlist(outputfilters,cmdlist,100);
    for (i=0;i<n;i++) {
      for (j=0;j<cmdlist[i]->command->length;j++) {
        buf[bufpos++]=ToLower(cmdlist[i]->command->content[j]);
      }
      if (i<(n-1)) {
        bufpos+=sprintf(&buf[bufpos],", ");
      }
      if (bufpos>180) {
        bufpos+=sprintf(&buf[bufpos]," ...");
        break;
      }
    }
    
    buf[bufpos]='\0';
    
    controlreply(sender," Valid display formats: %s",buf);
    
    /* Get list of search terms */
    
    bufpos=0;
    
    n=getcommandlist(searchfilters,cmdlist,100);
    for (i=0;i<n;i++) {
      for (j=0;j<cmdlist[i]->command->length;j++) {
        buf[bufpos++]=ToLower(cmdlist[i]->command->content[j]);
      }
      if (i<(n-1)) {
        bufpos+=sprintf(&buf[bufpos],", ");
      }
      if (bufpos>180) {
        bufpos+=sprintf(&buf[bufpos]," ...");
        break;
      }
    }
    buf[bufpos]='\0';
    
    controlreply(sender," Valid search terms: %s",buf);
    controlreply(sender," Terms can be inverted with !");
    return CMD_OK;
  }
  
  for (;i<cargc;) {
    if (cargv[i][0]=='!') {
      offset=1;
      terms[numterms].invert=1;
    } else {
      offset=0;
      terms[numterms].invert=0;
    }
    terms[numterms].mallocarg=0;

    /* Do some sanity checking.  Note that the "goto"s allows memory
     * to be freed if successful filters have already allocated some */    
    if ((mc=findcommandintree(searchfilters, &cargv[i][offset], 1))==NULL) {
      controlreply(sender,"Unrecognised search term: %s",cargv[i]);
      goto abort;
    }
    if ((cargc-i-1) < mc->maxparams) {
      controlreply(sender,"Not enough arguments supplied for %s",cargv[i]);
      goto abort;
    }
    
    /* Call the setup function.  This should fill in the function and argument in the structure */
    if ((mc->handler)((void *)&terms[numterms],mc->maxparams,cargv+i+1)) {
      controlreply(sender,"Error setting up filter: %s",cargv[i]);
      goto abort;
    }
    i+=(mc->maxparams+1);
    numterms++;
    
    if (numterms==MAXTERMS)
      break;
  }

  controlreply(sender,"The following channels match your criteria:");
  
  for(i=0;i<CHANNELHASHSIZE;i++) {
    for(cip=chantable[i];cip;cip=cip->next) {
      for(j=0;j<numterms;j++) {
        res=(terms[j].sf)(cip,terms[j].arg);
        if (res==0 && terms[j].invert)
          break;
        if (res==1 && !terms[j].invert)
          break;
      }
      if (j==numterms) {
        if (matched == limit) {
          controlreply(sender,"--- More than %d matches found, truncating list.",limit);
        }
        if (matched < limit) {
          (df)(sender, cip);
        }
        matched++;
      }
    }
  }
  
  if (matched==0) {
    controlreply(sender,"--- No matches found.");
  } else {
    controlreply(sender,"--- End of list: %d match(es).",matched);
  }
  
  /* GOTO target: here we just free the args if any were malloc()d */
abort:
  for (i=0;i<numterms;i++) {
    if (terms[i].mallocarg) {
      free(terms[i].arg);
    }
  }
  
  return CMD_OK;
}


/* Name search execute function: call match2strings */
int cs_nameexe(chanindex *cip, void *arg) {
  char *pattern=(char *)arg;
  
  return !match2strings(pattern, cip->name->content);
}

/* Name search setup function: fill in the struct */

int cs_name(void *source, int cargc, char **cargv) {
  filter *thefilter=(filter *)source;
  
  thefilter->sf=cs_nameexe;
  thefilter->arg=(char *)cargv[0];
  
  return 0;
}

int cs_topicexe(chanindex *cip, void *arg) {
  char *pattern=(char *)arg;
  
  return ((cip->channel==NULL) ||
          (cip->channel->topic==NULL) ||
          (!match2strings(pattern, cip->channel->topic->content)));
}
 
int cs_topic(void *source, int cargc, char **cargv) {
  filter *thefilter=(filter *)source;
  
  thefilter->sf=cs_topicexe;
  thefilter->arg=(void *)cargv[0];
  
  return 0;
}         

/* services - matches if the specified number of services are in the channel */

int cs_servicesexe(chanindex *cip, void *arg) {
  nick *np;
  int i;
  int count=(int)arg;
  
  if (cip->channel==NULL) 
    return 1;
    
  for(i=0;i<cip->channel->users->hashsize;i++) {
    if (cip->channel->users->content[i]==nouser)
      continue;
      
    if (!(np=getnickbynumeric(cip->channel->users->content[i])))
      continue;
      
    if (IsService(np) && !--count)
      return 0;      
  }
  
  return 1;
}

int cs_services(void *source, int cargc, char **cargv) {
  filter *thefilter=(filter *)source;
  
  thefilter->sf=cs_servicesexe;
  thefilter->arg=(void *)strtoul(cargv[0],NULL,10);
  
  return 0;
}

int cs_existsexe(chanindex *cip, void *arg) {
  return (cip->channel==NULL);
}  

int cs_exists(void *source, int cargc, char **cargv) {
  filter *thefilter=(filter *)source;

  thefilter->sf=cs_existsexe;
  thefilter->arg=NULL;
  
  return 0;
}

int cs_nickexe(chanindex *cip, void *arg) {
  nick *np=(nick *)arg;
  
  return ((cip->channel==NULL) || 
          (getnumerichandlefromchanhash(cip->channel->users, np->numeric)==NULL));
}

int cs_nick(void *source, int cargc, char **cargv) {
  filter *thefilter=(filter *)source;
  nick *np;
  
  if ((np=getnickbynick(cargv[0]))==NULL) {
    return 1;
  }
  
  thefilter->sf=cs_nickexe;
  thefilter->arg=(void *)np;
  
  return 0;
}

int cs_sizeexe(chanindex *cip, void *arg) {
  int lim=(int)arg;
  
  return ((cip->channel==NULL) ||
          (cip->channel->users->totalusers < lim));
}

int cs_size(void *source, int cargc, char **cargv) {
  filter *thefilter=(filter *)source;

  thefilter->sf=cs_sizeexe;
  thefilter->arg=(void *)strtol(cargv[0],NULL,10);
   
  return 0;
}

int cs_namelenexe(chanindex *cip, void *arg) {
  int lim=(int)arg;
  
  return (cip->name->length < lim);
}

int cs_namelen(void *source, int cargc, char **cargv) {
  filter *thefilter=(filter *)source;

  thefilter->sf=cs_namelenexe;
  thefilter->arg=(void *)strtol(cargv[0],NULL,10);
  
  return 0;
}

int cs_oppctexe(chanindex *cip, void *arg) {
  int percent=(int)arg;
  int nonop;
  int i;
  
  if (cip->channel==NULL) {
    return 1;
  }
  
  nonop=(cip->channel->users->totalusers)-((cip->channel->users->totalusers*percent)/100);
  
  for (i=0;i<cip->channel->users->hashsize;i++) {
    if (cip->channel->users->content[i]!=nouser) {
      if (!(cip->channel->users->content[i] & CUMODE_OP)) {
        if (!nonop--) {
          return 1;
        }
      }
    }
  }
  
  return 0;
}

int cs_oppct(void *source, int cargc, char **cargv) {
  filter *thefilter=(filter *)source;
  
  thefilter->sf=cs_oppctexe;
  thefilter->arg=(void *)strtol(cargv[0],NULL,10);
  
  return 0;
}          

int cs_hostpctexe(chanindex *cip, void *arg) {
  int percent=(int)arg;
  int hostreq;
  int i;
  unsigned int marker;
  nick *np;
  
  if (cip->channel==NULL) {
    return 1;
  }
  
  marker=nexthostmarker();
  
  hostreq=(cip->channel->users->totalusers * percent) / 100;
  
  for (i=0;i<cip->channel->users->hashsize;i++) {
    if (cip->channel->users->content[i]==nouser)
      continue;
      
    if (!(np=getnickbynumeric(cip->channel->users->content[i])))
      continue;

    if (np->host->marker!=marker) {
      /* new unique host */
      if (--hostreq <= 0) {
        return 0;
      }
      np->host->marker=marker;
    }
  }
  
  return 1;
}

int cs_hostpct(void *source, int cargc, char **cargv) {
  filter *thefilter=source;
  
  thefilter->sf=cs_hostpctexe;
  thefilter->arg=(void *)strtol(cargv[0],NULL,10);
  
  return 0;
}

int cs_authedpctexe(chanindex *cip, void *arg) {
  int i;
  int j=0;
  nick *np;
  int pct=(int)arg;
  
  if (!cip->channel)
    return 1;
  
  for (i=0;i<cip->channel->users->hashsize;i++) {
    if (cip->channel->users->content[i]==nouser)
      continue;
    
    if ((np=getnickbynumeric(cip->channel->users->content[i])) && IsAccount(np))
      j++;
  }
  
  if (((j * 100) / cip->channel->users->totalusers) >= pct)
    return 0;
  else
    return 1;
}

int cs_authedpct(void *source, int cargc, char **cargv) {
  filter *thefilter=source;
  
  thefilter->sf=cs_authedpctexe;
  thefilter->arg=(void *)strtol(cargv[0],NULL,10);

  return 0;
}

int cs_modesexe(chanindex *cip, void *arg) {
  struct modeflags *mf=(struct modeflags *)arg;
  
  return ((cip->channel == NULL) ||
          ((cip->channel->flags & mf->setflags) != mf->setflags) ||
          (cip->channel->flags & mf->clearflags));
}

int cs_modes(void *source, int cargc, char **cargv) {
  filter *thefilter=(filter *)source; 
  struct modeflags *mf;
  flag_t flags=0;
 
  mf=(void *)malloc(sizeof(struct modeflags));
  
  setflags(&flags, CHANMODE_ALL, cargv[0], cmodeflags, REJECT_NONE);
  
  mf->setflags=flags;  
  
  flags=CHANMODE_ALL;
  setflags(&flags, CHANMODE_ALL, cargv[0], cmodeflags, REJECT_NONE);
  
  mf->clearflags=~flags;

  thefilter->sf=cs_modesexe;
  thefilter->arg=(void *)mf;
  thefilter->mallocarg=1;
  
  return 0;  
}

void cs_describe(nick *sender, chanindex *cip) {
  int i;
  int op,voice,peon;
  int oper,service,hosts;
  nick *np;
  chanuserhash *cuhp;
  unsigned int marker;
  
  op=voice=peon=oper=service=hosts=0;
  marker=nexthostmarker();
  
  if (cip->channel==NULL) {
    controlreply(sender,"[         Channel currently empty          ] %s",cip->name->content);
  } else {
    cuhp=cip->channel->users;
    for (i=0;i<cuhp->hashsize;i++) {
      if (cuhp->content[i]!=nouser) {
        if (cuhp->content[i]&CUMODE_OP) {
          op++;
        } else if (cuhp->content[i]&CUMODE_VOICE) {
          voice++;
        } else {
          peon++;
        }
        if ((np=getnickbynumeric(cuhp->content[i]&CU_NUMERICMASK))!=NULL) {
          if (IsOper(np)) {
            oper++;
          }
          if (IsService(np)) {
            service++;
          }
          if (np->host->marker!=marker) {
            np->host->marker=marker;
            hosts++;
          }            
        }
      }
    }
    controlreply(sender,"[ %4dU %4d@ %4d+ %4d %4d* %4dk %4dH ] %s (%s)",cuhp->totalusers,op,voice,peon,oper,service,hosts,
                                                cip->name->content, printflags(cip->channel->flags, cmodeflags));
  }
}

void cs_desctopic(nick *sender, chanindex *cip) {
  if (cip->channel==NULL) {
    controlreply(sender,"[   empty  ] %-30s",cip->name->content);
  } else {
    controlreply(sender,"[%4u users] %s (%s)",cip->channel->users->totalusers,cip->name->content,cip->channel->topic?cip->channel->topic->content:"no topic");
  }
}

void cs_descservices(nick *sender, chanindex *cip) {
  int i;
  chanuserhash *cuhp;
  char servlist[300];
  int slpos=0,slfull=0;
  nick *np;
  int servs=0;
  
  if (cip->channel==NULL) {
    controlreply(sender,"%-30s empty",cip->name->content);
  } else {
    cuhp=cip->channel->users;
    for (i=0;i<cuhp->hashsize;i++) {
      if (cuhp->content[i]!=nouser) {
        if ((np=getnickbynumeric(cuhp->content[i]&CU_NUMERICMASK))) {
          if (IsService(np)) {
            servs++;
            if (!slfull) {
              if (slpos) {
                slpos+=sprintf(&servlist[slpos],", ");
              }
              slpos+=sprintf(&servlist[slpos],"%s",np->nick);
              if (slpos>280) {
                sprintf(&servlist[slpos],", ...");
                slfull=1;
              }
            }
          }
        }
      }
    }  
     
    controlreply(sender,"%-30s %5d user%c %2d service%c %s%s%s",cip->name->content,cuhp->totalusers,
                         cuhp->totalusers>1?'s':' ',servs,(servs==1)?' ':'s',servs?"(":"",slpos?servlist:"",servs?")":"");
  }
}

