/*
  nterface logging functions
  Copyright (C) 2003-2004 Chris Porter.

  v1.04
    - wrote own stat function as linux sucks
    - modified permissions to allow paul to read the log files.
  v1.03
    - modified debug newserv level
  v1.02
    - merged logging and stdout stuff
    - added log rotation
  v1.01
    - added new logging to file feature
    - added sanitation
*/

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>

#include "../core/error.h"
#include "../lib/sstring.h"
#include "../core/schedule.h"
#include "logging.h"
#include "library.h"
#include "esockets.h"

#define LOG_PERMISSIONS S_IRUSR | S_IWUSR | S_IRGRP
#define DIR_PERMISSIONS LOG_PERMISSIONS | S_IXUSR | S_IXGRP

void sanitise(char *buf) {
  char *p;
  for(p=buf;*p;p++)
    if(!isprint(*p))
      *p = '!';
}

/* 1 if exists, 0 if it doesn't */
/* linux sucks so we can't use stat */
int exists(char *filename) {
  FILE *fp = fopen(filename, "r");
  if(fp) {
    fclose(fp);
    return 1;
  }
  return 0;
}

int ifexistrename(char *from, char *to) {
  if(!exists(from)) {
    return 0;
  } else {
    return rename(from, to);
  }
}

void nterface_rotate_log(void *arg) {
  struct nterface_auto_log *us = (struct nterface_auto_log *)arg;
  char *filename, *filename2, *swap;
  int filenamelen = strlen(us->filename->content) + 15, i, aborted = 0;
  FILE *newlog;

  if(!us)
    return;

  filename = ntmalloc(filenamelen);
  MemCheck(filename);
  filename2 = ntmalloc(filenamelen);
  if(!filename2) {
    MemError();
    ntfree(filename);
    return;
  }

  /* ADD us->log checks, if rotation failed */
  
  snprintf(filename, filenamelen, "%s.n", us->filename->content);

  newlog = fopen(filename, "w");
  if(!newlog) {
    nterface_log(us, NL_WARNING, "Unable to rotate log %s: %s", us->filename->content, strerror(errno));
  } else {
    snprintf(filename, filenamelen, "%s.%d", us->filename->content, ROTATE_LOG_TIMES);
    for(i=ROTATE_LOG_TIMES-1;i>=0;i--) {
      snprintf(filename2, filenamelen, "%s.%d", us->filename->content, i);
      if(ifexistrename(filename2, filename)) 
        break;
      swap = filename;
      filename = filename2;
      filename2 = swap;
    }
    if(i >= 0) {
      nterface_log(us, NL_WARNING, "Error occured during log rotation (rename %s to %s), log files will have a gap: %s", filename2, filename, strerror(errno));
      aborted = 1;
    } else {
      if(ifexistrename(us->filename->content, filename)) {
        nterface_log(us, NL_WARNING, "Error occured during log rotation (rename %s to %s), log files will have a gap (.0 will be missing): %s", us->filename->content, filename, strerror(errno));
        aborted = 1;
      } else {
        snprintf(filename, filenamelen, "%s.n", us->filename->content);
        if(ifexistrename(filename, us->filename->content)) {
          nterface_log(us, NL_WARNING, "Error occured during log rotation (rename %s to %s): %s", filename, us->filename->content, strerror(errno));
          snprintf(filename, filenamelen, "%s.0", us->filename->content);
          if(ifexistrename(filename, us->filename->content))
            nterface_log(us, NL_WARNING, "Error occured during log rotation (rename %s to %s), now logging to: %s as rename back failed: %s", filename, us->filename->content, filename, strerror(errno));
          aborted = 1;
        } else {
          nterface_log(us, NL_SYSTEM, "Log rotated out of use");
          fclose(us->log);
          us->log = newlog;
          nterface_log(us, NL_SYSTEM, "Log rotated into use");
          nterface_log(us, NL_INFO|NL_DONT_LOG, "Log rotated");
        }
      }
    }
  }

  if(aborted) {
    fclose(newlog);
    snprintf(filename, filenamelen, "%s.n", us->filename->content);
    unlink(filename);
  }

  ntfree(filename);
  ntfree(filename2);
}

int direxists(char *filename) {
  char *p = filename, *lastp = NULL;
  for(;*p;p++)
    if(*p == '/')
      lastp = p;

  if(lastp) {
    int ret = 0;
    char *newp = ntmalloc(lastp - filename + 1);
    if(!newp) {
      MemError();
      return 0;
    }

    memcpy(newp, filename, lastp - filename);
    newp[lastp - filename] = '\0';

    if(exists(newp)) {
      chmod(newp, DIR_PERMISSIONS);
    } else {
      ret = mkdir(newp, DIR_PERMISSIONS);
    }

    ntfree(newp);
    if(ret)
      return 0;
  }
  return 1;
}

struct nterface_auto_log *nterface_open_log(char *name, char *filename, int debug) {
  struct nterface_auto_log *us = ntmalloc(sizeof(struct nterface_auto_log));
  MemCheckR(us, NULL);

  us->filename = getsstring(filename, strlen(filename));
  if(!us->filename) {
    MemError();
    ntfree(us);
  }

  us->name = getsstring(name, strlen(name));
  if(!us->name) {
    MemError();
    freesstring(us->filename);
    ntfree(us);
  }

  if(direxists(filename)) {
    us->log = fopen(filename, "a");
    if(!us->log)
      Error(name, ERR_INFO, "Unable to open log %s: %s", filename, strerror(errno));
  } else {
    Error(name, ERR_INFO, "Unable to open log %s, as directory does not exist and couldn't be created: %s", filename, strerror(errno));
  }

  us->debug = debug;

  nterface_log(us, NL_SYSTEM, "Log opened");
  
  chmod(filename, LOG_PERMISSIONS);

  us->schedule = schedulerecurring((time(NULL)/ROTATE_LOG_EVERY + 1) * ROTATE_LOG_EVERY, 0, ROTATE_LOG_EVERY, nterface_rotate_log, us);
  if(!us->schedule)
    nterface_log(us, NL_WARNING, "Unable to schedule log rotation!");

  return us;
}

struct nterface_auto_log *nterface_close_log(struct nterface_auto_log *ourlog) {
  if(ourlog) {
    if(ourlog->schedule)
      deleteschedule(ourlog->schedule, nterface_rotate_log, ourlog);
    freesstring(ourlog->name);
    freesstring(ourlog->filename);
    nterface_log(ourlog, NL_SYSTEM, "Log closed");
    if(ourlog->log)
      fclose(ourlog->log);
    ntfree(ourlog);
  }
  return NULL;
}

void nterface_log(struct nterface_auto_log *ourlog, int loglevel, char *format, ...) {
  char buf[MAX_BUFSIZE * 2], timebuf[100], loglevelname = '\0'; /* lazy */
  int newserverrorcode = -1;
  va_list va;
  time_t now;
  struct tm *tm;

  if(loglevel & NL_INFO) {
    loglevelname = 'I';
    newserverrorcode = ERR_INFO;
  } else if(loglevel & NL_DEBUG) {
    if(!ourlog || ourlog->debug) {
      loglevelname = 'D';
      newserverrorcode = ERR_INFO; /* debug only shows up if debug newserv is enabled */
    }
  } else if(loglevel & NL_WARNING) {
    loglevelname = 'W';
    newserverrorcode = ERR_WARNING;
  } else if(loglevel & NL_ERROR) {
    loglevelname = 'E';
    newserverrorcode = ERR_ERROR;
  } else if(loglevel & NL_SYSTEM) {
    loglevelname = 'S';
  }

  if(loglevel & NL_LOG_ONLY)
    newserverrorcode = -1;

  if(!loglevelname)
    return;

  va_start(va, format);
  vsnprintf(buf, sizeof(buf), format, va);
  va_end(va);

  if(newserverrorcode != -1)
    Error(ourlog?ourlog->name->content:"nterface-unknown", newserverrorcode, "%s", buf);

  sanitise(buf);

  if(!ourlog || !ourlog->log || (loglevel & NL_DONT_LOG))
    return;

  /* thanks splidge */
  now = time(NULL);
  tm = gmtime(&now);
  strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", tm);
  fprintf(ourlog->log, "[%s] %c: %s\n", timebuf, loglevelname, buf);
  fflush(ourlog->log);
}
