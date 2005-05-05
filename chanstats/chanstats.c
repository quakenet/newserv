
#include "chanstats.h"
#include "../channel/channel.h"
#include "../lib/sstring.h"
#include "../lib/splitline.h"
#include "../irc/irc.h"
#include "../core/schedule.h"
#include "../core/error.h"
#include "../control/control.h"
#include "../nick/nick.h"
#include "../lib/irc_string.h"

#include <stdio.h>
#include <string.h>

int csext;
int sampleindex;
time_t lastsample;
time_t lastday;
int uponehour;
int failedinit;
time_t lastsave;

unsigned int lastdaysamples[HISTORYDAYS];
unsigned int todaysamples;

void doupdate(void *arg);
void updatechanstats(chanindex *cip, time_t now); 
void rotatechanstats();
void savechanstats();
void loadchanstats();
int dochanstats(void *source, int argc, char **argv);
int doexpirecheck(void *source, int cargc, char **cargv);
int douserhistogram(void *source, int cargc, char **cargv);
int dochanhistogram(void *source, int cargc, char **cargv);
void dohist_now(int *data, float *bounds, int cats, nick *np);
void dohist_today(int *data, float *bounds, int cats);
void dohist_days(int *data, float *bounds, int cats, int days);
void dohist_namelen(int *data, float *bounds, int cats);

void _init() {
  time_t now,when;

  csext=registerchanext("chanstats");
  if (csext<0) {
    /* PANIC PANIC PANIC PANIC */
    Error("chanstats",ERR_ERROR,"Couldn't register channel extension");
    failedinit=1;
  } else {  
    initchanstatsalloc();
    lastday=(getnettime()/(24*3600));
    uponehour=0;
    sampleindex=-1;
    memset(lastdaysamples,0,sizeof(lastdaysamples));
    todaysamples=0; 
    failedinit=0;
  
    loadchanstats();
  
    /* Work out when to take the next sample */
    now=getnettime();
    if (now < lastsample) {
      Error("chanstats",ERR_WARNING,"Last sample time in future (%d > %d)",lastsample,now);
      when=now;
    } else if (now<(lastsample+SAMPLEINTERVAL)) {
      lastday=lastsample/(24*3600);
      when=lastsample+SAMPLEINTERVAL;
    } else {
      if ((lastsample/(24*3600))<lastday) {
        rotatechanstats();
      }
      when=now;
    }
    
    lastsave=now;
    registercontrolcmd("chanstats",10,1,&dochanstats);
    registercontrolcmd("channelhistogram",10,13,&dochanhistogram);
    registercontrolcmd("userhistogram",10,1,&douserhistogram);
    registercontrolcmd("expirecheck",10,1,&doexpirecheck);
    schedulerecurring(when,0,SAMPLEINTERVAL,&doupdate,NULL);  
  }
}

void _fini() {
  if (failedinit==0) {
    savechanstats();
    deleteschedule(NULL,&doupdate,NULL);
    deregistercontrolcmd("chanstats",&dochanstats);
    deregistercontrolcmd("channelhistogram",&dochanhistogram);
    deregistercontrolcmd("userhistogram",&douserhistogram);
    deregistercontrolcmd("expirecheck",&doexpirecheck);
    releasechanext(csext);
    cstsfreeall();
  }
}

/*
 * doupdate:
 *  This is the core function which is scheduled.
 */

void doupdate(void *arg) {
  int i;
  time_t now;
  chanindex *cip;

  /* Get the current time and move the history pointer along one and count the sample */
  now=getnettime();

  Error("chanstats",ERR_INFO,"Running chanstats update.");

  if (now/(24*3600) > lastday) {
    rotatechanstats();
    lastday=(now/(24*3600));
  }
  
  todaysamples++;
  sampleindex++;
  if (sampleindex==SAMPLEHISTORY) {
    sampleindex=0;
    uponehour=1;
  }

  /*
   * Loop over all chans doing update
   */
  
  for(i=0;i<CHANNELHASHSIZE;i++) {
    for (cip=chantable[i];cip;cip=cip->next) {
      if ((cip->channel!=NULL) || (cip->exts[csext]!=NULL)) {
        updatechanstats(cip,now);
      }
    }
  }
  
  lastsample=now;
  if (now-lastsave > SAVEINTERVAL) {
    savechanstats();
    lastsave=now;
  }
}

void updatechanstats(chanindex *cip, time_t now) {
  chanstats *csp;
  int currentusers;
  
  csp=cip->exts[csext];
  if (csp==NULL) {
    csp=cip->exts[csext]=getchanstats();
    memset(csp,0,sizeof(chanstats));
  }

  if (cip->channel!=NULL) {
    currentusers=countuniquehosts(cip->channel);
    csp->todaysamples++;
  } else {
    currentusers=0;
  }

  csp->lastsamples[sampleindex]=currentusers;
  csp->lastsampled=now;
  csp->todayusers+=currentusers;

  if (currentusers > csp->todaymax) {
    csp->todaymax=currentusers;
  }
}

void rotatechanstats() {
  int i,j,k;
  chanindex *cip,*ncip;
  chanstats *csp;

  for (i=0;i<CHANNELHASHSIZE;i++) {
    for (cip=chantable[i];cip;cip=ncip) {
      ncip=cip->next;
      
      if ((csp=cip->exts[csext])==NULL) {
        continue;
      }
      
      if (csp->todaysamples==0) {
        /* No samples today, might want to delete */
        k=0;
        for(j=0;j<HISTORYDAYS;j++) {
          k+=csp->lastdays[j];
        }
        if (k<50) {
          freechanstats(csp);
          cip->exts[csext]=NULL;
          releasechanindex(cip);
          continue;
        }
      }
      
      /* Move the samples along one */
      memmove(&(csp->lastdays[1]),&(csp->lastdays[0]),(HISTORYDAYS-1)*sizeof(short));
      memmove(&(csp->lastdaysamples[1]),&(csp->lastdaysamples[0]),(HISTORYDAYS-1)*sizeof(char));
      memmove(&(csp->lastmax[1]),&(csp->lastmax[0]),(HISTORYDAYS-1)*sizeof(unsigned short));
      
      csp->lastdaysamples[0]=csp->todaysamples;
      csp->lastdays[0]=(csp->todayusers * 10)/todaysamples;
      csp->lastmax[0]=csp->todaymax;
      
      csp->todaysamples=0;
      csp->todayusers=0;
      if (cip->channel) {
	csp->todaymax=cip->channel->users->totalusers;
      } else {
	csp->todaymax=0;
      }
    }
  }
  
  memmove(&lastdaysamples[1],&lastdaysamples[0],(HISTORYDAYS-1)*sizeof(unsigned int));
  lastdaysamples[0]=todaysamples;
  todaysamples=0;
}

void savechanstats() {
  FILE *fp;
  int i,j;
  chanindex *cip;
  chanstats *chp;
  
  if ((fp=fopen("chanstats","w"))==NULL) {
    return;
  }
  
  /* header: samples for today + last HISTORYDAYS days */
  
  fprintf(fp,"%lu %u",lastsample,todaysamples);
  for(i=0;i<HISTORYDAYS;i++) {
    fprintf(fp," %u",lastdaysamples[i]);
  }  
  fprintf(fp,"\n");
  
  /* body: channel, lastsample, samplestoday, sizetoday, <last sizes, last samples> */
  for (i=0;i<CHANNELHASHSIZE;i++) {
    for (cip=chantable[i];cip;cip=cip->next) {
      if ((chp=cip->exts[csext])==NULL) { 
        continue;
      }
      fprintf(fp,"%s %lu %u %u %u",cip->name->content,chp->lastsampled,chp->todaysamples,chp->todayusers,chp->todaymax);
    
      for (j=0;j<HISTORYDAYS;j++) {
        fprintf(fp," %u %u %u",chp->lastdays[j],chp->lastdaysamples[j],chp->lastmax[j]);
      }
      fprintf(fp,"\n");
    }
  }
  
  fclose(fp);
}

void loadchanstats() {
  FILE *fp;
  int i;
  char inbuf[2048];
  char *args[100];
  unsigned int chancount=0;
  chanstats *chp;
  chanindex *cip;
  
  if ((fp=fopen("chanstats","r"))==NULL) {
    Error("chanstats",ERR_ERROR,"Unable to load channel stats file");
    return;
  }
  
  fgets(inbuf,2048,fp);
  if (feof(fp)) {
    Error("chanstats",ERR_ERROR,"Corrupt channel stats file");
    fclose(fp);
    return;
  }
  
  if ((splitline(inbuf,args,50,0))!=(HISTORYDAYS+2)) {
    Error("chanstats",ERR_ERROR,"Corrupt channel stats file");
    fclose(fp);
    return;
  }
  
  lastsample=strtol(args[0],NULL,10);
  todaysamples=strtol(args[1],NULL,10);
  for(i=0;i<HISTORYDAYS;i++) {
    lastdaysamples[i]=strtol(args[i+2],NULL,10);
  }
  
  while (!feof(fp)) {
    fgets(inbuf,2048,fp);
    if (feof(fp)) {
      break;
    }
    
    if ((splitline(inbuf,args,100,0))!=5+(HISTORYDAYS*3)) {
      Error("chanstats",ERR_WARNING,"Corrupt channel stats line");
      continue;
    }
    
    cip=findorcreatechanindex(args[0]);
    if (cip->exts[csext]!=NULL) {
      Error("chanstats",ERR_ERROR,"Duplicate stats entry for channel %s",args[0]);
      continue;
    }
    cip->exts[csext]=chp=getchanstats();

    chp->lastsampled=strtol(args[1],NULL,10);
    chp->todaysamples=strtol(args[2],NULL,10);
    chp->todayusers=strtol(args[3],NULL,10);
    chp->todaymax=strtol(args[4],NULL,10);
    
    for(i=0;i<HISTORYDAYS;i++) {
      chp->lastdays[i]=strtol(args[5+(i*3)],NULL,10);
      chp->lastdaysamples[i]=strtol(args[6+(i*3)],NULL,10);
      chp->lastmax[i]=strtol(args[7+(i*3)],NULL,10);
    }
    
    chancount++;
  }
  
  Error("chanstats",ERR_INFO,"Loaded %u channels",chancount);
}

int dochanstats(void *source, int cargc, char **cargv) {
  chanstats *csp;
  chanindex *cip;
  nick *sender=(nick *)source;
  int i,j,k,l;
  int tot,emp;
  int themax;
  
  if (cargc<1) {
    controlreply(sender,"Usage: chanstats #channel");
    return CMD_ERROR;
  }
  
  cip=findchanindex(cargv[0]);
  
  if (cip==NULL) {
    controlreply(sender,"Can't find any record of %s.",cargv[0]);
    return CMD_ERROR;
  }
  
  csp=cip->exts[csext];

  if (csp==NULL) {
    controlreply(sender,"Can't find any stats for %s.",cip->name->content);
    return CMD_ERROR;
  }
  
  controlreply(sender,"Statistics for channel %s:",cip->name->content);
  
  if (uponehour==0) {
    controlreply(sender,"  [Service up <1hour, can't report last hour stats]");
  } else {
    tot=0; emp=0;
    for(i=0;i<SAMPLEHISTORY;i++) {
      tot+=csp->lastsamples[i];
      if (csp->lastsamples[i]==0) {
        emp++;
      }
    }
    controlreply(sender,"  Last hour     : %6.1f average users, empty %5.1f%% of the time",(float)tot/SAMPLEHISTORY,((float)emp/SAMPLEHISTORY)*100);
  }
  controlreply(sender,  "  Today so far  : %6.1f average users, empty %5.1f%% of the time, %4d max",
	       (float)csp->todayusers/todaysamples, ((float)(todaysamples-csp->todaysamples)/todaysamples)*100,
	       csp->todaymax);

  themax=csp->lastmax[0];
                                   
  controlreply(sender,  "  Yesterday     : %6.1f average users, empty %5.1f%% of the time, %4d max",
	       (float)csp->lastdays[0]/10, ((float)(lastdaysamples[0]-csp->lastdaysamples[0])/lastdaysamples[0])*100,
	       themax);
  
  /* 7-day average */
  j=k=l=0;
  for (i=0;i<7;i++) {
    j+=csp->lastdays[i];
    k+=csp->lastdaysamples[i];
    l+=lastdaysamples[i];
    if (csp->lastmax[i]>themax) {
      themax=csp->lastmax[i];
    }
  }
  controlreply(sender,  "  7-day average : %6.1f average users, empty %5.1f%% of the time, %4d max",
	       (float)j/70,(float)((l-k)*100)/l, themax);

  /* 14-day average: continuation of last loop */
  for (;i<14;i++) {
    j+=csp->lastdays[i];
    k+=csp->lastdaysamples[i];
    l+=lastdaysamples[i];
    if (csp->lastmax[i]>themax) {
      themax=csp->lastmax[i];
    }
  }
  controlreply(sender,  "  14-day average: %6.1f average users, empty %5.1f%% of the time, %4d max",
	       (float)j/140,(float)((l-k)*100)/l, themax);

  return CMD_OK;
}

#define EXPIREMIN 4

int doexpirecheck(void *source, int cargc, char **cargv) {
  nick *sender=(nick *)source;
  chanindex *cip;
  chanstats *csp;
  int i;
 
  if (cargc<1) {
    controlreply(sender, "Usage: expirecheck #channel");
    return CMD_ERROR;
  }

  if ((cip=findchanindex(cargv[0]))==NULL) {
    /* Couldn't find the channel at all: it's in no way big enough! */
    
    controlreply(sender,"delchan %s",cargv[0]);
    return CMD_OK;
  }
  
  if ((csp=(chanstats *)cip->exts[csext])==NULL) {
    /* No stats: similar */
    
    controlreply(sender,"delchan %s",cargv[0]);
    return CMD_OK;
  }
 
  /* Did they hit the minimum today? */
  if (csp->todaymax >= EXPIREMIN) {
    return CMD_OK;
  }
  
  /* Or recently? */
  for (i=0;i<HISTORYDAYS;i++) {
    if (csp->lastmax[i] >= EXPIREMIN) {
      return CMD_OK;
    }
  }
  
  /* If not, delchan time! */
  controlreply(sender,"delchan %s",cargv[0]);
  return CMD_OK;
}
    
int douserhistogram(void *source, int cargc, char **cargv) {
  int histdata[21];
  int serverdata[21];
  nick *np;
  nick *sender=(nick *)source;
  int top=0,tot=0,servertot=0,servertop=0;
  int i;
  int theserver=-1;
  int sig=0,serversig=0;
  
  if (cargc>0) {
    if ((theserver=findserver(cargv[0]))<0) {
      controlreply(sender,"Can't find server %s",cargv[0]);
      return CMD_ERROR;
    }
  }
  
  for (i=0;i<21;i++) {
    histdata[i]=0;
    serverdata[i]=0;
  } 
  
  for (i=0;i<NICKHASHSIZE;i++) {
    for (np=nicktable[i];np;np=np->next) {
      if (np->channels->cursi <= 20) {
        histdata[np->channels->cursi]++;
        if (theserver>=0 && homeserver(np->numeric)==theserver) {
          servertop++;
          serverdata[np->channels->cursi]++;
        }
        top++;
      }
    }
  }
  
  tot=top;
  servertot=servertop;
  
  controlreply(sender,"Size Count    %%ge Count+ %s",(theserver>=0?serverlist[theserver].name->content:""));
  
  for (i=0;i<21;i++) {
    if (theserver>=0) {
      controlreply(sender,"%4d %6d %4.1f%% %6d %6d %4.1f%% %6d",i,histdata[i],(100.0*histdata[i])/tot,top,
                   serverdata[i],(100.0*serverdata[i])/servertot, servertop);
      servertop-=serverdata[i];
      serversig += (i * serverdata[i]);
    } else { 
      controlreply(sender,"%4d %6d %4.1f%% %6d",i,histdata[i],(100.0*histdata[i])/tot,top);
    }
    sig += (i * histdata[i]);
    top-=histdata[i];
  }
  
  controlreply(sender,"Average channel count (all users): %.2f",(float)sig/tot);
  if (theserver>=0) {
    controlreply(sender,"Average channel count (%s): %.2f",serverlist[theserver].name->content,(float)serversig/servertot);
  }
}

#define HISTITEMS  30

int dochanhistogram(void *source, int cargc, char **cargv) {
  int histdata[6][HISTITEMS];
  float bounds[HISTITEMS] = {   0.0,   1.0,   2.0,   3.0,   4.0,   5.0,   6.0,   8.0,  10.0,  15.0,  20.0, 
                               25.0,  50.0,  75.0, 100.0, 150.0, 200.0, 250.0, 300.0, 400.0, 500.0,
                             1000.0,  1000000.0 };
  int histsize=23;
  int hists=0;
  nick *sender=(nick *)source;
  nick *targetnick;
  int i,j,pos;
  char header[(6*14)+10];
  char buf[512];
  
  header[0]='\0';
  
  if (cargc<1) {
    controlreply(sender,"Usage: channelhistogram <metrics>");
    controlreply(sender,"Valid metrics are: now, today, yesterday, days <daycount>, nick <nick>.");
    controlreply(sender,"Up to 6 metrics may be specified.");
    return CMD_ERROR;
  }
  
  for (i=0;i<cargc;i++) {
    /* Look for a valid metric */
    if (!ircd_strcmp(cargv[i],"now")) {
      dohist_now(histdata[hists++],bounds,histsize,NULL);
      strcat(header,"Current Chans ");
    } else if (!ircd_strcmp(cargv[i],"today")) {
      dohist_today(histdata[hists++],bounds,histsize);
      strcat(header,"Today Average ");
    } else if (!ircd_strcmp(cargv[i],"yesterday")) {
      dohist_days(histdata[hists++],bounds,histsize,1);
      strcat(header,"Yesterday Avg ");
    } else if (!ircd_strcmp(cargv[i],"namelen")) {
      dohist_namelen(histdata[hists++],bounds,histsize);
      strcat(header,"Name Length   "); 
    } else if (!ircd_strcmp(cargv[i],"days")) {
      if ((i+1)>=cargc) {
        controlreply(sender,"'days' metric requires a numeric argument.");
        return CMD_ERROR;
      }
      j=strtol(cargv[i+1],NULL,10);
      if (j<1 || j>14) {
        controlreply(sender,"Must specify a number of days between 1 and 14.");
        return CMD_ERROR;
      }
      dohist_days(histdata[hists++],bounds,histsize,j);
      sprintf(buf,"%2d Day Avg    ",j);
      strcat(header,buf);
      i++;
    } else if (!ircd_strcmp(cargv[i],"nick")) {
      if ((i+1)>=cargc) {
        controlreply(sender,"'nick' metric requires a nickname argument.");
        return CMD_ERROR;
      }
      if ((targetnick=getnickbynick(cargv[i+1]))==NULL) {
        controlreply(sender,"Couldn't find user %s.",cargv[i+1]);
        return CMD_ERROR;
      }
      dohist_now (histdata[hists++],bounds,histsize,targetnick);
      sprintf(buf,"w/%-11.11s ",targetnick->nick);
      strcat(header,buf);
      i++;
    } else {
      controlreply(sender,"Unknown metric %s.",cargv[i]);
      return CMD_ERROR;
    }
  }
  
  controlreply(sender,"              %s",header);

  buf[0]='\0';
  for (i=0;i<hists;i++) {
    strcat(buf,"InRnge Range+ ");
  }
  controlreply(sender,"              %s",buf);
  
  for(i=0;i<(histsize-1);i++) {
    if (i==(histsize-2)) {
      pos=sprintf(buf,"%6.1f+       ",bounds[i]);
    } else {
      pos=sprintf(buf,"%6.1f-%6.1f ",bounds[i],bounds[i+1]);
    }
    for(j=0;j<hists;j++) {
      pos+=sprintf(&(buf[pos]),"%6d %6d ",histdata[j][i]-histdata[j][i+1],histdata[j][i]);
    }
    controlreply(sender,"%s",buf);
  }
  
  return CMD_OK;
}

void dohist_now(int *data, float *bounds, int cats, nick *np) {
  int i,j;
  chanindex *cip;
  
  for (i=0;i<cats;i++) 
    data[i]=0;
  
  for (i=0;i<CHANNELHASHSIZE;i++) {
    for (cip=chantable[i];cip;cip=cip->next) {
      if (cip->channel==NULL) {
        continue;
      }
      if (np==NULL || NULL!=getnumerichandlefromchanhash(cip->channel->users, np->numeric))
      for (j=0;j<cats;j++) {
        if (cip->channel->users->totalusers>=bounds[j]) {
          data[j]++;
        }
      }
    }
  }    
}

void dohist_today(int *data, float *bounds, int cats) {
  int i,j;
  chanindex *cip;
  chanstats *csp;
  float f;

  for (i=0;i<cats;i++) 
    data[i]=0;
  
  for (i=0;i<CHANNELHASHSIZE;i++) {
    for (cip=chantable[i];cip;cip=cip->next) {
      if ((csp=cip->exts[csext])==NULL) {
        continue;
      }
      f=(float)csp->todayusers/todaysamples;
      for(j=0;j<cats;j++) {
        if (f>=bounds[j]) {
          data[j]++;
        }
      }
    }
  }      
}

void dohist_days(int *data, float *bounds, int cats, int days) {
  int i,j,k;
  chanindex *cip;
  chanstats *csp;
  float f;

  for (i=0;i<cats;i++) 
    data[i]=0;
  
  for (i=0;i<CHANNELHASHSIZE;i++) {
    for (cip=chantable[i];cip;cip=cip->next) {
      if ((csp=cip->exts[csext])==NULL) {
        continue;
      }
      k=0;
      for (j=0;j<days;j++) {
        k+=csp->lastdays[j];
      }
      f=(float)k/(days*10);
      for(j=0;j<cats;j++) {
        if (f>=bounds[j]) {
          data[j]++;
        }
      }
    }
  }
}

void dohist_namelen(int *data, float *bounds, int cats) {
  int i,j;
  chanindex *cip;      
  
  for (i=0;i<cats;i++)
    data[i]=0;
    
  for(i=0;i<CHANNELHASHSIZE;i++) {
    for (cip=chantable[i];cip;cip=cip->next) {
      for (j=0;j<cats;j++) {
        if (cip->name->length>=bounds[j]) {
          data[j]++;
        }
      }
    }
  }
}
