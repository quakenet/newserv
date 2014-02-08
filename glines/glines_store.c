#include <stdio.h>
#include "../lib/version.h"
#include "../core/schedule.h"
#include "../control/control.h"
#include "glines.h"

MODULE_VERSION("");

static int glstore_savefile(const char *file) {
  FILE *fp;
  gline *gl;
  int count;

  fp = fopen(file, "w");

  if (!fp)
    return -1;

  count = 0;

  for (gl = glinelist; gl; gl = gl->next) {
    fprintf(fp, "%s %jd,%jd,%jd,%d,%s,%s\n",
      glinetostring(gl), (intmax_t)gl->expire, (intmax_t)gl->lastmod, (intmax_t)gl->lifetime,
      (gl->flags & GLINE_ACTIVE) ? 1 : 0,
      gl->creator->content, gl->reason ? gl->reason->content : "");
    count++;
  }

  fclose(fp);

  return count;
}

static int glstore_loadfile(const char *file) {
  FILE *fp;
  char mask[512], creator[512], reason[512];
  intmax_t expire, lastmod, lifetime;
  int active, count;
  gline *gl;

  fp = fopen(file, "r");

  if (!fp)
    return -1;

  count = 0;

  while (!feof(fp)) {
    if (fscanf(fp, "%[^ ]%jd,%jd,%jd,%d,%[^,],%[^\n]\n", mask, &expire, &lastmod, &lifetime, &active, creator, reason) != 7)
      continue;

    count++;

    gl = findgline(mask);

    if (gl)
      continue; /* Don't update existing glines. */

    gl = makegline(mask);

    if (!gl)
      continue;

    gl->creator = getsstring(creator, 512);

    gl->flags |= active ? GLINE_ACTIVE : 0;

    gl->reason = getsstring(reason, 512);
    gl->expire = expire;
    gl->lastmod = lastmod;
    gl->lifetime = lifetime;
    
    gl->next = glinelist;
    glinelist = gl;
  }

  fclose(fp);

  return count;
}

int glstore_save(void) {
  char path[512], srcfile[512], dstfile[512];
  int i, count;

  snprintf(path, sizeof(path), "%s.temp", GLSTORE_PATH_PREFIX);

  count = glstore_savefile(path);

  if (count < 0) {
    Error("glines", ERR_ERROR, "Could not save glines to %s", path);
    return -1;
  }

  for (i = GLSTORE_SAVE_FILES; i > 0; i--) {
    snprintf(srcfile, sizeof(srcfile), "%s.%d", GLSTORE_PATH_PREFIX, i - 1);
    snprintf(dstfile, sizeof(dstfile), "%s.%d", GLSTORE_PATH_PREFIX, i);
    (void) rename(srcfile, dstfile);
  }

  (void) rename(path, srcfile);

  return count;
}

int glstore_load(void) {
  char path[512];
  snprintf(path, sizeof(path), "%s.0", GLSTORE_PATH_PREFIX);

  return glstore_loadfile(path);
}

static int glines_cmdsaveglines(void *source, int cargc, char **cargv) {
  nick *sender = source;
  int count;

  count = glstore_save();

  if (count < 0)
    controlreply(sender, "An error occured while saving G-Lines.");
  else
    controlreply(sender, "Saved %d G-Line%s.", count, (count == 1) ? "" : "s");

  return CMD_OK;
}

static int glines_cmdloadglines(void *source, int cargc, char **cargv) {
  nick *sender = source;
  int count;

  count = glstore_load();

  if (count < 0)
    controlreply(sender, "An error occured while loading the G-Lines file.");
  else
    controlreply(sender, "Loaded %d G-Line%s.", count, (count == 1) ? "" : "s");

  return CMD_OK;
}

static void glines_sched_save(void *arg) {
  glstore_save();
}

void _init() {
  registercontrolhelpcmd("loadglines", NO_DEVELOPER, 0, glines_cmdloadglines, "Usage: loadglines\nForce load of glines.");
  registercontrolhelpcmd("saveglines", NO_DEVELOPER, 0, glines_cmdsaveglines, "Usage: saveglines\nForce save of glines.");

  schedulerecurring(time(NULL), 0, GLSTORE_SAVE_INTERVAL, &glines_sched_save, NULL);

  glstore_load();
}

void _fini() {
  deregistercontrolcmd("loadglines", glines_cmdloadglines);
  deregistercontrolcmd("saveglines", glines_cmdsaveglines);

  deleteschedule(NULL, glines_sched_save, NULL);
}
