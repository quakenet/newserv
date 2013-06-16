#include "trusts.h"

void trusts_flush(void (*thflush)(trusthost_t *), void (*tgflush)(trustgroup_t *)) {
  trustgroup_t *tg;
  trusthost_t *th;
  time_t now = getnettime();
  int i;

  for ( i = 0; i < TRUSTS_HASH_GROUPSIZE ; i++ ) {
    for ( tg = trustgroupidtable[i]; tg; tg = tg -> nextbyid ) {
      if( tg->currenton > 0) {
        tg->lastused = now;
	tgflush(tg);
      }
    }
  }

  for ( i = 0; i < TRUSTS_HASH_HOSTSIZE ; i++ ) {
    for ( th = trusthostidtable[i]; th; th = th-> nextbyid ) {
      if( th->node->usercount > 0) {
        th->lastused = now;
        thflush(th);
      }
    }
  }
}

