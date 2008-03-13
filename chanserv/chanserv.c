/*
 * chanserv.c:
 *  The main chanserv core file.
 */

#include "chanserv.h"
#include "authlib.h"
#include "../core/hooks.h"
#include "../core/schedule.h"
#include "../lib/version.h"

MODULE_VERSION(QVERSION);

int chanservnext;

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
  { 's', QCFLAG_NOINFO },
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
  { 'i', QCUFLAG_INFO },
  { 'j', QCUFLAG_AUTOINVITE },
  { 'k', QCUFLAG_KNOWN },
  { 'm', QCUFLAG_MASTER },
  { 'n', QCUFLAG_OWNER },
  { 'o', QCUFLAG_OP },
  { 'p', QCUFLAG_PROTECT },
  { 'q', QCUFLAG_QUIET },
  { 's', QCUFLAG_NOINFO },
  { 't', QCUFLAG_TOPIC },
  { 'v', QCUFLAG_VOICE },
  { 'w', QCUFLAG_HIDEWELCOME },
  { '\0', 0 } };

const flag ruflags[] = {
  { 'a',  QUFLAG_ADMIN },
  { 'd',  QUFLAG_DEV },
  { 'D',  QUFLAG_CLEANUPEXEMPT },
  { 'g',  QUFLAG_GLINE },
  { 'G',  QUFLAG_DELAYEDGLINE },
  { 'h',  QUFLAG_HELPER },
  { 'i',  QUFLAG_INFO },
  { 'L',  QUFLAG_NOAUTHLIMIT },
  { 'n',  QUFLAG_NOTICE },
  { 'o',  QUFLAG_OPER },
  { 'p',  QUFLAG_PROTECT },
  { 's',  QUFLAG_NOINFO },
  { 'T',  QUFLAG_TRUST },
  { 'z',  QUFLAG_SUSPENDED },
  { '\0', 0 } };

const flag mdflags[] = {
  { 'l', MDFLAG_LIMIT },
  { 'b', MDFLAG_BANNED },
  { 'u', MDFLAG_ACTLIMIT },
  { '\0', 0 } };

void chanservfreestuff();
void chanservfinishinit(int hooknum, void *arg);

PQModuleIdentifier q9pqid;

void chanservdumpstuff(void *arg) {
  dumplastjoindata("lastjoin.dump");
}

void _init() {
  /* Register the nick extension - the others are registered in the db module */
  chanservnext=registernickext("nickserv");

  chanservcryptoinit();
  csa_initregex();

  q9pqid = pqgetid();

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
    chantypes[7]=getsstring("upgrade",20);
    
    /* And the log system */
    cs_initlog();

    /* Set up the command handler, and built in commands */
    chanservcommandinit();
    chanservaddcommand("showcommands", 0, 1, cs_doshowcommands, "Lists available commands.","Usage: SHOWCOMMANDS [<mask>]\nPrints a list of commands currently available to you, where:\nmask - Mask of commands to list (* or ? are wildcards).  If no mask is specified,\n       all available commands are displayed.");
    chanservaddcommand("quit", QCMD_DEV, 1, cs_doquit, "Makes the bot QUIT and \"reconnect\".","");
    chanservaddcommand("rename", QCMD_DEV, 1, cs_dorename, "Changes the bot's name.","");
    chanservaddcommand("rehash", QCMD_DEV, 0, cs_dorehash, "Reloads all text from database.","");
    /* Make "HELP" take 2 arguments so things like "HELP chanflags #channel" work.  Any junk after the command will go into arg 2 and be ignored. */
    chanservaddcommand("help", 0, 2, cs_dohelp, "Displays help on a specific command.","Usage: HELP <command>\nShows help for a command, where:\ncommand - the command to show help for.\nFor a list of available commands, see SHOWCOMMANDS.\n");
    chanservaddcommand("version", 0, 1, cs_doversion, "Show Version.","Usage: VERSION\nShows the version number of the running bot.");

    chanservaddctcpcommand("ping",cs_doctcpping);
    chanservaddctcpcommand("version",cs_doctcpversion);
    chanservaddctcpcommand("gender",cs_doctcpgender);

    registerhook(HOOK_CHANSERV_DBLOADED, chanservfinishinit);
    
    /* Now that the database is in a separate module it might be loaded already. */
    if (chanservdb_ready)
      chanservfinishinit(HOOK_CHANSERV_DBLOADED, NULL);
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
  nick *np, *nnp;

  /* Scan for users */
  for (i=0;i<NICKHASHSIZE;i++)
    for (np=nicktable[i];np;np=nnp) {
      nnp=np->next;
      cs_checknick(np);
    }
  
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

  chanserv_init_status = CS_INIT_READY;
  triggerhook(HOOK_CHANSERV_RUNNING, NULL);  
  Error("chanserv",ERR_INFO,"Ready to roll.");
}

void _fini() {
  pqfreeid(q9pqid);

  deleteallschedules(cs_timerfunc);
  deleteallschedules(chanservreguser);
  deleteallschedules(chanservdumpstuff);
  deleteallschedules(chanservdgline);

  if (chanservext>-1 && chanservnext>-1 && chanservaext>-1) {
    int i;
    for (i=0;i<CHANTYPES;i++)
      freesstring(chantypes[i]);

    free(chantypes);
  }
    
  /* Free everything */
  if (chanservnext!=-1) {
    releasenickext(chanservnext);
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
  chanservremovecommand("version", cs_doversion);
  chanservremovectcpcommand("ping",cs_doctcpping);
  chanservremovectcpcommand("version",cs_doctcpversion);
  chanservremovectcpcommand("gender",cs_doctcpgender);
  chanservcommandclose();

  csa_freeregex();
  chanservcryptofree();

  cs_closelog();
}
