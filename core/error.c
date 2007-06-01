/* error.c */

#include <stdarg.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include "error.h"
#include "hooks.h"

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

void Error(char *source, int severity, char *reason, ... ) {
  char buf[512];
  va_list va;
  struct tm *tm;
  time_t now;
  char timebuf[100];
  
  va_start(va,reason);
  vsnprintf(buf,512,reason,va);
  va_end(va);
  
  if (severity>ERR_DEBUG) {
    now=time(NULL);
    tm=gmtime(&now);
    strftime(timebuf,100,"%Y-%m-%d %H:%M:%S",tm);
    fprintf(stderr,"[%s] %s(%s): %s\n",timebuf,sevtostring(severity),source,buf);
  }
  
  if (severity>=ERR_STOP) {
    fprintf(stderr,"Terminal error occured, exiting...\n");
    triggerhook(HOOK_CORE_STOPERROR, NULL);
    exit(0);
  }
}
