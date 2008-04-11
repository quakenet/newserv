#include <string.h>
#include <stdio.h>

#include "../core/hooks.h"
#include "../core/config.h"
#include "../usercount/usercount.h"
#include "../lib/sstring.h"
#include "../core/schedule.h"
#include "../core/error.h"
#include "../lib/sha1.h"
#include "../lib/hmac.h"

#include "graphing.h"

static void gr_newserver(int hook, void *arg);
static void gr_lostserver(int hook, void *arg);
static void tick(void *arg);
static void openserver(int servernum);
static void closeserver(int servernum);

fsample_m *servergraphs[MAXSERVERS];
static void *sched;

static sstring *path;

void _init(void) {
  int i;

  memset(servergraphs, 0, sizeof(servergraphs));

  path = getcopyconfigitem("graphing", "path", "", 100);
  if(!path || !path->content || !path->content[0]) {
    Error("graphing", ERR_WARNING, "Path not set, not loaded.");
    return;
  }

  for(i=0;i<MAXSERVERS;i++)
    openserver(i);

  registerhook(HOOK_SERVER_NEWSERVER, gr_newserver);
  registerhook(HOOK_SERVER_LOSTSERVER, gr_lostserver);

  sched = schedulerecurring(time(NULL), 0, 1, tick, NULL);
}

void _fini(void) {
  int i;
  if(sched)
    deleteschedule(sched, tick, NULL);

  for(i=0;i<MAXSERVERS;i++)
    if(servermonitored(i))
      closeserver(i);

  freesstring(path);

  deregisterhook(HOOK_SERVER_NEWSERVER, gr_newserver);
  deregisterhook(HOOK_SERVER_LOSTSERVER, gr_lostserver);
}

int servermonitored(int servernum) {
  return servergraphs[servernum] != NULL;
}

static char *appendsuffix(char *prefix, char *suffix) {
  static char buf[1024];

  snprintf(buf, sizeof(buf), "%s%s", prefix, suffix);

  return buf;
}

static void openserver(int servernum) {
  unsigned char digest[SHA1_DIGESTSIZE];
  char filename[512], hexdigest[sizeof(digest)*2 + 1];
  FILE *f;
  SHA1_CTX sha;
  fsample_m *m;

  if(servermonitored(servernum))
    return;

  if(serverlist[servernum].linkstate == LS_INVALID)
    return;

  SHA1Init(&sha);
  SHA1Update(&sha, (unsigned char *)serverlist[servernum].name->content, serverlist[servernum].name->length);
  SHA1Final(digest, &sha);

  snprintf(filename, sizeof(filename), "%s/%s", path->content, hmac_printhex(digest, hexdigest, sizeof(digest)));

  f = fopen(appendsuffix(filename, ".name"), "w");
  if(!f) {
    Error("graphing", ERR_WARNING, "Unable to create name file for %s (%s.name)", serverlist[servernum].name->content, filename);
    return;
  }

  fprintf(f, "%s\n", serverlist[servernum].name->content);
  fclose(f);

  /* 0: seconds
     1: minutes
     2: hours
     3: days
     4: weeks
     5: months
   */

  m = fsopen_m(GRAPHING_DATASETS, appendsuffix(filename, ".0"), SAMPLES, (CoreHandlerAddFn)registercorehandler, (CoreHandlerDelFn)deregistercorehandler);
  if(!m) {
    Error("graphing", ERR_WARNING, "Unable to create main backing store for %s (%s.0)", serverlist[servernum].name->content, filename);
    return;
  }
/*
  if(!fsadd_m(m, appendsuffix(filename, ".1"), PERMINUTE, fsapmean, (void *)PERMINUTE) ||
     !fsadd_m(m, appendsuffix(filename, ".2"), PERMINUTE * 24, fsapmean, (void *)(PERMINUTE * 24)) ||
     !fsadd_m(m, appendsuffix(filename, ".3"), PERMINUTE * 24 * 7, fsapmean, (void *)(PERMINUTE * 24 * 7)) ||
     !fsadd_m(m, appendsuffix(filename, ".4"), PERMINUTE * 24 * 7 * 4, fsapmean, (void *)(PERMINUTE * 24 * 7 * 4)) ||
     !fsadd_m(m, appendsuffix(filename, ".5"), PERMINUTE * 24 * 7 * 4 * 12, fsapmean, (void *)(PERMINUTE * 24 * 7 * 4 * 12)))
  {
    Error("graphing", ERR_WARNING, "Unable to create main side store for %s (%s.X)", serverlist[servernum].name->content, filename);
    fsclose_m(m);
    return;
  }
*/
  servergraphs[servernum] = m;
}

static void closeserver(int servernum) {
  if(!servermonitored(servernum))
    return;

  fsclose_m(servergraphs[servernum]);
  servergraphs[servernum] = NULL;
}

static void gr_newserver(int hook, void *arg) {
  long num = (long)arg;

  openserver(num);
}

static void gr_lostserver(int hook, void *arg) {
  long num = (long)arg;

  closeserver(num);
}

static void tick(void *arg) {
  time_t t = time(NULL);
  int i;

  if(t % GRAPHING_RESOLUTION != 0)
    return;

  for(i=0;i<MAXSERVERS;i++)
    if(servermonitored(i))
      fsset_m(servergraphs[i], t / GRAPHING_RESOLUTION, servercount[i]);
}


