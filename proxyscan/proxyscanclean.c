/*
 * proxyscanclean.c:
 *  This file deals with the "clean" hosts.
 */

#include "proxyscan.h"
#include <time.h>
#include <stdio.h>
#include "../core/error.h"

#define CLEANHOSTHASHSIZE     20000

cleanhost *cleantable[CLEANHOSTHASHSIZE];
time_t rescaninterval;

void cleanhostinit(time_t ri) {
  rescaninterval=ri;
  memset(cleantable,0,sizeof(cleantable));
}

void addcleanhost(unsigned long IP, time_t timestamp) {
  cleanhost *chp;
  int hash;

  hash=(IP%CLEANHOSTHASHSIZE);

  chp=getcleanhost();
  chp->IP=IP;
  chp->lastscan=timestamp;
  chp->next=cleantable[hash];
  
  cleantable[hash]=chp;
}

void delcleanhost(cleanhost *chp) {
  cleanhost **chh;
  int hash;

  hash=(chp->IP%CLEANHOSTHASHSIZE);

  for (chh=&(cleantable[hash]);*chh;chh=&((*chh)->next)) {
    if (*chh==chp) {
      *chh=chp->next;
      break;
    }
  }

  freecleanhost(chp);
}

/*
 * checkcleanhost:
 *  Returns:
 *   0 - Host has been checked recently and is clean
 *   1 - Host has not been checked recently, or was not clean
 */

int checkcleanhost(unsigned long IP) {
  int hash;
  cleanhost *chp;

  hash=(IP%CLEANHOSTHASHSIZE);

  for (chp=cleantable[hash];chp;chp=chp->next) {
    if (chp->IP==IP) {
      /* match */
      if(chp->lastscan < (time(NULL)-rescaninterval)) {
	/* Needs rescan; delete and return 1 */
	delcleanhost(chp);
	return 1;
      } else {
	/* Clean */
	return 0;
      }
    }
  }

  /* Not found: return 1 */
  return 1;
}

/* 
 * dumpcleanhosts:
 *  Dumps all clean hosts to a savefile.  Expires hosts as it goes along
 */

void dumpcleanhosts(void *arg) {
  int i;
  FILE *fp;
  cleanhost *chp,*nchp;

  if ((fp=fopen("cleanhosts","w"))==NULL) {
    Error("proxyscan",ERR_ERROR,"Unable to open cleanhosts file for writing!");
    return;
  }

  for(i=0;i<CLEANHOSTHASHSIZE;i++) {
    for(chp=cleantable[i];chp;chp=nchp) {
      nchp=chp->next;
      if (chp->lastscan < (time(NULL)-rescaninterval)) {
	/* Needs rescan anyway, so delete it */
	delcleanhost(chp);
      } else {
	fprintf(fp,"%lu %lu\n",chp->IP,chp->lastscan);
      }
    }
  }

  fclose(fp);
}

/*
 * loadcleanhosts:
 *  Loads clean hosts in from database. 
 */

void loadcleanhosts() {
  FILE *fp;
  unsigned long IP,timestamp;
  char buf[512];

  if ((fp=fopen("cleanhosts","r"))==NULL) {
    Error("proxyscan",ERR_ERROR,"Unable to open cleanhosts file for reading!");
    return;
  }

  while (!feof(fp)) {
    fgets(buf,512,fp);
    if (feof(fp)) {
      break;
    }

    if ((sscanf(buf,"%lu %lu",&IP,&timestamp))==2) {
      addcleanhost(IP,timestamp);      
    }
  }
}

/*
 * cleancount:
 *  Returns the number of "clean" host entries present 
 */

unsigned int cleancount() {
  int i;
  unsigned int total=0;
  cleanhost *chp;

  for(i=0;i<CLEANHOSTHASHSIZE;i++) {
    for (chp=cleantable[i];chp;chp=chp->next) {
      total++;
    }
  }

  return total;
}
