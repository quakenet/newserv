/* Allocators for chanstats */

#include "chanstats.h"
#include "../core/nsmalloc.h"

void cstsfreeall() {
  nsfreeall(POOL_CHANSTATS);
}

chanstats *getchanstats() {
  return nsmalloc(POOL_CHANSTATS, sizeof(chanstats));
}

void freechanstats(chanstats *csp) {
  nsfree(POOL_CHANSTATS, csp);
}
