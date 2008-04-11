#include <string.h>
#include <stdio.h>

#include "../core/hooks.h"
#include "../core/config.h"
#include "../usercount/usercount.h"
#include "../lib/sstring.h"
#include "../server/server.h"
#include "../core/schedule.h"
#include "../core/error.h"
#include "../lib/sha1.h"
#include "../lib/hmac.h"

#include "fsample.h"

#define RESOLUTION 5
#define SAMPLES (60 / RESOLUTION) * 60 * 24 * 7 * 4 * 12

static void gr_newserver(int hook, void *arg);
static void gr_lostserver(int hook, void *arg);
static void tick(void *arg);
static void openserver(int servernum);
static void closeserver(int servernum);
static inline int servermonitored(int servernum);

static fsample_m *servergraphs[MAXSERVERS];
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

static inline int servermonitored(int servernum) {
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

  if(servermonitored(servernum))
    return;

  if(serverlist[servernum].linkstate == LS_INVALID)
    return;

  SHA1Init(&sha);
  SHA1Update(&sha, (unsigned char *)serverlist[servernum].name->content, serverlist[servernum].name->length);
  SHA1Final(digest, &sha);

  snprintf(filename, sizeof(filename), "%s/%s", path->content, hmac_printhex(digest, hexdigest, sizeof(digest)));

  f = fopen(appendsuffix(filename, ".name"), "w");
  if(!f)
    return;

  fprintf(f, "%s\n", serverlist[servernum].name->content);
  fclose(f);

  /* 0: seconds
     1: minutes
     2: hours
     3: days
     4: weeks
     5: months
   */

  servergraphs[servernum] = fsopen_m(5, appendsuffix(filename, ".0"), SAMPLES);
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

  if(t % RESOLUTION != 0)
    return;

  for(i=0;i<MAXSERVERS;i++)
    if(servermonitored(i))
      fsset_m(servergraphs[i], t / RESOLUTION, servercount[i]);
}


