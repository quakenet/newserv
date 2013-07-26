#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#include "../core/nsmalloc.h"
#include "../core/hooks.h"
#include "../control/control.h"
#include "../lib/version.h"

MODULE_VERSION("");

void nsmstats(int hookhum, void *arg);
int nsmhistogram(void *sender, int cargc, char **cargv);

void _init(void) {
  registerhook(HOOK_CORE_STATSREQUEST, &nsmstats);
  registercontrolhelpcmd("nsmhistogram", NO_DEVELOPER,1,&nsmhistogram,"Usage: nsmhistogram [pool id]\nDisplays memory information for given pool.");
}

void _fini(void) {
  deregisterhook(HOOK_CORE_STATSREQUEST, &nsmstats);
  deregistercontrolcmd("nsmhistogram", &nsmhistogram);
}

static char *formatmbuf(unsigned long count, size_t size, size_t realsize) {
  static char buf[1024];

  snprintf(buf, sizeof(buf), "%lu items, %luKb allocated for %luKb, %luKb (%.2f%%) overhead", count, (unsigned long)size / 1024, (unsigned long)realsize / 1024, (unsigned long)(realsize - size) / 1024, (double)(realsize - size) / (double)size * 100);
  return buf;
}

void nsmgenstats(struct nsmpool *pool, double *mean, double *stddev) {
  unsigned long long int sumsq = 0;
  struct nsminfo *np;

  *mean = (double)pool->size / pool->count;

  for (np=pool->first.next;np;np=np->next)
    sumsq+=np->size * np->size;

  *stddev = sqrtf((double)sumsq / pool->count - *mean * *mean);
}

void nsmstats(int hookhum, void *arg) {
  int i;
  char buf[1024], extra[1024];
  unsigned long totalcount = 0;
  size_t totalsize = 0, totalrealsize = 0;
  long level = (long)arg;

  for (i=0;i<MAXPOOL;i++) {
    struct nsmpool *pool=&nsmpools[i];
    size_t realsize;

    if (!pool->count)
      continue;

    realsize=pool->size + pool->count * sizeof(struct nsminfo) + sizeof(struct nsmpool);

    totalsize+=pool->size;
    totalrealsize+=realsize;
    totalcount+=pool->count;

    if(level > 10) {
      extra[0] = '\0';
      if(level > 100) {
        double mean, stddev;
        nsmgenstats(pool, &mean, &stddev);

        snprintf(extra, sizeof(extra), ", mean: %.2fKb stddev: %.2fKb", mean / 1024, stddev / 1024);
      }

      snprintf(buf, sizeof(buf), "NSMalloc: pool %2d (%10s): %s%s", i, nsmpoolnames[i]?nsmpoolnames[i]:"??", formatmbuf(pool->count, pool->size, realsize), extra);
      triggerhook(HOOK_CORE_STATSREPLY, buf);
    }
  }

  snprintf(buf, sizeof(buf), "NSMalloc: pool totals: %s", formatmbuf(totalcount, totalsize, totalrealsize));
  triggerhook(HOOK_CORE_STATSREPLY, buf);
}

struct nsmhistogram_s {
  size_t size;
  unsigned long freq;
} nsmhistogram_s;

static int hcompare_size(const void *a, const void *b) {
  return ((struct nsmhistogram_s *)a)->size - ((struct nsmhistogram_s *)b)->size;
}

static int hcompare_freq(const void *a, const void *b) {
  return ((struct nsmhistogram_s *)a)->freq - ((struct nsmhistogram_s *)b)->freq;
}

/*
 * since this is gonna process over a million allocations, we can't just do
 * it the simple O(n^2) way, so here's the crazy fast(er) way:
 * we create a list of all sizes and sort them, then store unique items
 * we can then calculate frequencies in O(n log n) time by searching this
 * sorted list using a binary chop...
 */
int nsmhistogram(void *sender, int cargc, char **cargv) {
  int i, max;
  unsigned int poolid;
  struct nsmpool *pool;
  struct nsminfo *np;
  struct nsmhistogram_s *freqs;
  size_t cval;
  unsigned long dst;

  if(cargc < 1)
    return CMD_USAGE;

  poolid = atoi(cargv[0]);
  if(poolid > MAXPOOL) {
    controlreply(sender, "Bad pool id.");
    return CMD_ERROR;
  }

  pool = &nsmpools[poolid];
  if(pool->count == 0) {
    controlreply(sender, "Pool is empty.");
    return CMD_ERROR;
  }

  freqs = (struct nsmhistogram_s *)malloc(sizeof(struct nsmhistogram_s) * pool->count);
  if(!freqs) {
    controlreply(sender, "Error allocating first BIG array.");
    return CMD_ERROR;
  }

  /* O(n) */
  memset(freqs, 0, sizeof(struct nsmhistogram_s) * pool->count);
  for(i=0,np=pool->first.next;np;np=np->next)
    freqs[i++].size = np->size;

  /* O(n log n) */
  qsort(freqs, pool->count, sizeof(struct nsmhistogram_s), hcompare_size);

  /* O(n) */
  cval = freqs[0].size;
  dst = 1;
  for(i=1;i<pool->count;i++) {
    if(cval != freqs[i].size) {
      cval = freqs[i].size;
      freqs[dst++].size = freqs[i].size;
    }
  }

  /* outer loop is O(n), inner loop is O(1 + log n) => O(n + n log n) => O(n log n) */
  for(np=pool->first.next;np;np=np->next) {
    unsigned long low = 0, high = dst;
    while(low < high) {
      unsigned long mid = (low + high) / 2;
      if (freqs[mid].size < np->size) {
        low = mid + 1;
      } else {
        high = mid;
      }
    }
    if((low > np->size) || (freqs[low].size != np->size)) {
      controlreply(sender, "ERROR");
      free(freqs);
      return CMD_ERROR;
    } else {
      freqs[low].freq++;
    }
  }

  /* O(n log n) */
  qsort(freqs, dst, sizeof(struct nsmhistogram_s), hcompare_freq);
  max = 2000;

  for(i=dst-1;i>=0&&max-->0;i--)
    controlreply(sender, "%10lu: %10lu", (unsigned long)freqs[i].size, (unsigned long)freqs[i].freq);

  free(freqs);
  return CMD_OK;
}
