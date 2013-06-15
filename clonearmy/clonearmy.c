#include "../localuser/localuser.h"
#include "../localuser/localuserchannel.h"
#include "../core/schedule.h"
#include "../lib/irc_string.h"

#define HOMECHANNEL "#twilightzone"
#define NICK "Clonemaster"
#define MAXCLONES 1000

nick *clones[MAXCLONES];
unsigned int nclones;
nick *cmnick;

void makenick(void *);

void clonehandler(nick *np, int message, void **args) {
  unsigned int i;

  switch(message) {
    case LU_KILLED:
      for (i=0;i<nclones;i++) {
        if (clones[i]==np) {
          clones[i]=clones[nclones-1];
          nclones--;
          break;
        }
      }
      break;
  }
}

void spam(unsigned int lines) {
  char message[100];
  unsigned int i,j,k,l,m;
  channel **cps;
  
  message[99]='\0';
  
  for (i=0;i<nclones;i++) {
    cps=clones[i]->channels->content;
    for(j=1;j<clones[i]->channels->cursi;j++) {
      for (k=0;k<lines;k++) {
        for (l=0;l<99;l++) {
          m=rand()%27;
          message[l]=m?(m-1)+'a':' ';
        }
        sendmessagetochannel(clones[i], cps[j], "%s", message);
      }
    }
  }
}

void join(char *chan) {
  channel *cp;
  unsigned int i;
  
  if (!(cp=findchannel(chan)))
    return;
  
  for(i=0;i<nclones;i++) {
    localjoinchannel(clones[i], cp);
  }
}

void spawnclones(unsigned int count) {
  nick *np;
  unsigned int i,j;
  char nick[11], ident[11], host[40];
  channel *cp;
  
  for (i=0;i<count;i++) {
    if (nclones >= MAXCLONES)
      return;
      
    for (j=0;j<10;j++) {
      nick[j]=(rand()%26)+'a';
      ident[j]=(rand()%26)+'a';
      host[j]=(rand()%26)+'a';
      host[j+11]=(rand()%26)+'a';
      host[j+22]=(rand()%26)+'a';
    }
    
    host[10]=host[21]='.';
    host[32]=nick[10]=ident[10]='\0';
    
    np=clones[nclones++]=registerlocaluser(nick, ident, host, host, "", 0, clonehandler);
    
    if ((cp=findchannel(HOMECHANNEL))) {
      localjoinchannel(np, cp);
    }
  }
}

void masterhandler(nick *np, int message, void **args) {
  char *msg;

  switch(message) {
    case LU_KILLED:
      cmnick=NULL;
      scheduleoneshot(time(NULL)+1, makenick, NULL);
      break;
    
    case LU_CHANMSG:
      msg=args[2];
      
      if (!ircd_strncmp(msg, "!spawn ",7)) {
        spawnclones(strtoul(msg+7,NULL,10));
      }
      
      if (!ircd_strncmp(msg,"!join ",6)) {
        join(msg+6);
      }
      
      if (!ircd_strncmp(msg,"!spam ", 6)) {
        spam(strtoul(msg+6, NULL, 10));
      }
      break; 
      
  }
}
void makenick(void *arg) {
  channel *cp;
  
  cmnick=registerlocaluser(NICK, "clone", "master", "Clone Master", "", 0, masterhandler);
  
  if ((cp=findchannel(HOMECHANNEL))) {
    localjoinchannel(cmnick, cp);
  } else {
    localcreatechannel(cmnick, HOMECHANNEL);
  }
}

void _init(void) {
  scheduleoneshot(time(NULL)+1, makenick, NULL);
}

void _fini(void) {
  unsigned int i;
  
  if (cmnick) {
    deregisterlocaluser(cmnick, NULL);
  }
  
  for(i=0;i<nclones;i++) {
    deregisterlocaluser(clones[i], NULL);
  }
}
