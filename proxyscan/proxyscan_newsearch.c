#include "../newsearch/newsearch.h"

struct searchNode *pscan_parse(searchCtx *ctx, int argc, char **argv);

void _init() {
  registersearchterm(reg_nicksearch, "pscan", pscan_parse, 0, "");
}

void _fini() {
  deregistersearchterm(reg_nicksearch, "pscan", pscan_parse);
}
