#include "../nick/nick.h"
#include "../channel/channel.h"

#define RQ_FASTTRACK_TARGETS 2
#define RQ_FASTTRACK_TIMEOUT (60 * 60)

int rq_initfasttrack(void);
void rq_finifasttrack(void);

int rq_tryfasttrack(nick *np);
