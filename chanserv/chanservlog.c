#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdio.h>
#include "chanserv.h"
#include "../core/hooks.h"
#include "../core/error.h"

int logfd;

/* When we get a sigusr1, reopen the logfile */
void cs_usr1handler(int hooknum, void *arg) {
  Error("chanserv",ERR_INFO,"Reopening logfile.");

  if (logfd>=0)
    close(logfd);

  logfd=open("chanservlog",O_WRONLY|O_CREAT|O_APPEND,S_IRUSR|S_IWUSR);
}

void cs_initlog() {
  logfd=open("chanservlog",O_WRONLY|O_CREAT|O_APPEND,S_IRUSR|S_IWUSR);
  registerhook(HOOK_CORE_SIGUSR1, cs_usr1handler);
}

void cs_closelog() {
  if (logfd>=0)
    close(logfd);
  deregisterhook(HOOK_CORE_SIGUSR1, cs_usr1handler);
}

void cs_log(nick *np, char *event, ... ) {
  char buf[512];
  char buf2[1024];
  char userbuf[512];
  va_list va;
  char timebuf[TIMELEN];
  int len;
  time_t now;

  if (logfd<0)
    return; 

  va_start(va,event);
  vsnprintf(buf,512,event,va);
  va_end(va);

  if (np) {
    snprintf(userbuf,511,"%s!%s@%s [%s%s] ",np->nick,np->ident,np->host->name->content,
	     getreguserfromnick(np)?"auth ":"noauth",getreguserfromnick(np)?getreguserfromnick(np)->username:"");
  } else {
    userbuf[0]='\0';
  }

  now=time(NULL);
  strftime(timebuf,sizeof(timebuf),Q9_LOG_FORMAT_TIME, gmtime(&now));
  len=snprintf(buf2,sizeof(buf2),"[%s] %s%s\n",timebuf,userbuf,buf);
  write(logfd, buf2, len);
}
