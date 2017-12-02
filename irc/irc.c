/* irc.c: handle the IRC interface */

#define _POSIX_SOURCE
#define _POSIX_C_SOURCE 201112L

#include "irc.h"
#include "irc_config.h"
#include "../parser/parser.h"
#include "../core/events.h"
#include "../core/schedule.h"
#include "../core/error.h"
#include "../core/config.h"
#include "../core/hooks.h"
#include "../lib/base64.h"
#include "../lib/splitline.h"
#include "../lib/version.h"
#include "../lib/irc_string.h"
#include "../lib/strlfunc.h"
#include <sys/poll.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>

MODULE_VERSION("");

#define READBUFSIZE          32768
#define MAX_SERVERARGS       20
#define MIN_NUMERIC          100
#define MAX_NUMERIC          999

void irc_connect(void *arg);
void ircstats(int hooknum, void *arg);
void checkhubconfig(void);
void ircrehash(int hooknum, void *arg);

CommandTree *servercommands;
Command *numericcommands[MAX_NUMERIC-MIN_NUMERIC];
int serverfd;
char inbuf[READBUFSIZE];
char *nextline;
char *args[MAX_SERVERARGS];
int bytesleft;
int linesreceived;

sstring *mynumeric;
sstring *myserver;
long mylongnum;

time_t starttime;
time_t timeoffset;
int awaitingping;
int connected;

static int hubnum, hubcount, previouslyconnected = 0;
static sstring **hublist;

void _init() {
  servercommands=newcommandtree();
  starttime=time(NULL);
  
  connected=0;
  timeoffset=0;
  
  /* These values cannot be changed whilst the IRC module is running */
  mynumeric=getcopyconfigitem("irc","servernumeric","A]",2);
  myserver=getcopyconfigitem("irc","servername","services.example.net",HOSTLEN);
  
  mylongnum=numerictolong(mynumeric->content,2);

  checkhubconfig();

  /* Schedule a connection to the IRC server */
  scheduleoneshot(time(NULL),&irc_connect,NULL);
    
  registerserverhandler("G",&handleping,1);
  registerserverhandler("SE",&handlesettime,1);
  registerserverhandler("Z",&handlepingreply,1);
  registerserverhandler("SERVER",&irc_handleserver,8);

  registerhook(HOOK_CORE_STATSREQUEST,&ircstats);
  registerhook(HOOK_CORE_REHASH,&ircrehash);
}

void _fini() {
  if (connected) {
    irc_send("%s SQ %s 0 :Shutting down",mynumeric->content,myserver->content);
    irc_disconnected(0);
  }

  deregisterserverhandler("G",&handleping);
  deregisterserverhandler("SE",&handlesettime);
  deregisterserverhandler("Z",&handlepingreply);
  deregisterserverhandler("SERVER",&irc_handleserver);

  deregisterhook(HOOK_CORE_STATSREQUEST,&ircstats);
  deregisterhook(HOOK_CORE_REHASH,&ircrehash);
 
  deleteschedule(NULL,&sendping,NULL);
  deleteschedule(NULL,&irc_connect,NULL);
  
  freesstring(mynumeric);
  freesstring(myserver);

  destroycommandtree(servercommands);
}

void resethubnum(void) {
  hubnum=0;
}

void nexthub(void) {
  /*
   * this is set if the connection before this (failed one)
   * was successful, so we attempted reconnecting to the
   * same server, and now we've failed, so reset to the first
   * which should be the preferred one.
   */

  if (previouslyconnected) {
    previouslyconnected=0;
    if((hubcount<2) || (hubnum!=0)) {
      hubnum=0;
    } else {
      hubnum=1;
    }
    return;
  }

  /* 1 % 0 is fun */
  if (hubcount>0)
    hubnum=(hubnum+1)%hubcount;
}

int gethub(int servernum, char **conto, char **port, char **conpass, int *pingfreq) {
  sstring *conto_s, *portnum_s, *conpass_s, *pingfreq_s;
  static char conto_b[512], conpass_b[512], conport_b[512];
  int ret, s;
  char *section;

  if (hubcount) {
    static char realsection[512];
    snprintf(realsection,sizeof(realsection),"hub-%s",hublist[servernum]->content);
    section=realsection;
    s=1;
  } else {
    section="irc";
    s=0;
  }

  conto_s=getconfigitem(section,s?"host":"hubhost");
  if (!conto_s || !conto_s->content[0]) {
    ret=0;
  } else {
    ret=1;
  }

  if (conto) {
    conto_s=getcopyconfigitem(section,s?"host":"hubhost","127.0.0.1",HOSTLEN);
    strlcpy(conto_b, conto_s->content, sizeof(conto_b));
    *conto=conto_b;

    freesstring(conto_s);
  }

  if (port) {
    portnum_s=getcopyconfigitem(section,s?"port":"hubport","4400",7);
    strlcpy(conport_b, portnum_s->content, sizeof(conport_b));
    *port = conport_b;

    freesstring(portnum_s);
  }

  if (conpass) {
    conpass_s=getcopyconfigitem(section,s?"pass":"hubpass","erik",20);
    strlcpy(conpass_b, conpass_s->content, sizeof(conpass_b));
    *conpass = conpass_b;

    freesstring(conpass_s);
  }

  if (pingfreq) {
    pingfreq_s=getcopyconfigitem(section,"pingfreq","90",10);
    *pingfreq=strtol(pingfreq_s->content,NULL,10);

    freesstring(pingfreq_s);
  }

  return ret;
}

static struct addrinfo *getsockhints(void) {
  static struct addrinfo hints;
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  return &hints;
}

/* we check at startup that (most things) resolve */
void checkhubconfig(void) {
  array *servers_ar;
  struct addrinfo *addrinfo;
  int i;

  resethubnum();
  hubcount=0;

  servers_ar=getconfigitems("irc", "hub");

  /*
   * we don't bother doing the gethostbyname check for
   * the old style of configuration, as it'll
   * be checked by irc_connect anyway.
   */
  if (!servers_ar || !servers_ar->cursi) {
    Error("irc",ERR_WARNING,"Using legacy hub configuration format.");
    return;
  }

  hublist=(sstring **)servers_ar->content;
  hubcount=servers_ar->cursi;

  for (i=0;i<hubcount;i++) {
    char *conto, *conport;

    if (!gethub(i, &conto, &conport, NULL, NULL)) {
      Error("irc",ERR_FATAL,"No server configuration specified for '%s'.",hublist[i]->content);
      exit(1);
    }

    if(getaddrinfo(conto, conport, getsockhints(), &addrinfo)) {
      Error("irc",ERR_FATAL,"Couldn't resolve host %s:%s.",conto,conport);
      exit(1);
    }

    freeaddrinfo(addrinfo);
  }
}

void ircrehash(int hookhum, void *arg) {
  checkhubconfig();
}

void irc_connect(void *arg) {  
  struct addrinfo *addrinfo, *res;
  sstring *mydesc;
  char *conto,*conpass,*conport;
  int pingfreq;
/*  socklen_t opt=1460;*/

  nextline=inbuf;
  bytesleft=0;
  linesreceived=0;
  awaitingping=0;

  if (!gethub(hubnum, &conto, &conport, &conpass, &pingfreq)) {
    Error("irc",ERR_ERROR,"No server configuration specified for '%s'.",hublist[hubnum]->content);
    goto fail;
  }

  if(getaddrinfo(conto, conport, getsockhints(), &addrinfo)) {
    Error("irc",ERR_ERROR,"Couldn't resolve host %s",conto);
    goto fail;
  }

  Error("irc",ERR_INFO,"Connecting to %s:%s",conto,conport);
  for(res=addrinfo;res;res=res->ai_next) {
    int fd = socket(addrinfo->ai_family, addrinfo->ai_socktype, addrinfo->ai_protocol);
    if (fd == -1)
      continue;

    if(connect(fd, addrinfo->ai_addr, addrinfo->ai_addrlen) != -1) {
      serverfd = fd;
      break;
    }

    close(fd);
  }
  freeaddrinfo(addrinfo);

  if(!res) {
    Error("irc",ERR_ERROR,"Couldn't connect to %s:%s",conto,conport);
    goto fail;
  }

/*  if (setsockopt(serverfd, SOL_SOCKET, SO_RCVBUF, &opt, sizeof(opt))) {
    Error("irc",ERR_WARNING,"Error setting socket buffer.");
  }
  */

  irc_send("PASS :%s",conpass);

  mydesc=getcopyconfigitem("irc","serverdescription","newserv 0.01",100);

  /* remember when changing modes to change server/server.c too */
  irc_send("SERVER %s 1 %ld %ld J10 %s%s +sh6n :%s",myserver->content,starttime,time(NULL),mynumeric->content,longtonumeric(MAXLOCALUSER,3),mydesc->content);
  freesstring(mydesc);

  registerhandler(serverfd, POLLIN|POLLPRI|POLLERR|POLLHUP|POLLNVAL, &handledata);

  /* Schedule our ping requests.  Note that this will also server
   * to time out a failed connection.. */

  schedulerecurring(time(NULL)+pingfreq,0,pingfreq,&sendping,NULL);
  return;

fail:
  Error("irc",ERR_INFO,"Will try next host in one minute");

  nexthub();
  scheduleoneshot(time(NULL)+60,&irc_connect,NULL);
}

int irc_handleserver(void *source, int cargc, char **cargv) {
  if (!strncmp((char *)source,"INIT",4) && !connected) {
    /* OK, this is the SERVER response back from the remote server */
    /* This completes the connection process. */
    Error("irc",ERR_INFO,"Connection accepted by %s",cargv[0]);

    /* Fix our timestamps before we do anything else */
    setnettime(strtol(cargv[3],NULL,10));

    previouslyconnected=1;
    connected=1;
  } else {
    Error("irc",ERR_INFO,"Unexpected SERVER message");
  }

  return CMD_OK;
}

static void *disconnect_schedule;
static void async_disconnect_handler(void *arg) {
  disconnect_schedule = NULL;
  irc_disconnected(0);
}

void irc_disconnected(int async) {
  if (async) {
    /* keep the fd "connected" until the next execution of the event loop */

    if(disconnect_schedule == NULL)
      disconnect_schedule = scheduleoneshot(time(NULL), &async_disconnect_handler, NULL);

    return;
  }

  if (serverfd>=0) {
    deregisterhandler(serverfd,1);
  }
  serverfd=-1;
  if (connected) {
    connected=0;
    triggerhook(HOOK_IRC_PRE_DISCON,NULL);
    triggerhook(HOOK_IRC_DISCON,NULL);
  } else {
    nexthub();
  }
  if (disconnect_schedule) {
    deleteschedule(disconnect_schedule, &async_disconnect_handler, NULL);
    disconnect_schedule = NULL;
  }
  deleteschedule(NULL,&irc_connect,NULL);
  deleteschedule(NULL,&sendping,NULL);
  scheduleoneshot(time(NULL)+2,&irc_connect,NULL);
}

/*
static int _write(int fd, char *buf, size_t len) {
  if(rand() % 10 == 0) {
    errno = EPIPE;
    return -1;
  }
  return write(fd, buf, len);
}
*/

int irc_send(char *format, ... ) {
  char buf[512];
  va_list val;
  int len;
  int ret;

  if(disconnect_schedule) {
    Error("irc",ERR_WARNING,"Writing to disconnected socket!");
    return -1;
  }

  if(serverfd < 0) {
    Error("irc",ERR_WARNING,"Writing to closed socket!");
    return -1;
  }

  va_start(val,format);
  len=vsnprintf(buf,509,format,val);
  va_end(val);
  
  if (len>509 || len<0) {
    len=509;
  }
  
  buf[len++]='\r';
  buf[len++]='\n';
  
  ret = write(serverfd,buf,len);
  if(ret < 0) {
    Error("irc",ERR_ERROR,"Got socket error %d, dropping connection.", errno);
    irc_disconnected(1);
    return -1;
  }

  return 0;
}

void handledata(int fd, short events) {
  int res;
  int again=1;
  
  if (events & (POLLPRI | POLLERR | POLLHUP | POLLNVAL)) {
    /* Oh shit, we got dropped */
    Error("irc",ERR_ERROR,"Got socket error, dropping connection.");
    irc_disconnected(0);
    return;  
  }

  while(again) {
    res=read(serverfd, inbuf+bytesleft, READBUFSIZE-bytesleft);
    if (res<=0) {
      Error("irc",ERR_ERROR,"Disconnected by remote server.");
      irc_disconnected(0);
      return;
    }

    again=((bytesleft+=res)==READBUFSIZE);
    while (!parseline())
      ; /* empty loop */
  
    memmove(inbuf, nextline, bytesleft);
    nextline=inbuf;
  }    
}
  
/* 
 * Parse and dispatch a line from the IRC server
 *
 * Returns:
 *  0: found at least one complete line in buffer
 *  1: no complete line found in buffer
 *
 * Note that this returns 0 even if it finds a line which wasn't handled.
 * The return code is just used to determine when there are no more lines
 * to parse.
 */
  
int parseline() {
  char *currentline=nextline;
  int foundcmd=0;
  int cargc;
  char *cargv[MAX_SERVERARGS];
  Command *c;
  
  while (bytesleft-->0) {
    if (*nextline=='\r' || *nextline=='\n' || *nextline=='\0') {
      /* Found a newline character.  Replace it with \0 */
      /* If we found some non-newline characters first, we need to parse this */
      /* Otherwise, don't bother and just skip ahead */
      if (currentline==nextline) {
        *nextline++='\0';
        currentline=nextline;
      } else {
        *nextline++='\0';
        foundcmd=1;
        break;
      }
    } else {
      nextline++;
    }
  }
  
  if (foundcmd==0) {
    bytesleft=(nextline-currentline);
    nextline=currentline;
    return 1;
  }
  
  /* OK, currentline points at a valid NULL-terminated line */
  /* and nextline points at where we are going next */      
  
  linesreceived++;

  /* Split it up */
  cargc=splitline(currentline,cargv,MAX_SERVERARGS,1);
  
  if (cargc<2) {
    /* Less than two arguments?  Not a valid command, sir */
    return 0;
  }
  
  if (linesreceived<3) {
    /* Special-case the first two lines 
     * These CANNOT be numeric responses, 
     * and the command is the FIRST thing on the line */
    if ((c=findcommandintree(servercommands,cargv[0],1))==NULL) {
      /* No handler, return. */
      return 0;
    }
    for (;c!=NULL;c=c->next) {
      c->calls++;
      if (((c->handler)("INIT",cargc-1,&cargv[1]))==CMD_LAST)
        return 0;
    }  
  } else {
    if (cargv[1][0]>='0' && cargv[1][0]<='9') {
      /* It's a numeric! */
      long numeric = strtol(cargv[1], NULL, 0);
      if((numeric >= MIN_NUMERIC) && (numeric <= MAX_NUMERIC)) {
        for(c=numericcommands[numeric];c;c=c->next) {
          c->calls++;
          if (((c->handler)((void *)numeric,cargc,cargv))==CMD_LAST)
            return 0;
        }
      }
    } else {
      if ((c=findcommandintree(servercommands,cargv[1],1))==NULL) {
        /* We don't have a handler for this command */
        return 0;
      }
      for (;c!=NULL;c=c->next) {
        c->calls++;
        if (((c->handler)(cargv[0],cargc-2,cargv+2))==CMD_LAST)
          return 0;
      }
    }
  }  
  return 0;
}

int registerserverhandler(const char *command, CommandHandler handler, int maxparams) {
  if ((addcommandtotree(servercommands,command,0,maxparams,handler))==NULL)
    return 1;
    
  return 0;
}

int deregisterserverhandler(const char *command, CommandHandler handler) {
  return deletecommandfromtree(servercommands, command, handler);
}

int registernumerichandler(const int numeric, CommandHandler handler, int maxparams) {
  Command *cp, *np;
  if((numeric < MIN_NUMERIC) || (numeric > MAX_NUMERIC))
    return 1;

  /* doesn't happen that often */
  np = (Command *)malloc(sizeof(Command));
  np->handler = handler;
  np->next = NULL;

  /* I know I could just add to the beginning, but I guess since we have that LAST stuff
   * it should go at the end */
  if(!(cp = numericcommands[numeric])) {
    numericcommands[numeric] = np;
  } else {
    for(;cp->next;cp=cp->next);
      /* empty loop */

    cp->next = np;
  }

  return 0;
}

int deregisternumerichandler(const int numeric, CommandHandler handler) {
  Command *cp, *lp;
  if((numeric < MIN_NUMERIC) || (numeric > MAX_NUMERIC))
    return 1;

  for(cp=numericcommands[numeric],lp=NULL;cp;lp=cp,cp=cp->next) {
    if(cp->handler == handler) {
      if(lp) {
        lp->next = cp->next;
      } else {
        numericcommands[numeric] = cp->next;
      }
      free(cp);
      return 0;
    }
  }

  return 1;
}

char *getmynumeric() {
  return mynumeric->content;
}

time_t getnettime() {
  return (time(NULL)+timeoffset);
}

void setnettime(time_t newtime) {
  timeoffset=newtime-time(NULL);
  Error("irc",ERR_INFO,"setnettime: Time offset is now %ld",timeoffset);
}

int handleping(void *sender, int cargc, char **cargv) {
  char *ss, *sms;
  unsigned long long s, ms, diff;
  struct timeval now;

  if (cargc==0)
    return CMD_OK;

  if (cargc < 3) {
    irc_send("%s Z %s",mynumeric->content,cargv[cargc-1]);
    return CMD_OK;
  }

  gettimeofday(&now, NULL);

  ss = cargv[2];
  if ((sms = strchr(cargv[2], '.')) == NULL) {
    diff = 0;
  } else {
    s = strtoull(ss, NULL, 10);
    ms = strtoull(sms + 1, NULL, 10);
    diff = (now.tv_sec - s) * 1000 + (now.tv_usec - ms) / 1000;
  }

  irc_send("%s Z %s %s %s %llu %llu.%llu",mynumeric->content,mynumeric->content,(const char *)sender,cargv[2],diff,(unsigned long long)now.tv_sec,(unsigned long long)now.tv_usec/1000);
  return CMD_OK;
}

int handlesettime(void *sender, int cargc, char **cargv) {
  time_t newtime;

  if (cargc>0) {
    newtime=strtol(cargv[0],NULL,10);
    Error("irc",ERR_INFO,"Received settime: %lu (current time is %lu, offset %ld)",newtime,getnettime(),newtime-getnettime());
    setnettime(newtime);
  }  
  return CMD_OK;
}

void sendping(void *arg) {
  if (connected) {
    if (awaitingping==1) {
      /* We didn't get a ping reply, kill the connection */
      Error("irc",ERR_INFO,"Connection closed due to ping timeout.");

      irc_send("%s SQ %s 0 :Ping timeout",mynumeric->content,myserver->content);
      irc_disconnected(0);
    } else {
      awaitingping=1;
      irc_send("%s G :%s",mynumeric->content,myserver->content);
    }
  } else {
    /* We tried to send a ping when we weren't connected.. */
    Error("irc",ERR_INFO,"Connection timed out.");

    /* have to have this here because of the EVIL HACK below */
    nexthub();

    connected=1; /* EVIL HACK */
    irc_disconnected(0);
  }
}

int handlepingreply(void *sender, int cargc, char **cargv) {
  awaitingping=0;
  return CMD_OK;
}


void ircstats(int hooknum, void *arg) {
  long level=(long)arg;
  char buf[100];

  if (level>5) {
    sprintf(buf,"irc     : start time %lu (running %s)", starttime,longtoduration(time(NULL)-starttime,0));
    triggerhook(HOOK_CORE_STATSREPLY,buf);
    sprintf(buf,"Time    : %lu (current time is %lu, offset %ld)",getnettime(),time(NULL),timeoffset);
    triggerhook(HOOK_CORE_STATSREPLY,buf);
  }
}


/* list stats commands / m to a user
 *
 *  sourcenum     numeric of the user requesting the listing
 */
void stats_commands(char *sourcenum) {
  Command *cmds[500];
  unsigned int c,i;
  
  c=getcommandlist(servercommands,cmds,500);
  
  for (i=0;i<c;i++) {
    /*
     * 212 RPL_STATSCOMMANDS "source 212 target command used_count bytes_count"
     *                       "irc.netsplit.net 212 foobar ACCOUNT 41 462"
     */
    irc_send("%s 212 %s %s %u 0", getmynumeric(), sourcenum, cmds[i]->command->content, cmds[i]->calls);
  }
}
