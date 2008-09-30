#include "../nterfacer/nterfacer.h"
#include "../nick/nick.h"
#include "../lib/strlfunc.h"
#include "trusts_migration.h"
#include <stdio.h>
#include <string.h>

#define Stringify(x) __Stringify(x)
#define __Stringify(x) #x

static void tm_trustdump(trustmigration *tm);

static void tm_fini(trustmigration *tm, int errcode) {
  tm->fini(tm, errcode);

  trusts_migration_stop(tm);
}

static int tm_parsegroup(trustmigration *tm, unsigned int id, const char *oline) {
  char *line, *createdby, *contact, *comment, *name;
  unsigned int trustedfor, maxperident, mode, maxseen;
  unsigned long expires, lastseen, lastmaxusereset;
  char xbuf[1024];
  int pos;

/* ticket35153,14,20,1,1,17,1879854575,1222639249,0,nterfacer,Qwhois&2120764,Non-Commercial Bouncer (Created by: doomie)
   name       ,current
                 ,trustedfor
                    ,mode
                      ,maxperident
                        ,maxseen
                           ,expires  ,lastseen   ,lastmaxusereset
                                                   ,createdby,contact       ,comment
*/
  int r;

  strlcpy(xbuf, oline, sizeof(xbuf));
  name = xbuf;

  line = strchr(name, ',');
  if(!line)
    return 1;
  *line++ = '\0';

  r = sscanf(line, "%*u,%u,%u,%u,%u,%lu,%lu,%lu,%n",
             /*current, */ &trustedfor, &mode, &maxperident, 
             &maxseen, &expires, &lastseen, &lastmaxusereset, &pos);
  if(r != 7)
    return 2;

  createdby = &line[pos];
  contact = strchr(createdby, ',');
  if(!contact)
    return 3;
  *contact++ = '\0';

  comment = strchr(contact, ',');
  if(!comment)
    return 4;
  *comment++ = '\0';  

  tm->group(tm, id, name, trustedfor, mode, maxperident, maxseen, (time_t)expires, (time_t)lastseen, (time_t)lastmaxusereset, createdby, contact, comment);
  return 0;
}

static int tm_parsehost(trustmigration *tm, unsigned int id, char *line) {
  unsigned int max;
  unsigned long lastseen;
  char *ip, xbuf[1024];

/* 213.230.192.128/26,20,23,1222732944
   ip                ,cur,max,lastseen */

  strlcpy(xbuf, line, sizeof(xbuf));
  ip = line = xbuf;

  line = strchr(line, ',');
  if(!line)
    return 5;
  *line++ = '\0';

  if(sscanf(line, "%*u,%u,%lu", /*current, */&max, &lastseen) != 2)
    return 6;

  tm->host(tm, id, ip, max, lastseen);
  return 0;
}

static void tm_stage2(int failure, int linec, char **linev, void *tag) {
  trustmigration *tm = tag;
  char *finishline;
  unsigned int groupcount, totallines, i, dummy;

#ifdef DEBUG
  Error("trusts_migration", ERR_INFO, "Total lines: %d", linec);

  for(i=0;i<linec;i++)
    Error("trusts_migration", ERR_INFO, "Line %d: |%s|", i, i, linev[i]);
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
    if(type == 'G' || type == 'H') {
      char *realline;
      unsigned int id;
      int pos, ret;

      if(sscanf(linestart, "#%u,%n", &id, &pos) != 1) {
        tm_fini(tm, 10);
        return;
      }

      if(id != tm->cur) {
        /* ignore, this one is missing and we've received a later one */
        /* we'll get this one again when we ask for it */
        continue;
      }

      realline = &linestart[pos];
      if(type == 'G') {
        ret = tm_parsegroup(tm, id, realline);
      } else {
        ret = tm_parsehost(tm, id, realline);
      }
      if(ret) {
        tm_fini(tm, ret);
        return;
      }
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
    tm->fini(tm, 0);
    return;
  }

  tm->cur++;

  snprintf(buf, sizeof(buf), "trustdump #%d 1", tm->cur);
  tm->schedule = nterfacer_sendline("R", "relay", 4, (char *[]){"2", "^(Start ID cannot exceed current maximum group ID \\(#\\d+\\)|End of list, 1 groups and \\d lines returned\\.)$", "O", buf}, tm_stage2, tm);
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

  Error("trusts_migration", ERR_INFO, "Migration in progress, total groups: %d", count);

  tm->count = count;

  tm_trustdump(tm);
}

trustmigration *trusts_migration_start(TrustMigrationGroup group, TrustMigrationHost host, TrustMigrationFini fini) {
  trustmigration *tm = malloc(sizeof(trustmigration));
  if(!tm)
    return NULL;

  tm->group = group;
  tm->host = host;
  tm->fini = fini;
  tm->count = 0;
  tm->cur = 0;

  tm->schedule = nterfacer_sendline("R", "relay", 4, (char *[]){"1", "1", "O", "trustdump #9999999 1"}, tm_stage1, tm);
  return tm;
}

void trusts_migration_stop(trustmigration *tm) {
  if(tm->schedule) {
    nterfacer_freeline(tm->schedule);
    tm->schedule = NULL;
  }

  free(tm);
}
