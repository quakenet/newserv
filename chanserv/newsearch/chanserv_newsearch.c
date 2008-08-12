#include "../chanserv.h"
#include "chanserv_newsearch.h"

int cs_donicksearch(void *source, int cargc, char **cargv);
int cs_dochansearch(void *source, int cargc, char **cargv);
int cs_dousersearch(void *source, int cargc, char **cargv);
int cs_dospewemail(void *source, int cargc, char **cargv);
int cs_dospewdb(void *source, int cargc, char **cargv);

UserDisplayFunc previousdefault;

void _init() {
  regdisp(reg_nicksearch, "auth", printnick_auth);
  regdisp(reg_nicksearch, "authchans", printnick_authchans);
  regdisp(reg_chansearch, "qusers", printchannel_qusers);
  regdisp(reg_usersearch, "auth", printauth);

  registersearchterm(reg_usersearch, "qusers", qusers_parse);
  registersearchterm(reg_usersearch, "qlasthost", qlasthost_parse);
  registersearchterm(reg_usersearch, "qemail", qemail_parse);
  registersearchterm(reg_usersearch, "qsuspendreason", qsuspendreason_parse);
  registersearchterm(reg_usersearch, "qusername", qusername_parse);
  registersearchterm(reg_chansearch, "qchanflags", qchanflags_parse);

  chanservaddcommand("nicksearch", QCMD_OPER, 5, cs_donicksearch, "Wrapper for standard newserv nicksearch command.", "");
  chanservaddcommand("chansearch", QCMD_OPER, 5, cs_dochansearch, "Wrapper for standard newserv chansearch command.", "");
  chanservaddcommand("usersearch", QCMD_OPER, 5, cs_dousersearch, "Wrapper for standard newserv usersearch command.", "");
  chanservaddcommand("spewemail", QCMD_OPER, 1, cs_dospewemail, "Search for an e-mail in the database.", "Usage: spewemail <pattern>\nDisplays all users with email addresses that match the supplied pattern.");
  chanservaddcommand("spewdb", QCMD_OPER, 1, cs_dospewdb, "Search for a user in the database.", "Usage: spewdb <pattern>\nDisplays all users with usernames that match the specified pattern.");

  previousdefault = defaultuserfn;
  defaultuserfn = printauth;
}

void _fini() {
  unregdisp(reg_nicksearch, "auth", printnick_auth);
  unregdisp(reg_nicksearch, "authchans", printnick_authchans);
  unregdisp(reg_chansearch, "qusers", printchannel_qusers);
  unregdisp(reg_usersearch, "auth", printauth);

  deregistersearchterm(reg_usersearch, "qusers", qusers_parse);
  deregistersearchterm(reg_usersearch, "qlasthost", qlasthost_parse);
  deregistersearchterm(reg_usersearch, "qemail", qemail_parse);
  deregistersearchterm(reg_usersearch, "qsuspendreason", qsuspendreason_parse);
  deregistersearchterm(reg_usersearch, "qusername", qusername_parse);
  deregistersearchterm(reg_usersearch, "qchanflags", qchanflags_parse);

  chanservremovecommand("nicksearch", cs_donicksearch);
  chanservremovecommand("chansearch", cs_dochansearch);
  chanservremovecommand("usersearch", cs_dousersearch);
  chanservremovecommand("spewemail", cs_dospewemail);
  chanservremovecommand("spewdb", cs_dospewdb);

  defaultuserfn = previousdefault;
}
