#include <stdio.h>

#include "../chanserv.h"
#include "../../newsearch/newsearch.h"
#include "../../control/control.h"
#include "../../lib/stringbuf.h"

void printchannel_qusers(searchCtx *ctx, nick *sender, chanindex *cip) {
  regchanuser *rcup;
  regchan *rcp;
  int i;
  int own=0,mas=0,op=0,voi=0,kno=0,ban=0,tot=0;
  
  if (!(rcp=cip->exts[chanservext])) {
    ctx->reply(sender,"[              - not registered -                 ] %s",cip->name->content);
    return;
  }
  
  for (i=0;i<REGCHANUSERHASHSIZE;i++) {
    for (rcup=rcp->regusers[i];rcup;rcup=rcup->nextbychan) {
      tot++;
      
      if (CUIsOwner(rcup)) own++; else
      if (CUIsMaster(rcup)) mas++; else
      if (CUIsOp(rcup)) op++; else
      if (CUIsVoice(rcup)) voi++; else
      if (CUIsKnown(rcup)) kno++;
      
      if (CUIsBanned(rcup)) ban++;
    }
  }
  
  ctx->reply(sender,"[%4dn %4dm %4do %4dv %4dk %4db - %4d total ] %s",own,mas,op,voi,kno,ban,tot,cip->name->content);
}
  

void printnick_auth(searchCtx *ctx, nick *sender, nick *np) {
  struct reguser *rup;
  
  if (!(rup=getreguserfromnick(np))) {
    ctx->reply(sender,"%s (not authed)",np->nick);
  } else {
    ctx->reply(sender,"%s (%s/%u) (%s) (%s)",np->nick,rup->username,rup->ID,
        rup->email ? rup->email->content : "no email",
        rup->comment ? rup->comment->content : "no comment" );
  }
}

void printnick_authchans(searchCtx *ctx, nick *sender, nick *np) {
  struct reguser *rup;
  struct regchanuser *rcup;
  char thebuf[1024];
  char buf2[512];
  unsigned int bufpos=0, buf2len;
  unsigned char ch;
  
  printnick_auth(ctx, sender,np);
  
  if (!(rup=getreguserfromnick(np)))
    return;
  
  if (!rup->knownon) {
    ctx->reply(sender, " (no channels)");
  } else {
    for (rcup=rup->knownon;rcup;rcup=rcup->nextbyuser) {
      if (CUIsOwner(rcup))
        ch='*';
      else if (CUHasMasterPriv(rcup))
        ch='&';
      else if (CUHasOpPriv(rcup))
        ch='@';
      else if (CUHasVoicePriv(rcup))
        ch='+';
      else if (CUKnown(rcup))
        ch=' ';
      else 
        ch='!';
      
      buf2len=sprintf(buf2,"%c%s",ch,rcup->chan->index->name->content);

      if (buf2len+bufpos > 400) {
        ctx->reply(sender," %s", thebuf);
        bufpos=0;
      } 
      bufpos+=sprintf(thebuf+bufpos,"%s ",buf2);
    }
    if (bufpos)
      ctx->reply(sender," %s", thebuf);
  }
}

void printauth(searchCtx *ctx, nick *sender, authname *anp) {
  reguser *rup;
  char *la;
  char timebuf[TIMELEN];

/*  StringBuf b;
  char output[1024];
  nick *tnp;
  int space = 0;
*/

  if (!(rup=anp->exts[chanservaext]))
    return;

/*
  output[0] = '\0';

  b.capacity = sizeof(output);
  b.len = 0;
  b.buf = output;

  for(tnp=anp->nicks;tnp;tnp=tnp->next) {
    if(space)
      sbaddchar(&b, ' ');
    space = 1;
    sbaddstr(&b, tnp->nick);
  }
  sbterminate(&b);

  ctx->reply(sender, " %s%s%s%s", rup->username, *output?" (":"", output, *output?")":"");
*/

  if (rup->lastauth) {
    q9strftime(timebuf, sizeof(timebuf), rup->lastauth);
    la = timebuf;
  } else {
    la = "(never)";
  }

  ctx->reply(sender, "%-15s %-10s %-30s %-15s %s", rup->username, UHasSuspension(rup)?"yes":"no", rup->email?rup->email->content:"(no email)", la, rup->lastuserhost?rup->lastuserhost->content:"(no last host)");
}
