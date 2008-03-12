#include "chanserv.h"
#include "../control/control.h"

int csa_docheckhashpass(void *source, int cargc, char **cargv);

void _init(void) {
  registercontrolhelpcmd("checkhashpass", NO_RELAY, 3, csa_docheckhashpass, "Usage: checkhashpass <username> <digest> ?junk?");
}

void _fini(void) {
  deregistercontrolcmd("checkhashpass", csa_docheckhashpass);
}

int csa_docheckhashpass(void *source, int cargc, char **cargv) {
  nick *sender=(nick *)source;
  reguser *rup;  
  char *flags;

  if(cargc<3) {
    controlreply(sender, "CHECKHASHPASS FAIL args");
    return CMD_ERROR;
  }

  if (!(rup=findreguserbynick(cargv[0]))) {
    controlreply(sender, "CHECKHASHPASS FAIL user");
    return CMD_OK;
  }

  flags = printflags(QUFLAG_ALL & rup->flags, ruflags);
  if(UHasSuspension(rup)) {
    controlreply(sender, "CHECKHASHPASS FAIL suspended %s %s %u", rup->username, flags, rup->ID);
  } else if(!checkhashpass(rup, cargc<3?NULL:cargv[2], cargv[1])) {
    controlreply(sender, "CHECKHASHPASS FAIL digest %s %s %u", rup->username, flags, rup->ID);
  } else {
    controlreply(sender, "CHECKHASHPASS OK %s %s %u %s", rup->username, flags, rup->ID, rup->email?rup->email->content:"-");
  }

  return CMD_OK;
}
