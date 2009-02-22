#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "trusts_newsearch.h"
#include "../trusts2/trusts.h"
#include "../lib/version.h"

MODULE_VERSION("");


void _init(void) {
  regdisp(reg_nodesearch, "tg", printtrust_group, 0, "");
  regdisp(reg_nodesearch, "tb", printtrust_block, 0, "");
  regdisp(reg_nodesearch, "tbprivate", printtrust_blockprivate, 0, "");

  /* TRUSTGROUP */
  registersearchterm(reg_tgsearch, "tgid", tsns_tgid_parse, 0, "Trustgroup ID");
  registersearchterm(reg_tgsearch, "tgstartdate", tsns_tgstartdate_parse, 0, "Trustgroup start date timestamp - represents date trust is due to go active or date trust was added");
  registersearchterm(reg_tgsearch, "tglastused", tsns_tglastused_parse, 0, "trust group last used timestamp");
  registersearchterm(reg_tgsearch, "tgexpire", tsns_tgexpire_parse, 0, "trust group expiry timestamp");
  registersearchterm(reg_tgsearch, "tgownerid", tsns_tgownerid_parse, 0, "Q auth id of trust group owner");
  registersearchterm(reg_tgsearch, "tgmaxperip", tsns_tgmaxperip_parse, 0, "trust group max per IP value");
  registersearchterm(reg_tgsearch, "tgmaxusage", tsns_tgmaxusage_parse, 0, "trust group max usage ever");
  registersearchterm(reg_tgsearch, "tgcurrenton", tsns_tgcurrenton_parse, 0, "trust group current usage");
  registersearchterm(reg_tgsearch, "tgmaxclones", tsns_tgmaxclones_parse, 0, "trust group maximum user limit");
  registersearchterm(reg_tgsearch, "tgmaxperident", tsns_tgmaxperident_parse, 0, "trust group max per ident value");
  registersearchterm(reg_tgsearch, "tgenforceident", tsns_tgenforceident_parse, 0, "trust group enforce ident (0/1)");
  registersearchterm(reg_tgsearch, "tgcreated", tsns_tgcreated_parse, 0, "trust group creation timestamp (note: we also store a startdate timestamp)");
  registersearchterm(reg_tgsearch, "tgmodified", tsns_tgmodified_parse, 0, "trust group last modified timestamp");

  registersearchterm(reg_thsearch, "thid", tsns_thid_parse, 0, "trust host ID");
  registersearchterm(reg_thsearch, "thstartdate", tsns_thstartdate_parse, 0, "trust host start date timestamp - represents date host is due to go active or date host was added");
  registersearchterm(reg_thsearch, "thlastused", tsns_thlastused_parse, 0, "trust host last used timestamp");
  registersearchterm(reg_thsearch, "thexpire", tsns_thexpire_parse, 0, "trust host expiry timestamp");
  registersearchterm(reg_thsearch, "thmaxusage", tsns_thmaxusage_parse, 0, "trust host max usage ever");
  registersearchterm(reg_thsearch, "thcreated", tsns_thcreated_parse, 0, "trust host creation timestamp (note: we also store a startdate timestamp)");
  registersearchterm(reg_thsearch, "thmodified", tsns_thmodified_parse, 0, "trust host last modified timestamp");

  registersearchterm(reg_nodesearch, "trusted", tsns_trusted_parse, 0, "IP node is trusted");
  registersearchterm(reg_nodesearch, "tgid", tsns_tgid_parse, 0, "Trust group ID");
  registersearchterm(reg_nodesearch, "tgexpire", tsns_tgexpire_parse, 0, "trust group expiry timestamp");
  registersearchterm(reg_nodesearch, "tgstartdate", tsns_tgstartdate_parse, 0, "Trustgroup start date timestamp - represents date trust is due to go active or date trust was added");
  registersearchterm(reg_nodesearch, "tglastused", tsns_tglastused_parse, 0, "trust group last used timestamp");
  registersearchterm(reg_nodesearch, "tgownerid", tsns_tgownerid_parse, 0, "Q auth id of trust group owner");
  registersearchterm(reg_nodesearch, "tgmaxperip", tsns_tgmaxperip_parse, 0, "trust group max per IP value");
  registersearchterm(reg_nodesearch, "tgmaxusage", tsns_tgmaxusage_parse, 0, "trust group max usage ever");
  registersearchterm(reg_nodesearch, "tgmaxclones", tsns_tgmaxclones_parse, 0, "trust group maximum user limit");
  registersearchterm(reg_nodesearch, "tgmaxperident", tsns_tgmaxperident_parse, 0, "trust group max per ident value");
  registersearchterm(reg_nodesearch, "tgenforceident", tsns_tgenforceident_parse, 0, "trust group enforce ident (0/1)");
  registersearchterm(reg_nodesearch, "tgcreated", tsns_tgcreated_parse, 0, "trust group creation timestamp (note: we also store a startdate timestamp)");
  registersearchterm(reg_nodesearch, "tgmodified", tsns_tgmodified_parse, 0, "trust group last modified timestamp");

  registersearchterm(reg_nodesearch, "thid", tsns_thid_parse, 0, "Trust Host ID");
  registersearchterm(reg_nodesearch, "thstartdate", tsns_thstartdate_parse, 0, "trust host start date timestamp - represents date host is due to go active or date host was added");
  registersearchterm(reg_nodesearch, "thlastused", tsns_thlastused_parse, 0, "trust host last used timestamp");
  registersearchterm(reg_nodesearch, "thexpire", tsns_thexpire_parse, 0, "trust host expiry timestamp");
  registersearchterm(reg_nodesearch, "thmaxusage", tsns_thmaxusage_parse, 0, "trust host max usage ever");
  registersearchterm(reg_nodesearch, "thcreated", tsns_thcreated_parse, 0, "trust host creation timestamp (note: we also store a startdate timestamp)");
  registersearchterm(reg_nodesearch, "thmodified", tsns_thmodified_parse, 0, "trust host last modified timestamp");

  registersearchterm(reg_nodesearch, "tbid", tsns_tbid_parse, 0, "Trust Block ID");

  registersearchterm(reg_nicksearch, "istrusted", tsns_istrusted_parse, 0, "user is on a trusted host");

  registercontrolhelpcmd("trustlist",10,1,tsns_dotrustlist, "Usage: trustlist <tgid>");
  registercontrolhelpcmd("trustdenylist",10,1,tsns_dotrustdenylist, "Usage: trustdenylist <tgid>");

}

void _fini(void) {
  unregdisp(reg_nodesearch, "tg", printtrust_group);
  unregdisp(reg_nodesearch, "tb", printtrust_block);
  unregdisp(reg_nodesearch, "tbprivate", printtrust_blockprivate);

  deregistersearchterm(reg_tgsearch, "tgid", tsns_tgid_parse);
  deregistersearchterm(reg_tgsearch, "tgstartdate", tsns_tgstartdate_parse);
  deregistersearchterm(reg_tgsearch, "tglastused", tsns_tglastused_parse);
  deregistersearchterm(reg_tgsearch, "tgexpire", tsns_tgexpire_parse);
  deregistersearchterm(reg_tgsearch, "tgownerid", tsns_tgownerid_parse);
  deregistersearchterm(reg_tgsearch, "tgmaxperip", tsns_tgmaxperip_parse);
  deregistersearchterm(reg_tgsearch, "tgmaxusage", tsns_tgmaxusage_parse);
  deregistersearchterm(reg_tgsearch, "tgcurrenton", tsns_tgcurrenton_parse);
  deregistersearchterm(reg_tgsearch, "tgmaxclones", tsns_tgmaxclones_parse);
  deregistersearchterm(reg_tgsearch, "tgmaxperident", tsns_tgmaxperident_parse);
  deregistersearchterm(reg_tgsearch, "tgenforceident", tsns_tgenforceident_parse);
  deregistersearchterm(reg_tgsearch, "tgcreated", tsns_tgcreated_parse);
  deregistersearchterm(reg_tgsearch, "tgmodified", tsns_tgmodified_parse);

  deregistersearchterm(reg_thsearch, "thid", tsns_thid_parse);
  deregistersearchterm(reg_thsearch, "thstartdate", tsns_thstartdate_parse);
  deregistersearchterm(reg_thsearch, "thlastused", tsns_thlastused_parse);
  deregistersearchterm(reg_thsearch, "thexpire", tsns_thexpire_parse);
  deregistersearchterm(reg_thsearch, "thmaxusage", tsns_thmaxusage_parse);
  deregistersearchterm(reg_thsearch, "thcreated", tsns_thcreated_parse);
  deregistersearchterm(reg_thsearch, "thmodified", tsns_thmodified_parse);

  deregistersearchterm(reg_nodesearch, "trusted", tsns_trusted_parse);
  deregistersearchterm(reg_nodesearch, "tgid", tsns_tgid_parse);
  deregistersearchterm(reg_nodesearch, "tgexpire", tsns_tgexpire_parse);
  deregistersearchterm(reg_nodesearch, "tgstartdate", tsns_tgstartdate_parse);
  deregistersearchterm(reg_nodesearch, "tglastused", tsns_tglastused_parse);
  deregistersearchterm(reg_nodesearch, "tgownerid", tsns_tgownerid_parse);
  deregistersearchterm(reg_nodesearch, "tgmaxperip", tsns_tgmaxperip_parse);
  deregistersearchterm(reg_nodesearch, "tgmaxusage", tsns_tgmaxusage_parse);
  deregistersearchterm(reg_nodesearch, "tgmaxclones", tsns_tgmaxclones_parse);
  deregistersearchterm(reg_nodesearch, "tgmaxperident", tsns_tgmaxperident_parse);
  deregistersearchterm(reg_nodesearch, "tgenforceident", tsns_tgenforceident_parse);
  deregistersearchterm(reg_nodesearch, "tgcreated", tsns_tgcreated_parse);
  deregistersearchterm(reg_nodesearch, "tgmodified", tsns_tgmodified_parse);

  deregistersearchterm(reg_nodesearch, "thid", tsns_thid_parse);
  deregistersearchterm(reg_nodesearch, "thstartdate", tsns_thstartdate_parse);
  deregistersearchterm(reg_nodesearch, "thlastused", tsns_thlastused_parse);
  deregistersearchterm(reg_nodesearch, "thexpire", tsns_thexpire_parse);
  deregistersearchterm(reg_nodesearch, "thmaxusage", tsns_thmaxusage_parse);
  deregistersearchterm(reg_nodesearch, "thcreated", tsns_thcreated_parse);
  deregistersearchterm(reg_nodesearch, "thmodified", tsns_thmodified_parse);

  deregistersearchterm(reg_nodesearch, "tbid", tsns_tbid_parse);

  deregistersearchterm(reg_nicksearch, "istrusted", tsns_istrusted_parse);

  deregistercontrolcmd("trustlist",tsns_dotrustlist);
  deregistercontrolcmd("trustdenylist",tsns_dotrustdenylist);
}

