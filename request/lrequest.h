#include <time.h>
#include "../lib/sstring.h"
#include "../channel/channel.h"
#include "../chanfix/chanfix.h"

#define LR_TOPX 5
#define LR_CFSCORE 12 
#define LR_MAXCHANLEN 29

int lr_requestl(nick *svc, nick *np, channel *cp, nick *lnick);
void lr_requeststats(nick *rqnick, nick *np);
