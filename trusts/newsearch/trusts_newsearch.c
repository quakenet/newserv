#include "../../lib/version.h"
#include "../../newsearch/newsearch.h"
#include "../../core/hooks.h"
#include "../trusts.h"
#include "trusts_newsearch.h"

MODULE_VERSION("");

struct searchNode *tgroup_parse(searchCtx *, int argc, char **argv);
struct searchNode *thastrust_parse(searchCtx *, int argc, char **argv);

static int commandsregistered;
static void registercommands(int hooknum, void *arg) {
  if(commandsregistered)
    return;
  commandsregistered = 1;

  registersearchterm(reg_nicksearch, "tgroup", tgroup_parse, 0, "");
  registersearchterm(reg_nicksearch, "thastrust", thastrust_parse, 0, "");
}

static void deregistercommands(int hooknum, void *arg) {
  if(!commandsregistered)
    return;
  commandsregistered = 0;

  deregistersearchterm(reg_nicksearch, "tgroup", tgroup_parse);
  deregistersearchterm(reg_nicksearch, "thastrust", thastrust_parse);
}

void _init(void) {
  registerhook(HOOK_TRUSTS_DB_LOADED, registercommands);
  registerhook(HOOK_TRUSTS_DB_CLOSED, deregistercommands);

  if(trustsdbloaded)
    registercommands(0, NULL);
}

void _fini(void) {
  deregisterhook(HOOK_TRUSTS_DB_LOADED, registercommands);
  deregisterhook(HOOK_TRUSTS_DB_CLOSED, deregistercommands);

  deregistercommands(0, NULL);
}
