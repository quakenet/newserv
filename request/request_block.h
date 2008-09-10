#include "../nick/nick.h"
#include "../channel/channel.h"

typedef struct {
  int count;
  time_t created;
  time_t expire;
} rq_flood;

typedef struct {
  sstring *pattern;
  sstring *reason;

  sstring *creator;
  time_t created;
  time_t expires;
} rq_block;

extern array rqblocks;

#define RQ_BLOCKFILE "data/rqblocks"
#define RQ_BLOCKLEN 256

#define RQ_SPAMCOUNT 5
#define RQ_SPAMBLOCK 3600

int rq_initblocks(void);
void rq_finiblocks(void);

int rq_loadblocks(void);
int rq_saveblocks(void);

/* long-term blocks */
rq_block *rq_findblock(const char *pattern);
void rq_addblock(const char *pattern, const char *reason, const char *creator, time_t created, time_t expires);
int rq_removeblock(const char *pattern);

/* anti-spam blocks */
int rq_isspam(nick *np);
time_t rq_blocktime(nick *np);
