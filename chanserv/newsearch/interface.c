#include "../chanserv.h"
#include "../../newsearch/newsearch.h"
#include <stdio.h>
#include <stdarg.h>

static void chanservmessagewrapper(nick *np, char *format, ...) {
  char buf[1024];
  va_list ap;

  va_start(ap, format);
  vsnprintf(buf, sizeof(buf), format, ap);
  va_end(ap);

  chanservsendmessage(np, "%s", buf);
}

static void chanservwallwrapper(int level, char *format, ...) {
  char buf[1024];
  va_list ap;

  va_start(ap, format);
  vsnprintf(buf, sizeof(buf), format, ap);
  va_end(ap);

  chanservwallmessage("%s", buf);
}

int cs_donicksearch(void *source, int cargc, char **cargv) {
  nick *sender=source;
  
  if (cargc < 1) {
    chanservstdmessage(sender, QM_NOTENOUGHPARAMS, "nicksearch");
    return CMD_ERROR;
  }

  do_nicksearch_real(chanservmessagewrapper, chanservwallwrapper, source, cargc, cargv);

  chanservstdmessage(sender, QM_DONE);  
  return CMD_OK;
}

int cs_dochansearch(void *source, int cargc, char **cargv) {
  nick *sender=source;
  
  if (cargc < 1) {
    chanservstdmessage(sender, QM_NOTENOUGHPARAMS, "chansearch");
    return CMD_ERROR;
  }

  do_chansearch_real(chanservmessagewrapper, chanservwallwrapper, source, cargc, cargv);

  chanservstdmessage(sender, QM_DONE);  
  return CMD_OK;
}

