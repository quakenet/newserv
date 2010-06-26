
#include "chanprofile.h"
#include "../channel/channel.h"
#include "../control/control.h"
#include "../newsearch/newsearch.h"
#include "../lib/version.h"

MODULE_VERSION("");

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

unsigned int activeprofiles;
struct chanprofile *cptable[CPHASHSIZE];

struct chanprofile *getcprec(nick *np) {
  unsigned int hash=0,mhash;
  unsigned int ccount=0;
  unsigned int clen=0;
  unsigned int i;
  struct chanprofile *cpp;
  
  struct channel **cs=(np->channels->content);
  
  ccount=np->channels->cursi;
  
  for (i=0;i<np->channels->cursi;i++) {
    clen+=cs[i]->index->name->length;
    hash ^= (unsigned long)cs[i];
  }
  
  mhash=hash%CPHASHSIZE;
  
  for (cpp=cptable[mhash];cpp;cpp=cpp->next) {
    if ((cpp->hashval==hash) && (cpp->ccount==ccount) && 
          (cpp->clen==clen)) 
      return cpp;
  }
  
  cpp=malloc(sizeof(struct chanprofile));
  
  cpp->clones=0;
  cpp->hashval=hash;
  cpp->ccount=ccount;
  cpp->clen=clen;
  cpp->nicks=NULL;
  cpp->next=cptable[mhash];
  cptable[mhash]=cpp;
  
  activeprofiles++;
  
  return cpp;
}

void popprofiles() {
  unsigned int i;
  struct chanprofile *cpp;
  nick *np;
  
  /* Create the chanprofile records and count clones for each profile */
  for (i=0;i<NICKHASHSIZE;i++) {
    for (np=nicktable[i];np;np=np->next) {
      cpp=getcprec(np);
      cpp->clones++;
    }
  }
  
  /* Allocate space for nick array in each profile */
  for (i=0;i<CPHASHSIZE;i++) {
    for (cpp=cptable[i];cpp;cpp=cpp->next) {
      cpp->nicks=(nick **)malloc(cpp->clones * sizeof(nick *));
      cpp->clones=0;
    }
  }
  
  /* Populate the nick arrays */
  for (i=0;i<NICKHASHSIZE;i++) {
    for (np=nicktable[i];np;np=np->next) {
      cpp=getcprec(np);
      cpp->nicks[cpp->clones++]=np;
    }
  }
}

void clearprofiles() {
  unsigned int i;
  struct chanprofile *cpp, *ncpp;
  
  for (i=0;i<CPHASHSIZE;i++) {
    for (cpp=cptable[i];cpp;cpp=ncpp) {
      ncpp=cpp->next;
      free(cpp->nicks);
      free(cpp);
    }
    cptable[i]=NULL;
  }
  
  activeprofiles=0;
}

int cpcompare(const void *a, const void *b) {
  const struct chanprofile **cpa=(const struct chanprofile **)a, **cpb=(const struct chanprofile **)b;
  
  return (*cpb)->clones - (*cpa)->clones;
}

void reportprofile(nick *sender, struct chanprofile *cpp) {
  channel **cps, *cp;
  unsigned int i;
  nick *np;
  char buf[1024];
  unsigned int repwidth=80;
  unsigned int bufpos;
  searchCtx ctx;

  controlreply(sender,"============================================================");

  if (cpp->ccount==0) {
    controlreply(sender,"(no channels): %u users",cpp->clones);
    return;
  }
  
  controlreply(sender,"Channels (%u):",cpp->ccount);
  
  np=cpp->nicks[0];
  cps=np->channels->content;
  bufpos=0;
  for (i=0;i<np->channels->cursi;i++) {
    cp=cps[i];
    if (bufpos && ((bufpos + cp->index->name->length) > repwidth)) {
      controlreply(sender," %s",buf);
      bufpos=0;
    }
    
    bufpos += sprintf(buf+bufpos,"%s ",cp->index->name->content);
  }
  
  if (bufpos)
    controlreply(sender," %s",buf);
    
  controlreply(sender,"Users (%u):",cpp->clones);

  ctx.reply = controlreply;
  for (i=0;i<cpp->clones;i++) {
    printnick(&ctx, sender,cpp->nicks[i]);
  }
}

int reportprofiles(void *source, int cargc, char **cargv) {
  unsigned int i,j;
  struct chanprofile **cpary, *cpp;
  unsigned int repnum=20;
  nick *sender=source;
  
  if (cargc>0) 
    repnum=strtoul(cargv[0],NULL,10);
  
  popprofiles();
  
  /* Make a big fat array */
  cpary=malloc(activeprofiles*sizeof(struct chanprofile *));
  for (i=0,j=0;i<CPHASHSIZE;i++)
    for (cpp=cptable[i];cpp;cpp=cpp->next)
      cpary[j++]=cpp;

  controlreply(sender,"Populated array, lest number=%u (should be %u)",j,activeprofiles);
      
  qsort(cpary, activeprofiles, sizeof(struct chanprofile *), cpcompare); 
  
  controlreply(sender,"Top %u channel profiles (%u total):",repnum,activeprofiles);
  
  for (i=0;i<repnum;i++) {
    reportprofile(sender,cpary[i]);
  }
   
  controlreply(sender,"--- End of list."); 

  clearprofiles();  
  
  return CMD_OK;
}

void _init() {
  memset(cptable,0,sizeof(cptable));
  registercontrolhelpcmd("chanprofiles",NO_DEVELOPER,1,reportprofiles,"Usage: chanprofiles [count]\nDisplays the most common channel profiles.  count defaults to 20.");
}

void _fini() {
  deregistercontrolcmd("chanprofiles",reportprofiles);
}

