#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdio.h>
#include "chanserv.h"

int logfd;

void cs_initlog() {
  logfd=open("chanservlog.0",O_WRONLY|O_CREAT|O_APPEND,S_IRUSR|S_IWUSR);
}

void cs_closelog() {
  if (logfd>=0)
    close(logfd);
}

void cs_log(nick *np, char *event, ... ) {
  char buf[512];
  char buf2[1024];
  char userbuf[512];
  va_list va;
  struct tm *tm;
  time_t now;
  char timebuf[100];
  int len;

  if (logfd<0)
    return; 

  va_start(va,event);
  vsnprintf(buf,512,event,va);
  va_end(va);

  if (np) {
    snprintf(userbuf,511,"%s!%s@%s [%s%s]",np->nick,np->ident,np->host->name->content,
	     getreguserfromnick(np)?"auth ":"noauth",getreguserfromnick(np)?getreguserfromnick(np)->username:"");
  } else {
    userbuf[0]='\0';
  }

  now=time(NULL);
  tm=gmtime(&now);
  strftime(timebuf,100,"%Y-%m-%d %H:%M:%S",tm);
  len=snprintf(buf2,1024,"[%s] %s %s\n",timebuf,userbuf,buf);
  write(logfd, buf2, len);
}
