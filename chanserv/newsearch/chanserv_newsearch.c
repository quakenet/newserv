#include "../chanserv.h"
#include "chanserv_newsearch.h"

int cs_donicksearch(void *source, int cargc, char **cargv);
int cs_dochansearch(void *source, int cargc, char **cargv);
int cs_dousersearch(void *source, int cargc, char **cargv);
int cs_dospewemailtwo(void *source, int cargc, char **cargv);
int cs_dospewdbtwo(void *source, int cargc, char **cargv);

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

  chanservaddcommand("nicksearch", QCMD_OPER, 5, cs_donicksearch, "Wrapper for standard newserv nicksearch command.", "");
  chanservaddcommand("chansearch", QCMD_OPER, 5, cs_dochansearch, "Wrapper for standard newserv chansearch command.", "");
  chanservaddcommand("usersearch", QCMD_OPER, 5, cs_dousersearch, "Wrapper for standard newserv usersearch command.", "");
  chanservaddcommand("spewemailtwo", QCMD_OPER, 1, cs_dospewemailtwo, "Spews email...", "");
  chanservaddcommand("spewdbtwo", QCMD_OPER, 1, cs_dospewdbtwo, "Spews db...", "");

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

  chanservremovecommand("nicksearch", cs_donicksearch);
  chanservremovecommand("chansearch", cs_dochansearch);
  chanservremovecommand("usersearch", cs_dousersearch);
  chanservremovecommand("spewemailtwo", cs_dospewemailtwo);
  chanservremovecommand("spewdbtwo", cs_dospewdbtwo);

  defaultuserfn = previousdefault;
}
