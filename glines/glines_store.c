#include <stdio.h>
#include "../core/schedule.h"
#include "../control/control.h"
#include "glines.h"

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
      gl->creator->content, gl->reason->content);
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

    gl->flags = active ? GLINE_ACTIVE : 0;

    gl->reason = getsstring(reason, 512);
    gl->expire = expire;
    gl->lastmod = lastmod;
    gl->lifetime = lifetime;
    
    gl->next = glinelist;
    glinelist = gl;
  }

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

static int glines_cmdcleanupglines(void *source, int cargc, char **cargv) {
  nick *sender = source;
  gline **pnext, *gl;
  int count;
  time_t now;
  
  count = 0;
  time(&now);
  
  for (pnext = &glinelist; *pnext;) {
    gl = *pnext;
    
    /* Remove inactivate glines that have been last changed more than a week ago */
    if (!(gl->flags & GLINE_ACTIVE) && gl->lastmod < now - 7 * 24 * 60 * 60) {
      gline_destroy(gl, 0, 1);
      count++;
    } else {
      pnext = &((*pnext)->next);
    }
    
    if (!*pnext)
      break;
  }
  
  controlwall(NO_OPER, NL_GLINES, "%s CLEANUPGLINES'd %d G-Lines.",
    controlid(sender), count);
  
  controlreply(sender, "Done.");
  
  return CMD_OK;
}

static int glines_cmdsyncglines(void *source, int cargc, char **cargv) {
  nick *sender = source;
  gline *gl;
  int count;

  count = 0;

  for (gl = glinelist; gl; gl = gl->next) {
    gline_propagate(gl);
    count++;
  }
  
  controlwall(NO_OPER, NL_GLINES, "%s SYNCGLINE'd %d G-Lines.",
    controlid(sender), count);

  controlreply(sender, "Done.");

  return CMD_OK;
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
  registercontrolhelpcmd("cleanupglines", NO_OPER, 0, glines_cmdcleanupglines, "Usage: cleanupglines\nDestroys all deactivated G-Lines.");
  registercontrolhelpcmd("syncglines", NO_DEVELOPER, 0, glines_cmdsyncglines, "Usage: syncglines\nSends all G-Lines to all other servers.");
  registercontrolhelpcmd("loadglines", NO_DEVELOPER, 0, glines_cmdloadglines, "Usage: loadglines\nForce load of glines.");
  registercontrolhelpcmd("saveglines", NO_DEVELOPER, 0, glines_cmdsaveglines, "Usage: saveglines\nForce save of glines.");

  schedulerecurring(time(NULL), 0, GLSTORE_SAVE_INTERVAL, &glines_sched_save, NULL);

  glstore_load();
}

void _fini() {
  deregistercontrolcmd("cleanupglines", glines_cmdcleanupglines);
  deregistercontrolcmd("syncglines", glines_cmdsyncglines);
  deregistercontrolcmd("loadglines", glines_cmdloadglines);
  deregistercontrolcmd("saveglines", glines_cmdsaveglines);

  deleteschedule(NULL, glines_sched_save, NULL);
}
