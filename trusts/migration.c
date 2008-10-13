#include "../nterfacer/nterfacer.h"
#include "../lib/strlfunc.h"
#include "../core/nsmalloc.h"
#include "trusts.h"
#include <stdio.h>
#include <string.h>

#define Stringify(x) __Stringify(x)
#define __Stringify(x) #x

static void tm_trustdump(trustmigration *tm);

static void tm_fini(trustmigration *tm, int errcode) {
  tm->fini(tm->tag, errcode);

  if(tm->schedule) {
    nterfacer_freeline(tm->schedule);
    tm->schedule = NULL;
  }

  nsfree(POOL_TRUSTS, tm);
}

void migration_stop(trustmigration *tm) {
  tm_fini(tm, MIGRATION_STOPPED);
}

static void tm_stage2(int failure, int linec, char **linev, void *tag) {
  trustmigration *tm = tag;
  char *finishline;
  unsigned int groupcount, totallines, i, dummy;

#ifdef TRUSTS_MIGRATION_DEBUG
  Error("trusts", ERR_INFO, "Migration total lines: %d", linec);

  for(i=0;i<linec;i++)
    Error("trusts", ERR_INFO, "Migration line %d: |%s|", i, linev[i]);
#endif

  tm->schedule = NULL;
  if(failure || (linec < 1)) {
    tm_fini(tm, 7);
    return;
  }

  finishline = linev[linec - 1];
  if(sscanf(finishline, "Start ID cannot exceed current maximum group ID (#%u).", &dummy) == 1) {
    /* the list was truncated while we were reading it, we're done! */
    tm_fini(tm, 0);
    return;
  }

  if(sscanf(finishline, "End of list, %u groups and %u lines returned.", &groupcount, &totallines) != 2) {
    tm_fini(tm, 8);
    return;
  }

  if(totallines != linec - 1) {
    tm_fini(tm, 9);
    return;
  }

  for(i=0;i<linec-1;i++) {
    char *linestart = &linev[i][2], type = linev[i][0];
    if(type == 'G') {
      trustgroup tg;

      if(!parsetg(linestart, &tg, 1)) {
        tm_fini(tm, 150);
        return;
      }

      if(tg.id >= tm->cur)
        tm->group(tm->tag, &tg);
          
      freesstring(tg.name);
      freesstring(tg.createdby);
      freesstring(tg.contact);
      freesstring(tg.comment);

      if(tg.id < tm->cur) {
        tm_fini(tm, 11);
        return;
      }

      if(tg.id > tm->cur)
        tm->cur = tg.id;
    } else if (type == 'H') {
      trusthost th;
      unsigned int groupid;

      if(!parseth(linestart, &th, &groupid, 1)) {
        tm_fini(tm, 151);
        return;
      }

      if(groupid < tm->cur) {
        tm_fini(tm, 11);
        return;
      }

      if(groupid > tm->cur)
        tm->cur = groupid;

      tm->host(tm->tag, &th, groupid);
    } else {
      tm_fini(tm, 11);
      return;
    }
  }

  tm_trustdump(tm);
}

static void tm_trustdump(trustmigration *tm) {
  char buf[100];

  if(tm->cur >= tm->count) {
    tm_fini(tm, 0);
    return;
  }

  tm->cur++;

  snprintf(buf, sizeof(buf), "trustdump #%d 1", tm->cur);
  tm->schedule = nterfacer_sendline("R", "relay", 4, (char *[]){"2", "^(Start ID cannot exceed current maximum group ID \\(#\\d+\\)|End of list, 1 groups and \\d+ lines returned\\.)$", "O", buf}, tm_stage2, tm);
}

static void tm_stage1(int failure, int linec, char **linev, void *tag) {
  int count;
  trustmigration *tm = tag;

  tm->schedule = NULL;

  if(failure || (linec != 1)) {
    tm_fini(tm, 12);
    return;
  }

  if(sscanf(linev[0], "Start ID cannot exceed current maximum group ID (#%u).", &count) != 1) {
    tm_fini(tm, 13);
    return;
  }

  Error("trusts", ERR_INFO, "Migration in progress, total groups: %d", count);

  tm->count = count;

  tm_trustdump(tm);
}

trustmigration *migration_start(TrustMigrationGroup group, TrustMigrationHost host, TrustMigrationFini fini, void *tag) {
  trustmigration *tm = nsmalloc(POOL_TRUSTS, sizeof(trustmigration));
  if(!tm)
    return NULL;

  tm->group = group;
  tm->host = host;
  tm->fini = fini;
  tm->count = 0;
  tm->cur = 0;
  tm->tag = tag;

  tm->schedule = nterfacer_sendline("R", "relay", 4, (char *[]){"1", "1", "O", "trustdump #9999999 1"}, tm_stage1, tm);
  if(!tm->schedule) {
    nsfree(POOL_TRUSTS, tm);
    return NULL;
  }

  return tm;
}
