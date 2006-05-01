/* usercmds.c */

#include "chanserv.h"
#include "../lib/irc_string.h"

#include <stdio.h>
#include <string.h>

int csu_douserflags(void *source, int cargc, char **cargv);
int csu_doinfo(void *source, int cargc, char **cargv);
int csu_dowhois(void *source, int cargc, char **cargv);
int csu_dousercomment(void *source, int cargc, char **cargv);
int csu_dodeluser(void *source, int cargc, char **cargv);
int csu_dolanguage(void *source, int cargc, char **cargv);
int csu_dosuspenduser(void *source, int cargc, char **cargv);
int csu_dounsuspenduser(void *source, int cargc, char **cargv);
int csu_dospewdb(void *source, int cargc, char **cargv);
int csu_dospewpass(void *source, int cargc, char **cargv);
int csu_dospewemail(void *source, int cargc, char **cargv);
int csu_dolistflags(void *source, int cargc, char **cargv);
int csu_dosuspenduserlist(void *source, int cargc, char **cargv);

void _init() {
  chanservaddcommand("userflags"      ,   QCMD_AUTHED, 2, csu_douserflags      ,"Shows or changes user flags.");
  chanservaddcommand("info"           ,   QCMD_AUTHED, 2, csu_doinfo           ,"Shows or changes info line.");
  chanservaddcommand("whois"          ,   QCMD_AUTHED, 1, csu_dowhois          ,"Displays information about a user.");
  chanservaddcommand("usercomment"    ,   QCMD_OPER  , 2, csu_dousercomment    ,"Shows or changes staff comment for a user.");
  chanservaddcommand("deluser"        ,   QCMD_OPER  , 2, csu_dodeluser        ,"Removes a user from the bot.");
  chanservaddcommand("language"       ,   QCMD_AUTHED, 1, csu_dolanguage       ,"Shows or changes your current language.");
  chanservaddcommand("suspenduser"    ,   QCMD_OPER  , 1, csu_dosuspenduser    ,"Suspend/Delay GLINE/Instantly GLINE a user.");
  chanservaddcommand("unsuspenduser"  ,   QCMD_OPER  , 1, csu_dounsuspenduser  ,"Unsuspend a user.");
  chanservaddcommand("spewdb"         ,   QCMD_OPER  , 1, csu_dospewdb         ,"Search for a user in the database.");
  chanservaddcommand("spewpass"       ,   QCMD_OPER  , 1, csu_dospewpass       ,"Search for a password in the database.");
  chanservaddcommand("spewemail"      ,   QCMD_OPER  , 1, csu_dospewemail      ,"Search for an e-mail in the database.");
  chanservaddcommand("listflags"      ,   QCMD_OPER  , 1, csu_dolistflags      ,"List users with the specified user flags.");
  chanservaddcommand("suspenduserlist",   QCMD_HELPER, 1, csu_dosuspenduserlist,"Lists suspended/locked users.");
}

void _fini() {
  chanservremovecommand("userflags",       csu_douserflags);
  chanservremovecommand("info",            csu_doinfo);
  chanservremovecommand("whois",           csu_dowhois);
  chanservremovecommand("usercomment",     csu_dousercomment);
  chanservremovecommand("deluser",         csu_dodeluser);
  chanservremovecommand("language",        csu_dolanguage);
  chanservremovecommand("suspenduser",     csu_dosuspenduser);
  chanservremovecommand("unsuspenduser",   csu_dounsuspenduser);
  chanservremovecommand("spewdb",          csu_dospewdb);
  chanservremovecommand("spewpass",        csu_dospewpass);
  chanservremovecommand("spewemail",       csu_dospewemail);
  chanservremovecommand("listflags",       csu_dolistflags);
  chanservremovecommand("suspenduserlist", csu_dosuspenduserlist);
}

int csu_douserflags(void *source, int cargc, char **cargv) {
  nick *sender=source;
  reguser *rup=getreguserfromnick(sender), *target;
  int arg=0;
  flag_t flagmask, changemask;
  char flagbuf[30];

  if (!rup)
    return CMD_ERROR;
  
  if (cargc<1) {
    chanservstdmessage(sender, QM_NOTENOUGHPARAMS, "userflags");
    return CMD_ERROR;
  }

  if (*cargv[0]!='+' && *cargv[0]!='-') {
    arg++;
    /* If the first char isn't a "change" character, it must specify a target */

    if (!(target=findreguser(sender,cargv[0])))
      return CMD_ERROR;

    if (target!=rup && !cs_privcheck(QPRIV_VIEWUSERFLAGS, sender)) {
      chanservstdmessage(sender, QM_NOACCESS, "userflags");
      return CMD_ERROR;
    }
  } else {
    target=rup;
  }
    
  if (cargc>arg) {
    /* OK, now we have a changestring.. */
    if (target!=rup && !cs_privcheck(QPRIV_CHANGEUSERFLAGS, sender)) {
      chanservstdmessage(sender, QM_NOACCESS, "userflags");
      return CMD_ERROR;
    }

    strcpy(flagbuf,printflags(target->flags, ruflags));

    changemask=QUFLAG_NOTICE | QUFLAG_INFO;

    if (target==rup) {
      /* If you're changing yourself, you can give up the "status" flags and add/remove notice */
      changemask|=(target->flags & (QUFLAG_OPER | QUFLAG_DEV | QUFLAG_PROTECT | QUFLAG_HELPER | QUFLAG_ADMIN));
    }

    /* Warning, policy ahead */

    if (UHasOperPriv(rup))
      changemask |= QUFLAG_GLINE | QUFLAG_DELAYEDGLINE | QUFLAG_RESTRICTED | QUFLAG_PROTECT;

    if (UHasAdminPriv(rup))
      changemask |= (QUFLAG_OPER | QUFLAG_HELPER);
    
    if (UIsDev(rup))
      changemask=QUFLAG_ALL;
    
    setflags(&target->flags, changemask, cargv[arg], ruflags, REJECT_NONE);
    
    /* More policy */
    if (!UHasHelperPriv(target)) {
      target->flags &= ~QUFLAG_PROTECT;
    }

    cs_log(sender,"USERFLAGS #%s %s (%s -> %s)",target->username,cargv[arg],flagbuf,printflags(target->flags, ruflags));
    csdb_updateuser(target);
    chanservstdmessage(sender, QM_DONE);
  }

  if (cs_privcheck(QPRIV_VIEWUSERFLAGS, sender))
    flagmask=QUFLAG_ALL;
  else
    flagmask=QUFLAG_INFO | QUFLAG_NOTICE | QUFLAG_OPER | QUFLAG_HELPER | QUFLAG_DEV | QUFLAG_ADMIN;
  
  chanservstdmessage(sender, QM_CURUSERFLAGS, target->username, printflags(target->flags & flagmask, ruflags));

  return CMD_OK;
}

int csu_doinfo(void *source, int cargc, char **cargv) {
  nick *sender=source;
  reguser *rup=getreguserfromnick(sender);
  chanindex *cip;
  regchan *rcp;
  regchanuser *rcup;
  char linebuf[INFOLEN+10];
  char *newline="";
  int doupdate=0;

  if (cargc==0 || *cargv[0]!='#') {
    /* Global info line */
    if (cargc==1) {
      /* Setting to either one word or "none" */
      if (!ircd_strcmp(cargv[0],"none")) {
	newline="";
	doupdate=1;
      } else {
	newline=cargv[0];
	doupdate=1;
      }
    } else if (cargc>1) {
      /* More than one word: we need to stick them back together */
      snprintf(linebuf,INFOLEN,"%s %s",cargv[0],cargv[1]);
      newline=linebuf;
      doupdate=1;
    }

    if (doupdate) {
      if (rup->info)
	freesstring(rup->info);
      
      rup->info=getsstring(newline, INFOLEN);

      chanservstdmessage(sender, QM_DONE);
      csdb_updateuser(rup);
    }
    
    chanservstdmessage(sender, QM_GLOBALINFO, rup->info?rup->info->content:"(none)");
  } else {
    /* Channel info line */

    if (!(cip=findchanindex(cargv[0])) || !(rcp=cip->exts[chanservext]) || 
	(CIsSuspended(rcp) && !cs_privcheck(QPRIV_SUSPENDBYPASS, sender))) {
      chanservstdmessage(sender, QM_UNKNOWNCHAN, cargv[0]);
      return CMD_ERROR;
    }
    
    if ((!(rcup=findreguseronchannel(rcp, rup)) || !CUHasVoicePriv(rcup))) {
      chanservstdmessage(sender, QM_NOACCESSONCHAN, cargv[0], "info");
      return CMD_ERROR;
    }

    if (cargc>1) {
      if (rcup->info) 
	freesstring(rcup->info);

      if (!ircd_strcmp(cargv[1],"none")) 
	rcup->info=NULL;
      else	
	rcup->info=getsstring(cargv[1],INFOLEN);
      
      csdb_updatechanuser(rcup);
      chanservstdmessage(sender, QM_DONE);
    }

    chanservstdmessage(sender, QM_CHANNELINFO, cip->name->content, 
		       rcup->info?rcup->info->content:"(none)");
  }

  return CMD_OK;
}

int csu_dowhois(void *source, int cargc, char **cargv) {
  nick *sender=source;
  reguser *rup=getreguserfromnick(sender), *target;
  char buf[200];
  char nbpos=0;
  nicklist *nlp;
  struct tm *tmp;
  regchanuser *rcup, *rcup2;
  flag_t flagmask, flags;
  int doneheader=0;

  if (!(rup=getreguserfromnick(sender)))
    return CMD_ERROR;
  
  if (cargc<1) {
    chanservstdmessage(sender, QM_NOTENOUGHPARAMS, "whois");
    return CMD_ERROR;
  }
  
  if (!(target=findreguser(sender, cargv[0]))) {
    nick* np;
    
    if ((np=getnickbynick(cargv[0]))) {
      activeuser* aup=getactiveuserfromnick(np);
      chanservsendmessage(sender, "%s has attempted to auth %d time%s.", np->nick, aup->authattempts, 
        aup->authattempts==1?"":"s");
    }
    return CMD_ERROR;
  }

  if (cargv[0][0]=='#') {
    chanservstdmessage(sender, QM_WHOISHEADER_AUTH, target->username);
  } else {
    chanservstdmessage(sender, QM_WHOISHEADER_NICK, cargv[0], target->username);
  }

  if (rup==target || cs_privcheck(QPRIV_VIEWFULLWHOIS, sender)) {
    chanservstdmessage(sender, QM_WHOIS_USERID, target->ID);
  }

  if (cs_privcheck(QPRIV_VIEWUSERFLAGS, sender)) {
    flagmask=QUFLAG_ALL;
  } else {  
    if (UIsAdmin(target))
      chanservstdmessage(sender, QM_USERISADMIN, target->username);
    else if (UIsOper(target))
      chanservstdmessage(sender, QM_USERISOPER, target->username);
    else if (UIsHelper(target))
      chanservstdmessage(sender, QM_USERISHELPER, target->username);
    
    if (UIsDev(target))
      chanservstdmessage(sender, QM_USERISDEV, target->username);

    flagmask=0;
  }

  if (rup==target)
    flagmask|=(QUFLAG_OPER | QUFLAG_DEV | QUFLAG_HELPER | 
	       QUFLAG_ADMIN | QUFLAG_INFO | QUFLAG_NOTICE);

  if (flagmask & target->flags)
    chanservstdmessage(sender, QM_WHOIS_FLAGS, printflags(flagmask & target->flags, ruflags));

  if (!target->nicks) {
    chanservstdmessage(sender, QM_WHOIS_USERS, "(none)");
  } else {
    for (nlp=target->nicks; ;nlp=nlp->next) {
      if (nbpos>0 && (!nlp || nbpos+strlen(nlp->np->nick) > 60)) {
	chanservstdmessage(sender, QM_WHOIS_USERS, buf);
	nbpos=0;
      }

      if (!nlp)
	break;

      nbpos+=sprintf(buf+nbpos,"%s ",nlp->np->nick);
    }
  }

  if (target->created) {
    tmp=gmtime(&(target->created));
    strftime(buf,15,"%d/%m/%y %H:%M",tmp);
    
    chanservstdmessage(sender, QM_WHOIS_CREATED, buf);
  }

  tmp=gmtime(&(target->lastauth));
  strftime(buf,15,"%d/%m/%y %H:%M",tmp);

  chanservstdmessage(sender, QM_WHOIS_LASTAUTH, buf);
  
  if (target->lastuserhost && (rup==target || cs_privcheck(QPRIV_VIEWFULLWHOIS, sender))) {
    chanservstdmessage(sender, QM_WHOIS_USERLANG, cslanguages[target->languageid] ?
		       cslanguages[target->languageid]->name->content : "(unknown)");
    chanservstdmessage(sender, QM_WHOIS_LASTUSERHOST, target->lastuserhost->content);
  }

  if (target->email && (rup==target || cs_privcheck(QPRIV_VIEWEMAIL, sender))) {
    chanservstdmessage(sender, QM_WHOIS_EMAIL, target->email->content);

    tmp=gmtime(&(target->lastemailchange));
    strftime(buf,15,"%d/%m/%y %H:%M",tmp);
    
    chanservstdmessage(sender, QM_WHOIS_EMAILSET, buf);    
  }

  if (target->info) {
    chanservstdmessage(sender, QM_WHOIS_INFO, target->info->content);
  }

  if (target->comment && (cs_privcheck(QPRIV_VIEWCOMMENTS, sender))) {
    chanservstdmessage(sender, QM_WHOIS_COMMENT, target->comment->content);
  }
  
  for (rcup=target->knownon;rcup;rcup=rcup->nextbyuser) {
    if (!UHasHelperPriv(rup)) {
      if (!(rcup2=findreguseronchannel(rcup->chan,rup)))
	continue;
      
      if (!CUHasVoicePriv(rcup2))
	continue;
      
      flagmask = (QCUFLAG_OWNER | QCUFLAG_MASTER | QCUFLAG_OP | QCUFLAG_VOICE | QCUFLAG_AUTOVOICE | 
		  QCUFLAG_AUTOOP | QCUFLAG_TOPIC | QCUFLAG_SPAMCON);
      
      if (CUHasMasterPriv(rcup2))
	flagmask |= (QCUFLAG_DENY | QCUFLAG_QUIET | QCUFLAG_BANNED);
    } else {
      flagmask=QCUFLAG_ALL;
    }

    if (!(flags=rcup->flags & flagmask)) 
      continue;

    if (!doneheader) {
      doneheader=1;
      chanservstdmessage(sender, QM_WHOIS_CHANHEADER, target->username);
    }

    chanservsendmessage(sender, " %-30s %s",rcup->chan->index->name->content,printflags(flags, rcuflags));
  }

  chanservstdmessage(sender, QM_ENDOFLIST);

  return CMD_OK;
}

int csu_dodeluser(void *source, int cargc, char **cargv) {
  nick *sender=source;
  reguser *rup=getreguserfromnick(sender), *target;

  if (!rup)
    return CMD_ERROR;
  
  if (cargc<1) {
    chanservstdmessage(sender, QM_NOTENOUGHPARAMS, "deluser");
    return CMD_ERROR;
  }

  if (!(target=findreguser(sender, cargv[0])))
    return CMD_ERROR;
  
  cs_log(sender,"DELUSER %s (%s)",target->username,cargc>1?cargv[1]:"");
  cs_removeuser(target);

  chanservstdmessage(sender, QM_DONE);

  return CMD_OK;
}

int csu_dousercomment(void *source, int cargc, char **cargv) {
  nick *sender=source;
  reguser *rup=getreguserfromnick(sender), *target;
  char buf[300];
  int bufpos;

  if (!rup)
    return CMD_ERROR;

  if (cargc<1) {
    chanservstdmessage(sender, QM_NOTENOUGHPARAMS, "usercomment");
    return CMD_ERROR;
  }

  if (!(target=findreguser(sender, cargv[0])))
    return CMD_ERROR;

  if (cargc>1) {
    if (!ircd_strcmp(cargv[1],"none")) {
      freesstring(target->comment);
      target->comment=NULL;
    } else {
      if (*cargv[1]=='+') {
	if (target->comment) {
	  strcpy(buf,target->comment->content);
	  bufpos=target->comment->length;
	  buf[bufpos++]=' ';
	} else {
	  bufpos=0;
	}
	strncpy(buf+bufpos, cargv[1]+1, 250-bufpos);
      } else {
	strncpy(buf, cargv[1], 250);
      }
    
      freesstring(target->comment);
      target->comment=getsstring(buf,250);
    }
    csdb_updateuser(target);
  }
  
  if (target->comment) 
    chanservstdmessage(sender, QM_COMMENT, target->username, target->comment->content);
  else
    chanservstdmessage(sender, QM_NOCOMMENT, target->username);

  return CMD_OK;
}

int csu_dolanguage(void *source, int cargc, char **cargv) {
  nick *sender=source;
  reguser *rup=getreguserfromnick(sender);
  char buf[300];
  int bufpos=0;
  int i;
  int len;

  if (!rup)
    return CMD_ERROR;

  if (cargc==0) {
    /* Display language */
    i=rup->languageid;
    chanservstdmessage(sender, QM_YOURLANGUAGE, cslanguages[i] ? cslanguages[i]->name->content : "Unknown");
    
    /* Display available lanaguages */
    chanservstdmessage(sender, QM_LANGUAGELIST);
    
    for (i=0;i<MAXLANG;i++) {
      if (cslanguages[i]) {
	if (bufpos > 70) {
	  chanservsendmessage(sender, "%s", buf);
	  bufpos=0;
	}
	len=sprintf(buf+bufpos, "%.14s (%.2s)",cslanguages[i]->name->content,cslanguages[i]->code);
	memset(buf+bufpos+len,' ',20-len);
	bufpos+=20;
	buf[bufpos]='\0';
      }
    }

    if (bufpos) 
      chanservsendmessage(sender, "%s", buf);

    chanservstdmessage(sender, QM_ENDOFLIST);
  } else {
    /* Set language */
    for (i=0;i<MAXLANG;i++) {
      if (cslanguages[i] && !ircd_strcmp(cargv[0],cslanguages[i]->code)) {
	/* Match. */
	rup->languageid=i;
	csdb_updateuser(rup);
	
	chanservstdmessage(sender, QM_DONE);
	chanservstdmessage(sender, QM_YOURLANGUAGE, cslanguages[i]->name->content);
	break;
      }
    }

    if (i==MAXLANG)
      chanservstdmessage(sender, QM_UNKNOWNLANGUAGE, cargv[0]);
  }
  
  return CMD_OK;
}

int csu_dosuspenduser(void *source, int cargc, char **cargv) {
  nick *sender=source;
  reguser *rup=getreguserfromnick(sender);
  reguser *vrup;
  char* flag;
  char* victim;
  char* dur_p;
  char* reason;
  int kill=1, gline=0, email=0, password=0, hitcount=0;
  time_t expires=0;
  int duration=0;
  struct tm* tmp;
  char buf[200]="";
  int dgwait;
  
  if (!rup)
    return CMD_ERROR;
  
  if (cargc < 1) {
    chanservstdmessage(sender, QM_NOTENOUGHPARAMS, "suspenduser");
    return CMD_ERROR;
  }
  
  if (cargv[0][0] == '-') {
    flag=cargv[0];
    if (!(victim=strchr(flag, ' '))) {
      chanservstdmessage(sender, QM_NOTENOUGHPARAMS, "suspenduser");
      return CMD_ERROR;
    }
    *(victim++)='\0';
    if (!(dur_p=strchr(victim, ' '))) {
      chanservstdmessage(sender, QM_NOTENOUGHPARAMS, "suspenduser");
      return CMD_ERROR;
    }
    *(dur_p++)='\0';
    if ((reason=strchr(dur_p, ' '))) {
      *(reason++)='\0';
      if ((duration=durationtolong(dur_p))) {
        if ((duration < 86400) || (duration > 2592000)) {
          chanservstdmessage(sender, QM_INVALIDDURATION);
          return CMD_ERROR;
        }
        expires=time(0)+duration;
      }
      else {
        *(reason-1)=' ';
        reason=dur_p;
        expires=0;
      }
    }
    else {
      reason=dur_p;
      expires=0;
    }
    
    if (!ircd_strcmp(flag, "-nokill")) {
      kill=0;
    }
    else if (!ircd_strcmp(flag, "-gline")) {
      gline=1;
    }
    else if (!ircd_strcmp(flag, "-instantgline")) {
      gline=2;
    }
    else if (!ircd_strcmp(flag, "-email")) {
      email=1;
    }
    else if (!ircd_strcmp(flag, "-password")) {
      password=1;
    }
    else {
      chanservstdmessage(sender, QM_INVALIDCHANLEVCHANGE);
      return CMD_ERROR;
    }
  }
  else {
    victim=cargv[0];
    if (!(dur_p=strchr(victim, ' '))) {
      chanservstdmessage(sender, QM_NOTENOUGHPARAMS, "suspenduser");
      return CMD_ERROR;
    }
    *(dur_p++)='\0';
    if ((reason=strchr(dur_p, ' '))) {
      *(reason++)='\0';
      if ((duration=durationtolong(dur_p))) {
        if ((duration < 86400) || (duration > 2592000)) {
          chanservstdmessage(sender, QM_INVALIDDURATION);
          return CMD_ERROR;
        }
        expires=time(0)+duration;
      }
      else {
        *(reason-1)=' ';
        reason=dur_p;
        expires=0;
      }
    }
    else {
      reason=dur_p;
      expires=0;
    }
  }
  
  if (expires) {
    tmp=gmtime(&expires);
    strftime(buf,15,"%d/%m/%y %H:%M",tmp);
  }
  
  if (email) {
    int i;
    
    for (i=0;i<REGUSERHASHSIZE;i++) {
      for (vrup=regusernicktable[i]; vrup; vrup=vrup->nextbyname) {
        if (!ircd_strcmp(vrup->email->content, victim)) {
          if (UHasSuspension(vrup))
            continue;
          
          if (UHasOperPriv(vrup) && !UHasAdminPriv(rup))
            continue;
          
          hitcount++;
          vrup->flags|=QUFLAG_SUSPENDED;
          vrup->suspendby=rup->ID;
          vrup->suspendexp=expires;
          vrup->suspendreason=getsstring(reason, strlen(reason)+1);
          
          while (vrup->nicks) {
            if (!vrup->nicks->np)
              continue;

            chanservstdmessage(sender, QM_DISCONNECTINGUSER, vrup->nicks->np->nick, vrup->username);
            chanservkillstdmessage(vrup->nicks->np, QM_SUSPENDKILL);
          }
          csdb_updateuser(vrup);
        }
      }
    }
    
    chanservwallmessage("%s (%s) bulk suspended <%s>, hit %d account/s (expires: %s)", sender->nick, rup->username, victim, hitcount, expires?buf:"never");
  }
  else if (password) {
    int i;
    
    for (i=0;i<REGUSERHASHSIZE;i++) {
      for (vrup=regusernicktable[i]; vrup; vrup=vrup->nextbyname) {
        if (!strcmp(vrup->password, victim)) {
          if (UHasSuspension(vrup))
            continue;
          
          if (UHasOperPriv(vrup) && !UHasAdminPriv(rup))
            continue;
          
          hitcount++;
          vrup->flags|=QUFLAG_SUSPENDED;
          vrup->suspendby=rup->ID;
          vrup->suspendexp=expires;
          vrup->suspendreason=getsstring(reason, strlen(reason)+1);
          
          while (vrup->nicks) {
            if (!vrup->nicks->np)
              continue;

            chanservstdmessage(sender, QM_DISCONNECTINGUSER, vrup->nicks->np->nick, vrup->username);
            chanservkillstdmessage(vrup->nicks->np, QM_SUSPENDKILL);
          }
          csdb_updateuser(vrup);
        }
      }
    }
    
    chanservwallmessage("%s (%s) bulk suspended password \"%s\", hit %d account/s (expires: %s)", sender->nick, rup->username, victim, hitcount, expires?buf:"never");
  }
  else {
    if (!(vrup=findreguser(sender, victim)))
      return CMD_ERROR;
    
    if (!ircd_strcmp(vrup->username, rup->username)) {
      chanservsendmessage(sender, "You can't suspend yourself, silly.");
      return CMD_ERROR;
    }
    
    if (UHasSuspension(vrup)) {
      chanservstdmessage(sender, QM_USERALREADYSUSPENDED);
      return CMD_ERROR;
    }
    
    if (UHasOperPriv(vrup) && !UHasAdminPriv(rup)) {
      snprintf(buf, 199, "suspenduser on %s", vrup->username);
      chanservstdmessage(sender, QM_NOACCESS, buf);
      chanservwallmessage("%s (%s) FAILED to suspend %s", sender->nick, rup->username, vrup->username);
      return CMD_ERROR;
    }
    
    if (gline == 2)
      vrup->flags|=QUFLAG_GLINE;
    else if (gline == 1)
      vrup->flags|=QUFLAG_DELAYEDGLINE;
    else
      vrup->flags|=QUFLAG_SUSPENDED;
    vrup->suspendby=rup->ID;
    vrup->suspendexp=expires;
    vrup->suspendreason=getsstring(reason, strlen(reason)+1);
    
    chanservwallmessage("%s (%s) %s %s (expires: %s)", sender->nick, rup->username, (gline)?((gline == 2)?"instantly glined":"delayed glined"):"suspended", vrup->username, expires?buf:"never");
    if (gline) {
      dgwait=(gline==2)?0:rand()%900;
      chanservsendmessage(sender, "Scheduling delayed GLINE for account %s in %d %s", 
        vrup->username, (dgwait>60)?(dgwait/60):dgwait, (dgwait>60)?"minutes":"seconds");
      deleteschedule(NULL, &chanservdgline, (void*)vrup);
      scheduleoneshot(time(NULL)+dgwait, &chanservdgline, (void*)vrup);
    }
    else if (kill) {
      while (vrup->nicks) {
        if (!vrup->nicks->np)
          continue;
        
        chanservstdmessage(sender, QM_DISCONNECTINGUSER, vrup->nicks->np->nick, vrup->username);
        chanservkillstdmessage(vrup->nicks->np, QM_SUSPENDKILL);
        hitcount++;
      }
    }

    csdb_updateuser(vrup);
  }
  
  return CMD_OK;
}

int csu_dounsuspenduser(void *source, int cargc, char **cargv) {
  nick *sender=source;
  reguser *rup=getreguserfromnick(sender);
  reguser *vrup;
  char action[100];
  
  if (!rup)
    return CMD_ERROR;
  
  if (cargc < 1) {
    chanservstdmessage(sender, QM_NOTENOUGHPARAMS, "unsuspenduser");
    return CMD_ERROR;
  }
  
  if (cargv[0][0] == '#') {
    if (!(vrup=findreguserbynick(&cargv[0][1]))) {
      chanservstdmessage(sender, QM_UNKNOWNUSER, &cargv[0][1]);
      return CMD_ERROR;
    }
  }
  else {
    nick *np;
    
    if (!(np=getnickbynick(cargv[0]))) {
      chanservstdmessage(sender, QM_UNKNOWNUSER, cargv[0]);
      return CMD_ERROR;
    }
    
    if (!(vrup=getreguserfromnick(np)) && sender) {
      chanservstdmessage(sender, QM_USERNOTAUTHED, cargv[0]);
      return CMD_ERROR;
    }
  }
  
  if (!UHasSuspension(vrup)) {
    chanservstdmessage(sender, QM_USERNOTSUSPENDED, cargv[0]);
    return CMD_ERROR;
  }
  
  if (UHasOperPriv(vrup) && !UHasAdminPriv(rup)) {
    snprintf(action, 99, "unsuspenduser on %s", vrup->username);
    chanservstdmessage(sender, QM_NOACCESS, action);
    chanservwallmessage("%s (%s) FAILED to unsuspend %s", sender->nick, rup->username, vrup->username);
    return CMD_ERROR;
  }
  
  if (UIsDelayedGline(vrup)) {
    strcpy(action, "removed delayed gline on");
  }
  else if (UIsGline(vrup)) {
    strcpy(action, "removed instant gline on");
  }
  else if (UIsSuspended(vrup)) {
    strcpy(action, "unsuspended");
  }
  else if (UIsNeedAuth(vrup)) {
    strcpy(action, "enabled");
  }
  else {
    chanservsendmessage(sender, "Unknown suspend type encountered.");
    return CMD_ERROR;
  }
  
  vrup->flags&=(~(QUFLAG_GLINE|QUFLAG_DELAYEDGLINE|QUFLAG_SUSPENDED|QUFLAG_NEEDAUTH));
  vrup->suspendby=0;
  vrup->suspendexp=0;
  freesstring(vrup->suspendreason);
  vrup->suspendreason=0;
  csdb_updateuser(vrup);
  
  chanservwallmessage("%s (%s) %s %s", sender->nick, rup->username, action, vrup->username);
  chanservstdmessage(sender, QM_DONE);
  return CMD_OK;
}

int csu_dospewdb(void *source, int cargc, char **cargv) {
  nick *sender=source;
  reguser *rup=getreguserfromnick(sender);
  reguser *dbrup;
  int i;
  unsigned int count=0;
  
  if (!rup)
    return CMD_ERROR;
  
  if (cargc < 1) {
    chanservstdmessage(sender, QM_NOTENOUGHPARAMS, "spewdb");
    return CMD_ERROR;
  }
  
  chanservstdmessage(sender, QM_SPEWHEADER);
  for (i=0;i<REGUSERHASHSIZE;i++) {
    for (dbrup=regusernicktable[i]; dbrup; dbrup=dbrup->nextbyname) {
      if (!match(cargv[0], dbrup->username)) {
        chanservsendmessage(sender, "%-15s %-10s %-30s %s", dbrup->username, UHasSuspension(dbrup)?"yes":"no", dbrup->email?dbrup->email->content:"none set", dbrup->lastuserhost?dbrup->lastuserhost->content:"none");
        count++;
        if (count >= 2000) {
          chanservstdmessage(sender, QM_TOOMANYRESULTS, 2000, "users");
          return CMD_ERROR;
        }
      }
    }
  }
  chanservstdmessage(sender, QM_RESULTCOUNT, count, "user", (count==1)?"":"s");
  
  return CMD_OK;
}

int csu_dospewpass(void *source, int cargc, char **cargv) {
  nick *sender=source;
  reguser *rup=getreguserfromnick(sender);
  reguser *dbrup;
  int i;
  unsigned int count=0;
  
  if (!rup)
    return CMD_ERROR;
  
  if (cargc < 1) {
    chanservstdmessage(sender, QM_NOTENOUGHPARAMS, "spewpass");
    return CMD_ERROR;
  }
  
  chanservstdmessage(sender, QM_SPEWHEADER);
  for (i=0;i<REGUSERHASHSIZE;i++) {
    for (dbrup=regusernicktable[i]; dbrup; dbrup=dbrup->nextbyname) {
      if (!match(cargv[0], dbrup->password)) {
        chanservsendmessage(sender, "%-15s %-10s %-30s %s", dbrup->username, UHasSuspension(dbrup)?"yes":"no", dbrup->email?dbrup->email->content:"none set", dbrup->lastuserhost?dbrup->lastuserhost->content:"none");
        count++;
        if (count >= 2000) {
          chanservstdmessage(sender, QM_TOOMANYRESULTS, 2000, "users");
          return CMD_ERROR;
        }
      }
    }
  }
  chanservstdmessage(sender, QM_RESULTCOUNT, count, "user", (count==1)?"":"s");
  
  return CMD_OK;
}

int csu_dospewemail(void *source, int cargc, char **cargv) {
  nick *sender=source;
  reguser *rup=getreguserfromnick(sender);
  reguser *dbrup;
  int i;
  unsigned int count=0;
  
  if (!rup)
    return CMD_ERROR;
  
  if (cargc < 1) {
    chanservstdmessage(sender, QM_NOTENOUGHPARAMS, "spewemail");
    return CMD_ERROR;
  }
  
  chanservstdmessage(sender, QM_SPEWHEADER);
  for (i=0;i<REGUSERHASHSIZE;i++) {
    for (dbrup=regusernicktable[i]; dbrup; dbrup=dbrup->nextbyname) {
      if (!dbrup->email)
        continue;
      if (!match(cargv[0], dbrup->email->content)) {
        chanservsendmessage(sender, "%-15s %-10s %-30s %s", dbrup->username, UHasSuspension(dbrup)?"yes":"no", dbrup->email?dbrup->email->content:"none set", dbrup->lastuserhost?dbrup->lastuserhost->content:"none");
        count++;
        if (count >= 2000) {
          chanservstdmessage(sender, QM_TOOMANYRESULTS, 2000, "users");
          return CMD_ERROR;
        }
      }
    }
  }
  chanservstdmessage(sender, QM_RESULTCOUNT, count, "user", (count==1)?"":"s");
  
  return CMD_OK;
}

int csu_dolistflags(void *source, int cargc, char **cargv) {
  nick *sender=source;
  reguser *rup=getreguserfromnick(sender);
  reguser *dbrup;
  flag_t matchflags = 0;
  char *ch;
  int i, j;
  unsigned int count=0;
  
  if (!rup)
    return CMD_ERROR;
  
  if (cargc < 1) {
    chanservstdmessage(sender, QM_NOTENOUGHPARAMS, "listflags");
    return CMD_ERROR;
  }
  
  ch=cargv[0][0]=='+'?cargv[0]+1:cargv[0];
  
  for (i=0; ch[i]; i++) {
    for (j = 0; ruflags[j].flagchar; j++) {
      if (ruflags[j].flagchar == ch[i]) {
        matchflags|=ruflags[j].flagbit;
        break;
      }
    }
  }
  
  chanservstdmessage(sender, QM_LISTFLAGSHEADER);
  for (i=0;i<REGUSERHASHSIZE;i++) {
    for (dbrup=regusernicktable[i]; dbrup; dbrup=dbrup->nextbyname) {
      if ((dbrup->flags & matchflags) == matchflags) {
        chanservsendmessage(sender, "%-15s %-17s %-10s %-30s %s", dbrup->username, printflags(dbrup->flags, ruflags), 
          UHasSuspension(dbrup)?"yes":"no", dbrup->email?dbrup->email->content:"none set", 
          dbrup->lastuserhost?dbrup->lastuserhost->content:"none");
        count++;
        if (count >= 2000) {
          chanservstdmessage(sender, QM_TOOMANYRESULTS, 2000, "users");
          return CMD_ERROR;
        }
      }
    }
  }
  chanservstdmessage(sender, QM_RESULTCOUNT, count, "user", (count==1)?"":"s");
  
  return CMD_OK;
}

int csu_dosuspenduserlist(void *source, int cargc, char **cargv) {
  nick *sender=source;
  reguser *rup=getreguserfromnick(sender);
  reguser *vrup;
  reguser *dbrup;
  int i;
  unsigned int count=0;
  struct tm *tmp;
  char buf[200];
  
  if (!rup)
    return CMD_ERROR;
  
  if (cargc < 1) {
    chanservstdmessage(sender, QM_NOTENOUGHPARAMS, "suspenduserlist");
    return CMD_ERROR;
  }
  
  vrup=findreguserbynick(cargv[0]);

  chanservstdmessage(sender, QM_SUSPENDUSERLISTHEADER);
  for (i=0;i<REGUSERHASHSIZE;i++) {
    for (dbrup=regusernicktable[i]; dbrup; dbrup=dbrup->nextbyname) {
      if (!UHasSuspension(dbrup))
        continue;
      
      /*if (!ircd_strcmp(dbrup->username, cargv[0]) || (dbrup->suspendby == vrup->ID)) {*/
      if (!match(cargv[0], dbrup->username) || (vrup && (dbrup->suspendby == vrup->ID))) {
        char suspendtype[100];
        char *bywhom=0;
        
        if ((UIsGline(dbrup) || UIsDelayedGline(dbrup)) && !UHasOperPriv(rup))
          continue;
        
        if (UIsDelayedGline(dbrup))
          strcpy(suspendtype, "delayed gline");
        else if (UIsGline(dbrup))
          strcpy(suspendtype, "instant gline");
        else if (UIsSuspended(dbrup))
          strcpy(suspendtype, "suspended");
        else
          strcpy(suspendtype, "not used");
        
        if (vrup && (dbrup->suspendby == vrup->ID)) {
          bywhom=vrup->username;
        }
        else {
          reguser* trup=findreguserbyID(dbrup->suspendby);
          if (trup)
            bywhom=trup->username;
        }
        
        if (dbrup->suspendexp) {
          tmp=gmtime(&(dbrup->suspendexp));
          strftime(buf,15,"%d/%m/%y %H:%M",tmp);
        }
        
        count++;
        chanservsendmessage(sender, "%-15s %-13s %-15s %-15s %s", dbrup->username, suspendtype, UHasOperPriv(rup)?(bywhom?bywhom:"unknown"):"not shown", (dbrup->suspendexp)?((time(0) >= dbrup->suspendexp)?"next auth":buf):"never", dbrup->suspendreason->content);
        if (count >= 2000) {
          chanservstdmessage(sender, QM_TOOMANYRESULTS, 2000, "users");
          return CMD_ERROR;
        }
      }
    }
  }
  chanservstdmessage(sender, QM_RESULTCOUNT, count, "user", (count==1)?"":"s");
  
  return CMD_OK;
}
