#define CS_NODB
#include "../chanserv/chanserv.h"

static nick *fakeq;

void chanservuserhandler(nick *target, int message, void **params);

void _init(void) {
  fakeq=registerlocaluser("Q","TheQBot","CServe.quakenet.org",
                                 "The Q Bot","Q",
                                 UMODE_INV|UMODE_SERVICE|UMODE_DEAF|UMODE_OPER|UMODE_ACCOUNT,
                                 &chanservuserhandler);  
}

void _fini(void) {
  if(fakeq)
    deregisterlocaluser(fakeq, "Leaving");
}
