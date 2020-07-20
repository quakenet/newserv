#define _GNU_SOURCE

#include "proxyscan.h"

#include <sys/poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include "../core/error.h"
#include "../core/events.h"
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include "../nick/nick.h"
#include "../core/hooks.h"
#include "../lib/sstring.h"
#include "../irc/irc_config.h"
#include "../localuser/localuser.h"
#include "../core/config.h"
#include <arpa/inet.h>
#include <unistd.h>
#include "../core/schedule.h"
#include <string.h>
#include "../irc/irc.h"
#include "../lib/irc_string.h"
#include "../lib/version.h"
#include "../channel/channel.h"
#include "../localuser/localuserchannel.h"
#include "../core/nsmalloc.h"
#include "../lib/irc_ipv6.h"
#include "../glines/glines.h"

MODULE_VERSION("")

#define SCANTIMEOUT      60

#define SCANHOSTHASHSIZE 1000
#define SCANHASHSIZE     400

/* It's unlikely you'll get 10k of preamble before a connect... */
#define READ_SANITY_LIMIT 10240

scan *scantable[SCANHASHSIZE];

CommandTree *ps_commands;

static int listenfd = -1;
int activescans;
int maxscans;
int queuedhosts;
int scansdone;
int rescaninterval;
int warningsent;
int glinedhosts;
time_t ps_starttime;
int ps_cache_ext;
int ps_extscan_ext;
int ps_ready;

int numscans; /* number of scan types currently valid */
scantype thescans[PSCAN_MAXSCANS];

unsigned int hitsbyclass[10];
unsigned int scansbyclass[10];

unsigned int myip;
sstring *myipstr;
unsigned short listenport;
int brokendb;

unsigned int ps_mailip;
unsigned int ps_mailport;
sstring *ps_mailname;

sstring *exthost;
int extport;

unsigned long scanspermin;
unsigned long tempscanspermin=0;
unsigned long lastscants=0;

unsigned int ps_start_ts=0;

nick *proxyscannick;

FILE *ps_logfile;

/* Local functions */
void handlescansock(int fd, short events);
void timeoutscansock(void *arg);
void proxyscan_newnick(int hooknum, void *arg);
void proxyscan_lostnick(int hooknum, void *arg);
void proxyscan_onconnect(int hooknum, void *arg);
void proxyscanuserhandler(nick *target, int message, void **params);
void registerproxyscannick();
void killsock(scan *sp, int outcome);
void killallscans();
void proxyscanstats(int hooknum, void *arg);
void sendlagwarning();
void proxyscan_newip(nick *np, unsigned long ip);
int proxyscan_addscantype(int type, int port);
int proxyscan_delscantype(int type, int port);

int proxyscandostatus(void *sender, int cargc, char **cargv);
int proxyscandebug(void *sender, int cargc, char **cargv);
int proxyscandosave(void *sender, int cargc, char **cargv);
int proxyscandospew(void *sender, int cargc, char **cargv);
int proxyscandoshowkill(void *sender, int cargc, char **cargv);
int proxyscandoscan(void *sender, int cargc, char **cargv);
int proxyscandoscanfile(void *sender, int cargc, char **cargv);
int proxyscandoaddscan(void *sender, int cargc, char **cargv);
int proxyscandodelscan(void *sender, int cargc, char **cargv);
int proxyscandoshowcommands(void *sender, int cargc, char **cargv);

int proxyscan_addscantype(int type, int port) {
  /* Check we have a spare scan slot */
  
  if (numscans>=PSCAN_MAXSCANS)
    return 1;

  thescans[numscans].type=type;
  thescans[numscans].port=port;
  thescans[numscans].hits=0;
  
  numscans++;
  
  return 0;
}

int proxyscan_delscantype(int type, int port) {
  int i;
  
  for (i=0;i<numscans;i++)
    if (thescans[i].type==type && thescans[i].port==port)
      break;
      
  if (i>=numscans)
    return 1;
    
  memmove(thescans+i, thescans+(i+1), (PSCAN_MAXSCANS-(i+1)) * sizeof(scantype));
  numscans--;
  
  return 0;
}

static void listener_accept(int fd, short events) {
  int newfd = accept4(fd, NULL, NULL, SOCK_NONBLOCK);
  if (newfd < 0) {
    return;
  }

  write(newfd, MAGICSTRING, MAGICSTRINGLENGTH);
  close(newfd);
}

static int open_listener(sstring *ip, int port) {
  /* ipv4 only for now */
  struct sockaddr_in s;
  int fd;
  int optval;

  memset(&s, 0, sizeof(struct sockaddr_in));
  s.sin_family = AF_INET;
  if (inet_aton(ip->content, &s.sin_addr) == 0) {
    Error("proxyscan", ERR_STOP, "Invalid address %s", ip->content);
    return -1;
  }
  s.sin_port = htons(port);

  fd = socket(PF_INET, SOCK_STREAM|SOCK_NONBLOCK, 0);
  if(fd < 0) {
    Error("proxyscan", ERR_STOP, "Unable to get socket for listener fd.");
    return -1;
  }

  optval = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

  if(bind(fd, (struct sockaddr *)&s, sizeof(struct sockaddr_in)) < 0) {
    Error("proxyscan", ERR_STOP, "Unable to bind listener fd.");
    close(fd);
    return -1;
  }

  if(listen(fd, 500) < 0) {
    Error("proxyscan", ERR_STOP, "Unable to listen on listener fd.");
    close(fd);
    return -1;
  }

  registerhandler(fd, POLLIN, listener_accept);
  return fd;
}

extern unsigned int polltoepoll(short events);
void _init(void) {
  /* HACK: make sure we're running in epoll mode -- no point running otherwise */
  polltoepoll(0);

  sstring *cfgstr;
  int ipbits[4];

  ps_start_ts = time(NULL);
  ps_ready = 0;
  ps_commands = NULL;

  ps_cache_ext = registernodeext("proxyscancache");
  if( ps_cache_ext == -1 ) {
    Error("proxyscan",ERR_INFO,"failed to reg node ext");
    return;
  }
  ps_extscan_ext = registernodeext("proxyscanextscan");
  if ( ps_extscan_ext == -1) { 
    Error("proxyscan",ERR_INFO,"failed to reg node ext");
    return;
  }

  memset(scantable,0,sizeof(scantable));
  maxscans=200;
  activescans=0;
  queuedhosts=0;
  scansdone=0;
  warningsent=0;
  ps_starttime=time(NULL);
  glinedhosts=0;
 
  scanspermin=0;
  lastscants=time(NULL);

  /* Listen port */
  cfgstr=getcopyconfigitem("proxyscan","port","9999",6);
  listenport=strtol(cfgstr->content,NULL,10);
  freesstring(cfgstr);
  
  /* Max concurrent scans */
  cfgstr=getcopyconfigitem("proxyscan","maxscans","200",10);
  maxscans=strtol(cfgstr->content,NULL,10);
  freesstring(cfgstr);

  /* Clean host timeout */
  cfgstr=getcopyconfigitem("proxyscan","rescaninterval","3600",7);
  rescaninterval=strtol(cfgstr->content,NULL,10);
  cachehostinit(rescaninterval);
  freesstring(cfgstr);

  sstring *cfgextport = getcopyconfigitem("proxyscan","extport","0",16);
  extport = strtol(cfgextport->content,NULL,10);
  freesstring(cfgextport);

  exthost = getcopyconfigitem("proxyscan","exthost","",16);

  /* this default will NOT work well */
  myipstr=getcopyconfigitem("proxyscan","ip","127.0.0.1",16);
  
  sscanf(myipstr->content,"%d.%d.%d.%d",&ipbits[0],&ipbits[1],&ipbits[2],&ipbits[3]);
  
  myip=((ipbits[0]&0xFF)<<24)+((ipbits[1]&0xFF)<<16)+
    ((ipbits[2]&0xFF)<<8)+(ipbits[3]&0xFF);

#if defined(PROXYSCAN_MAIL)
  /* Mailer host */
  cfgstr=getcopyconfigitem("proxyscan","mailerip","",16);
  
  psm_mailerfd=-1;
  if (cfgstr) {
    sscanf(cfgstr->content,"%d.%d.%d.%d",&ipbits[0],&ipbits[1],&ipbits[2],&ipbits[3]);
    ps_mailip = ((ipbits[0]&0xFF)<<24)+((ipbits[1]&0xFF)<<16)+
                 ((ipbits[2]&0xFF)<<8)+(ipbits[3]&0xFF);
    ps_mailport=25;
    freesstring(cfgstr);
    
    ps_mailname=getcopyconfigitem("proxyscan","mailname","some.mail.server",HOSTLEN);
    Error("proxyscan",ERR_INFO,"Proxyscan mailer enabled; mailing to %s as %s.",IPlongtostr(ps_mailip),ps_mailname->content);
  } else {
    ps_mailport=0;
    ps_mailname=NULL;
  }
#endif

  listenfd = open_listener(myipstr, listenport);

  proxyscannick=NULL;
  /* Set up our nick on the network */
  scheduleoneshot(time(NULL),&registerproxyscannick,NULL);

  registerhook(HOOK_SERVER_END_OF_BURST, &proxyscan_onconnect);

  registerhook(HOOK_NICK_NEWNICK,&proxyscan_newnick);

  registerhook(HOOK_CORE_STATSREQUEST,&proxyscanstats);

  /* Read in the clean hosts */
  loadcachehosts();

  /* Read in any custom ports to scan */
  loadextrascans();

  /* Set up the database */
  if ((proxyscandbinit())!=0) {
    brokendb=1;
  } else {
    brokendb=0;
  }

  ps_commands = newcommandtree();
  addcommandtotree(ps_commands, "showcommands", 0, 0, &proxyscandoshowcommands);
  addcommandtotree(ps_commands, "status", 0, 0, &proxyscandostatus);
  addcommandtotree(ps_commands, "listopen", 0, 0, &proxyscandolistopen);
  addcommandtotree(ps_commands, "save", 0, 0, &proxyscandosave);
  addcommandtotree(ps_commands, "spew", 0, 1, &proxyscandospew);
  addcommandtotree(ps_commands, "showkill", 0, 1, &proxyscandoshowkill);
  addcommandtotree(ps_commands, "scan", 0, 1, &proxyscandoscan);
  addcommandtotree(ps_commands, "scanfile", 0, 1, &proxyscandoscanfile);
//  addcommandtotree(ps_commands, "addscan", 0, 1, &proxyscandoaddscan);
//  addcommandtotree(ps_commands, "delscan", 0, 1, &proxyscandodelscan);

  /* Default scan types */
  proxyscan_addscantype(STYPE_HTTP, 8080);
  proxyscan_addscantype(STYPE_HTTP, 8118);
  proxyscan_addscantype(STYPE_HTTP, 80);
  proxyscan_addscantype(STYPE_HTTP, 6588);
  proxyscan_addscantype(STYPE_HTTP, 8000);
  proxyscan_addscantype(STYPE_HTTP, 3128);
  proxyscan_addscantype(STYPE_HTTP, 3802);
  proxyscan_addscantype(STYPE_HTTP, 5490);
  proxyscan_addscantype(STYPE_HTTP, 7441);
  proxyscan_addscantype(STYPE_HTTP, 808);
  proxyscan_addscantype(STYPE_HTTP, 3332);
  proxyscan_addscantype(STYPE_HTTP, 2282);

  proxyscan_addscantype(STYPE_HTTP, 1644);
  proxyscan_addscantype(STYPE_HTTP, 8081);
  proxyscan_addscantype(STYPE_HTTP, 443);
  proxyscan_addscantype(STYPE_HTTP, 1337);
  proxyscan_addscantype(STYPE_HTTP, 8888);
  proxyscan_addscantype(STYPE_HTTP, 8008);
  proxyscan_addscantype(STYPE_HTTP, 6515);
  proxyscan_addscantype(STYPE_HTTP, 27977);

  proxyscan_addscantype(STYPE_SOCKS4, 1080);
  proxyscan_addscantype(STYPE_SOCKS5, 1080);
  proxyscan_addscantype(STYPE_SOCKS4, 1180);
  proxyscan_addscantype(STYPE_SOCKS5, 1180);
  proxyscan_addscantype(STYPE_SOCKS4, 9999);
  proxyscan_addscantype(STYPE_SOCKS5, 9999);
  proxyscan_addscantype(STYPE_WINGATE, 23);
  proxyscan_addscantype(STYPE_CISCO, 23);
  proxyscan_addscantype(STYPE_HTTP, 63809);
  proxyscan_addscantype(STYPE_HTTP, 63000);
  proxyscan_addscantype(STYPE_DIRECT, 6666);
  proxyscan_addscantype(STYPE_DIRECT, 6667);
  proxyscan_addscantype(STYPE_DIRECT, 6668);
  proxyscan_addscantype(STYPE_DIRECT, 6669);
  proxyscan_addscantype(STYPE_ROUTER, 3128);
  proxyscan_addscantype(STYPE_SOCKS5, 45554);

  proxyscan_addscantype(STYPE_EXT, 0);

  /* Schedule saves */
  schedulerecurring(time(NULL)+3600,0,3600,&dumpcachehosts,NULL);
 
  ps_logfile=fopen("logs/proxyscan.log","a");

  if (connected) {
    /* if we're already connected, assume we're just reloading module (i.e. have a completed burst) */
    ps_ready = 1;
    startqueuedscans();
  }
}

void registerproxyscannick(void *arg) {
  sstring *psnick,*psuser,*pshost,*psrealname;
  /* Set up our nick on the network */
  channel *cp;

  psnick=getcopyconfigitem("proxyscan","nick","P",NICKLEN);
  psuser=getcopyconfigitem("proxyscan","user","proxyscan",USERLEN);
  pshost=getcopyconfigitem("proxyscan","host","some.host",HOSTLEN);
  psrealname=getcopyconfigitem("proxyscan","realname","Proxyscan",REALLEN);

  proxyscannick=registerlocaluser(psnick->content,psuser->content,pshost->content,
				  psrealname->content,
				  NULL,UMODE_OPER|UMODE_SERVICE|UMODE_DEAF,
				  &proxyscanuserhandler);

  freesstring(psnick);
  freesstring(psuser);
  freesstring(pshost);
  freesstring(psrealname);

  cp=findchannel("#twilightzone");
  if (!cp) {
    localcreatechannel(proxyscannick,"#twilightzone");
  } else {
    localjoinchannel(proxyscannick,cp);
    localgetops(proxyscannick,cp);
  }
}

void _fini(void) {
  if (listenfd != -1)
    deregisterhandler(listenfd, 1);

  deregisterlocaluser(proxyscannick,NULL);
  
  deregisterhook(HOOK_SERVER_END_OF_BURST, &proxyscan_onconnect);

  deregisterhook(HOOK_NICK_NEWNICK,&proxyscan_newnick);

  deregisterhook(HOOK_CORE_STATSREQUEST,&proxyscanstats);

  deleteschedule(NULL,&dumpcachehosts,NULL);
 
  destroycommandtree(ps_commands);
 
  /* Kill any scans in progress */
  killallscans();

  /* Dump the database - AFTER killallscans() which prunes it */
  dumpcachehosts(NULL);

  /* dump any cached hosts before deleting the extensions */
  releasenodeext(ps_cache_ext);
  releasenodeext(ps_extscan_ext);

  /* free() all our structures */
  nsfreeall(POOL_PROXYSCAN);
  
  freesstring(myipstr);
  freesstring(ps_mailname);
  freesstring(exthost);
#if defined(PROXYSCAN_MAIL)
  if (psm_mailerfd!=-1)
    deregisterhandler(psm_mailerfd,1);
#endif
    
  if (ps_logfile)
    fclose(ps_logfile);
}

void proxyscanuserhandler(nick *target, int message, void **params) {
  nick *sender;
  Command *ps_command;
  char *cargv[20];
  int cargc;

  switch(message) {
  case LU_KILLED:
    scheduleoneshot(time(NULL)+1,&registerproxyscannick,NULL);
    proxyscannick=NULL;
    break;

  case LU_PRIVMSG:
  case LU_SECUREMSG:
    sender=(nick *)params[0];
    
    if (IsOper(sender)) {
      cargc = splitline((char *)params[1], cargv, 20, 0);

      if ( cargc == 0 )
        return;

      ps_command = findcommandintree(ps_commands, cargv[0], 1);

      if ( !ps_command ) {
        sendnoticetouser(proxyscannick,sender, "Unknown command.");
        return;
      }

      if ( ps_command->maxparams < (cargc-1) ) {
        rejoinline(cargv[ps_command->maxparams], cargc - (ps_command->maxparams));
        cargc = (ps_command->maxparams) + 1;
      }

      (ps_command->handler)((void *)sender, cargc - 1, &(cargv[1]));
      break;
    }

  default:
    break;
  }
}

void addscantohash(scan *sp) {
  int hash;
  hash=(sp->fd)%SCANHASHSIZE;
  
  sp->next=scantable[hash];
  scantable[hash]=sp;
  
  activescans++;
}

void delscanfromhash(scan *sp) {
  int hash;
  scan **sh;
  
  hash=(sp->fd)%SCANHASHSIZE;
  
  for (sh=&(scantable[hash]);*sh;sh=&((*sh)->next)) {
    if (*sh==sp) {
      (*sh)=sp->next;
      break;
    }
  } 
  
  activescans--;
}

scan *findscan(int fd) {
  int hash;
  scan *sp;
  
  hash=fd%SCANHASHSIZE;
  
  for (sp=scantable[hash];sp;sp=sp->next)
    if (sp->fd==fd)
      return sp;
  
  return NULL;
}

void startscan(patricia_node_t *node, int type, int port, int class) {
  scan *sp;
  float scantmp;

  if (scansdone>maxscans)
  {
    /* ignore the first maxscans as this will skew our scans per second! */
    tempscanspermin++;
    if ((lastscants+60) <= time(NULL))
    {
      /* ok, at least 60 seconds has passed, calculate the scans per minute figure */
      scantmp = time(NULL) - lastscants;
      scantmp = tempscanspermin / scantmp;
      scantmp = (scantmp * 60);
      scanspermin = scantmp;
      lastscants = time(NULL);
      tempscanspermin = 0;
    }
  }
  
  sp=getscan();
  
  sp->outcome=SOUTCOME_INPROGRESS;
  sp->port=port;
  sp->node=node;
  sp->type=type;
  sp->class=class;
  sp->bytesread=0;
  sp->totalbytesread=0;
  memset(sp->readbuf, '\0', PSCAN_READBUFSIZE);

  sp->fd=createconnectsocket(&((patricia_node_t *)sp->node)->prefix->sin,sp->port);
  sp->state=SSTATE_CONNECTING;
  if (sp->fd<0) {
    /* Couldn't set up the socket? */
    derefnode(iptree,sp->node);
    freescan(sp);
    return;
  }
  /* Wait until it is writeable */
  registerhandler(sp->fd,POLLERR|POLLHUP|POLLOUT,&handlescansock);
  /* And set a timeout */
  sp->sch=scheduleoneshot(time(NULL)+SCANTIMEOUT,&timeoutscansock,(void *)sp);
  addscantohash(sp);
}

void timeoutscansock(void *arg) {
  scan *sp=(scan *)arg;

  killsock(sp, SOUTCOME_CLOSED);
}

void killsock(scan *sp, int outcome) {
  int i;
  cachehost *chp;
  foundproxy *fpp;
  time_t now;
  char reason[200];

  scansdone++;
  scansbyclass[sp->class]++;

  /* Remove the socket from the schedule/event lists */
  deregisterhandler(sp->fd,1);  /* this will close the fd for us */
  deleteschedule(sp->sch,&timeoutscansock,(void *)sp);

  sp->outcome=outcome;
  delscanfromhash(sp);

  /* See if we need to queue another scan.. */
  if (sp->outcome==SOUTCOME_CLOSED &&
      ((sp->class==SCLASS_CHECK) ||
       (sp->class==SCLASS_NORMAL && (sp->state==SSTATE_SENTREQUEST || sp->state==SSTATE_GOTRESPONSE))))
    queuescan(sp->node, sp->type, sp->port, SCLASS_PASS2, time(NULL)+300);

  if (sp->outcome==SOUTCOME_CLOSED && sp->class==SCLASS_PASS2)
    queuescan(sp->node, sp->type, sp->port, SCLASS_PASS3, time(NULL)+300);

  if (sp->outcome==SOUTCOME_CLOSED && sp->class==SCLASS_PASS3)
    queuescan(sp->node, sp->type, sp->port, SCLASS_PASS4, time(NULL)+300);

  if (sp->outcome==SOUTCOME_OPEN) {
    hitsbyclass[sp->class]++;
  
    /* Lets try and get the cache record.  If there isn't one, make a new one. */
    if (!(chp=findcachehost(sp->node))) {
      chp=addcleanhost(time(NULL));
      patricia_ref_prefix(sp->node->prefix);
      sp->node->exts[ps_cache_ext] = chp;
    }
    /* Stick it on the cache's list of proxies, if necessary */
    for (fpp=chp->proxies;fpp;fpp=fpp->next)
      if (fpp->type==sp->type && fpp->port==sp->port)
	break;

    if (!fpp) {
      fpp=getfoundproxy();
      fpp->type=sp->type;
      fpp->port=sp->port;
      fpp->next=chp->proxies;
      chp->proxies=fpp;
    }

    now=time(NULL);    
    /* the purpose of this lastgline stuff is to stop gline spam from one scan */
    if (!chp->glineid || (now>=chp->lastgline+SCANTIMEOUT)) {
      char buf[512];
      struct irc_in_addr *ip;

      chp->lastgline=now;
      glinedhosts++;

      loggline(chp, sp->node);   
      ip = &(((patricia_node_t *)sp->node)->prefix->sin);
      snprintf(reason, sizeof(reason), "Open Proxy, see http://www.quakenet.org/openproxies.html - ID: %d", chp->glineid);
      glinebyip("*", ip, 128, 43200, reason, GLINE_IGNORE_TRUST, "proxyscan");
      Error("proxyscan",ERR_DEBUG,"Found open proxy on host %s",IPtostr(*ip));

      snprintf(buf, sizeof(buf), "proxy-gline %lu %s %s %hu %s", time(NULL), IPtostr(*ip), scantostr(sp->type), sp->port, "irc.quakenet.org");
      triggerhook(HOOK_SHADOW_SERVER, (void *)buf);
    } else {
      loggline(chp, sp->node);  /* Update log only */
    }

    /* Update counter */
    for(i=0;i<numscans;i++) {
      if (thescans[i].type==sp->type && thescans[i].port==sp->port) {
	thescans[i].hits++;
	break;
      }
    }
  }

  /* deref prefix (referenced in queuescan) */
  derefnode(iptree,sp->node);
  freescan(sp);

  /* kick the queue.. */
  startqueuedscans();
}

void handlescansock(int fd, short events) {
  scan *sp;
  char buf[512];
  int res;
  unsigned long netip;
  unsigned short netport;

  if ((sp=findscan(fd))==NULL) {
    /* Not found; return and hope it goes away */
    Error("proxyscan",ERR_ERROR,"Unexpected message from fd %d",fd);
    return;
  }

  /* It woke up, delete the alarm call.. */
  deleteschedule(sp->sch,&timeoutscansock,(void *)sp);

  if (events & (POLLERR|POLLHUP)) {
    /* Some kind of error; give up on this socket */
    if (sp->state==SSTATE_GOTRESPONSE) {
      /* If the error occured while we were waiting for a response, we might have
       * received the "OPEN PROXY!" message and the EOF at the same time, so continue
       * processing */
/*      Error("proxyscan",ERR_DEBUG,"Got error in GOTRESPONSE state for %s, continuing.",IPtostr(sp->host->IP)); */
    } else {
      killsock(sp, SOUTCOME_CLOSED);
      return;
    }
  }  

  /* Otherwise, we got what we wanted.. */

  switch(sp->state) {
  case SSTATE_CONNECTING:
    /* OK, we got activity while connecting, so we're going to send some
     * request depending on scan type.  However, we can reregister everything
     * here to save duplicate code: This code is common for all handlers */

    /* Delete the old handler */
    deregisterhandler(fd,0);
    /* Set the new one */
    registerhandler(fd,POLLERR|POLLHUP|POLLIN,&handlescansock);
    sp->sch=scheduleoneshot(time(NULL)+SCANTIMEOUT,&timeoutscansock,(void *)sp);
    /* Update state */
    sp->state=SSTATE_SENTREQUEST;

    switch(sp->type) {
    case STYPE_HTTP:
      sprintf(buf,"CONNECT %s:%d HTTP/1.0\r\n\r\n\r\n",myipstr->content,listenport);
      if ((write(fd,buf,strlen(buf)))<strlen(buf)) {
	/* We didn't write the full amount, DIE */
	killsock(sp,SOUTCOME_CLOSED);
	return;
      }
      break;

    case STYPE_SOCKS4:
      /* set up the buffer */
      netip=htonl(myip);
      netport=htons(listenport);
      memcpy(&buf[4],&netip,4);
      memcpy(&buf[2],&netport,2);
      buf[0]=4;
      buf[1]=1;
      buf[8]=0;
      if ((write(fd,buf,9))<9) {
	/* Didn't write enough, give up */
	killsock(sp,SOUTCOME_CLOSED);
	return;
      }
      break;

    case STYPE_SOCKS5:
      /* Set up initial request buffer */
      buf[0]=5;
      buf[1]=1;
      buf[2]=0;
      if ((write(fd,buf,3))>3) {
	/* Didn't write enough, give up */
	killsock(sp,SOUTCOME_CLOSED);
	return;
      }
      
      /* Now the actual connect request */
      buf[0]=5;
      buf[1]=1;
      buf[2]=0;
      buf[3]=1;      
      netip=htonl(myip);
      netport=htons(listenport);
      memcpy(&buf[4],&netip,4);
      memcpy(&buf[8],&netport,2);
      res=write(fd,buf,10);
      if (res<10) {
	killsock(sp,SOUTCOME_CLOSED);
	return;
      }
      break;
      
    case STYPE_WINGATE:
      /* Send wingate request */
      sprintf(buf,"%s:%d\r\n",myipstr->content,listenport);
      if((write(fd,buf,strlen(buf)))<strlen(buf)) {
	killsock(sp,SOUTCOME_CLOSED);
	return;
      }
      break;
    
    case STYPE_CISCO:
      /* Send cisco request */
      sprintf(buf,"cisco\r\n");
      if ((write(fd,buf,strlen(buf)))<strlen(buf)) {
	killsock(sp, SOUTCOME_CLOSED);
        return;
      }
      
      sprintf(buf,"telnet %s %d\r\n",myipstr->content,listenport);
      if ((write(fd,buf,strlen(buf)))<strlen(buf)) {
	killsock(sp, SOUTCOME_CLOSED);
        return;
      }
      
      break;
      
    case STYPE_DIRECT:
      sprintf(buf,"PRIVMSG\r\n");
      if ((write(fd,buf,strlen(buf)))<strlen(buf)) {
        killsock(sp, SOUTCOME_CLOSED);
        return;
      }
      
      break;    

    case STYPE_ROUTER:
      sprintf(buf,"GET /nonexistent HTTP/1.0\r\n\r\n");
      if ((write(fd,buf,strlen(buf)))<strlen(buf)) {
        killsock(sp, SOUTCOME_CLOSED);
        return;
      }
      
      break;    

    case STYPE_EXT:
      sprintf(buf,"SCAN %s\n", IPtostr(((patricia_node_t *)sp->node)->prefix->sin));
      if ((write(fd,buf,strlen(buf)))<strlen(buf)) {
        killsock(sp, SOUTCOME_CLOSED);
        return;
      }

      break;
    }                
    break;
    
  case SSTATE_SENTREQUEST:
    res=read(fd, sp->readbuf+sp->bytesread, PSCAN_READBUFSIZE-sp->bytesread);
    
    if (res<=0) {
      if ((errno!=EINTR && errno!=EWOULDBLOCK) || res==0) {
        /* EOF, forget it */
        killsock(sp, SOUTCOME_CLOSED);
        return;
      }
    }
    
    sp->bytesread+=res;
    sp->totalbytesread+=res;

    {
      char *magicstring;
      int magicstringlength;

      if(sp->type == STYPE_ROUTER) {
        magicstring = MAGICROUTERSTRING;
        magicstringlength = MAGICROUTERSTRINGLENGTH;
      } else if(sp->type == STYPE_EXT) {
        magicstring = MAGICEXTSTRING;
        magicstringlength = MAGICEXTTRINGLENGTH;
      } else {
        magicstring = MAGICSTRING;
        magicstringlength = MAGICSTRINGLENGTH;
        if(sp->totalbytesread - res == 0) {
          buf[0] = '\r';
          buf[1] = '\n';
          write(fd,buf,2);
        }
      }

      if (memmem(sp->readbuf, sp->bytesread, magicstring, magicstringlength)) {
        killsock(sp, SOUTCOME_OPEN);
        return;
      }
    }
    
    /* If the buffer is full, move half of it along to make room */
    if (sp->bytesread == PSCAN_READBUFSIZE) {
      memcpy(sp->readbuf, sp->readbuf + (PSCAN_READBUFSIZE)/2, PSCAN_READBUFSIZE/2);
      sp->bytesread = PSCAN_READBUFSIZE/2;
    }
    
    /* Don't read data forever.. */
    if (sp->totalbytesread > READ_SANITY_LIMIT) {
      killsock(sp, SOUTCOME_CLOSED);
      return;
    }
    
    /* No magic string yet, we schedule another timeout in case it comes later. */
    sp->sch=scheduleoneshot(time(NULL)+SCANTIMEOUT,&timeoutscansock,(void *)sp);
    return;    
  }
}

void killallscans() {
  int i;
  scan *sp;
  cachehost *chp;
  
  for(i=0;i<SCANHASHSIZE;i++) {
    for(sp=scantable[i];sp;sp=sp->next) {
      /* If there is a pending scan, delete it's clean host record.. */
      if ((chp=findcachehost(sp->node)) && !chp->proxies) {
        sp->node->exts[ps_cache_ext] = NULL;
        derefnode(iptree,sp->node); 
        delcachehost(chp);
      }
        
      if (sp->fd!=-1) {
	deregisterhandler(sp->fd,1);
	deleteschedule(sp->sch,&timeoutscansock,(void *)(sp));
      }
    }
  }
}

void proxyscanstats(int hooknum, void *arg) {
  char buf[512];
  
  sprintf(buf, "Proxyscn: %6d/%4d scans complete/in progress.  %d hosts queued.",
	  scansdone,activescans,queuedhosts);
  triggerhook(HOOK_CORE_STATSREPLY,buf);
  sprintf(buf, "Proxyscn: %6u known clean hosts",cleancount());
  triggerhook(HOOK_CORE_STATSREPLY,buf);  
}

void sendlagwarning() {
  int i,j;
  nick *np;

  for (i=0;i<MAXSERVERS;i++) {
    if (serverlist[i].maxusernum>0) {
      for(j=0;j<serverlist[i].maxusernum;j++) {
	np=servernicks[i][j];
	if (np!=NULL && IsOper(np)) {
	  sendnoticetouser(proxyscannick,np,"Warning: More than 20,000 hosts to scan - I'm lagging behind badly!");
	}
      }
    }
  }
}

int pscansort(const void *a, const void *b) {
  int ra = *((const int *)a);
  int rb = *((const int *)b);
  
  return thescans[ra].hits - thescans[rb].hits;
}

int proxyscandostatus(void *sender, int cargc, char **cargv) {
  nick *np = (nick *) sender;
  int i;
  int totaldetects=0;
  int ord[PSCAN_MAXSCANS];
  
  sendnoticetouser(proxyscannick,np,"Service uptime: %s",longtoduration(time(NULL)-ps_starttime, 1));
  sendnoticetouser(proxyscannick,np,"Total scans completed:  %d",scansdone);
  sendnoticetouser(proxyscannick,np,"Total hosts glined:     %d",glinedhosts);

  sendnoticetouser(proxyscannick,np,"pendingscan structures: %lu x %lu bytes = %lu bytes total",countpendingscan,
	sizeof(pendingscan), (countpendingscan * sizeof(pendingscan)));

  sendnoticetouser(proxyscannick,np,"Currently active scans: %d/%d",activescans,maxscans);
  sendnoticetouser(proxyscannick,np,"Processing speed:       %lu scans per minute",scanspermin);
  sendnoticetouser(proxyscannick,np,"Normal queued scans:    %d",normalqueuedscans);
  sendnoticetouser(proxyscannick,np,"Timed queued scans:     %d",prioqueuedscans);
  sendnoticetouser(proxyscannick,np,"'Clean' cached hosts:   %d",cleancount());
  sendnoticetouser(proxyscannick,np,"'Dirty' cached hosts:   %d",dirtycount());
 
  sendnoticetouser(proxyscannick,np,"Extra scans: %d", extrascancount());
  for (i=0;i<5;i++)
    sendnoticetouser(proxyscannick,np,"Open proxies, class %1d:  %d/%d (%.2f%%)",i,hitsbyclass[i],scansbyclass[i],((float)hitsbyclass[i]*100)/scansbyclass[i]);
  
  for (i=0;i<numscans;i++)
    totaldetects+=thescans[i].hits;
  
  for (i=0;i<numscans;i++)
    ord[i]=i;
  
  qsort(ord,numscans,sizeof(int),pscansort);
  
  sendnoticetouser(proxyscannick,np,"Scan type Port  Detections");
  for (i=0;i<numscans;i++)
    sendnoticetouser(proxyscannick,np,"%-9s %-5d %d (%.2f%%)",
                     scantostr(thescans[ord[i]].type), thescans[ord[i]].port, thescans[ord[i]].hits, ((float)thescans[ord[i]].hits*100)/totaldetects);
  
  sendnoticetouser(proxyscannick,np,"End of list.");
  return CMD_OK;
}

int proxyscandebug(void *sender, int cargc, char **cargv) {
  /* Dump all scans.. */
  int i;
  int activescansfound=0;
  int totalscansfound=0;
  scan *sp;
  nick *np = (nick *)sender;

  sendnoticetouser(proxyscannick,np,"Active scans : %d",activescans);
  
  for (i=0;i<SCANHASHSIZE;i++) {
    for (sp=scantable[i];sp;sp=sp->next) {
      if (sp->outcome==SOUTCOME_INPROGRESS) {
	activescansfound++;
      }
      totalscansfound++;
      sendnoticetouser(proxyscannick,np,"fd: %d type: %d port: %d state: %d outcome: %d IP: %s",
		       sp->fd,sp->type,sp->port,sp->state,sp->outcome,IPtostr(((patricia_node_t *)sp->node)->prefix->sin));
    }
  }

  sendnoticetouser(proxyscannick,np,"Total %d scans actually found (%d active)",totalscansfound,activescansfound);
  return CMD_OK;
}

void proxyscan_onconnect(int hooknum, void *arg) {
  ps_ready = 1;

  /* kick the queue.. */
  startqueuedscans();
}

int proxyscandosave(void *sender, int cargc, char **cargv) {
  nick *np = (nick *)sender;

  sendnoticetouser(proxyscannick,np,"Saving cached hosts...");
  dumpcachehosts(NULL);
  sendnoticetouser(proxyscannick,np,"Done.");
  return CMD_OK;
}

int proxyscandospew(void *sender, int cargc, char **cargv) {
  nick *np = (nick *)sender;

  if(cargc < 1)
    return CMD_USAGE;

  /* check our database for the ip supplied */
  unsigned long a,b,c,d;
  if (4 != sscanf(cargv[0],"%lu.%lu.%lu.%lu",&a,&b,&c,&d)) {
    sendnoticetouser(proxyscannick,np,"Usage: spew x.x.x.x");
  } else {
    /* check db */
    proxyscanspewip(proxyscannick,np,a,b,c,d);
  }
  return CMD_OK;
}

int proxyscandoshowkill(void *sender, int cargc, char **cargv) {
  nick *np = (nick *)sender;

  if(cargc < 1)
    return CMD_USAGE;

  /* check our database for the id supplied */
  unsigned long a;
  if (1 != sscanf(cargv[0],"%lu",&a)) {
    sendnoticetouser(proxyscannick,np,"Usage: showkill <id>");
  } else {
    /* check db */
    proxyscanshowkill(proxyscannick,np,a);
  }
  return CMD_OK;
}

void startnickscan(nick *np) {
  time_t t = time(NULL);
  int i;
  for(i=0;i<numscans;i++) {
    /* @@@TODO: we allow a forced scan to scan the same IP multiple times atm */
    queuescan(np->ipnode,thescans[i].type,thescans[i].port,SCLASS_NORMAL,t);
  }
}

int proxyscandoscan(void *sender, int cargc, char **cargv) {
  nick *np = (nick *)sender;
  patricia_node_t *node;
  struct irc_in_addr sin;
  unsigned char bits;
  int i;

  if(cargc < 1)
    return CMD_USAGE;

  if (0 == ipmask_parse(cargv[0],&sin, &bits)) {
    sendnoticetouser(proxyscannick,np,"Usage: scan <ip>");
  } else {
    if (bits != 128 || !irc_in_addr_is_ipv4(&sin) || irc_in_addr_is_loopback(&sin)) {
      sendnoticetouser(proxyscannick,np,"You may only scan single IPv4 IP's");
      return CMD_OK;
    }
    if (bits != 128 || irc_in_addr_is_loopback(&sin)) {
      sendnoticetouser(proxyscannick,np,"You may only scan single IP's");
      return CMD_OK;
    }

    time_t t;
    sendnoticetouser(proxyscannick,np,"Forcing scan of %s",IPtostr(sin));
    // * Just queue the scans directly here.. plonk them on the priority queue * /
    node = refnode(iptree, &sin, bits); /* node leaks node here - should only allow to scan a nick? */
    t = time(NULL);
    for(i=0;i<numscans;i++) {
      /* @@@TODO: we allow a forced scan to scan the same IP multiple times atm */
      queuescan(node,thescans[i].type,thescans[i].port,SCLASS_NORMAL,t);
    }
  }
  return CMD_OK;
}

int proxyscandoscanfile(void *sender, int cargc, char **cargv) {
  nick *np = (nick *)sender;
  int i;
  time_t t = time(NULL);
  int pscantypes[PSCAN_MAXSCANS];
  int maxtypes;
  FILE *fp;
  int count;

  if ((fp=fopen("data/doscan.txt","r"))==NULL) {
    sendnoticetouser(proxyscannick,np,"Unable to open file for reading!");
    return CMD_ERROR;
  }

  {
    int *tscantypes;
    int maxno = -1;

    for(i=0;i<numscans;i++)
      if(thescans[i].type > maxno)
        maxno = thescans[i].type;

    tscantypes = malloc(sizeof(int) * maxno);
    for(i=0;i<maxno;i++)
      tscantypes[i] = 0;

    for(i=0;i<numscans;i++)
      tscantypes[thescans[i].type] = 1;

    for(i=0,maxtypes=0;i<maxno;i++)
      if(tscantypes[i])
        pscantypes[maxtypes++] = i;

    free(tscantypes);
  }

  count = 0;
  while (!feof(fp)) {
    patricia_node_t *node;
    struct irc_in_addr sin;
    unsigned char bits;
    unsigned short port;
    char buf[512], ip[512];
    int res;

    fgets(buf,512,fp);
    if (feof(fp)) {
      break;
    }

    res=sscanf(buf,"%s %hu",ip,&port);

    if (res<2)
      continue;

    if (0 == ipmask_parse(ip,&sin, &bits)) {
      /* invalid mask */
    } else {
      node = refnode(iptree, &sin, bits); /* LEAKS */
      if( node ) {
        for(i=0;i<maxtypes;i++) {
          queuescan(node,pscantypes[i],port,SCLASS_NORMAL,t);
          count++;
        }
      }
    }
  }

  fclose(fp);

  sendnoticetouser(proxyscannick,np,"Started %d scans...", count);
  return CMD_OK;
}

int proxyscandoaddscan(void *sender, int cargc, char **cargv) {
  nick *np = (nick *)sender;

  if(cargc < 1)
    return CMD_USAGE;

  unsigned int a,b;
  if (sscanf(cargv[0],"%u %u",&a,&b) != 2) {
    sendnoticetouser(proxyscannick,np,"Usage: addscan <type> <port>");
  } else {
    sendnoticetouser(proxyscannick,np,"Added scan type %u port %u",a,b);
    proxyscan_addscantype(a,b);
    scanall(a,b);
  }
  return CMD_OK;
}

int proxyscandodelscan(void *sender, int cargc, char **cargv) {
  nick *np = (nick *)sender;

  if(cargc < 1)
    return CMD_USAGE;

  unsigned int a,b;
  if (sscanf(cargv[0],"%u %u",&a,&b) != 2) {
    sendnoticetouser(proxyscannick,np,"Usage: delscan <type> <port>");
  } else {
    sendnoticetouser(proxyscannick,np,"Delete scan type %u port %u",a,b);
    proxyscan_delscantype(a,b);
  }
  return CMD_OK;
}

int proxyscandoshowcommands(void *sender, int cargc, char **cargv) {
  nick *np = (nick *)sender;
  Command *cmdlist[100];
  int i,n;

  n=getcommandlist(ps_commands,cmdlist,100);

  sendnoticetouser(proxyscannick,np,"The following commands are registered at present:");
  for(i=0;i<n;i++) {
    sendnoticetouser(proxyscannick,np,"%s",cmdlist[i]->command->content);
  }
  sendnoticetouser(proxyscannick,np,"End of list.");
  return CMD_OK;
}
