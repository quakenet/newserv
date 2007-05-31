/*
 * chanserv.c:
 *  The main chanserv core file.
 */

#include "chanserv.h"
#include "../core/hooks.h"
#include "../core/schedule.h"

int chanservext;
int chanservnext;
int chanservaext;

int chanserv_init_status;

sstring **chantypes;

const flag rcflags[] = {
  { 'a', QCFLAG_AUTOOP },
  { 'b', QCFLAG_BITCH },
  { 'c', QCFLAG_AUTOLIMIT },
  { 'e', QCFLAG_ENFORCE },
  { 'f', QCFLAG_FORCETOPIC },
  { 'g', QCFLAG_AUTOVOICE },
  { 'i', QCFLAG_INFO },
  { 'j', QCFLAG_JOINED },
  { 'k', QCFLAG_KNOWNONLY },
  { 'p', QCFLAG_PROTECT },
  { 's', QCFLAG_SPAMPROT },
  { 't', QCFLAG_TOPICSAVE },
  { 'v', QCFLAG_VOICEALL },
  { 'w', QCFLAG_WELCOME },
  { 'z', QCFLAG_SUSPENDED },
  { '\0', 0 } };   

const flag rcuflags[] = {
  { 'a', QCUFLAG_AUTOOP },
  { 'b', QCUFLAG_BANNED },
  { 'd', QCUFLAG_DENY },
  { 'g', QCUFLAG_AUTOVOICE },
  { 'i', QCUFLAG_HIDEINFO },
  { 'j', QCUFLAG_AUTOINVITE },
  { 'k', QCUFLAG_KNOWN },
  { 'm', QCUFLAG_MASTER },
  { 'n', QCUFLAG_OWNER },
  { 'o', QCUFLAG_OP },
  { 'p', QCUFLAG_PROTECT },
  { 'q', QCUFLAG_QUIET },
  { 's', QCUFLAG_SPAMCON },
  { 't', QCUFLAG_TOPIC },
  { 'v', QCUFLAG_VOICE },
  { 'w', QCUFLAG_HIDEWELCOME },
  { '\0', 0 } };

const flag ruflags[] = {
  { 'a',  QUFLAG_ADMIN },
  { 'd',  QUFLAG_DEV },
  { 'g',  QUFLAG_GLINE },
  { 'G',  QUFLAG_DELAYEDGLINE },
  { 'h',  QUFLAG_HELPER },
  { 'i',  QUFLAG_INFO },
  { 'l',  QUFLAG_NEEDAUTH },
  { 'L',  QUFLAG_NOAUTHLIMIT },
  { 'n',  QUFLAG_NOTICE },
  { 'o',  QUFLAG_OPER },
  { 'p',  QUFLAG_PROTECT },
  { 'r',  QUFLAG_RESTRICTED },
  { 'z',  QUFLAG_SUSPENDED },
  { '\0', 0 } };

void chanservfreestuff();
void chanservfinishinit(int hooknum, void *arg);

void chanservdumpstuff(void *arg) {
  dumplastjoindata("lastjoin.dump");
}

void _init() {
  /* Register the extension */
  chanservext=registerchanext("chanserv");
  chanservnext=registernickext("nickserv");
  chanservaext=registerauthnameext("chanserv");

  if (chanservext!=-1 && chanservnext!=-1 && chanservaext!=-1) {
    /* Set up the chantypes */
    chantypes=(sstring **)malloc(CHANTYPES*sizeof(sstring *));
    chantypes[0]=getsstring("(unspecified)",20);
    chantypes[1]=getsstring("clan",20);
    chantypes[2]=getsstring("league",20);
    chantypes[3]=getsstring("private",20);
    chantypes[4]=getsstring("special",20);
    chantypes[5]=getsstring("gamesite",20);
    chantypes[6]=getsstring("game",20);
    
    /* Set up the allocators and the hashes */
    chanservallocinit();
    chanservhashinit();

    /* And the messages */
    initmessages();
    
    /* And the log system */
    cs_initlog();

    /* Now load the database */
    chanserv_init_status = CS_INIT_DB;
    chanservdbinit();

    /* Set up the command handler, and built in commands */
    chanservcommandinit();
    chanservaddcommand("showcommands", 0, 1, cs_doshowcommands, "Lists available commands.");
    chanservaddcommand("quit", QCMD_DEV, 1, cs_doquit, "Makes the bot QUIT and \"reconnect\".");
    chanservaddcommand("rename", QCMD_DEV, 1, cs_dorename, "Changes the bot's name.");
    chanservaddcommand("rehash", QCMD_DEV, 0, cs_dorehash, "Reloads all text from database.");
    chanservaddcommand("help", 0, 1, cs_dohelp, "Displays help on a specific command.");
    
    chanservaddctcpcommand("ping",cs_doctcpping);
    chanservaddctcpcommand("version",cs_doctcpversion);
    chanservaddctcpcommand("gender",cs_doctcpgender);

    registerhook(HOOK_CHANSERV_DBLOADED, chanservfinishinit);
  }
}

void chanservfinishinit(int hooknum, void *arg) {
  Error("chanserv",ERR_INFO,"Database loaded, finishing initialisation.");

  deregisterhook(HOOK_CHANSERV_DBLOADED, chanservfinishinit);

  readlastjoindata("lastjoin.dump");
  
  /* Schedule the dumps */
  schedulerecurring(time(NULL)+DUMPINTERVAL,0,DUMPINTERVAL,chanservdumpstuff,NULL);

  chanserv_init_status = CS_INIT_NOUSER;

  /* Register the user */
  scheduleoneshot(time(NULL)+1,&chanservreguser,NULL);
}
  
void chanserv_finalinit() {
  int i;
  nick *np;

  /* Scan for users */
  for (i=0;i<NICKHASHSIZE;i++)
    for (np=nicktable[i];np;np=np->next)
      cs_checknick(np);
  
  /* Register core hooks */
  registerhook(HOOK_NICK_NEWNICK, cs_handlenick);
  registerhook(HOOK_NICK_ACCOUNT, cs_handlenick);
  registerhook(HOOK_NICK_LOSTNICK, cs_handlelostnick);
  registerhook(HOOK_NICK_SETHOST, cs_handlesethost);
  registerhook(HOOK_CHANNEL_NEWCHANNEL, cs_handlenewchannel);
  registerhook(HOOK_CHANNEL_LOSTCHANNEL, cs_handlelostchannel);
  registerhook(HOOK_CHANNEL_JOIN, cs_handlejoin);
  registerhook(HOOK_CHANNEL_CREATE, cs_handlejoin);
  registerhook(HOOK_CHANNEL_MODECHANGE, cs_handlemodechange);
  registerhook(HOOK_CHANNEL_BURST, cs_handleburst);
  registerhook(HOOK_CHANNEL_TOPIC, cs_handletopicchange);
  registerhook(HOOK_CHANNEL_LOSTNICK, cs_handlechanlostuser);
  
  Error("chanserv",ERR_INFO,"Ready to roll.");
}

void _fini() {
  deleteallschedules(cs_timerfunc);
  deleteallschedules(chanservreguser);
  deleteallschedules(chanservdumpstuff);
  deleteallschedules(chanservdgline);

  if (chanservext>-1 && chanservnext>-1) {
    int i;
    for (i=0;i<CHANTYPES;i++)
      freesstring(chantypes[i]);

    free(chantypes);
  }
    
  chanservfreestuff();

  /* Free everything */
  if (chanservext!=-1) {
    releasechanext(chanservext);
  }
  
  if (chanservnext!=-1) {
    releasenickext(chanservnext);
  }
  
  if (chanservaext!=-1) {
    releaseauthnameext(chanservaext);
  }

  if (chanservnick)
    deregisterlocaluser(chanservnick, "Leaving");

  deregisterhook(HOOK_NICK_NEWNICK, cs_handlenick);
  deregisterhook(HOOK_NICK_ACCOUNT, cs_handlenick);
  deregisterhook(HOOK_NICK_LOSTNICK, cs_handlelostnick);
  deregisterhook(HOOK_NICK_SETHOST, cs_handlesethost);
  deregisterhook(HOOK_CHANNEL_NEWCHANNEL, cs_handlenewchannel);
  deregisterhook(HOOK_CHANNEL_LOSTCHANNEL, cs_handlelostchannel);
  deregisterhook(HOOK_CHANNEL_JOIN, cs_handlejoin);
  deregisterhook(HOOK_CHANNEL_CREATE, cs_handlejoin);
  deregisterhook(HOOK_CHANNEL_MODECHANGE, cs_handlemodechange);
  deregisterhook(HOOK_CHANNEL_BURST, cs_handleburst);
  deregisterhook(HOOK_CHANNEL_TOPIC, cs_handletopicchange);
  deregisterhook(HOOK_CHANNEL_LOSTNICK, cs_handlechanlostuser);
  /*  deregisterhook(HOOK_CHANNEL_OPPED, cs_handleopchange);
  deregisterhook(HOOK_CHANNEL_DEOPPED, cs_handleopchange);
  deregisterhook(HOOK_CHANNEL_DEVOICED, cs_handleopchange);
  deregisterhook(HOOK_CHANNEL_BANSET, cs_handlenewban); */
  deregisterhook(HOOK_CHANSERV_DBLOADED, chanservfinishinit); /* for safety */

  chanservremovecommand("showcommands", cs_doshowcommands);
  chanservremovecommand("quit", cs_doquit);
  chanservremovecommand("rename", cs_dorename);
  chanservremovecommand("rehash", cs_dorehash);
  chanservremovecommand("help", cs_dohelp);
  chanservremovectcpcommand("ping",cs_doctcpping);
  chanservremovectcpcommand("version",cs_doctcpversion);
  chanservremovectcpcommand("gender",cs_doctcpgender);
  chanservcommandclose();

  cs_closelog();

  chanservdbclose();
  csfreeall();
}

void chanservfreestuff() {
  int i;
  chanindex *cip, *ncip;
  regchan *rcp;
  reguser *rup;
  regchanuser *rcup;
  regban *rbp;
  
  for (i=0;i<REGUSERHASHSIZE;i++) {
    for (rup=regusernicktable[i];rup;rup=rup->nextbyname) {
      freesstring(rup->email);
      freesstring(rup->lastuserhost);
      freesstring(rup->suspendreason);
      freesstring(rup->comment);
      freesstring(rup->info);
      
      for (rcup=rup->knownon;rcup;rcup=rcup->nextbyuser)
	freesstring(rcup->info);
    }
  }

  for (i=0;i<CHANNELHASHSIZE;i++) {
    for (cip=chantable[i];cip;cip=ncip) {
      ncip=cip->next;
      if ((rcp=cip->exts[chanservext])) {
	freesstring(rcp->welcome);
	freesstring(rcp->topic);
	freesstring(rcp->key);
	freesstring(rcp->suspendreason);
	freesstring(rcp->comment);
	for (rbp=rcp->bans;rbp;rbp=rbp->next) {
	  freesstring(rbp->reason);
	  freechanban(rbp->cbp);
	}
	cip->exts[chanservext]=NULL;
	releasechanindex(cip);
      }
    }
  }

}
