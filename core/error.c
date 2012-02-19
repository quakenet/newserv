/* error.c */

#include <stdarg.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include "error.h"
#include "hooks.h"

FILE *logfile;

static corehandler *coreh, *coret;

char *sevtostring(int severity) {
  switch(severity) {
    case ERR_DEBUG:
      return "debug";
    
    case ERR_INFO:
      return "info";
      
    case ERR_WARNING:
      return "warning";
    
    case ERR_ERROR:
      return "error";
      
    case ERR_FATAL:
      return "fatal error";
    
    case ERR_STOP:
      return "terminal error";
      
    default:
      return "unknown error";
  }
}

void reopen_logfile(int hooknum, void *arg) {
  if (logfile)
    fclose(logfile);
  
  logfile=fopen("logs/newserv.log","a");
}

void init_logfile() {
  logfile=fopen("logs/newserv.log","a");
  if (!logfile) {
    fprintf(stderr,"Failed to open logfile...\n");
  }
  registerhook(HOOK_CORE_SIGUSR1, reopen_logfile);
}

void fini_logfile() {
  if (logfile) 
    fclose(logfile);
  deregisterhook(HOOK_CORE_SIGUSR1, reopen_logfile);
  if (logfile)
    fclose(logfile);
}

void Error(char *source, int severity, char *reason, ... ) {
  char buf[512];
  va_list va;
  struct tm *tm;
  time_t now;
  char timebuf[100];
  struct error_event evt;
    
  va_start(va,reason);
  vsnprintf(buf,512,reason,va);
  va_end(va);
  
  evt.severity=severity;
  evt.message=buf;
  evt.source=source;
  triggerhook(HOOK_CORE_ERROR, &evt);
  
  if (severity>ERR_DEBUG) {
    now=time(NULL);
    tm=gmtime(&now);
    strftime(timebuf,100,"%Y-%m-%d %H:%M:%S",tm);
    fprintf(stderr,"[%s] %s(%s): %s\n",timebuf,sevtostring(severity),source,buf);
    fflush(stderr);

    if (logfile)  {
      fprintf(logfile,"[%s] %s(%s): %s\n",timebuf,sevtostring(severity),source,buf);
      fflush(logfile);
    }
  }
  
  if (severity>=ERR_STOP) {
    fprintf(stderr,"Terminal error occured, exiting...\n");
    triggerhook(HOOK_CORE_STOPERROR, NULL);
    exit(0);
  }
}

void handlecore(void) { 
  corehandler *n;

  /* no attempt is made to clean these up */
  for(n=coreh;coreh;n=coreh->next)
    (n->fn)(n->arg);
}

corehandler *registercorehandler(CoreHandlerFn fn, void *arg) {
  corehandler *c = (corehandler *)malloc(sizeof(corehandler));
  /* core if we can't allocate!! */

  c->fn = fn;
  c->arg = arg;
  c->next = NULL;
  c->prev = coret;
  coret = c->prev;

  if(!coreh)
    coreh = c;

  return c;
}

void deregistercorehandler(corehandler *c) {
  if(!c->prev) {
    coreh = c->next;
  } else {
    c->prev->next = c->next;
  }

  if(!c->next) {
    coret = c->prev;
  } else {
    c->next->prev = c->prev;
  }

  free(c);
}

