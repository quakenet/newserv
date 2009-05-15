/*
 * chanservuser.c:
 *  This file maintains the functions associated with the
 * user on the network relating to the channel service
 */

#include "chanserv.h"

#include "../core/hooks.h"
#include "../core/schedule.h"
#include "../core/config.h"
#include "../localuser/localuser.h"
#include "../localuser/localuserchannel.h"
#include "../irc/irc_config.h"
#include "../lib/sstring.h"
#include "../nick/nick.h"
#include "../parser/parser.h"
#include "../lib/splitline.h"
#include "../lib/irc_string.h"

#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

nick *chanservnick;
CommandTree *cscommands;
CommandTree *csctcpcommands;

/* Local prototypes */
void chanservuserhandler(nick *target, int message, void **params);

void chanservreguser(void *arg) {
  sstring *csnick,*csuser,*cshost,*csrealname,*csaccount;
  chanindex *cip;
  regchan   *rcp;
  int i;

  csnick=getcopyconfigitem("chanserv","nick","Q",NICKLEN);
  csuser=getcopyconfigitem("chanserv","user","TheQBot",USERLEN);
  cshost=getcopyconfigitem("chanserv","host","some.host",HOSTLEN);
  csrealname=getcopyconfigitem("chanserv","realname","ChannelService",REALLEN);
  csaccount=getcopyconfigitem("chanserv","account",csnick&&csnick->content&&csnick->content[0]?csnick->content:"Q",ACCOUNTLEN);

  Error("chanserv",ERR_INFO,"Connecting %s...",csnick->content);

  chanservnick=registerlocaluser(csnick->content,csuser->content,cshost->content,
				 csrealname->content,csaccount->content,
				 UMODE_INV|UMODE_SERVICE|UMODE_DEAF|UMODE_OPER|UMODE_ACCOUNT,
				 &chanservuserhandler);

  freesstring(csnick);
  freesstring(csuser);
  freesstring(cshost);
  freesstring(csrealname);
  freesstring(csaccount);

  /* Now join channels */
  for (i=0;i<CHANNELHASHSIZE;i++) {
    for (cip=chantable[i];cip;cip=cip->next) {
      if (cip->channel && (rcp=cip->exts[chanservext]) && !CIsSuspended(rcp)) {
        /* This will do timestamp faffing even if it won't actually join */
        chanservjoinchan(cip->channel);
	/* Do a check at some future time.. */
	cs_schedupdate(cip, 1, 5);
	rcp->status |= (QCSTAT_OPCHECK | QCSTAT_MODECHECK | QCSTAT_BANCHECK);
        if (CIsForceTopic(rcp) || CIsTopicSave(rcp))
          localsettopic(chanservnick, cip->channel, (rcp->topic) ? rcp->topic->content : "");
      }
    }
  }

  Error("chanserv",ERR_INFO,"Loaded and joined channels.");

  if (chanserv_init_status == CS_INIT_NOUSER) {
    /* If this is the first time, perform last init tasks */
    chanserv_finalinit();
  }
}

void chanservuserhandler(nick *target, int message, void **params) {
  nick *sender;
  char *msg;
  Command *cmd;
  char *cargv[30];
  int cargc;
  reguser *rup;
  char *chp;

  switch(message) {
  case LU_KILLED:
    scheduleoneshot(time(NULL),&chanservreguser,NULL);
    chanservnick=NULL;
    break;
    
  case LU_PRIVMSG:
  case LU_SECUREMSG:
    sender=(nick *)params[0];
    msg=(char *)params[1];
    
    if (!(cargc=splitline(msg,cargv,30,0)))
      return; /* Ignore blank lines */     

    if (cargv[0][0]==1) {
      /* CTCP */
      for (chp=cargv[0]+1;*chp;chp++) {
	if (*chp=='\001') {
	  *chp='\0';
	  break;
	}
      }
      cmd=findcommandintree(csctcpcommands, cargv[0]+1, 1);
      if (cmd) {
	cmd->calls++;
	rejoinline(cargv[1],cargc-1);
	cmd->handler((void *)sender, cargc-1, &(cargv[1]));
      }      
    } else {
      cmd=findcommandintree(cscommands, cargv[0], 1);
      
      if (!cmd) {
	chanservstdmessage(sender, QM_UNKNOWNCMD, cargv[0]);
	break;
      }
      
      if ((cmd->level & QCMD_SECURE) && (message != LU_SECUREMSG)) {
	chanservstdmessage(sender, QM_SECUREONLY, cargv[0], chanservnick->nick, myserver->content);
	break;
      }
      
      if ((cmd->level & QCMD_AUTHED)) {
	if (!(rup=getreguserfromnick(sender))) {
	  chanservstdmessage(sender, QM_AUTHEDONLY, cargv[0]);
	  break;
	}
	if (UIsSuspended(rup) || (rup->status & QUSTAT_DEAD)) {
	  chanservstdmessage(sender, QM_BADAUTH, cargv[0]);
	  break;
	}
      }
      
      if ((cmd->level & QCMD_NOTAUTHED) && (getreguserfromnick(sender))) {
	chanservstdmessage(sender, QM_UNAUTHEDONLY, cargv[0]);
	break;
      }
      
      if ((cmd->level & QCMD_STAFF) && 
	  (!(rup=getreguserfromnick(sender)) || !UHasStaffPriv(rup))) {
	chanservstdmessage(sender, QM_NOACCESS, cargv[0]);
	break;
      }
      
      if ((cmd->level & QCMD_HELPER) && 
	  (!(rup=getreguserfromnick(sender)) || !UHasHelperPriv(rup))) {
	chanservstdmessage(sender, QM_NOACCESS, cargv[0]);
	break;
      }
      
      if ((cmd->level & QCMD_OPER) &&
	  (!IsOper(sender) || !(rup=getreguserfromnick(sender)) || !UHasOperPriv(rup))) {
	chanservstdmessage(sender, QM_NOACCESS, cargv[0]);
	break;
      }
      
      if ((cmd->level & QCMD_ADMIN) &&
	  (!IsOper(sender) || !(rup=getreguserfromnick(sender)) || !UHasAdminPriv(rup))) {
	chanservstdmessage(sender, QM_NOACCESS, cargv[0]);
	break;
      }      
      
      if ((cmd->level & QCMD_DEV) &&
	  (!IsOper(sender) || !(rup=getreguserfromnick(sender)) || !UIsDev(rup))) {
	chanservstdmessage(sender, QM_NOACCESS, cargv[0]);
	break;
      }
      
      cmd->calls++;
      
      if (cmd->maxparams < (cargc-1)) {
	rejoinline(cargv[cmd->maxparams],cargc-(cmd->maxparams));
	cargc=(cmd->maxparams)+1;
      }
      
      cmd->handler((void *)sender, cargc-1, &(cargv[1]));
    }
    break;

  default:
    break;
  }
}

void chanservcommandinit() {
  cscommands=newcommandtree();
  csctcpcommands=newcommandtree();
}

void chanservcommandclose() {
  destroycommandtree(cscommands);
  destroycommandtree(csctcpcommands);
}

void chanservaddcommand(char *command, int flags, int maxparams, CommandHandler handler, char *description, const char *help) {
  Command *newcmd;
  cmdsummary *summary;

  newcmd=addcommandtotree(cscommands, command, flags, maxparams, handler);
  /* Allocate a summary record to put the description in */
  summary=(cmdsummary *)malloc(sizeof(cmdsummary));
  memset((void *)summary,0,sizeof(cmdsummary));

  summary->def=getsstring(description, 250);
  summary->defhelp=(char *)help; /* Assume that help is a constant */
  
  newcmd->ext=(void *)summary;
  loadcommandsummary(newcmd);
}

void chanservaddctcpcommand(char *command, CommandHandler handler) {
  addcommandtotree(csctcpcommands, command, 0, 1, handler);
}

void chanservremovectcpcommand(char *command, CommandHandler handler) {
  deletecommandfromtree(csctcpcommands, command, handler);
}

void chanservremovecommand(char *command, CommandHandler handler) {
  Command *cmd;
  cmdsummary *summary;
  int i;

  if (!(cmd=findcommandintree(cscommands, command, 1))) {
    Error("chanserv",ERR_WARNING,"Tried to unregister unknown command %s",command);
    return;
  }
 
  summary=(cmdsummary *)cmd->ext;
  freesstring(summary->def);
  for (i=0;i<MAXLANG;i++) {
    if (summary->bylang[i])
      freesstring(summary->bylang[i]);
  }

  free(summary);

  deletecommandfromtree(cscommands, command, handler);
}

void chanservpartchan(channel *cp, char *reason) {
  /* Sanity check that we exist and are on the channel.
   *
   * Note that we don't do any of the other usual sanity checks here; if
   * this channel is unregged or suspended or whatever then all the more
   * reason to get Q off it ASAP! */
  if (!chanservnick || !cp || !cp->users || !getnumerichandlefromchanhash(cp->users, chanservnick->numeric))
    return;  
  
  localpartchannel(chanservnick, cp, reason);
}

void chanservjoinchan(channel *cp) {
  regchan *rcp;
  unsigned int i;
  nick *np;
  reguser *rup;
  regchanuser *rcup;
  flag_t themodes = 0;

  /* Skip unregistered channels */
  if (!(rcp=cp->index->exts[chanservext]))
    return;
    
  /* Check for a new timestamp.  But don't do anything stupid if the incoming timestamp is 0 */
  if (cp->timestamp && ((!rcp->ltimestamp) || (cp->timestamp < rcp->ltimestamp))) {
    rcp->ltimestamp=cp->timestamp;
    csdb_updatechanneltimestamp(rcp);
  }

  /* We won't be doing any joining or parting if we're not online */
  if (!chanservnick)
    return;

  /* In gerenal this function shouldn't be used any more as a reason to get
   * Q to leave, but if it should be leaving then just part with no reason. */
  if ((CIsSuspended(rcp) || !CIsJoined(rcp)) && getnumerichandlefromchanhash(cp->users, chanservnick->numeric)) {
    localpartchannel(chanservnick, cp, NULL);
  }
  
  /* Right, we are definately going to either join the channel or at least
   * burst some modes onto it.
   *
   * We will try and burst our view of the world; if the timestamps are
   * actually equal this will be mostly ignored and we will have to fix it
   * up later.  For modes we use the forced modes, plus the default channel
   * modes (unless any of those are explicitly denied) */
    
  /* By default, we set the forcemodes and the default modes, but never denymodes..
   * OK, actually we should only set the default modes if our timestamp is older.*/
  if (cp->timestamp > rcp->ltimestamp)
    themodes |= CHANMODE_DEFAULT;
  else
    themodes=0;
    
  themodes = (themodes | rcp->forcemodes) & ~rcp->denymodes;
  
  /* Now, if someone has just created a channel and we are going to set +i
   * or +k on it, this will kick them off.  This could be construed as a
   * bit rude if it's their channel, so if there is only one person on the
   * channel and they have a right to be there, burst with default modes
   * only to avoid them being netrider kicked.
   */
  if (cp->users->totalusers==1) {
    for (i=0;i<cp->users->hashsize;i++) {
      if (cp->users->content[i] != nouser) {
        if ((np=getnickbynumeric(cp->users->content[i]&CU_NUMERICMASK)) &&
            (rup=getreguserfromnick(np)) &&
            (rcup=findreguseronchannel(rcp,rup)) &&
            CUKnown(rcup)) {
          /* OK, there was one user, and they are known on this channel. 
           * Don't burst with +i or +k */
          themodes &= ~(CHANMODE_INVITEONLY | CHANMODE_KEY);
        }
      }
    }
  }

  /* We should be on the channel - join with our nick */
  if (!CIsSuspended(rcp) && CIsJoined(rcp) && !getnumerichandlefromchanhash(cp->users, chanservnick->numeric)) {
    /* If we pass the key parameter here but are not setting +k (see above)
     * then localburstontochannel() will ignore the key */
    localburstontochannel(cp, chanservnick, rcp->ltimestamp, themodes, 
                         rcp->limit, (rcp->key)?rcp->key->content:NULL);
  }

  /* We're not joining the channel - send the burst with no nick */  
  if (!CIsSuspended(rcp) && !CIsJoined(rcp) && (cp->timestamp > rcp->ltimestamp)) {
    localburstontochannel(cp, NULL, rcp->ltimestamp, themodes,
                          rcp->limit, (rcp->key)?rcp->key->content:NULL);
  }
}

void chanservstdmessage(nick *np, int messageid, ... ) {
  char buf[5010];
  char buf2[512];
  int notice;
  reguser *rup;
  int language;
  va_list va, va2;
  char *message, *messageargs;
  char *bp2,*bp;
  int len;

  if (getreguserfromnick(np) == NULL) { 
    notice=1;
    language=0;
  } else {
    rup=getreguserfromnick(np);
    if(UIsNotice(rup)) {
      notice=1;
    } else {
      notice=0;
    }
    language=rup->languageid;
  }

  if (csmessages[language][messageid]) {
    message=csmessages[language][messageid]->content;
  } else if (csmessages[0][messageid]) {
    message=csmessages[0][messageid]->content;
  } else {
    message=defaultmessages[messageid*2];
  }

  messageargs=defaultmessages[messageid*2+1];

  va_start(va,messageid);
  va_copy(va2, va);
  q9vsnprintf(buf,5000,message,messageargs,va);
  va_end(va);

  len=0;
  bp2=buf2;
  for (bp=buf; ;bp++) {
    /* We send something if we hit a \n, fill the buffer or run out of source */
    if (*bp=='\n' || len>490 || !*bp) {
      if (*bp && *bp!='\n') {
	bp--;
      }
      
      *bp2='\0';

      if (chanservnick && *buf2) {
	if (notice) {
	  sendnoticetouser(chanservnick,np,"%s",buf2);
	} else {
	  sendmessagetouser(chanservnick,np,"%s",buf2);
	}
      }

      /* If we ran out of buffer, get out here */
      if (!*bp)
	break;
      
      bp2=buf2;
      len=0;
    } else {
      len++;
      *bp2++=*bp;
    }
  }
  
  /* Special case: If it's a "not enough parameters" message, show the first line of help */
  if (messageid==QM_NOTENOUGHPARAMS) {
    char *command=va_arg(va2, char *);
    cs_sendhelp(np, command, 1);
    chanservstdmessage(np, QM_TYPEHELPFORHELP, command); 
  }
  
  va_end(va2);
}

void chanservsendmessage_real(nick *np, int oneline, char *message, ... ) {
  char buf[5010]; /* Very large buffer.. */
  char buf2[512], *bp, *bp2;
  int notice;
  int len;
  reguser *rup;

  va_list va;

  va_start(va,message);
  vsnprintf(buf,5000,message,va);
  va_end(va);
  
  if (getreguserfromnick(np) == NULL) { 
    notice=1;
  } else {
    rup=getreguserfromnick(np);
    if(UIsNotice(rup)) {
      notice=1;
    } else {
      notice=0;
    }
  }

  len=0;
  bp2=buf2;
  for (bp=buf; ;bp++) {
    /* We send something if we hit a \n, fill the buffer or run out of source */
    if (*bp=='\n' || len>490 || !*bp) {
      if (*bp && *bp!='\n') {
	bp--;
      }
      
      *bp2='\0';

      if (chanservnick && *buf2) {
	if (notice) {
	  sendnoticetouser(chanservnick,np,"%s",buf2);
	} else {
	  sendmessagetouser(chanservnick,np,"%s",buf2);
	}
      }

      /* If we ran out of buffer, get out here */
      if (!*bp || (*bp=='\n' && oneline))
	break;
      
      bp2=buf2;
      len=0;
    } else {
      len++;
      *bp2++=*bp;
    }
  }
}

void chanservwallmessage(char *message, ... ) {
  char buf[5010]; /* Very large buffer.. */
  va_list va;
  nick *np;
  unsigned int i=0;

  va_start(va,message);
  vsnprintf(buf,5000,message,va);
  va_end(va);
  
  /* Scan for users */
  for (i=0;i<NICKHASHSIZE;i++) {
    for (np=nicktable[i];np;np=np->next) {
      if (!IsOper(np)) /* optimisation, if VIEWWALLMESSAGE changes change this */
        continue;

      if (!cs_privcheck(QPRIV_VIEWWALLMESSAGE, np))
        continue;

      chanservsendmessage(np, "%s", buf);
    }
  }
}

void chanservkillstdmessage(nick *target, int messageid, ... ) {
  char buf[512];
  int language;
  char *message, *messageargs;
  va_list va;
  reguser *rup;
  
  if (!(rup=getreguserfromnick(target)))
    language=0;
  else
    language=rup->languageid;
  
  if (csmessages[language][messageid])
    message=csmessages[language][messageid]->content;
  else if (csmessages[0][messageid])
    message=csmessages[0][messageid]->content;
  else
    message=defaultmessages[messageid*2];

  messageargs=defaultmessages[messageid*2+1];
  va_start(va, messageid);
  q9vsnprintf(buf, 511, message, messageargs, va);
  va_end(va);
  killuser(chanservnick, target, "%s", buf);
}

int checkpassword(reguser *rup, const char *pass) {
  if (!(*rup->password))
    return 0;

  if (!strncmp(rup->password, pass, PASSLEN))
    return 1;
  return 0;
}

int checkresponse(reguser *rup, const unsigned char *entropy, const char *response, CRAlgorithm algorithm) {
  char usernamel[NICKLEN+1], *dp, *up;

  if (!(*rup->password))
    return 0;

  for(up=rup->username,dp=usernamel;*up;)
    *dp++ = ToLower(*up++);
  *dp = '\0';

  return algorithm(usernamel, rup->password, cs_calcchallenge(entropy), response);
}

int checkhashpass(reguser *rup, const char *junk, const char *hash) {
  char usernamel[NICKLEN+1], *dp, *up;

  for(up=rup->username,dp=usernamel;*up;)
    *dp++ = ToLower(*up++);
  *dp = '\0';

  return cs_checkhashpass(usernamel, rup->password, junk, hash);
}

int setpassword(reguser *rup, const char *pass) {
  strncpy(rup->password, pass, PASSLEN);
  rup->password[PASSLEN]='\0';
  return 1;
}

void cs_checknick(nick *np) {
  activeuser* aup;
  reguser *rup;
  char userhost[USERLEN+HOSTLEN+3];
  
  if (!(aup=getactiveuserfromnick(np))) {
    aup=getactiveuser();
    np->exts[chanservnext]=aup;
  }
  
  assert(getactiveuserfromnick(np));

  if (IsAccount(np) && np->auth) {
    if (np->auth->exts[chanservaext]) {
      rup=getreguserfromnick(np);

      /* safe? */
      if(rup) {
        if (UIsDelayedGline(rup)) {
          /* delayed-gline - schedule the user's squelching */
          deleteschedule(NULL, &chanservdgline, (void*)rup); /* icky, but necessary unless we stick more stuff in reguser structure */
          scheduleoneshot(time(NULL)+rand()%900, &chanservdgline, (void*)rup);
        } else if (UIsGline(rup)) {
          /* instant-gline - lets be lazy and set a schedule expiring now :) */
          deleteschedule(NULL, &chanservdgline, (void*)rup); /* icky, but necessary unless we stick more stuff in reguser structure */
          scheduleoneshot(time(NULL), &chanservdgline, (void*)rup);
        } else if(UHasSuspension(rup)) {
          chanservkillstdmessage(np, QM_SUSPENDKILL);
          return;
        }
      }
      cs_doallautomodes(np);
    } else {
      /* Auto create user.. */
      rup=getreguser();
      rup->status=0;
      rup->ID=np->auth->userid;
      if (rup->ID > lastuserID)
        lastuserID=rup->ID;
      strncpy(rup->username,np->authname,NICKLEN); rup->username[NICKLEN]='\0';
      rup->created=time(NULL);
      rup->lastauth=0;
      rup->lastemailchange=0;
      rup->lastpasschange=0;
      rup->flags=QUFLAG_NOTICE;
      rup->languageid=0;
      rup->suspendby=0;
      rup->suspendexp=0;
      rup->suspendtime=0;
      rup->lockuntil=0;
      rup->password[0]='\0';
      rup->email=NULL;
      rup->lastemail=NULL;
      rup->localpart=NULL;
      rup->domain=NULL;
      rup->info=NULL;
      sprintf(userhost,"%s@%s",np->ident,np->host->name->content);
      rup->lastuserhost=getsstring(userhost,USERLEN+HOSTLEN+1);
      rup->suspendreason=NULL;
      rup->comment=NULL;
      rup->knownon=NULL;
      rup->checkshd=NULL;
      rup->stealcount=0;
      rup->fakeuser=NULL;
      addregusertohash(rup);

      csdb_createuser(rup);
    }
  }

  cs_checknickbans(np);
}                                                 

void cs_checkchanmodes(channel *cp) {
  modechanges changes;
  
  localsetmodeinit (&changes, cp, chanservnick);
  cs_docheckchanmodes(cp, &changes);
  localsetmodeflush (&changes, 1);
}

void cs_docheckchanmodes(channel *cp, modechanges *changes) {
  regchan *rcp;

  if (!cp)
    return;

  if ((rcp=cp->index->exts[chanservext])==NULL || CIsSuspended(rcp))
    return;

  /* Take care of the simple modes */
  localdosetmode_simple(changes, rcp->forcemodes & ~(cp->flags), rcp->denymodes & cp->flags);

  /* Check for forced key.  Note that we ALWAYS call localdosetmode_key 
   * in case the wrong key is set atm */
  if (rcp->forcemodes & CHANMODE_KEY) {
    if (!rcp->key) {
      Error("chanserv",ERR_WARNING,"Key forced but no key specified for %s: cleared forcemode.",rcp->index->name->content);
      rcp->forcemodes &= ~CHANMODE_KEY;
      csdb_updatechannel(rcp);
    } else {
      localdosetmode_key(changes, rcp->key->content, MCB_ADD);
    }
  }
  
  /* Check for denied key */
  if ((rcp->denymodes & CHANMODE_KEY) && IsKey(cp)) {
    localdosetmode_key(changes, NULL, MCB_DEL);
  }

  /* Check for forced limit.  Always call in case of wrong limit */
  if (rcp->forcemodes & CHANMODE_LIMIT) {
    localdosetmode_limit(changes, rcp->limit, MCB_ADD);
  }
}

/* Slightly misnamed function that checks each USER on the channel against channel
 * settings.  Possible actions are opping/deopping, voicing/devoicing, and banning
 * for chanflag +k or chanlev flag +b */
void cs_docheckopvoice(channel *cp, modechanges *changes) {
  regchan *rcp;
  reguser *rup;
  regchanuser *rcup;
  nick *np;
  int i;

  if (!cp)
    return;

  if ((rcp=cp->index->exts[chanservext])==NULL || CIsSuspended(rcp))
    return;
  
  for (i=0;i<cp->users->hashsize;i++) {
    if (cp->users->content[i]==nouser)
      continue;
      
    if ((np=getnickbynumeric(cp->users->content[i]))==NULL) {
      Error("chanserv",ERR_ERROR,"Found non-existent numeric %lu on channel %s",cp->users->content[i],
            cp->index->name->content);
      continue;
    }
    
    if ((rup=getreguserfromnick(np)))
      rcup=findreguseronchannel(rcp,rup);
    else
      rcup=NULL;
    
    /* Various things that might ban the user - don't apply these to +o, +k or +X users */
    if (!IsService(np) && !IsXOper(np) && !IsOper(np)) {
      if (rcup && CUIsBanned(rcup)) {
        cs_banuser(changes, cp->index, np, NULL);
        continue;
      }
      
      /* chanflag +k checks; kick them if they "obviously" can't rejoin without a ban */
      if (CIsKnownOnly(rcp) && !(rcup && CUKnown(rcup))) {
        if (IsInviteOnly(cp) || (IsRegOnly(cp) && !IsAccount(np))) {
          localkickuser(chanservnick,cp,np,"Authorised users only.");
        } else {
          cs_banuser(NULL, cp->index, np, "Authorised users only.");
        }
        continue;
      }
    }
                                      
    if ((cp->users->content[i] & CUMODE_OP) && !IsService(np)) {
      if ((CIsBitch(rcp) && (!rcup || !CUHasOpPriv(rcup))) ||
           (rcup && CUIsDeny(rcup)))
        localdosetmode_nick(changes, np, MC_DEOP);
    } else {
      if (rcup && (CUIsProtect(rcup) || CIsProtect(rcp)) && CUIsOp(rcup) && !CUIsDeny(rcup)) {
        localdosetmode_nick(changes, np, MC_OP);    
        cs_logchanop(rcp, np->nick, rcup->user);
      }
    }
    
    if (cp->users->content[i] & CUMODE_VOICE) {
      if (rcup && CUIsQuiet(rcup))
        localdosetmode_nick(changes, np, MC_DEVOICE);
    } else {
      if (rcup && (CUIsProtect(rcup) || CIsProtect(rcp)) && CUIsVoice(rcup) && !CUIsQuiet(rcup) && !(cp->users->content[i] & CUMODE_OP))
        localdosetmode_nick(changes, np, MC_VOICE);
    }
  }  
}

void cs_doallautomodes(nick *np) {
  reguser *rup;
  regchanuser *rcup;
  unsigned long *lp;
  modechanges changes;

  if (!(rup=getreguserfromnick(np)))
    return;
  
  for (rcup=rup->knownon;rcup;rcup=rcup->nextbyuser) {
    /* Skip suspended channels */
    if (CIsSuspended(rcup->chan))
      continue;
      
    if (rcup->chan->index->channel) {
      /* Channel exists */
      if ((lp=getnumerichandlefromchanhash(rcup->chan->index->channel->users, np->numeric))) {
        /* User is on channel.. */

        if (CUKnown(rcup) && rcup->chan->index->channel->users->totalusers >= 3) {
          /* This meets the channel use criteria, update. */
          rcup->chan->lastactive=time(NULL);
          
          /* Don't spam the DB though for channels with lots of joins */
          if (rcup->chan->lastcountersync < (time(NULL) - COUNTERSYNCINTERVAL)) {
            csdb_updatechannelcounters(rcup->chan);
            rcup->chan->lastcountersync=time(NULL);
          }
        }

        /* Update last use time */
        rcup->usetime=getnettime();

	localsetmodeinit(&changes, rcup->chan->index->channel, chanservnick);
	if (*lp & CUMODE_OP) {
	  if (!IsService(np) && (CUIsDeny(rcup) || (CIsBitch(rcup->chan) && !CUHasOpPriv(rcup))))
	    localdosetmode_nick(&changes, np, MC_DEOP);
	} else {
	  if (!CUIsDeny(rcup) && CUIsOp(rcup) && 
	      (CUIsProtect(rcup) || CIsProtect(rcup->chan) || CUIsAutoOp(rcup))) {
	    localdosetmode_nick(&changes, np, MC_OP);
	    cs_logchanop(rcup->chan, np->nick, rup);
          }
	}
	
	if (*lp & CUMODE_VOICE) {
	  if (CUIsQuiet(rcup))
	    localdosetmode_nick(&changes, np, MC_DEVOICE);
	} else {
	  if (!CUIsQuiet(rcup) && CUIsVoice(rcup) && !(*lp & CUMODE_OP) && 
	      (CUIsProtect(rcup) || CIsProtect(rcup->chan) || CUIsAutoVoice(rcup)))
	    localdosetmode_nick(&changes, np, MC_VOICE);
	}
	localsetmodeflush(&changes, 1);
      } else {
	/* Channel exists but user is not joined: invite if they are +j-b */
	if (CUIsAutoInvite(rcup) && CUKnown(rcup) && !CUIsBanned(rcup)) {
	  localinvite(chanservnick, rcup->chan->index->channel, np);
	}
      }
    }
  }
}

void cs_checknickbans(nick *np) {
  channel **ca;
  regchan *rcp;
  int i,j;

  if (IsService(np) || IsOper(np) || IsXOper(np))
    return;

  /* Avoid races: memcpy the channel array */
  i=np->channels->cursi;  
  ca=(channel **)malloc(i*sizeof(channel *));
  memcpy(ca, np->channels->content,i*sizeof(channel *));

  for (j=0;j<i;j++) {
    if ((rcp=ca[j]->index->exts[chanservext]) && !CIsSuspended(rcp) && 
	CIsEnforce(rcp) && nickbanned_visible(np, ca[j]))
      localkickuser(chanservnick, ca[j], np, "Banned.");
  }

  free(ca);
}
       
void cs_checkbans(channel *cp) {
  regchan *rcp;
  int i;
  nick *np;
  chanban *cbp;
  regban *rbp;
  time_t now;
  modechanges changes;

  if (!cp)
    return;

  if ((rcp=cp->index->exts[chanservext])==NULL || CIsSuspended(rcp))
    return;

  now=time(NULL);

  localsetmodeinit(&changes, cp, chanservnick);

  for (i=0;i<cp->users->hashsize;i++) {
    if (cp->users->content[i]==nouser)
      continue;
    
    if ((np=getnickbynumeric(cp->users->content[i]))==NULL) {
      Error("chanserv",ERR_ERROR,"Found numeric %lu on channel %s who doesn't exist.",
	    cp->users->content[i], cp->index->name->content);
      continue;
    }
    
    if (IsService(np) || IsOper(np) || IsXOper(np))
      continue;

    for (rbp=rcp->bans;rbp;rbp=rbp->next) {
      if (((!rbp->expiry) || (rbp->expiry <= now)) &&
	  nickmatchban_visible(np, rbp->cbp)) {
	if (!nickbanned_visible(np, cp)) {
	  localdosetmode_ban(&changes, bantostring(rbp->cbp), MCB_ADD);
	}
	localkickuser(chanservnick,cp,np,rbp->reason?rbp->reason->content:"Banned.");
	break;
      }
    }

    if (rbp) /* If we broke out of above loop (due to kick) rbp will be set.. */
      continue; 

    if (CIsEnforce(rcp)) {
      for (cbp=cp->bans;cbp;cbp=cbp->next) {
	if ((cbp->timeset>=rcp->lastbancheck) && nickmatchban_visible(np, cbp))
	  localkickuser(chanservnick,cp,np,"Banned.");
      }
      rcp->lastbancheck=time(NULL);
    }    
  }

  localsetmodeflush(&changes,1);
}

/*
 * cs_schedupdate:
 *  This function schedules an update check on a channel
 */
void cs_schedupdate(chanindex *cip, int mintime, int maxtime) {
  int delay=mintime+(rand()%(maxtime-mintime));
  regchan *rcp;

  if (!(rcp=cip->exts[chanservext]) || CIsSuspended(rcp))
    return;

  if (rcp->checksched) {
    deleteschedule(rcp->checksched, cs_timerfunc, cip);
  }

  rcp->checksched=scheduleoneshot(time(NULL)+delay, cs_timerfunc, cip);
}

/*
 * cs_timerfunc:
 *  This function does several things: 
 *   * Updates auto-limit and/or removes bans as necessary
 *   * Schedules the next timed call
 *   * Does any pending op/ban/mode checks
 * It is safe to use either as a target for a schedule() call, or to call
 * directly when parameters change (e.g. autolimit settings or ban expire
 * timers).
 * To force a limit update, set rcp->limit to 0..
 */

void cs_timerfunc(void *arg) {
  chanindex *cip=arg;
  channel *cp=cip->channel;
  regchan *rcp;
  chanban *cbp, *ncbp;
  regban **rbh, *rbp;
  time_t nextsched=0;
  time_t now=time(NULL);
  modechanges changes;
  
  if (!(rcp=cip->exts[chanservext]))
    return;

  verifyregchan(rcp);

  /* Always nullify the checksched even if the channel is suspended.. */
  if (rcp->checksched) {
    deleteschedule(rcp->checksched, cs_timerfunc, arg);
    rcp->checksched=NULL;
  }
  
  if (!cp || CIsSuspended(rcp))
    return;
  
  /* Check if we need to leave the channel first */
  if (chanservnick && CIsJoined(rcp) && cip->channel && 
      cip->channel->users->totalusers==1 &&
      getnumerichandlefromchanhash(cip->channel->users, chanservnick->numeric)) {

    /* Only chanserv left in this channel */
    if (now >= (rcp->lastpart + LINGERTIME)) {
      /* Time to go */
      localpartchannel(chanservnick, cip->channel, "Empty Channel");
      return;
    } else {
      if (!nextsched || nextsched > (rcp->lastpart + LINGERTIME))
	nextsched=rcp->lastpart+LINGERTIME;
    }
  }

  localsetmodeinit(&changes, cp, chanservnick);
  
  if (CIsAutoLimit(rcp)) {
    if (!rcp->limit || rcp->autoupdate <= now) {
      /* Only update the limit if there are <= (autolimit/2) or 
       * >= (autolimit * 1.5) slots free */
      
      if ((cp->users->totalusers >= (rcp->limit - rcp->autolimit/2)) ||
          (cp->users->totalusers <= (rcp->limit - (3 * rcp->autolimit)/2))) {
        rcp->limit=cp->users->totalusers+rcp->autolimit;
        rcp->status |= QCSTAT_MODECHECK;
      }
      
      /* And set the schedule for the next update */
      rcp->autoupdate = now + AUTOLIMIT_INTERVAL;
    } 
    
    if (!nextsched || rcp->autoupdate <= nextsched)
      nextsched=rcp->autoupdate;
  }
  
  if (rcp->status & QCSTAT_OPCHECK) {
    rcp->status &= ~QCSTAT_OPCHECK;
    cs_docheckopvoice(cp, &changes);
  }
  
  if (rcp->status & QCSTAT_MODECHECK) {
    rcp->status &= ~QCSTAT_MODECHECK;
    cs_docheckchanmodes(cp, &changes);
  }
  
  if (rcp->status & QCSTAT_BANCHECK) {
    rcp->status &= ~QCSTAT_BANCHECK;
    cs_checkbans(cp);
  }

  /* This ban check is AFTER docheckopvoice in case checkopvoice set a 
   * ban which we need to automatically remove later.. */
   
  if (rcp->banduration) {
    for (cbp=cp->bans;cbp;cbp=ncbp) {
      ncbp=cbp->next;
      if (cbp->timeset+rcp->banduration<=now) {
	localdosetmode_ban(&changes, bantostring(cbp), MCB_DEL);
      } else {
	if (!nextsched || cbp->timeset+rcp->banduration <= nextsched) {
	  nextsched=rcp->banduration+cbp->timeset;
	}
      }
    }
  }

  /* Check for expiry of registered bans */
  if (rcp->bans) {
    for (rbh=&(rcp->bans);*rbh;) {
      rbp=*rbh;
      verifyregchanban(rbp);
      if (rbp->expiry && (rbp->expiry < now)) {
	/* Expired ban */
	/* Remove ban if it's on the channel (localdosetmode_ban will silently 
	 * skip bans that don't exist) */
	localdosetmode_ban(&changes, bantostring(rbp->cbp), MCB_DEL);
	/* Remove from database */
	csdb_deleteban(rbp);
	/* Remove from list */
	(*rbh)=rbp->next;
	/* Free ban/string and update setby refcount, and free actual regban */
	freesstring(rbp->reason);
	freechanban(rbp->cbp);
	freeregban(rbp);
      } else {
	if (rbp->expiry && (!nextsched || rbp->expiry <= nextsched)) {
	  nextsched=rbp->expiry;
	}
	rbh=&(rbp->next);
      }
    }
  }

  if (nextsched) 
    rcp->checksched=scheduleoneshot(nextsched, cs_timerfunc, arg);
  
  localsetmodeflush(&changes, 1);
}

void cs_removechannel(regchan *rcp, char *reason) {
  int i;
  chanindex *cip;
  regchanuser *rcup, *nrcup;
  regban *rbp, *nrbp;

  cip=rcp->index;
  
  for (i=0;i<REGCHANUSERHASHSIZE;i++) {
    for (rcup=rcp->regusers[i];rcup;rcup=nrcup) {
      nrcup=rcup->nextbychan;
      delreguserfromchannel(rcp, rcup->user);
      freeregchanuser(rcup);
    }
  }
  
  for (rbp=rcp->bans;rbp;rbp=nrbp) {
    nrbp=rbp->next;
    freesstring(rbp->reason);
    freechanban(rbp->cbp);
    freeregban(rbp);
  }

  rcp->bans=NULL;

  if (rcp->checksched)
    deleteschedule(rcp->checksched, cs_timerfunc, rcp->index);
    
  if (cip->channel) {
    chanservpartchan(cip->channel, reason);
  }
  
  csdb_deletechannel(rcp);
    
  freesstring(rcp->welcome);
  freesstring(rcp->topic);
  freesstring(rcp->key);
  freesstring(rcp->suspendreason);
  freesstring(rcp->comment);

  freeregchan(rcp);
    
  cip->exts[chanservext]=NULL;
  releasechanindex(cip);
}

/* Sender is who the DELCHAN is attributed to.. */
int cs_removechannelifempty(nick *sender, regchan *rcp) {
  unsigned int i;
  regchanuser *rcup;
  
  for (i=0;i<REGCHANUSERHASHSIZE;i++) {
    for (rcup=rcp->regusers[i];rcup;rcup=rcup->nextbychan) {
      if (rcup->flags & ~(QCUFLAGS_PUNISH))
        return 0;
    }
  }
  
  cs_log(sender,"DELCHAN %s (Empty)",rcp->index->name->content);
  cs_removechannel(rcp, "Last user removed - channel deleted.");
  
  return 1;
}

void cs_removeuser(reguser *rup) {
  regchanuser *rcup, *nrcup;
  regchan *rcp;
  struct authname *anp;
  
  /* Remove the user from all its channels */
  for (rcup=rup->knownon;rcup;rcup=nrcup) {
    nrcup=rcup->nextbyuser;
    rcp=rcup->chan;

    delreguserfromchannel(rcp, rup);
    cs_removechannelifempty(NULL, rcp);
  }

  if(rup->domain)
    delreguserfrommaildomain(rup,rup->domain);
  freesstring(rup->localpart);
  freesstring(rup->email);
  freesstring(rup->lastemail);
  freesstring(rup->lastuserhost);
  freesstring(rup->suspendreason);
  freesstring(rup->comment);
  freesstring(rup->info);

  csdb_deleteuser(rup);

  removereguserfromhash(rup);
  
  if ((anp=findauthname(rup->ID)) && anp->nicks) {
    rup->status |= QUSTAT_DEAD;
  } else {
    freereguser(rup);
  }
}

int cs_bancheck(nick *np, channel *cp) {
  time_t now=time(NULL);
  regban **rbh, *rbp;
  modechanges changes;
  regchan *rcp;

  if (!(rcp=cp->index->exts[chanservext]))
    return 0;

  for (rbh=&(rcp->bans);*rbh;) {
    if ((*rbh)->expiry && ((*rbh)->expiry < now)) {
      /* Expired ban */
      csdb_deleteban(*rbh);
      rbp=*rbh;
      (*rbh)=rbp->next;

      freesstring(rbp->reason);
      freechanban(rbp->cbp);
      freeregban(rbp);
    } else if (nickmatchban_visible(np,(*rbh)->cbp)) {
      /* This user matches this ban.. */
      if (!nickbanned_visible(np,cp)) {
	/* Only bother putting the ban on the channel if they're not banned already */
	/* (might be covered by this ban or a different one.. doesn't really matter */
	localsetmodeinit(&changes, cp, chanservnick);
	localdosetmode_ban(&changes, bantostring((*rbh)->cbp), MCB_ADD);
	localsetmodeflush(&changes, 1);
	cs_timerfunc(cp->index);
      }
      localkickuser(chanservnick, cp, np, (*rbh)->reason ? (*rbh)->reason->content : "Banned.");
      return 1;
    } else {
      rbh=&((*rbh)->next);
    }
  }

  return 0;
}

void cs_setregban(chanindex *cip, regban *rbp) {
  modechanges changes;
  int i;
  nick *np;

  if (!cip->channel)
    return;
    
  localsetmodeinit(&changes, cip->channel, chanservnick);
  localdosetmode_ban(&changes, bantostring(rbp->cbp), MCB_ADD);
  localsetmodeflush(&changes, 1);

  for (i=0;(cip->channel) && i<cip->channel->users->hashsize;i++) {
    if (cip->channel->users->content[i]!=nouser &&
	(np=getnickbynumeric(cip->channel->users->content[i])) &&
	!IsService(np) && !IsOper(np) && !IsXOper(np) &&
	nickmatchban_visible(np, rbp->cbp))
      localkickuser(chanservnick, cip->channel, np, rbp->reason ? rbp->reason->content : "Banned.");
  }

  cs_timerfunc(cip);
}

void cs_banuser(modechanges *changes, chanindex *cip, nick *np, const char *reason) {
  modechanges lchanges;
  char banmask[NICKLEN+USERLEN+HOSTLEN+3];
  
  if (!cip->channel)
    return;

  if (nickbanned_visible(np, cip->channel)) {
    localkickuser(chanservnick, cip->channel, np, reason?reason:"Banned.");
    return;
  }
  
  if (IsAccount(np)) {
    /* Ban their account.. */
    sprintf(banmask,"*!*@%s.%s",np->authname,HIS_HIDDENHOST);
  } else if (IsSetHost(np)) {
    /* sethosted: ban ident@host */
    sprintf(banmask,"*!%s@%s",np->shident->content,np->sethost->content);
  } else if (np->host->clonecount>3) {
      /* >3 clones, ban user@host */
    sprintf(banmask,"*!%s@%s",np->ident,np->host->name->content);
  } else {
    sprintf(banmask,"*!*@%s",np->host->name->content);
  }

  if (!changes) {
    localsetmodeinit(&lchanges, cip->channel, chanservnick);
    localdosetmode_ban(&lchanges, banmask, MCB_ADD);
    localsetmodeflush(&lchanges, 1);
  } else {
    localdosetmode_ban(changes, banmask, MCB_ADD);
  }
  
  localkickuser(chanservnick, cip->channel, np, reason?reason:"Banned.");
}

/*
 * cs_sanitisechanlev: Removes impossible combinations from chanlev flags.
 * chanservuser.c is probably not the right file for this, but nowhere better
 * presented itself...
 */
flag_t cs_sanitisechanlev(flag_t flags) {
  /* +m or +n cannot have any "punishment" flags */
  if (flags & (QCUFLAG_MASTER | QCUFLAG_OWNER))
    flags &= ~(QCUFLAGS_PUNISH);
  
  /* +d can't be +o */
  if (flags & QCUFLAG_DENY)
    flags &= ~QCUFLAG_OP;
  
  /* +q can't be +v */
  if (flags & QCUFLAG_QUIET)
    flags &= ~QCUFLAG_VOICE;
  
  /* +p trumps +a and +g */
  if (flags & QCUFLAG_PROTECT)
    flags &= ~(QCUFLAG_AUTOOP | QCUFLAG_AUTOVOICE);
    
  /* -o can't be +a */
  if (!(flags & QCUFLAG_OP)) 
    flags &= ~QCUFLAG_AUTOOP;
  
  /* +a or -v can't be +g.  +a implies +o at this stage (see above) */
  if (!(flags & QCUFLAG_VOICE) || (flags & QCUFLAG_AUTOOP))
    flags &= ~QCUFLAG_AUTOVOICE;
  
  /* +p requires +o or +v */
  if (!(flags & (QCUFLAG_VOICE | QCUFLAG_OP)))
    flags &= ~QCUFLAG_PROTECT;
  
  /* The personal flags require one of +mnovk, as does +t */
  if (!(flags & (QCUFLAG_OWNER | QCUFLAG_MASTER | QCUFLAG_OP | QCUFLAG_VOICE | QCUFLAG_KNOWN)))
    flags &= ~(QCUFLAGS_PERSONAL | QCUFLAG_TOPIC);
  
  return flags;
}
	      
/*
 * findreguser:
 *  This function does the standard "nick or #user" lookup.
 *  If "sender" is not NULL, a suitable error message will
 *  be sent if the lookup fails.
 *  "sender" MUST be sent when a user is requesting a lookup
 *  as there is some policy code here.
 */

reguser *findreguser(nick *sender, const char *str) {
  reguser *rup, *vrup = getreguserfromnick(sender);;
  nick *np;

  if (!str || !*str)
    return NULL;

  if (*str=='#') {
    if (str[1]=='\0') {
      if (sender)
      	chanservstdmessage(sender, QM_UNKNOWNUSER, str);
      return NULL;
    }
    if (!(rup=findreguserbynick(str+1)) && sender)
      chanservstdmessage(sender, QM_UNKNOWNUSER, str);
  } else if (*str=='&' && vrup && UHasStaffPriv(vrup)) {
    if (str[1]=='\0') {
      if (sender)
      	chanservstdmessage(sender, QM_UNKNOWNUSER, str);
      return NULL;
    }
    if (!(rup=findreguserbyID(atoi(str+1))) && sender)
      chanservstdmessage(sender, QM_UNKNOWNUSER, str);
  } else {
    if (!(np=getnickbynick(str))) {
      if (sender)
      	chanservstdmessage(sender, QM_UNKNOWNUSER, str);
      return NULL;
    }
    if (!(rup=getreguserfromnick(np)) && sender)
      chanservstdmessage(sender, QM_USERNOTAUTHED, str);
  }

  /* removed the suspended check from here, I don't see the point... */
  if (rup && (rup->status & QUSTAT_DEAD)) {
    chanservstdmessage(sender, QM_USERHASBADAUTH, rup->username);
    return NULL;
  }

  return rup;
}

/*
 * Unbans a mask from a channel, including permbans if user has correct privs.
 *
 * Return 0 if it works, 1 if it don't.
 */
int cs_unbanfn(nick *sender, chanindex *cip, UnbanFN fn, void *arg, int removepermbans, int abortonfailure) {
  regban **rbh, *rbp;
  chanban **cbh, *cbp;
  regchan *rcp;
  modechanges changes;
  char *banstr;

  rcp=cip->exts[chanservext];

  if (cip->channel)
    localsetmodeinit(&changes, cip->channel, chanservnick);

  for (rbh=&(rcp->bans); *rbh; ) {
    rbp=*rbh;
    if (fn(arg, rbp->cbp)) {
      banstr=bantostring(rbp->cbp);
      /* Check perms and remove */
      if(!removepermbans) {
        chanservstdmessage(sender, QM_WARNNOTREMOVEDPERMBAN, banstr, cip->name->content);
        rbh=&(rbp->next);
      } else if (!cs_checkaccess(sender, NULL, CA_MASTERPRIV, cip, NULL, 0, 1)) {
        chanservstdmessage(sender, QM_NOTREMOVEDPERMBAN, banstr, cip->name->content);
        if (abortonfailure) return 1; /* Just give up... */
        rbh=&(rbp->next);
      } else {
        chanservstdmessage(sender, QM_REMOVEDPERMBAN, banstr, cip->name->content);
        if (cip->channel)
          localdosetmode_ban(&changes, banstr, MCB_DEL);
        /* Remove from database */
        csdb_deleteban(rbp);
        /* Remove from list */
        (*rbh)=rbp->next;
        /* Free ban/string and update setby refcount, and free actual regban */
        freesstring(rbp->reason);
        freechanban(rbp->cbp);
        freeregban(rbp);
      }
    } else {
      rbh=&(rbp->next);
    }
  }

  if (cip->channel) {
    for (cbh=&(cip->channel->bans); *cbh; ) {
      cbp=*cbh;
      if (fn(arg, cbp)) {
        /* Remove */
        banstr=bantostring(cbp);
        chanservstdmessage(sender, QM_REMOVEDCHANBAN, banstr, cip->name->content);
        localdosetmode_ban(&changes, banstr, MCB_DEL);
      } else {
        cbh=&(cbp->next);
      }
    }
    localsetmodeflush(&changes,1);
  }
  
  return 0;
}

/* Add entry to channel op history when someone gets ops. */
void cs_logchanop(regchan *rcp, char *nick, reguser *rup) {
  strncpy(rcp->chanopnicks[rcp->chanoppos], nick, NICKLEN);
  rcp->chanopnicks[rcp->chanoppos][NICKLEN]='\0';
  rcp->chanopaccts[rcp->chanoppos]=rup->ID;
  rcp->chanoppos=(rcp->chanoppos+1)%CHANOPHISTORY;
}

int checkreason(nick *np, char *reason) {
  if((strlen(reason) < MIN_REASONLEN) || !strchr(reason, ' ')) {
    chanservstdmessage(np, QM_REASONREQUIRED);
    return 0;
  }

  return 1;
}
