#include "trusts.h"

trustgroup *tglist;

int trusts_loaddb(void);
void trusts_closedb(void);

void _init(void) {
  if(!trusts_loaddb())
    return;
}

void _fini(void) {
  trusts_closedb();
}
