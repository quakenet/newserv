#include "../chanserv.h"
#include "chanserv_newsearch.h"

int cs_donicksearch(void *source, int cargc, char **cargv);
int cs_dochansearch(void *source, int cargc, char **cargv);
int cs_dousersearch(void *source, int cargc, char **cargv);
int cs_dospewemail(void *source, int cargc, char **cargv);
int cs_dospewdb(void *source, int cargc, char **cargv);

UserDisplayFunc previousdefault;

void _init() {
  regnickdisp("auth", printnick_auth);
  regnickdisp("authchans", printnick_authchans);
  regchandisp("qusers", printchannel_qusers);
  reguserdisp("auth", printauth);

  registersearchterm("qusers", qusers_parse);
  registersearchterm("qlasthost", qlasthost_parse);
  registersearchterm("qemail", qemail_parse);
  registersearchterm("qsuspendreason", qsuspendreason_parse);
  registersearchterm("qusername", qusername_parse);
  registersearchterm("qchanflags", qchanflags_parse);

  chanservaddcommand("nicksearch", QCMD_OPER, 5, cs_donicksearch, "Wrapper for standard newserv nicksearch command.", "");
  chanservaddcommand("chansearch", QCMD_OPER, 5, cs_dochansearch, "Wrapper for standard newserv chansearch command.", "");
  chanservaddcommand("usersearch", QCMD_OPER, 5, cs_dousersearch, "Wrapper for standard newserv usersearch command.", "");
  chanservaddcommand("spewemail", QCMD_OPER, 1, cs_dospewemail, "Search for an e-mail in the database.", "Usage: spewemail <pattern>\nDisplays all users with email addresses that match the supplied pattern.");
  chanservaddcommand("spewdb", QCMD_OPER, 1, cs_dospewdb, "Search for a user in the database.", "Usage: spewdb <pattern>\nDisplays all users with usernames that match the specified pattern.");

  previousdefault = defaultuserfn;
  defaultuserfn = printauth;
}

void _fini() {
  unregnickdisp("auth", printnick_auth);
  unregnickdisp("authchans", printnick_authchans);
  unregchandisp("qusers", printchannel_qusers);
  unreguserdisp("auth", printauth);

  deregistersearchterm("qusers", qusers_parse);
  deregistersearchterm("qlasthost", qlasthost_parse);
  deregistersearchterm("qemail", qemail_parse);
  deregistersearchterm("qsuspendreason", qsuspendreason_parse);
  deregistersearchterm("qusername", qusername_parse);
  deregistersearchterm("qchanflags", qchanflags_parse);

  chanservremovecommand("nicksearch", cs_donicksearch);
  chanservremovecommand("chansearch", cs_dochansearch);
  chanservremovecommand("usersearch", cs_dousersearch);
  chanservremovecommand("spewemail", cs_dospewemail);
  chanservremovecommand("spewdb", cs_dospewdb);

  defaultuserfn = previousdefault;
}
