#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "gline_newsearch.h"
#include "../glines.h"
#include "../../lib/version.h"

MODULE_VERSION("")

void _init(void) {
  //regdisp(reg_nodesearch, "tg", printtrust_group, 0, "");

  registersearchterm(reg_glsearch, "glexpire", gsns_glexpire_parse, 0, "gline expiry timestamp");
  registersearchterm(reg_glsearch, "glisrealname", gsns_glisrealname_parse, 0, "gline is on a realname");
  registersearchterm(reg_glsearch, "glrealname", gsns_glrealname_parse, 0, "gline - realname");
  registersearchterm(reg_glsearch, "glbadchan", gsns_glbadchan_parse, 0, "gline - badchan");
  registersearchterm(reg_glsearch, "glcreator", gsns_glcreator_parse, 0, "gline - creator");

  //registercontrolhelpcmd("trustlist",10,1,tsns_dotrustlist, "Usage: trustlist <tgid>");
  registercontrolhelpcmd("rnglist",10,0,glns_dornglist, "Usage: rnglist");
  registercontrolhelpcmd("clearchan",10,4,glns_doclearchan, "Usage: Clearchan");
}

void _fini(void) {
  //unregdisp(reg_nodesearch, "tg", printtrust_group);
  //deregistersearchterm(reg_tgsearch, "tgstartdate", tsns_tgstartdate_parse);
  //deregistercontrolcmd("trustlist",tsns_dotrustlist);

  deregistersearchterm(reg_glsearch, "glexpire", gsns_glexpire_parse);
  deregistersearchterm(reg_glsearch, "glisrealname", gsns_glisrealname_parse);
  deregistersearchterm(reg_glsearch, "glrealname", gsns_glrealname_parse);
  deregistersearchterm(reg_glsearch, "glbadchan", gsns_glbadchan_parse);
  deregistersearchterm(reg_glsearch, "glcreator", gsns_glcreator_parse);

  deregistercontrolcmd("rnglist",glns_dornglist);
  deregistercontrolcmd("clearchan", glns_doclearchan);
}

