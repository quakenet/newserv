#include "../nick/nick.h"
#include "../channel/channel.h"

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

int rq_initblocks(void);
void rq_finiblocks(void);

int rq_loadblocks(void);
int rq_saveblocks(void);

/* long-term blocks */
rq_block *rq_findblock(const char *pattern);
void rq_addblock(const char *pattern, const char *reason, const char *creator, time_t created, time_t expires);
int rq_removeblock(const char *pattern);
