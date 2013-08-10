
#include <stdio.h>
#include "newsearch.h"

#include "../irc/irc_config.h"
#include "../lib/irc_string.h"
#include "../parser/parser.h"
#include "../control/control.h"
#include "../lib/splitline.h"
#include "../lib/version.h"

void printnick_header(searchCtx *ctx, nick *sender, void *args) {
  ctx->reply(sender, "Hostmask [IP] (modes) (realname)");
}

void printnick(searchCtx *ctx, nick *sender, nick *np) {
  char hostbuf[HOSTLEN+NICKLEN+USERLEN+4];

  ctx->reply(sender,"%s [%s] (%s) (%s)",visiblehostmask(np,hostbuf),
	       IPtostr(np->ipaddress), printflags(np->umodes, umodeflags), np->realname->name->content);
}

void printnick_channels(searchCtx *ctx, nick *sender, nick *np) {
  char chanlistbuf[512];
  unsigned int bufpos=0, overflow=0;
  channel **cs, *cp;
  unsigned int i;
  unsigned long *lp;
  
  /* Include the default format too */
  printnick(ctx, sender,np);
  
  /* Now add the channels.. */
  cs=(channel **)(np->channels->content);
  for (i=0;i<np->channels->cursi;i++) {
    cp=cs[i];
    
    if (!(lp=getnumerichandlefromchanhash(cp->users,np->numeric)))
      /* "Impossible" error case - nick not on this channel */
      continue;
    
    if (bufpos + cp->index->name->length > 400) {
      overflow=1;
      break;
    }
    
    if (*lp & CUMODE_OP) {
      chanlistbuf[bufpos++]='@';
    } else if (*lp & CUMODE_VOICE) {
      chanlistbuf[bufpos++]='+';
    }
    
    bufpos+=sprintf(chanlistbuf+bufpos,"%s ",cp->index->name->content);
  }
  
  if (!bufpos) {
    ctx->reply(sender,"  Not an any channels.");
  } else {
    ctx->reply(sender,"  On channel%s: %s%s",np->channels->cursi>1?"s":"", chanlistbuf, overflow?"[...]":"");
  }
}

void printchannel(searchCtx *ctx, nick *sender, chanindex *cip) {
  /* shamelessly stolen from (now defunct) chansearch.c */
  int i;
  int op,voice,peon;
  int oper,service,hosts;
  nick *np;
  chanuserhash *cuhp;
  unsigned int marker;
  
  op=voice=peon=oper=service=hosts=0;
  marker=nexthostmarker();
  
  if (cip->channel==NULL) {
    ctx->reply(sender,"[         Channel currently empty          ] %s",cip->name->content);
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
    ctx->reply(sender,"[ %4dU %4d@ %4d+ %4d %4d* %4dk %4dH ] %s (%s)",cuhp->totalusers,op,voice,peon,oper,
      service,hosts,cip->name->content, printflags(cip->channel->flags, cmodeflags));
  }
}


void printchannel_topic(searchCtx *ctx, nick *sender, chanindex *cip) {
  if (cip->channel==NULL) {
    ctx->reply(sender,"[   empty  ] %-30s",cip->name->content);
  } else {
    ctx->reply(sender,"[%4u users] %s (%s)",cip->channel->users->totalusers,cip->name->content,cip->channel->topic?cip->channel->topic->content:"no topic");
  }
}

void printchannel_services(searchCtx *ctx, nick *sender, chanindex *cip) {
  int i;
  chanuserhash *cuhp;
  char servlist[300];
  int slpos=0,slfull=0;
  nick *np;
  int servs=0;
  
  if (cip->channel==NULL) {
    ctx->reply(sender,"%-30s empty",cip->name->content);
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
     
    ctx->reply(sender,"%-30s %5d user%c %2d service%c %s%s%s",cip->name->content,cuhp->totalusers,
                         cuhp->totalusers>1?'s':' ',servs,(servs==1)?' ':'s',servs?"(":"",slpos?servlist:"",servs?")":"");
  }
}

void printuser(searchCtx *ctx, nick *sender, authname *aup) {
  ctx->reply(sender,"%d", aup->userid);
}

void printwhowas(searchCtx *ctx, nick *sender, whowas *ww) {
  ctx->reply(sender,"%s", whowas_format(ww));
  ctx->reply(sender,"%s", whowas_formatchannels(ww));
}
