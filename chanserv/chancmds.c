/*
 * chancmds.c:
 *  Provides basic channel commands:
 *   CHANLEV
 *   WHOIS
 *   CHANFLAGS
 * etc.
 */
 
#include "chanserv.h"
#include "../nick/nick.h"
#include "../lib/flags.h"
#include "../lib/irc_string.h"
#include "../channel/channel.h"
#include "../parser/parser.h"
#include "../irc/irc.h"
#include "../localuser/localuserchannel.h"

#include <string.h>
#include <stdio.h>

int csc_dochanlev(void *source, int cargc, char **cargv);
int csc_dochanflags(void *source, int cargc, char **cargv);
int csc_dochanmode(void *source, int cargc, char **cargv);
int csc_doautolimit(void *source, int cargc, char **cargv);
int csc_dobantimer(void *source, int cargc, char **cargv);
int csc_doaddchan(void *source, int cargc, char **cargv);
int csc_dosuspendchan(void *source, int cargc, char **cargv);
int csc_dosuspendchanlist(void *source, int cargc, char **cargv);
int csc_dounsuspendchan(void *source, int cargc, char **cargv);
int csc_dodelchan(void *source, int cargc, char **cargv);
int csc_dorenchan(void *source, int cargc, char **cargv);
int csc_dowelcome(void *source, int cargc, char **cargv);
int csc_doinvite(void *source, int cargc, char **cargv);
int csc_doop(void *source, int cargc, char **cargv);
int csc_dovoice(void *source, int cargc, char **cargv);
int csc_dodeopall(void *source, int cargc, char **cargv);
int csc_dounbanall(void *source, int cargc, char **cargv);
int csc_doclearchan(void *source, int cargc, char **cargv);
int csc_dorecover(void *source, int cargc, char **cargv);
int csc_dounbanme(void *source, int cargc, char **cargv);
int csc_dodevoiceall(void *source, int cargc, char **cargv);
int csc_dosettopic(void *source, int cargc, char **cargv);
int csc_dopermban(void *source, int cargc, char **cargv);
int csc_dotempban(void *source, int cargc, char **cargv);
int csc_dobanlist(void *source, int cargc, char **cargv);
int csc_dounbanmask(void *source, int cargc, char **cargv);
int csc_dobandel(void *source, int cargc, char **cargv);
int csc_dochannelcomment(void *source, int cargc, char **cargv);
int csc_dorejoin(void *source, int cargc, char **cargv);
int csc_doadduser(void *source, int cargc, char **cargv);
int csc_doremoveuser(void *source, int cargc, char **cargv);
int csc_dobanclear(void *source, int cargc, char **cargv);
int csc_dochantype(void *source, int cargc, char **cargv);
int csc_dochanstat(void *source, int cargc, char **cargv);

char *getchanmode(regchan *rcp);

void _init() {
  chanservaddcommand("chanflags", QCMD_AUTHED, 2, csc_dochanflags, "Shows or changes the flags on a channel.");
  chanservaddcommand("chanmode",  QCMD_AUTHED, 4, csc_dochanmode,  "Shows which modes are forced or denied on a channel.");
  chanservaddcommand("chanlev",   QCMD_AUTHED, 3, csc_dochanlev,   "Shows or modifies user access on a channel.");
  chanservaddcommand("autolimit", QCMD_AUTHED, 2, csc_doautolimit, "Shows or changes the autolimit threshold on a channel.");
  chanservaddcommand("bantimer",  QCMD_AUTHED, 2, csc_dobantimer,  "Shows or changes the time after which bans are removed.");
  chanservaddcommand("addchan",   QCMD_OPER,   4, csc_doaddchan,   "Adds a new channel to the bot.");
  chanservaddcommand("suspendchan",   QCMD_OPER,   2, csc_dosuspendchan,   "Suspends a channel from the bot.");
  chanservaddcommand("suspendchanlist", QCMD_HELPER, 1, csc_dosuspendchanlist,   "Lists suspended channels.");
  chanservaddcommand("unsuspendchan", QCMD_OPER,   1, csc_dounsuspendchan, "Unsuspends a channel from the bot.");
  chanservaddcommand("delchan",   QCMD_OPER,   2, csc_dodelchan,   "Removes a channel from the bot.");
  chanservaddcommand("renchan",   QCMD_OPER,   2, csc_dorenchan,   "Renames a channel on the bot.");
  chanservaddcommand("welcome",   QCMD_AUTHED, 2, csc_dowelcome,   "Shows or changes the welcome message on a channel.");
  chanservaddcommand("invite",    QCMD_AUTHED, 1, csc_doinvite,    "Invites you to a channel.");
  chanservaddcommand("op",        QCMD_AUTHED, 20,csc_doop,        "Ops you or other users on channel(s).");
  chanservaddcommand("voice",     QCMD_AUTHED, 20,csc_dovoice,     "Voices you or other users on channel(s).");
  chanservaddcommand("deopall",   QCMD_AUTHED, 1, csc_dodeopall,   "Deops all users on channel.");
  chanservaddcommand("devoiceall",QCMD_AUTHED, 1, csc_dodevoiceall,"Devoices all users on a channel.");
  chanservaddcommand("unbanall",  QCMD_AUTHED, 1, csc_dounbanall,  "Removes all bans from a channel.");
  chanservaddcommand("unbanme",   QCMD_AUTHED, 1, csc_dounbanme,   "Removes any bans affecting you from a channel.");
  chanservaddcommand("clearchan", QCMD_AUTHED, 1, csc_doclearchan, "Removes all modes from a channel.");
  chanservaddcommand("recover",   QCMD_AUTHED, 1, csc_dorecover,   "Recovers a channel (same as deopall, unbanall, clearchan).");
  chanservaddcommand("settopic",  QCMD_AUTHED, 2, csc_dosettopic,  "Changes the topic on a channel.");
  chanservaddcommand("permban",   QCMD_AUTHED, 3, csc_dopermban,   "Permanently bans a hostmask on a channel.");
  chanservaddcommand("tempban",   QCMD_AUTHED, 4, csc_dotempban,   "Bans a hostmask on a channel for a specified time period.");
  chanservaddcommand("banlist",   QCMD_AUTHED, 1, csc_dobanlist,   "Displays all persistent bans on a channel.");
  chanservaddcommand("unbanmask", QCMD_AUTHED, 2, csc_dounbanmask, "Removes bans matching a particular mask from a channel.");
  chanservaddcommand("bandel",    QCMD_AUTHED, 2, csc_dobandel,    "Removes a single ban from a channel.");
  chanservaddcommand("banclear",  QCMD_AUTHED, 1, csc_dobanclear,  "Removes all bans from a channel including persistent bans.");
  chanservaddcommand("channelcomment",QCMD_OPER,2,csc_dochannelcomment,"Shows or changes the staff comment for a channel.");
  chanservaddcommand("rejoin",    QCMD_OPER,   1, csc_dorejoin,    "Makes the bot rejoin a channel.");
  chanservaddcommand("adduser",   QCMD_AUTHED, 20,csc_doadduser,   "Adds one or more users to a channel as +aot.");
  chanservaddcommand("removeuser",QCMD_AUTHED, 20,csc_doremoveuser,"Removes one or more users from a channel.");
  chanservaddcommand("chantype",  QCMD_OPER,    2,csc_dochantype,  "Shows or changes a channel's type.");
  chanservaddcommand("chanstat",  QCMD_AUTHED,  1,csc_dochanstat,  "Displays channel activity statistics.");
}

void _fini() {
  chanservremovecommand("chanflags", csc_dochanflags);
  chanservremovecommand("chanmode",  csc_dochanmode);
  chanservremovecommand("chanlev",   csc_dochanlev);
  chanservremovecommand("autolimit", csc_doautolimit);
  chanservremovecommand("bantimer",  csc_dobantimer);
  chanservremovecommand("addchan",   csc_doaddchan);
  chanservremovecommand("suspendchan",csc_dosuspendchan);
  chanservremovecommand("suspendchanlist",csc_dosuspendchanlist);
  chanservremovecommand("unsuspendchan",csc_dounsuspendchan);
  chanservremovecommand("delchan",   csc_dodelchan);
  chanservremovecommand("renchan",   csc_dorenchan);
  chanservremovecommand("welcome",   csc_dowelcome);
  chanservremovecommand("invite",    csc_doinvite);
  chanservremovecommand("op",        csc_doop);
  chanservremovecommand("voice",     csc_dovoice);
  chanservremovecommand("deopall",   csc_dodeopall);
  chanservremovecommand("devoiceall",csc_dodevoiceall);
  chanservremovecommand("unbanall",  csc_dounbanall);
  chanservremovecommand("unbanme",   csc_dounbanme);
  chanservremovecommand("clearchan", csc_doclearchan);
  chanservremovecommand("recover",   csc_dorecover);
  chanservremovecommand("settopic",  csc_dosettopic);
  chanservremovecommand("permban",   csc_dopermban);
  chanservremovecommand("tempban",   csc_dotempban);
  chanservremovecommand("banlist",   csc_dobanlist);
  chanservremovecommand("unbanmask", csc_dounbanmask);
  chanservremovecommand("bandel",    csc_dobandel);
  chanservremovecommand("banclear",    csc_dobanclear);
  chanservremovecommand("channelcomment",csc_dochannelcomment);
  chanservremovecommand("rejoin",    csc_dorejoin);
  chanservremovecommand("adduser",   csc_doadduser);
  chanservremovecommand("removeuser",csc_doremoveuser);
  chanservremovecommand("chantype",  csc_dochantype);
  chanservremovecommand("chanstat",  csc_dochanstat);
}

int csc_dochanflags(void *source, int cargc, char **cargv) {
  regchan *rcp;
  nick *sender=source;
  reguser *rup=getreguserfromnick(sender);
  chanindex *cip;
  flag_t oldflags,changemask;
  char flagbuf[20];

  if (cargc<1) {
    chanservstdmessage(sender,QM_NOTENOUGHPARAMS,"chanflags");
    return CMD_ERROR;
  }
  
  if (!(cip=cs_checkaccess(sender, cargv[0], CA_OPPRIV, NULL, 
			   "chanflags", QPRIV_VIEWCHANFLAGS, 0))) 
    return CMD_ERROR;

  rcp=cip->exts[chanservext];

  if (cargc>1) {
    if (!cs_checkaccess(sender, NULL, CA_MASTERPRIV, cip, "chanflags", 
			QPRIV_CHANGECHANFLAGS, 0))
      return CMD_ERROR;

    oldflags=rcp->flags;
    changemask=QCFLAG_USERCONTROL;
    if (UIsDev(rup)) {
      changemask=QCFLAG_ALL;
    }
    setflags(&rcp->flags, changemask, cargv[1], rcflags, REJECT_NONE);

    /* We might need to do things in response to the flag changes.. */
    if (cip->channel) {
      if ((oldflags ^ rcp->flags) & (QCFLAG_JOINED | QCFLAG_SUSPENDED)) {
        chanservjoinchan(cip->channel);
	rcp->status |= (QCSTAT_OPCHECK | QCSTAT_MODECHECK | QCSTAT_BANCHECK);
	rcp->lastbancheck=0;
	cs_timerfunc(cip);
      } else {
        if (CIsEnforce(rcp)) {
	  rcp->lastbancheck=0;
          cs_checkbans(cip->channel);  
	}
    
        if (CIsProtect(rcp) || CIsBitch(rcp) || CIsAutoOp(rcp) || CIsAutoVoice(rcp) || CIsKnownOnly(rcp)) {
	  rcp->status |= QCSTAT_OPCHECK;
	  cs_timerfunc(cip);
	}
      }
    }
    
    if (CIsAutoLimit(rcp) && !(oldflags & QCFLAG_AUTOLIMIT)) {
      rcp->forcemodes |= CHANMODE_LIMIT;
      rcp->denymodes &= ~CHANMODE_LIMIT;
      rcp->limit=0;
      cs_timerfunc(cip);
    }

    if (!CIsAutoLimit(rcp) && (oldflags & QCFLAG_AUTOLIMIT)) {
      rcp->forcemodes &= ~CHANMODE_LIMIT;
      if (cip->channel) 
        cs_checkchanmodes(cip->channel);
    }

    strcpy(flagbuf,printflags(oldflags, rcflags));
    cs_log(sender,"CHANFLAGS %s %s (%s -> %s)",cip->name->content,cargv[1],flagbuf,printflags(rcp->flags,rcflags));
    chanservstdmessage(sender, QM_DONE);
    csdb_updatechannel(rcp);
  }
  
  chanservstdmessage(sender,QM_CURCHANFLAGS,cip->name->content,printflags(rcp->flags, rcflags));
  return CMD_OK;
}

int csc_dochanmode(void *source, int cargc, char **cargv) {
  regchan *rcp;
  nick *sender=source;
  chanindex *cip;
  flag_t forceflags,denyflags;
  char buf1[60];
  int carg=2,limdone=0;
  sstring *newkey=NULL;
  unsigned int newlim=0;

  if (cargc<1) {
    chanservstdmessage(sender,QM_NOTENOUGHPARAMS,"chanmode");
    return CMD_ERROR;
  }

  if (!(cip=cs_checkaccess(sender, cargv[0], CA_OPPRIV, 
			   NULL, "chanmode", QPRIV_VIEWCHANMODES, 0)))
    return CMD_ERROR;
  
  rcp=cip->exts[chanservext];

  if (cargc>1) {
    if (!cs_checkaccess(sender, NULL, CA_MASTERPRIV,
			cip, "chanmode", QPRIV_CHANGECHANMODES, 0))
      return CMD_ERROR;

    /* Save the current modes.. */
    strcpy(buf1,getchanmode(rcp));

    /* Pick out the + flags: start from 0 */
    forceflags=0;
    setflags(&forceflags, CHANMODE_ALL, cargv[1], cmodeflags, REJECT_NONE);    

    /* Pick out the - flags: start from everything and invert afterwards.. */
    denyflags=CHANMODE_ALL;
    setflags(&denyflags, CHANMODE_ALL, cargv[1], cmodeflags, REJECT_NONE);
    denyflags = (~denyflags) & CHANMODE_ALL;

    forceflags &= ~denyflags; /* Can't force and deny the same mode (shouldn't be possible anyway) */
    if (forceflags & CHANMODE_SECRET) {
      forceflags &= ~CHANMODE_PRIVATE;
      denyflags |= CHANMODE_PRIVATE;
    }
    if (forceflags & CHANMODE_PRIVATE) {
      forceflags &= ~CHANMODE_SECRET;
      denyflags |= CHANMODE_SECRET;
    }

    if ((forceflags & CHANMODE_LIMIT) && 
	(!(forceflags & CHANMODE_KEY) || strrchr(cargv[1],'l') < strrchr(cargv[1],'k'))) {
      if (cargc<=carg) {
	chanservstdmessage(sender,QM_NOTENOUGHPARAMS,"chanmode");
	return CMD_ERROR;
      }
      newlim=strtol(cargv[carg++],NULL,10);
      limdone=1;
    }

    if (forceflags & CHANMODE_KEY) {
      if (cargc<=carg) {
	chanservstdmessage(sender,QM_NOTENOUGHPARAMS,"chanmode");
	return CMD_ERROR;
      }
      newkey=getsstring(cargv[carg++], KEYLEN);
    }

    if ((forceflags & CHANMODE_LIMIT) && !limdone) {
      if (cargc<=carg) {
	chanservstdmessage(sender,QM_NOTENOUGHPARAMS,"chanmode");
	return CMD_ERROR;
      }
      newlim=strtol(cargv[carg++],NULL,10);
      limdone=1;
    }

    if (CIsAutoLimit(rcp)) {
      forceflags |= CHANMODE_LIMIT;
      denyflags &= ~CHANMODE_LIMIT;
      newlim=rcp->limit;
    }

    /* It parsed OK, so update the structure.. */
    rcp->forcemodes=forceflags;
    rcp->denymodes=denyflags;      
    if (rcp->key)
      freesstring(rcp->key);
    rcp->key=newkey;
    rcp->limit=newlim;
    
    chanservstdmessage(sender, QM_DONE);
    cs_log(sender,"CHANMODE %s %s (%s -> %s)",cip->name->content,cargv[1],buf1,getchanmode(rcp));
    csdb_updatechannel(rcp);
    cs_checkchanmodes(cip->channel);    
  }
  
  chanservstdmessage(sender,QM_CURFORCEMODES,cip->name->content,getchanmode(rcp));

  return CMD_OK;
}

char *getchanmode(regchan *rcp) {
  static char buf1[50];
  char buf2[30];

  if (rcp->forcemodes) {
    strcpy(buf1,printflags(rcp->forcemodes, cmodeflags));
  } else {
    buf1[0]='\0';
  }

  strcpy(buf2,printflagdiff(CHANMODE_ALL, ~(rcp->denymodes), cmodeflags));
  strcat(buf1, buf2);

  if (rcp->forcemodes & CHANMODE_LIMIT) {
    sprintf(buf2, " %d",rcp->limit);
    strcat(buf1, buf2);
  }

  if (rcp->forcemodes & CHANMODE_KEY) {
    sprintf(buf2, " %s",rcp->key->content);
    strcat(buf1, buf2);
  }

  if (*buf1=='\0') {
    strcpy(buf1,"(none)");
  }

  return buf1;
}

int compareflags(const void *u1, const void *u2) {
  const regchanuser *r1=*(void **)u1, *r2=*(void **)u2;
  flag_t f1,f2;
  
  for (f1=QCUFLAG_OWNER;f1;f1>>=1)
    if (r1->flags & f1)
      break;

  for (f2=QCUFLAG_OWNER;f2;f2>>=1)
    if (r2->flags & f2) 
      break;
  
  if (f1==f2) {
    return ircd_strcmp(r1->user->username, r2->user->username);
  } else {
    return f2-f1;
  }
}

int csc_dochanlev(void *source, int cargc, char **cargv) {
  nick *sender=source;
  chanindex *cip;
  regchan *rcp;
  regchanuser *rcup, *rcuplist;
  regchanuser **rusers;
  reguser *rup=getreguserfromnick(sender), *target;
  char time1[15],time2[15];
  char flagbuf[30];
  struct tm *tmp;
  flag_t flagmask, changemask, flags, oldflags;
  int showtimes=0;
  int donehead=0;
  int i,j;
  int newuser=0;
  int usercount;

  if (cargc<1) {
    chanservstdmessage(sender, QM_NOTENOUGHPARAMS, "chanlev");
    return CMD_ERROR;
  }

  if (!(cip=cs_checkaccess(sender, cargv[0], CA_KNOWN,
			   NULL, "chanlev", QPRIV_VIEWFULLCHANLEV, 0)))
    return CMD_ERROR;
  
  rcp=cip->exts[chanservext];
  rcup=findreguseronchannel(rcp, rup);

  /* Set flagmask for +v/+o users (can't see bans etc.) */
  flagmask = (QCUFLAG_OWNER | QCUFLAG_MASTER | QCUFLAG_OP | QCUFLAG_VOICE | QCUFLAG_AUTOVOICE | 
	      QCUFLAG_AUTOOP | QCUFLAG_TOPIC | QCUFLAG_SPAMCON | QCUFLAG_PROTECT | QCUFLAG_KNOWN);
  
  /* If user has +m or above, or helper access, show everything */
  if (cs_privcheck(QPRIV_VIEWFULLCHANLEV, sender) || CUHasMasterPriv(rcup)) {
    flagmask = QCUFLAG_ALL;
    showtimes=1;
  }
  
  if (cargc==1) {
    /* One arg: list chanlev */
    if (cs_privcheck(QPRIV_VIEWFULLCHANLEV, sender)) {
      reguser *founder=NULL, *addedby=NULL;
      addedby=findreguserbyID(rcp->addedby);
      chanservstdmessage(sender, QM_ADDEDBY, addedby ? addedby->username : "(unknown)");
      founder=findreguserbyID(rcp->founder);
      chanservstdmessage(sender, QM_FOUNDER, founder ? founder->username : "(unknown)");      
      chanservstdmessage(sender, QM_CHANTYPE, chantypes[rcp->chantype]->content);
    }

    /* Count users */
    for (i=0,usercount=0;i<REGCHANUSERHASHSIZE;i++)
      for (rcuplist=rcp->regusers[i];rcuplist;rcuplist=rcuplist->nextbychan)
	usercount++;
    
    /* Allocate array */
    rusers=(regchanuser **)malloc(usercount * sizeof(regchanuser *));

    /* Fill array */
    for (j=i=0;i<REGCHANUSERHASHSIZE;i++) {
      for (rcuplist=rcp->regusers[i];rcuplist;rcuplist=rcuplist->nextbychan) {
	if (!(flags=rcuplist->flags & flagmask))
	  continue;
	
	rusers[j++]=rcuplist;
      }
    }

    /* Sort */
    qsort(rusers, j, sizeof(regchanuser *), compareflags);

    /* List */
    for (i=0;i<j;i++) {
      rcuplist=rusers[i];

      if (!(flags=rcuplist->flags & flagmask)) 
	continue;
      
      if (!donehead) {
	chanservstdmessage(sender, QM_CHANLEVHEADER, cip->name->content);
	if (showtimes) 
	  chanservstdmessage(sender, QM_CHANLEVCOLFULL);
	else
	  chanservstdmessage(sender, QM_CHANLEVCOLSHORT);
	donehead=1;
      }
      
      if (showtimes) {
	if (!rcuplist->usetime) {
	  strcpy(time1,"Never");
	} else {
	  tmp=localtime(&(rcuplist->usetime));
	  strftime(time1,15,"%d/%m/%y %H:%M",tmp);
	}
	if (!rcuplist->changetime) {
	  strcpy(time2, "Unknown");
	} else {
	  tmp=localtime(&(rcuplist->changetime));
	  strftime(time2,15,"%d/%m/%y %H:%M",tmp);
	}
	chanservsendmessage(sender, " %-15s %-13s %-14s  %-14s  %s", rcuplist->user->username, 
			    printflags(flags, rcuflags), time1, time2, rcuplist->info?rcuplist->info->content:"");
      } else 
	chanservsendmessage(sender, " %-15s %s", rcuplist->user->username, printflags(flags, rcuflags));
    }
    
    if (donehead) {
      chanservstdmessage(sender, QM_ENDOFLIST);
    } else {
      chanservstdmessage(sender, QM_NOUSERSONCHANLEV, cip->name->content);
    }

    free(rusers);
  } else {
    /* 2 or more args.. relates to one specific user */
    if (!(target=findreguser(sender, cargv[1])))
      return CMD_ERROR; /* If there was an error, findreguser will have sent a message saying why.. */
    
    rcuplist=findreguseronchannel(rcp, target);
    
    if (cargc>2) {
      /* To change chanlev you have to either.. */
      if (!( cs_privcheck(QPRIV_CHANGECHANLEV, sender) ||             /* Have override privilege */
	     (rcup && rcuplist && (rcup==rcuplist) && CUKnown(rcup)) || /* Be manipulting yourself (oo er..) */
	     (rcup && CUHasMasterPriv(rcup) &&                        /* Have +m or +n on the channel */
	      !(rcuplist && CUIsOwner(rcuplist) && !CUIsOwner(rcup))) /* masters can't screw with owners */
	     )) {
	chanservstdmessage(sender, QM_NOACCESSONCHAN, cip->name->content, "chanlev");
	return CMD_ERROR;
      }
      
      if (!rcuplist) {
	rcuplist=getregchanuser();
	rcuplist->user=target;
	rcuplist->chan=rcp;
	rcuplist->flags=0;
	rcuplist->changetime=time(NULL);
	rcuplist->usetime=0;
	rcuplist->info=NULL;
	newuser=1;
      }
      
      if (cs_privcheck(QPRIV_CHANGECHANLEV, sender)) {
	/* Opers are allowed to change everything */
	changemask = QCUFLAG_ALL;
      } else {
	changemask=0;
	
	/* Everyone can change their own flags (except +dqb), and turn +iwj on/off */
	if (rcup==rcuplist) {
	  changemask = (rcup->flags | QCUFLAG_HIDEWELCOME | QCUFLAG_HIDEINFO | QCUFLAG_AUTOINVITE) & 
	              ~(QCUFLAG_BANNED | QCUFLAG_DENY | QCUFLAG_QUIET);
	  flagmask |= (QCUFLAG_HIDEWELCOME | QCUFLAG_HIDEINFO | QCUFLAG_AUTOINVITE);
	}
	
	/* Masters are allowed to manipulate +ovagtbqdpk */
	if (CUHasMasterPriv(rcup))
	  changemask |= ( QCUFLAG_KNOWN | QCUFLAG_OP | QCUFLAG_VOICE | QCUFLAG_AUTOOP | QCUFLAG_AUTOVOICE | 
			  QCUFLAG_TOPIC | QCUFLAG_BANNED | QCUFLAG_QUIET | QCUFLAG_DENY | QCUFLAG_PROTECT);
	
	/* Owners are allowed to manipulate +ms as well */
	if (CUIsOwner(rcup))
	  changemask |= ( QCUFLAG_MASTER | QCUFLAG_SPAMCON );
      }

      oldflags=rcuplist->flags;
      if (setflags(&(rcuplist->flags), changemask, cargv[2], rcuflags, REJECT_UNKNOWN | REJECT_DISALLOWED)) {
	chanservstdmessage(sender, QM_INVALIDCHANLEVCHANGE);
	return CMD_ERROR;
      }

      /* Now fix up some "impossible" combinations.. */
      /* +m can't be any of +qdb */
      if (CUHasMasterPriv(rcuplist))
	rcuplist->flags &= ~(QCUFLAG_BANNED | QCUFLAG_QUIET | QCUFLAG_DENY);
      
      /* +d can't be +o */
      if (CUIsDeny(rcuplist))
	rcuplist->flags &= ~QCUFLAG_OP;

      /* +q can't be +v */
      if (CUIsQuiet(rcuplist))
	rcuplist->flags &= ~QCUFLAG_VOICE;

      /* -o or +p can't be +a */
      if (!CUIsOp(rcuplist) || CUIsProtect(rcuplist))
	rcuplist->flags &= ~QCUFLAG_AUTOOP;
      
      /* +a or -v or +p can't be +g */
      if (!CUIsVoice(rcuplist) || CUIsAutoOp(rcuplist) || CUIsProtect(rcuplist))
	rcuplist->flags &= ~QCUFLAG_AUTOVOICE;

      /* and -ov can't be +p */
      if (!CUIsOp(rcuplist) && !CUIsVoice(rcuplist)) 
      	rcuplist->flags &= ~QCUFLAG_PROTECT;

      /* Check if anything "significant" has changed */
      if ((oldflags ^ rcuplist->flags) & (QCUFLAG_OWNER | QCUFLAG_MASTER | QCUFLAG_OP))
	rcuplist->changetime=time(NULL);

      strcpy(flagbuf,printflags(oldflags,rcuflags));
      cs_log(sender,"CHANLEV %s #%s %s (%s -> %s)",cip->name->content,rcuplist->user->username,cargv[2],
	     flagbuf,printflags(rcuplist->flags,rcuflags));

      /* Now see what we do next */
      if (rcuplist->flags) {
	/* User still valid: update or create */
	if (newuser) {
	  addregusertochannel(rcuplist);
	  csdb_createchanuser(rcuplist);
	} else {
	  csdb_updatechanuser(rcuplist);
	}
      } else {
	/* User has no flags: delete */
	if (!newuser) {
	  csdb_deletechanuser(rcuplist);
	  delreguserfromchannel(rcp, target);
	}
	freeregchanuser(rcuplist);
	rcuplist=NULL;
        for (i=0;i<REGCHANUSERHASHSIZE;i++)
          if (rcp->regusers[i])
            break;
        if (i==REGCHANUSERHASHSIZE) {
	  cs_log(sender,"DELCHAN %s (Cleared chanlev)",cip->name->content);
          cs_removechannel(rcp);	
	}
      }

      /* Say we've done it */
      chanservstdmessage(sender, QM_DONE);
      rcp->status |= QCSTAT_OPCHECK;
      cs_timerfunc(cip);
    }
    
    if (rcuplist && (rcuplist->flags & flagmask)) {
      chanservstdmessage(sender, QM_CHANUSERFLAGS, cargv[1], cip->name->content, 
			 printflags(rcuplist->flags & flagmask, rcuflags));
    } else {
      chanservstdmessage(sender, QM_CHANUSERUNKNOWN, cargv[1], cip->name->content);
    }
  }
  
  return CMD_OK;
}

int csc_doautolimit(void *source, int cargc, char **cargv) {
  nick *sender=source;
  chanindex *cip;
  regchan *rcp;
  int oldlimit;

  if (cargc<1) {
    chanservstdmessage(sender, QM_NOTENOUGHPARAMS, "autolimit");
    return CMD_ERROR;
  }

  if (!(cip=cs_checkaccess(sender, cargv[0], CA_OPPRIV,
			   NULL, "autolimit", QPRIV_VIEWAUTOLIMIT, 0)))
    return CMD_ERROR;

  rcp=cip->exts[chanservext];

  if (cargc>1) {
    if (!cs_checkaccess(sender, NULL, CA_MASTERPRIV, 
			cip, "autolimit", QPRIV_CHANGEAUTOLIMIT, 0))
      return CMD_ERROR;

    oldlimit=rcp->autolimit;
    rcp->autolimit=strtol(cargv[1],NULL,10);

    if (rcp->autolimit<1)
      rcp->autolimit=1;
    
    csdb_updatechannel(rcp);
    
    cs_log(sender,"AUTOLIMIT %s %s (%d -> %d)",cip->name->content,cargv[1],oldlimit,rcp->autolimit);
    chanservstdmessage(sender, QM_DONE);
    rcp->limit=0;
    cs_timerfunc(cip);
  }

  chanservstdmessage(sender, QM_CHANAUTOLIMIT, cargv[0], rcp->autolimit);
  return CMD_OK;
}

int csc_dobantimer(void *source, int cargc, char **cargv) {
  nick *sender=source;
  chanindex *cip;
  regchan *rcp;
  int oldtimer;

  if (cargc<1) {
    chanservstdmessage(sender, QM_NOTENOUGHPARAMS, "bantimer");
    return CMD_ERROR;
  }

  if (!(cip=cs_checkaccess(sender, cargv[0], CA_OPPRIV, NULL, "bantimer",
			   QPRIV_VIEWBANTIMER, 0)))
    return CMD_ERROR;

  rcp=cip->exts[chanservext];

  if (cargc>1) {
    if (!cs_checkaccess(sender, NULL, CA_MASTERPRIV, cip, "bantimer",
			QPRIV_CHANGEBANTIMER, 0))
      return CMD_ERROR;

    oldtimer=rcp->banduration;
    rcp->banduration=durationtolong(cargv[1]);

    if (rcp->banduration<0)
      rcp->banduration=0;
    
    /* Arbitrary limit */
    if (rcp->banduration > 31622400)
      rcp->banduration = 31622400;
      
    csdb_updatechannel(rcp);
    
    cs_log(sender,"BANTIMER %s %s (%u -> %u)",cip->name->content,cargv[1],oldtimer,rcp->banduration);
    chanservstdmessage(sender, QM_DONE);
    cs_timerfunc(cip);
  }

  if (rcp->banduration)
    chanservstdmessage(sender, QM_CHANBANAUTOREMOVE, cargv[0], longtoduration(rcp->banduration, 1));
  else
    chanservstdmessage(sender, QM_NOCHANBANAUTOREMOVE, cargv[0]);

  return CMD_OK;
}

int csc_dowelcome(void *source, int cargc, char **cargv) {
  nick *sender=source;
  chanindex *cip;
  regchan *rcp;
  sstring *oldwelcome;

  if (cargc<1) {
    chanservstdmessage(sender, QM_NOTENOUGHPARAMS, "welcome");
    return CMD_ERROR;
  }

  if (!(cip=cs_checkaccess(sender, cargv[0], CA_OPPRIV, NULL, "welcome",
			   QPRIV_VIEWWELCOME, 0)))
    return CMD_ERROR;

  rcp=cip->exts[chanservext];

  if (cargc>1) {
    if (!cs_checkaccess(sender, NULL, CA_MASTERPRIV, cip, "welcome",
			QPRIV_CHANGEWELCOME, 0))
      return CMD_ERROR;

    oldwelcome=rcp->welcome;
    
    rcp->welcome=getsstring(cargv[1], 500);
    csdb_updatechannel(rcp);

    cs_log(sender,"WELCOME %s %s (was %s)",cip->name->content,rcp->welcome->content,oldwelcome?oldwelcome->content:"unset");
    freesstring(oldwelcome);
    chanservstdmessage(sender, QM_DONE);
  }

  chanservstdmessage(sender, QM_WELCOMEMESSAGEIS, rcp->index->name->content, 
		     rcp->welcome?rcp->welcome->content:"(none)");

  return CMD_OK;
}

int csc_doaddchan(void *source, int cargc, char **cargv) {
  nick *sender=source;
  reguser *rup=getreguserfromnick(sender);
  chanindex *cip;
  regchan *rcp;
  regchanuser *rcup;
  reguser *founder;
  flag_t flags;
  short type=0;
  
  if (!rup)
    return CMD_ERROR;
    
  if (cargc<1) {
    chanservstdmessage(sender, QM_NOTENOUGHPARAMS, "addchan");
    return CMD_ERROR;
  }
  
  if (*cargv[0] != '#') {
    chanservstdmessage(sender, QM_INVALIDCHANNAME, cargv[0]);
    return CMD_ERROR;
  } 
  
  if (cargc>1) {
    if (!(founder=findreguser(sender, cargv[1])))
      return CMD_ERROR;
  } else {
    founder=rup;
  }      
  
  if (cargc>2) {
    flags=0;
    setflags(&flags, QCFLAG_ALL, cargv[2], rcflags, REJECT_NONE);    
  } else {
    flags = (QCFLAG_JOINED | QCFLAG_BITCH | QCFLAG_PROTECT | QCFLAG_ENFORCE);
  }
  
  /* Pick up the chantype */
  if (cargc>3) {
    for (type=CHANTYPES-1;type;type--) {
      if (!ircd_strcmp(chantypes[type]->content, cargv[3]))
	break;
    }
    if (!type) {
      chanservstdmessage(sender, QM_UNKNOWNCHANTYPE, cargv[3]);
      return CMD_ERROR;
    }
  }
  
  if (!(cip=findorcreatechanindex(cargv[0]))) {
    chanservstdmessage(sender, QM_INVALIDCHANNAME, cargv[0]);
    return CMD_ERROR;
  }
  
  if (cip->exts[chanservext]) {
    chanservstdmessage(sender, QM_ALREADYREGISTERED, cip->name->content);
    return CMD_ERROR;
  }
  
  /* Initialise the channel */ 
  rcp=getregchan();
  
  /* ID, index */
  rcp->ID=++lastchannelID;
  rcp->index=cip;
  cip->exts[chanservext]=rcp;
  
  rcp->chantype=type;
  rcp->flags=flags;
  rcp->status=0;
  rcp->bans=NULL;
  rcp->lastcountersync=0;
  
  rcp->limit=0;
  rcp->forcemodes=CHANMODE_NOEXTMSG | CHANMODE_TOPICLIMIT;
  rcp->denymodes=0;

  if (CIsAutoLimit(rcp)) {
    rcp->forcemodes |= CHANMODE_LIMIT;
  }
  
  rcp->autolimit=5;
  rcp->banstyle=0;
  
  rcp->created=rcp->lastactive=rcp->statsreset=rcp->ostatsreset=time(NULL);
  rcp->banduration=1800;
  rcp->autoupdate=0;
  rcp->lastbancheck=0;
  
  /* Added by */
  rcp->addedby=rup->ID;
  
  /* Founder */
  rcp->founder=founder->ID;
 
  /* Suspend by */
  rcp->suspendby=0;
  
  rcp->totaljoins=rcp->tripjoins=rcp->otripjoins=rcp->maxusers=rcp->tripusers=rcp->otripusers=0;
  rcp->welcome=rcp->topic=rcp->key=rcp->suspendreason=rcp->comment=NULL;
  
  /* Users */
  memset(rcp->regusers,0,REGCHANUSERHASHSIZE*sizeof(reguser *));   
  
  rcp->checksched=NULL;

  /* Add new channel to db.. */  
  csdb_createchannel(rcp);
  
  /* Add the founder as +ano */
  rcup=getregchanuser();
  rcup->chan=rcp;
  rcup->user=founder;
  rcup->flags=(QCUFLAG_OWNER | QCUFLAG_OP | QCUFLAG_AUTOOP);
  rcup->usetime=0;
  rcup->info=NULL;
  rcup->changetime=time(NULL);
  
  addregusertochannel(rcup);
  csdb_createchanuser(rcup);

  /* If the channel exists, get the ball rolling */
  if (cip->channel) {
    chanservjoinchan(cip->channel);
    rcp->status |= QCSTAT_MODECHECK | QCSTAT_OPCHECK | QCSTAT_BANCHECK;
    cs_timerfunc(cip);
  }

  cs_log(sender, "ADDCHAN %s #%s %s %s",cip->name->content,founder->username,printflags(rcp->flags,rcflags), chantypes[type]->content);
  chanservstdmessage(sender, QM_DONE);
  return CMD_OK;
}

int csc_dosuspendchan(void *source, int cargc, char **cargv) {
  nick *sender=source;
  reguser *rup=getreguserfromnick(sender);
  chanindex *cip;
  regchan *rcp;

  if (!rup)
    return CMD_ERROR;

  if (cargc<2) {
    chanservstdmessage(sender, QM_NOTENOUGHPARAMS, "suspendchan");
    return CMD_ERROR;
  }

  if (!(cip=findchanindex(cargv[0])) || !(rcp=cip->exts[chanservext])) {
    chanservstdmessage(sender, QM_UNKNOWNCHAN, cargv[0]);
    return CMD_ERROR;
  }

  if (CIsSuspended(rcp)) {
    chanservstdmessage(sender, QM_CHANNELALREADYSUSPENDED, cip->name->content);
    return CMD_ERROR;
  }

  CSetSuspended(rcp);
  rcp->suspendreason = getsstring(cargv[1], 250);
  rcp->suspendby = rup->ID;
  cs_log(sender,"SUSPENDCHAN %s (%s)",cip->name->content,rcp->suspendreason->content);
  chanservjoinchan(cip->channel);

  csdb_updatechannel(rcp);
  chanservstdmessage(sender, QM_DONE);

  return CMD_OK;
}

int csc_dosuspendchanlist(void *source, int cargc, char **cargv) {
  nick *sender=source;
  reguser *rup=getreguserfromnick(sender);
  chanindex *cip;
  regchan *rcp;
  int i;
  char *bywhom;
  unsigned int count=0;
  
  if (!rup)
    return CMD_ERROR;
  
  if (cargc < 1) {
    chanservstdmessage(sender, QM_NOTENOUGHPARAMS, "suspendchanlist");
    return CMD_ERROR;
  }
  
  chanservstdmessage(sender, QM_SUSPENDCHANLISTHEADER);
  for (i=0; i<CHANNELHASHSIZE; i++) {
    for (cip=chantable[i]; cip; cip=cip->next) {
      if (!(rcp=(regchan*)cip->exts[chanservext]))
        continue;
      
      if (!CIsSuspended(rcp))
        continue;
      
      if ((rcp->suspendby != rup->ID) && match(cargv[0], cip->name->content))
        continue;
      
      if (rcp->suspendby == rup->ID)
        bywhom=rup->username;
      else {
        reguser *trup=findreguserbyID(rcp->suspendby);
        if (trup)
          bywhom=trup->username;
        else
          bywhom="unknown";
      }
      count++;
      chanservsendmessage(sender, "%-30s %-15s %s", cip->name->content, bywhom, rcp->suspendreason->content);
      if (count >= 2000) {
        chanservstdmessage(sender, QM_TOOMANYRESULTS, 2000, "channels");
        return CMD_ERROR;
      }
    }
  }
  chanservstdmessage(sender, QM_RESULTCOUNT, count, "channel", (count==1)?"":"s");
  
  return CMD_OK;
}

int csc_dounsuspendchan(void *source, int cargc, char **cargv) {
  nick *sender=source;
  reguser *rup=getreguserfromnick(sender);
  chanindex *cip;
  regchan *rcp;

  if (!rup)
    return CMD_ERROR;

  if (cargc<1) {
    chanservstdmessage(sender, QM_NOTENOUGHPARAMS, "unsuspendchan");
    return CMD_ERROR;
  }

  if (!(cip=findchanindex(cargv[0])) || !(rcp=cip->exts[chanservext])) {
    chanservstdmessage(sender, QM_UNKNOWNCHAN, cargv[0]);
    return CMD_ERROR;
  }

  if(!CIsSuspended(rcp)) {
    chanservstdmessage(sender, QM_CHANNELNOTSUSPENDED, cip->name->content);
    cs_log(sender,"UNSUSPENDCHAN %s is not suspended",cip->name->content);
    return CMD_ERROR;
  }

  CClearSuspended(rcp);
  cs_log(sender,"UNSUSPENDCHAN %s (%s)",cip->name->content,rcp->suspendreason->content);
  freesstring(rcp->suspendreason);
  rcp->suspendreason = NULL;
  rcp->suspendby = 0;

  chanservjoinchan(cip->channel);

  csdb_updatechannel(rcp);
  chanservstdmessage(sender, QM_DONE);

  return CMD_OK;
}

int csc_dodelchan(void *source, int cargc, char **cargv) {
  nick *sender=source;
  reguser *rup=getreguserfromnick(sender);
  chanindex *cip;
  regchan *rcp;

  if (!rup)
    return CMD_ERROR;

  if (cargc<1) {
    chanservstdmessage(sender, QM_NOTENOUGHPARAMS, "delchan");
    return CMD_ERROR;
  }

  if (!(cip=findchanindex(cargv[0])) || !(rcp=cip->exts[chanservext])) {
    chanservstdmessage(sender, QM_UNKNOWNCHAN, cargv[0]);
    return CMD_ERROR;
  }

  cs_log(sender,"DELCHAN %s (%s)",cip->name->content,cargc>1?cargv[1]:"");
  cs_removechannel(rcp);
  chanservstdmessage(sender, QM_DONE);

  return CMD_OK;
}

int csc_doinvite(void *source, int cargc, char **cargv) {
  nick *sender=source;
  chanindex *cip;

  if (cargc<1) {
    chanservstdmessage(sender, QM_NOTENOUGHPARAMS, "invite");
    return CMD_ERROR;
  }

  if (!(cip=cs_checkaccess(sender, cargv[0], CA_KNOWN | CA_OFFCHAN, 
			   NULL, "invite", 0, 0)))
    return CMD_ERROR;

  if (cip->channel) {
    localinvite(chanservnick, cip->channel, sender);
  }
  chanservstdmessage(sender, QM_DONE);
 
  return CMD_OK;
}

int csc_dosettopic(void *source, int cargc, char **cargv) {
  nick *sender=source;
  chanindex *cip;
  regchan *rcp;

  if (cargc<1) {
    chanservstdmessage(sender, QM_NOTENOUGHPARAMS, "settopic");
    return CMD_ERROR;
  }

  if (!(cip=cs_checkaccess(sender, cargv[0], CA_TOPICPRIV, 
			   NULL, "settopic", 0, 0)))
    return CMD_ERROR;

  rcp=cip->exts[chanservext];

  if (cargc>1) {
    if (rcp->topic)
      freesstring(rcp->topic);
    rcp->topic=getsstring(cargv[1],TOPICLEN);
  } 
  
  if (rcp->topic && cip->channel) {
    localsettopic(chanservnick, cip->channel, rcp->topic->content);
  }

  chanservstdmessage(sender, QM_DONE);
  csdb_updatechannel(rcp);
  return CMD_OK;
}

int csc_doop(void *source, int cargc, char **cargv) {
  nick *sender=source, *np;
  reguser *rup=getreguserfromnick(sender);
  chanindex *cip;
  regchan *rcp=NULL;
  regchanuser *rcup;
  channel **ca;
  unsigned long *lp;
  int i;
  modechanges changes;

  if (!rup)
    return CMD_ERROR;
  
  if (cargc==0) {
    /* No args: "op me on every channel you can */
    ca=sender->channels->content;
    for (i=0;i<sender->channels->cursi;i++) {
      if ((rcp=ca[i]->index->exts[chanservext]) && !CIsSuspended(rcp)) {
	/* It's a Q channel */
	if (!(*(getnumerichandlefromchanhash(ca[i]->users, sender->numeric)) & 
	      CUMODE_OP)) {
	  /* They're not opped */
	  if ((rcup=findreguseronchannel(rcp, rup)) && CUHasOpPriv(rcup) && 
	      !CUIsDeny(rcup)) {
	    /* And they have op priv on the chan: op them */
	    localsetmodeinit(&changes, ca[i], chanservnick);
	    localdosetmode_nick(&changes, sender, MC_OP);
	    localsetmodeflush(&changes,1);
	  }
	}
      }
    }
    
    chanservstdmessage(sender, QM_DONE);
    return CMD_OK;
  }

  /* If there is at least one arg, the first is a channel */

  if (!(cip=cs_checkaccess(sender, cargv[0], CA_OPPRIV, NULL, "op", 0, 0)))
    return CMD_ERROR;

  rcp=cip->exts[chanservext];

  if (cargc==1) {
    /* Only one arg: "op me" */
    if (!cs_checkaccess(sender, NULL, CA_OPPRIV | CA_DEOPPED, cip, "op", 0, 0))
      return CMD_ERROR;
    
    localsetmodeinit(&changes, cip->channel, chanservnick);
    localdosetmode_nick(&changes, sender, MC_OP);
    localsetmodeflush(&changes,1);

    chanservstdmessage(sender, QM_DONE);
    return CMD_OK;
  }

  /* Set up the modes */
  localsetmodeinit(&changes, cip->channel, chanservnick);

  for(i=1;i<cargc;i++) {
    if (!(np=getnickbynick(cargv[i]))) {
      chanservstdmessage(sender, QM_UNKNOWNUSER, cargv[i]);
      continue;
    }

    if (!(lp=getnumerichandlefromchanhash(cip->channel->users, np->numeric))) {
      chanservstdmessage(sender, QM_USERNOTONCHAN, np->nick, cip->name->content);
      continue;
    }

    if (*lp & CUMODE_OP) {
      chanservstdmessage(sender, QM_USEROPPEDONCHAN, np->nick, cip->name->content);
      continue;
    }

    rup=getreguserfromnick(np);
    if (rup)
      rcup=findreguseronchannel(rcp,rup);
    else
      rcup=NULL;

    /* Bitch mode: check that this user is allowed to be opped.. */
    if ((CIsBitch(rcp) && (!rcup || !CUHasOpPriv(rcup))) || (rcup && CUIsDeny(rcup))) {
      chanservstdmessage(sender, QM_CANTOP, np->nick, cip->name->content);
      continue;
    }

    localdosetmode_nick(&changes, np, MC_OP);
  }

  localsetmodeflush(&changes, 1);
  chanservstdmessage(sender, QM_DONE);

  return CMD_OK;
}

int csc_dovoice(void *source, int cargc, char **cargv) {
  nick *sender=source, *np;
  reguser *rup=getreguserfromnick(sender);
  chanindex *cip;
  regchan *rcp=NULL;
  regchanuser *rcup;
  channel **ca;
  unsigned long *lp;
  int i;
  modechanges changes;

  if (!rup)
    return CMD_ERROR;
  
  if (cargc==0) {
    /* No args: "voice me on every channel you can */
    ca=sender->channels->content;
    for (i=0;i<sender->channels->cursi;i++) {
      if ((rcp=ca[i]->index->exts[chanservext]) && !CIsSuspended(rcp)) {
	/* It's a Q channel */
	if (!(*(getnumerichandlefromchanhash(ca[i]->users, sender->numeric)) & 
	      (CUMODE_OP|CUMODE_VOICE))) {
	  /* They're not opped or voiced */
	  rcup=findreguseronchannel(rcp, rup);
	  if ((!rcup || !CUIsQuiet(rcup)) && 
	      ((rcup && CUHasVoicePriv(rcup)) ||
	      (CIsVoiceAll(rcp)))) {
	    /* And they have voice priv on the chan (or it's autovoice): 
	     * voice them */
	    localsetmodeinit(&changes, ca[i], chanservnick);
	    localdosetmode_nick(&changes, sender, MC_VOICE);
	    localsetmodeflush(&changes,1);
	  }
	}
      }
    }
    
    chanservstdmessage(sender, QM_DONE);
    return CMD_OK;
  }

  /* If there is at least one arg, the first is a channel */

  if (!(cip=cs_checkaccess(sender, cargv[0], CA_VOICEPRIV, NULL, "voice", 0, 0)))
    return CMD_ERROR;

  if (cargc==1) {
    /* Only one arg: "voice me" */
    if (!cs_checkaccess(sender, NULL, CA_VOICEPRIV | CA_DEVOICED, cip,
			"voice", 0, 0))
      return CMD_ERROR;

    localsetmodeinit(&changes, cip->channel, chanservnick);
    localdosetmode_nick(&changes, sender, MC_VOICE);
    localsetmodeflush(&changes,1);

    chanservstdmessage(sender, QM_DONE);
    return CMD_OK;
  }

  if (!(cip=cs_checkaccess(sender, NULL, CA_OPPRIV, cip, "voice", 0, 0)))
    return CMD_ERROR;

  rcp=cip->exts[chanservext];

  /* Set up the modes */
  localsetmodeinit(&changes, cip->channel, chanservnick);

  for(i=1;i<cargc;i++) {
    if (!(np=getnickbynick(cargv[i]))) {
      chanservstdmessage(sender, QM_UNKNOWNUSER, cargv[i]);
      continue;
    }

    if (!(lp=getnumerichandlefromchanhash(cip->channel->users, np->numeric))) {
      chanservstdmessage(sender, QM_USERNOTONCHAN, np->nick, cip->name->content);
      continue;
    }

    if (*lp & CUMODE_VOICE) {
      chanservstdmessage(sender, QM_USERVOICEDONCHAN, np->nick, cip->name->content);
      continue;
    }
   
    if ((rup=getreguserfromnick(np)) && (rcup=findreguseronchannel(rcp, rup)) && CUIsQuiet(rcup)) {
      chanservstdmessage(sender, QM_CANTVOICE, np->nick, cip->name->content);
      continue;
    }

    localdosetmode_nick(&changes, np, MC_VOICE);
  }

  localsetmodeflush(&changes, 1);
  chanservstdmessage(sender, QM_DONE);

  return CMD_OK;
}

int csc_dodeopall(void *source, int cargc, char **cargv) {
  nick *sender=source,*np;
  reguser *rup;
  regchanuser *rcup;
  regchan *rcp;
  chanindex *cip;
  unsigned long *lp;
  int i;
  modechanges changes;
  
  if (cargc<1) {
    chanservstdmessage(sender, QM_NOTENOUGHPARAMS, "deopall");
    return CMD_ERROR;
  }

  if (!(cip=cs_checkaccess(sender, cargv[0], CA_MASTERPRIV, NULL, "deopall",0, 0)))
    return CMD_ERROR;

  rcp=cip->exts[chanservext];

  if (cip->channel) {
    localsetmodeinit(&changes, cip->channel, chanservnick);

    for (i=0,lp=cip->channel->users->content;
	 i<cip->channel->users->hashsize;i++,lp++) {
      if (*lp!=nouser && (*lp & CUMODE_OP)) {
	if (!(np=getnickbynumeric(*lp)) || 
	    (!IsService(np) && (!(rup=getreguserfromnick(np)) || 
	    !(rcup=findreguseronchannel(rcp, rup)) || !(CUHasOpPriv(rcup)) ||
	    !(CUIsProtect(rcup) || CIsProtect(rcp))))) {
	  localdosetmode_nick(&changes, np, MC_DEOP);
	}
      }
    }

    localsetmodeflush(&changes, 1);
  }

  cs_log(sender,"DEOPALL %s",cip->name->content);
  chanservstdmessage(sender, QM_DONE);
  return CMD_OK;
}     
      
int csc_dodevoiceall(void *source, int cargc, char **cargv) {
  nick *sender=source,*np;
  reguser *rup;
  regchanuser *rcup;
  regchan *rcp;
  chanindex *cip;
  unsigned long *lp;
  int i;
  modechanges changes;
  
  if (cargc<1) {
    chanservstdmessage(sender, QM_NOTENOUGHPARAMS, "devoiceall");
    return CMD_ERROR;
  }

  if (!(cip=cs_checkaccess(sender, cargv[0], CA_MASTERPRIV, NULL, "devoiceall",0, 0)))
    return CMD_ERROR;

  rcp=cip->exts[chanservext];

  if (cip->channel) {
    localsetmodeinit(&changes, cip->channel, chanservnick);

    for (i=0,lp=cip->channel->users->content;
	 i<cip->channel->users->hashsize;i++,lp++) {
      if (*lp!=nouser && (*lp & CUMODE_VOICE)) {
	if (!(np=getnickbynumeric(*lp)) || 
	    (!IsService(np) && (!(rup=getreguserfromnick(np)) || 
	    !(rcup=findreguseronchannel(rcp, rup)) || !(CUHasVoicePriv(rcup)) ||
	    !(CUIsProtect(rcup) || CIsProtect(rcp))))) {
	  localdosetmode_nick(&changes, np, MC_DEVOICE);
	}
      }
    }

    localsetmodeflush(&changes, 1);
  }

  cs_log(sender,"DEVOICEALL %s",cip->name->content);
  chanservstdmessage(sender, QM_DONE);
  return CMD_OK;
}     
      
int csc_dounbanall(void *source, int cargc, char **cargv) {
  nick *sender=source;
  regchan *rcp;
  chanindex *cip;
  modechanges changes;

  if (cargc<1) {
    chanservstdmessage(sender, QM_NOTENOUGHPARAMS, "unbanall");
    return CMD_ERROR;
  }

  if (!(cip=cs_checkaccess(sender, cargv[0], CA_MASTERPRIV, NULL, "unbanall",0, 0)))
    return CMD_ERROR;

  rcp=cip->exts[chanservext];

  if (cip->channel) {
    localsetmodeinit(&changes, cip->channel, chanservnick);

    while (cip->channel->bans) {
      localdosetmode_ban(&changes, bantostring(cip->channel->bans), MCB_DEL);
    }

    localsetmodeflush(&changes, 1);
  }

  cs_log(sender,"UNBANALL %s",cip->name->content);
  chanservstdmessage(sender, QM_DONE);
  return CMD_OK;
}     
      
int csc_dounbanme(void *source, int cargc, char **cargv) {
  nick *sender=source;
  regchan *rcp;
  chanindex *cip;
  modechanges changes;
  chanban **cbh;

  if (cargc<1) {
    chanservstdmessage(sender, QM_NOTENOUGHPARAMS, "unbanme");
    return CMD_ERROR;
  }

  if (!(cip=cs_checkaccess(sender, cargv[0], CA_OPPRIV, NULL, "unbanme", 0, 0)))
    return CMD_ERROR;

  rcp=cip->exts[chanservext];

  if (cip->channel) {
    localsetmodeinit(&changes, cip->channel, chanservnick);

    for (cbh=&(cip->channel->bans);*cbh;) {
      if (nickmatchban(sender, *cbh))
	localdosetmode_ban(&changes, bantostring(*cbh), MCB_DEL);
      else
	cbh=&((*cbh)->next);
    }

    localsetmodeflush(&changes, 1);
  }

  cs_log(sender,"UNBANME %s",cip->name->content);
  chanservstdmessage(sender, QM_DONE);
  return CMD_OK;
}     
      
int csc_doclearchan(void *source, int cargc, char **cargv) {
  nick *sender=source;
  regchan *rcp;
  chanindex *cip;
  modechanges changes;
  
  if (cargc<1) {
    chanservstdmessage(sender, QM_NOTENOUGHPARAMS, "clearchan");
    return CMD_ERROR;
  }

  if (!(cip=cs_checkaccess(sender, cargv[0], CA_MASTERPRIV, NULL, "clearchan",0, 0)))
    return CMD_ERROR;

  rcp=cip->exts[chanservext];

  if (cip->channel) {
    localsetmodeinit(&changes, cip->channel, chanservnick);
    localdosetmode_key(&changes, NULL, MCB_DEL);
    localdosetmode_simple(&changes, 0, cip->channel->flags);
    cs_docheckchanmodes(cip->channel, &changes);
    localsetmodeflush(&changes, 1);
  }

  cs_log(sender,"CLEARCHAN %s",cip->name->content);
  chanservstdmessage(sender, QM_DONE);
  return CMD_OK;
}     
      
int csc_dorecover(void *source, int cargc, char **cargv) {
  nick *sender=source,*np;
  reguser *rup;
  regchanuser *rcup;
  regchan *rcp;
  chanindex *cip;
  unsigned long *lp;
  int i;
  modechanges changes;
  
  if (cargc<1) {
    chanservstdmessage(sender, QM_NOTENOUGHPARAMS, "recover");
    return CMD_ERROR;
  }

  if (!(cip=cs_checkaccess(sender, cargv[0], CA_MASTERPRIV, NULL, "recover",0, 0)))
    return CMD_ERROR;

  rcp=cip->exts[chanservext];

  if (cip->channel) {
    localsetmodeinit(&changes, cip->channel, chanservnick);

    /* clearchan */
    localdosetmode_key(&changes, NULL, MCB_DEL);
    localdosetmode_simple(&changes, 0, cip->channel->flags);
    cs_docheckchanmodes(cip->channel, &changes);

    /* unbanall */
    while (cip->channel->bans) {
      localdosetmode_ban(&changes, bantostring(cip->channel->bans), MCB_DEL);
    }

    /* deopall */
    for (i=0,lp=cip->channel->users->content;
	 i<cip->channel->users->hashsize;i++,lp++) {
      if (*lp!=nouser && (*lp & CUMODE_OP)) {
	if (!(np=getnickbynumeric(*lp)) || 
	    (!IsService(np) && (!(rup=getreguserfromnick(np)) || 
	    !(rcup=findreguseronchannel(rcp, rup)) || !(CUHasOpPriv(rcup)) ||
	    !(CUIsProtect(rcup) || CIsProtect(rcp))))) {
	  localdosetmode_nick(&changes, np, MC_DEOP);
	}
      }
    }

    localsetmodeflush(&changes, 1);
  }

  cs_log(sender,"RECOVER %s",cip->name->content);
  chanservstdmessage(sender, QM_DONE);
  return CMD_OK;
}     

int csc_dopermban(void *source, int cargc, char **cargv) {
  nick *sender=source;
  chanindex *cip;
  regban *rbp;
  regchan *rcp;
  reguser *rup=getreguserfromnick(sender);

  if (cargc<2) {
    chanservstdmessage(sender, QM_NOTENOUGHPARAMS, "permban");
    return CMD_ERROR;
  }

  if (!(cip=cs_checkaccess(sender, cargv[0], CA_MASTERPRIV, NULL, "permban",0, 0)))
    return CMD_ERROR;

  rcp=cip->exts[chanservext];

  rbp=getregban();
  rbp->ID=++lastbanID;
  rbp->cbp=makeban(cargv[1]);
  rbp->setby=rup->ID;
  rbp->expiry=0;
  if (cargc>2)
    rbp->reason=getsstring(cargv[2],200);
  else
    rbp->reason=NULL;
  rbp->next=rcp->bans;
  rcp->bans=rbp;

  cs_setregban(cip, rbp);
  csdb_createban(rcp, rbp);
  
  chanservstdmessage(sender, QM_DONE);
  return CMD_OK;
}
  
int csc_dotempban(void *source, int cargc, char **cargv) {
  nick *sender=source;
  chanindex *cip;
  regban *rbp;
  regchan *rcp;
  reguser *rup=getreguserfromnick(sender);
  unsigned int duration;

  if (cargc<3) {
    chanservstdmessage(sender, QM_NOTENOUGHPARAMS, "tempban");
    return CMD_ERROR;
  }

  if (!(cip=cs_checkaccess(sender, cargv[0], CA_MASTERPRIV, NULL, "tempban",0, 0)))
    return CMD_ERROR;

  rcp=cip->exts[chanservext];

  duration=durationtolong(cargv[2]);
  
  rbp=getregban();
  rbp->ID=++lastbanID;
  rbp->cbp=makeban(cargv[1]);
  rbp->setby=rup->ID;
  rbp->expiry=time(NULL)+duration;
  if (cargc>3)
    rbp->reason=getsstring(cargv[3],200);
  else
    rbp->reason=NULL;
  rbp->next=rcp->bans;
  rcp->bans=rbp;

  cs_setregban(cip, rbp);
  csdb_createban(rcp, rbp);
  
  chanservstdmessage(sender, QM_DONE);
  return CMD_OK;
}

int csc_dobanlist(void *source, int cargc, char **cargv) {
  nick *sender=source;
  chanindex *cip;
  regchan *rcp;
  regban *rbp;
  reguser *rup;
  chanban *cbp;
  int i=0;
  int exp;

  if (cargc<1) {
    chanservstdmessage(sender, QM_NOTENOUGHPARAMS, "banlist");
    return CMD_ERROR;
  }
  
  if(!(cip=cs_checkaccess(sender, cargv[0], CA_OPPRIV, NULL, "banlist", 0, 0)))
    return CMD_ERROR;

  rcp=cip->exts[chanservext];
  
  if (rcp->bans || cip->channel->bans) {
    chanservstdmessage(sender, QM_REGBANHEADER, cip->name->content);
    for(rbp=rcp->bans;rbp;rbp=rbp->next) {
      rup=findreguserbyID(rbp->setby);
      chanservsendmessage(sender," #%-2d %-29s %-18s %-15s  %s",++i,bantostring(rbp->cbp),
			  rbp->expiry?longtoduration(rbp->expiry-time(NULL),0):"Permanent",
			  rup?rup->username:"<unknown>",
			  rbp->reason?rbp->reason->content:"");
    }
    for (cbp=cip->channel->bans;cbp;cbp=cbp->next) {
      for (rbp=rcp->bans;rbp;rbp=rbp->next) {
	if (banequal(rbp->cbp, cbp))
	  break;
      }
      if (!rbp) {
	if (rcp->banduration) {
	  exp=(cbp->timeset + rcp->banduration) - time(NULL);
	} else {
	  exp=0;
	} 
	chanservsendmessage(sender, " #%-2d %-29s %-18s %-15s",++i,bantostring(cbp),
			    exp ? longtoduration(exp,0) : "Permanent",
			    "(channel ban)");
      }
    }
    chanservstdmessage(sender, QM_ENDOFLIST);
  } else {
    chanservstdmessage(sender, QM_NOBANS, cip->name->content);
  }
      
  return CMD_OK;
}

int csc_dobandel(void *source, int cargc, char **cargv) {
  nick *sender=source;
  chanindex *cip;
  regban **rbh, *rbp;
  chanban *cbp;
  regchan *rcp;
  chanban *theban=NULL;
  modechanges changes;
  int i,banid=0;

  if (cargc<2) {
    chanservstdmessage(sender, QM_NOTENOUGHPARAMS, "unban");
    return CMD_ERROR;
  }

  if (!(cip=cs_checkaccess(sender, cargv[0], CA_OPPRIV, NULL, "unban", 0, 0)))
    return CMD_ERROR;

  rcp=cip->exts[chanservext];

  /* OK, let's see what they want to remove.. */
  if (*cargv[1]=='#') {
    /* Remove by ID number */
    if (!(banid=strtoul(cargv[1]+1, NULL, 10))) {
      chanservstdmessage(sender, QM_UNKNOWNBAN, cargv[1], cip->name->content);
      return CMD_ERROR;
    }
  } else {
    /* Remove by ban string */
    theban=makeban(cargv[1]);
  }
   
  i=0;
  for (rbh=&(rcp->bans);*rbh;rbh=&((*rbh)->next)) {
    i++;
    if ((banid  && i==banid) ||
	(theban && banequal(theban, (*rbh)->cbp))) {
      /* got it - they will need master access to remove this */
      rbp=*rbh;
      
      if (!cs_checkaccess(sender, NULL, CA_MASTERPRIV, cip, "unban", 0, 0))
	return CMD_ERROR;
      
      chanservstdmessage(sender, QM_REMOVEDPERMBAN, bantostring(rbp->cbp), cip->name->content);
      if (cip->channel) {
	localsetmodeinit(&changes, cip->channel, chanservnick);    
	localdosetmode_ban(&changes, bantostring(rbp->cbp), MCB_DEL);
	localsetmodeflush(&changes, 1);
      }
      
      /* Remove from database */
      csdb_deleteban(rbp);
      /* Remove from list */
      (*rbh)=rbp->next;
      /* Free ban/string and actual regban */
      freesstring(rbp->reason);
      freechanban(rbp->cbp);
      freeregban(rbp);

      if (theban)
	freechanban(theban);

      return CMD_OK;
    }
  }
  
  /* If we've run out of registered bans, let's try channel bans */
  if (cip->channel && cip->channel->bans) {
    for (cbp=cip->channel->bans;cbp;cbp=cbp->next) {
      for (rbp=rcp->bans;rbp;rbp=rbp->next) {
	if (banequal(rbp->cbp,cbp))
	  break;
      }
      
      if (rbp)
	continue;
      
      i++;
      if ((banid  && (i==banid)) ||
	  (theban && banequal(theban, cbp))) {
	/* got it - this is just a channel ban */
	chanservstdmessage(sender, QM_REMOVEDCHANBAN, bantostring(cbp), cip->name->content);
	localsetmodeinit(&changes, cip->channel, chanservnick);
	localdosetmode_ban(&changes, cargv[1], MCB_DEL);
	localsetmodeflush(&changes, 1);

	if (theban)
	  freechanban(theban);
	
	return CMD_OK;
      }
    }
  }
	
  chanservstdmessage(sender, QM_UNKNOWNBAN, cargv[1], cip->name->content);

  return CMD_OK;
} 
  
int csc_dobanclear(void *source, int cargc, char **cargv) {
  nick *sender=source;
  chanindex *cip;
  regban **rbh, *rbp;
  chanban **cbh, *cbp;
  regchan *rcp;
  modechanges changes;
  char *banstr;

  if (cargc<1) {
    chanservstdmessage(sender, QM_NOTENOUGHPARAMS, "banclear");
    return CMD_ERROR;
  }

  if (!(cip=cs_checkaccess(sender, cargv[0], CA_MASTERPRIV, NULL, "banclear", 0, 0)))
    return CMD_ERROR;

  rcp=cip->exts[chanservext];

  if (cip->channel)
    localsetmodeinit(&changes, cip->channel, chanservnick);
    
  for (rbh=&(rcp->bans); *rbh; ) {
    rbp=*rbh;
    banstr=bantostring(rbp->cbp);
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

  if (cip->channel) {
    for (cbh=&(cip->channel->bans); *cbh; ) {
      cbp=*cbh;
      banstr=bantostring(cbp);
      chanservstdmessage(sender, QM_REMOVEDCHANBAN, banstr, cip->name->content);
      localdosetmode_ban(&changes, banstr, MCB_DEL);
    }
    localsetmodeflush(&changes,1);
  }
    
  chanservstdmessage(sender, QM_DONE);
  return CMD_OK;
}
      
int csc_dounbanmask(void *source, int cargc, char **cargv) {
  nick *sender=source;
  chanindex *cip;
  regban **rbh, *rbp;
  chanban **cbh, *cbp;
  regchan *rcp;
  chanban *theban;
  modechanges changes;
  char *banstr;

  if (cargc<2) {
    chanservstdmessage(sender, QM_NOTENOUGHPARAMS, "unbanmask");
    return CMD_ERROR;
  }

  if (!(cip=cs_checkaccess(sender, cargv[0], CA_OPPRIV, NULL, "unbanmask", 0, 0)))
    return CMD_ERROR;

  rcp=cip->exts[chanservext];
  theban=makeban(cargv[1]);

  if (cip->channel)
    localsetmodeinit(&changes, cip->channel, chanservnick);
    
  for (rbh=&(rcp->bans); *rbh; ) {
    rbp=*rbh;
    if (banoverlap(theban, rbp->cbp)) {
      banstr=bantostring(rbp->cbp);
      /* Check perms and remove */
      if (!cs_checkaccess(sender, NULL, CA_MASTERPRIV, cip, NULL, 0, 1)) {
	chanservstdmessage(sender, QM_NOTREMOVEDPERMBAN, banstr, cip->name->content);
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
      if (banoverlap(theban, cbp)) {
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
  
  
  chanservstdmessage(sender, QM_DONE);
  return CMD_OK;
}
      
int csc_dorenchan(void *source, int cargc, char **cargv) {
  nick *sender=source;
  reguser *rup=getreguserfromnick(sender);
  chanindex *cip1,*cip2;
  regchan *rcp;

  if (!rup)
    return CMD_ERROR;

  if (cargc<2) {
    chanservstdmessage(sender, QM_NOTENOUGHPARAMS, "renchan");
    return CMD_ERROR;
  }

  if (!(cip1=findchanindex(cargv[0])) || !(rcp=cip1->exts[chanservext])) {
    chanservstdmessage(sender, QM_UNKNOWNCHAN, cargv[0]);
    return CMD_ERROR;
  }

  if (*cargv[1] != '#') {
    chanservstdmessage(sender, QM_INVALIDCHANNAME, cargv[0]);
    return CMD_ERROR;
  } 

  if (!(cip2=findorcreatechanindex(cargv[1])) || cip2->exts[chanservext]) {
    chanservstdmessage(sender, QM_ALREADYREGISTERED, cip2->name->content);
    return CMD_ERROR;
  }

  cs_log(sender,"RENCHAN %s -> %s",cip1->name->content,cip2->name->content);

  /* Remove from the channel.  Don't bother if the channel doesn't exist. */
  if (!CIsSuspended(rcp) && cip1->channel) {
    CSetSuspended(rcp);    
    chanservjoinchan(cip1->channel);
    CClearSuspended(rcp);
  }
  
  cip1->exts[chanservext]=NULL;
  releasechanindex(cip1);

  cip2->exts[chanservext]=rcp;
  rcp->index=cip2;
  if (cip2->channel) {
    chanservjoinchan(cip2->channel);
  }

  csdb_updatechannel(rcp);
  chanservstdmessage(sender, QM_DONE);

  return CMD_OK;
}

int csc_dochannelcomment(void *source, int cargc, char **cargv) {
  nick *sender=source;
  reguser *rup=getreguserfromnick(sender);
  regchan *rcp;
  chanindex *cip;
  char buf[300];
  int bufpos;

  if (!rup)
    return CMD_ERROR;

  if (cargc<1) {
    chanservstdmessage(sender, QM_NOTENOUGHPARAMS, "channelcomment");
    return CMD_ERROR;
  }

  if (!(cip=findchanindex(cargv[0])) || !(rcp=cip->exts[chanservext])) {
    chanservstdmessage(sender, QM_UNKNOWNCHAN, cargv[0]);
    return CMD_ERROR;
  }

  if (cargc>1) {
    if (!ircd_strcmp(cargv[1],"none")) {
      freesstring(rcp->comment);
      rcp->comment=NULL;
    } else {
      if (*cargv[1]=='+') {
	if (rcp->comment) {
	  strcpy(buf,rcp->comment->content);
	  bufpos=rcp->comment->length;
	  buf[bufpos++]=' ';
	} else {
	  bufpos=0;
	}
	strncpy(buf+bufpos, cargv[1]+1, 250-bufpos);
      } else {
	strncpy(buf, cargv[1], 250);
      }
    
      freesstring(rcp->comment);
      rcp->comment=getsstring(buf,250);
    }
    csdb_updatechannel(rcp);
  }
  
  if (rcp->comment) 
    chanservstdmessage(sender, QM_COMMENT, cip->name->content, rcp->comment->content);
  else
    chanservstdmessage(sender, QM_NOCOMMENT, cip->name->content);

  return CMD_OK;
}

int csc_dorejoin(void *source, int cargc, char **cargv) {
  nick *sender=source;
  chanindex *cip;
  regchan *rcp;

  if (cargc<1) {
    chanservstdmessage(sender, QM_NOTENOUGHPARAMS, "rejoin");
    return CMD_ERROR;
  }

  if (!(cip=findchanindex(cargv[0])) || !(rcp=cip->exts[chanservext])) {
    chanservstdmessage(sender, QM_UNKNOWNCHAN, cargv[0]);
    return CMD_ERROR;
  }

  if (CIsJoined(rcp) && !CIsSuspended(rcp)) {
    CSetSuspended(rcp);
    chanservjoinchan(cip->channel);
    CClearSuspended(rcp);
    chanservjoinchan(cip->channel);
  }

  chanservstdmessage(sender, QM_DONE);

  return CMD_OK;
}

int csc_dochantype(void *source, int cargc, char **cargv) {
  nick *sender=source;
  chanindex *cip;
  regchan *rcp;
  int type;

  if (cargc<1) {
    chanservstdmessage(sender, QM_NOTENOUGHPARAMS, "chantype");
    return CMD_ERROR;
  }

  if (!(cip=findchanindex(cargv[0])) || !(rcp=cip->exts[chanservext])) {
    chanservstdmessage(sender, QM_UNKNOWNCHAN, cargv[0]);
    return CMD_ERROR;
  }

  if (cargc>1) {
    /* Set type */
    for (type=CHANTYPES-1;type;type--) {
      if (!ircd_strcmp(chantypes[type]->content, cargv[1]))
	break;
    }
    if (!type) {
      chanservstdmessage(sender, QM_UNKNOWNCHANTYPE, cargv[1]);
      return CMD_ERROR;
    }
    rcp->chantype=type;

    csdb_updatechannel(rcp);
    chanservstdmessage(sender, QM_DONE);
  }

  chanservstdmessage(sender, QM_CHANTYPEIS, cip->name->content, chantypes[rcp->chantype]->content);

  return CMD_OK;
}

int csc_doadduser(void *source, int cargc, char **cargv) {
  nick *sender=source;
  chanindex *cip;
  regchanuser *rcup;
  regchan *rcp;
  reguser *rup;
  int i;

  if (cargc<2) {
    chanservstdmessage(sender, QM_NOTENOUGHPARAMS, "adduser");
    return CMD_ERROR;
  }

  if (!(cip=cs_checkaccess(sender, cargv[0], CA_MASTERPRIV, NULL, "adduser", QPRIV_CHANGECHANLEV, 0)))
    return CMD_ERROR;

  rcp=cip->exts[chanservext];

  for (i=1;i<cargc;i++) {
    if (!(rup=findreguser(sender, cargv[i])))
      continue;

    if ((rcup=findreguseronchannel(rcp, rup))) {
      chanservstdmessage(sender, QM_ALREADYKNOWNONCHAN, cargv[i], cip->name->content);
      continue;
    }

    rcup=getregchanuser();
    rcup->chan=rcp;
    rcup->user=rup;
    rcup->flags = QCUFLAG_OP | QCUFLAG_AUTOOP | QCUFLAG_TOPIC;
    rcup->changetime=time(NULL);
    rcup->usetime=0;
    rcup->info=NULL;
   
    cs_log(sender,"CHANLEV %s #%s +aot (+ -> +aot)",cip->name->content,rup->username);
    addregusertochannel(rcup);
    csdb_createchanuser(rcup);
  }

  rcp->status |= QCSTAT_OPCHECK;
  cs_timerfunc(cip);

  chanservstdmessage(sender, QM_DONE);

  return CMD_OK;
}

int csc_doremoveuser(void *source, int cargc, char **cargv) {
  nick *sender=source;
  chanindex *cip;
  regchanuser *rcup;
  regchan *rcp;
  reguser *rup;
  int isowner=0;
  int i;

  if (cargc<2) {
    chanservstdmessage(sender, QM_NOTENOUGHPARAMS, "removeuser");
    return CMD_ERROR;
  }

  if (!(cip=cs_checkaccess(sender, cargv[0], CA_MASTERPRIV, NULL, "adduser", QPRIV_CHANGECHANLEV, 0)))
    return CMD_ERROR;

  if (cs_checkaccess(sender, NULL, CA_OWNERPRIV, cip, "adduser", QPRIV_CHANGECHANLEV, 1))
    isowner=1;

  rcp=cip->exts[chanservext];

  for (i=1;i<cargc;i++) {
    if (!(rup=findreguser(sender, cargv[i])))
      continue;

    if (!(rcup=findreguseronchannel(rcp, rup))) {
      chanservstdmessage(sender, QM_CHANUSERUNKNOWN, cargv[i], cip->name->content);
      continue;
    }

    if (CUIsOwner(rcup)) {
      chanservstdmessage(sender, QM_CANNOTREMOVEOWNER, cargv[i], cip->name->content);
      continue;
    }

    if (CUIsMaster(rcup) && !isowner && (rup != getreguserfromnick(sender))) {
      chanservstdmessage(sender, QM_CANNOTREMOVEMASTER, cargv[i], cip->name->content);
      continue;
    }
    
    cs_log(sender,"CHANLEV %s #%s -%s (%s -> +)",cip->name->content,rup->username,
	   printflags_noprefix(rcup->flags, rcuflags), printflags(rcup->flags, rcuflags));

    csdb_deletechanuser(rcup);
    delreguserfromchannel(rcp, rup);
  }

  rcp->status |= QCSTAT_OPCHECK;
  cs_timerfunc(cip);

  chanservstdmessage(sender, QM_DONE);

  return CMD_OK;
}

int csc_dochanstat(void *source, int cargc, char **cargv) {
  nick *sender=source;
  chanindex *cip;
  regchan *rcp;
  char timebuf[30];
  
  if (cargc<1) {
    chanservstdmessage(sender, QM_NOTENOUGHPARAMS, "chanstat");
    return CMD_ERROR;
  }

  if (!(cip=cs_checkaccess(sender, cargv[0], CA_MASTERPRIV,
			   NULL, "chanstat", QPRIV_VIEWFULLCHANLEV, 0)))
    return CMD_ERROR;
  
  rcp=cip->exts[chanservext];

  chanservstdmessage(sender, QM_STATSHEADER, cip->name->content);

  strftime(timebuf, 30, "%d/%m/%y %H:%M", localtime(&(rcp->created)));
  chanservstdmessage(sender, QM_STATSADDED, timebuf);

  /* Show opers founder/addedby/type info */
  if (cs_privcheck(QPRIV_VIEWFULLCHANLEV, sender)) {
    reguser *founder=NULL, *addedby=NULL;

    strftime(timebuf, 30, "%d/%m/%y %H:%M", localtime(&(rcp->lastactive)));
    chanservstdmessage(sender, QM_STATSLASTACTIVE, timebuf);


    addedby=findreguserbyID(rcp->addedby);
    chanservstdmessage(sender, QM_ADDEDBY, addedby ? addedby->username : "(unknown)");
    founder=findreguserbyID(rcp->founder);
    chanservstdmessage(sender, QM_FOUNDER, founder ? founder->username : "(unknown)");      
    chanservstdmessage(sender, QM_CHANTYPE, chantypes[rcp->chantype]->content);
  }

  strftime(timebuf, 30, "%d/%m/%y %H:%M", localtime(&(rcp->created)));

  chanservstdmessage(sender, QM_STATSJOINS, timebuf, rcp->maxusers, rcp->totaljoins, 
		     (float)rcp->totaljoins/ ((time(NULL)-rcp->created)/(3600*24)));

  strftime(timebuf, 30, "%d/%m/%y %H:%M", localtime(&(rcp->statsreset)));

  chanservstdmessage(sender, QM_STATSJOINS, timebuf, rcp->tripusers, rcp->tripjoins, 
		     (float)rcp->tripjoins / ((time(NULL)-rcp->statsreset)/(3600*24)));

  return CMD_OK;
}
  

  
  
