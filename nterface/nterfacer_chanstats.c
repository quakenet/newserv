/*
  nterfacer newserv chanstats module
  Copyright (C) 2004 Chris Porter.
*/

#include "../chanstats/chanstats.h"
#include "../core/error.h"
#include "../lib/version.h"

#include "library.h"
#include "nterfacer_control.h"

MODULE_VERSION("$Id")

int handle_chanstats(struct rline *li, int argc, char **argv);
struct handler *hl = NULL;

void _init(void) {
  if(!n_node) {
    Error("nterfacer_chanstats", ERR_ERROR, "Unable to register chanstats as nterfacer_control isn't loaded!");
    return;
  }
  hl = register_handler(n_node, "chanstats", 1, handle_chanstats);
}

void _fini(void) {
  if(hl)
    deregister_handler(hl);
}

int handle_chanstats(struct rline *li, int argc, char **argv) {
  chanstats *csp;
  chanindex *cip;
  int i,j,k,l;
  int tot,emp;
  int themax;
  float details[13];
  
  cip=findchanindex(argv[0]);
  
  if (cip==NULL)
    return ri_error(li, ERR_TARGET_NOT_FOUND, "Channel not found");

  csp=cip->exts[csext];

  if (csp==NULL)
    return ri_error(li, ERR_CHANSTATS_STATS_NOT_FOUND, "Stats not found");

  if (uponehour==0) {
    details[0] = -1;
    details[1] = -1;
  } else {
    tot=0; emp=0;
    for(i=0;i<SAMPLEHISTORY;i++) {
      tot+=csp->lastsamples[i];
      if (csp->lastsamples[i]==0) {
        emp++;
      }
    }
    details[0] = tot/SAMPLEHISTORY;
    details[1] = emp/SAMPLEHISTORY * 100;

  }

  details[2] = csp->todayusers/todaysamples;
  details[3] = ((float)(todaysamples-csp->todaysamples)/todaysamples)*100;
  details[4] = csp->todaymax;

  themax=csp->lastmax[0];

  details[5] = csp->lastdays[0]/10;
  details[6] = ((float)(lastdaysamples[0]-csp->lastdaysamples[0])/lastdaysamples[0])*100;
  details[7] = themax;
  
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

  details[8] = j/70;
  details[9] = ((l-k)*100)/l;
  details[10] = themax;

  /* 14-day average: continuation of last loop */
  for (;i<14;i++) {
    j+=csp->lastdays[i];
    k+=csp->lastdaysamples[i];
    l+=lastdaysamples[i];
    if (csp->lastmax[i]>themax) {
      themax=csp->lastmax[i];
    }
  }

  details[11] = j/140;
  details[12] = ((l-k)*100)/l;
  details[13] = themax;

  ri_append(li, "%.1f", details[0]);
  ri_append(li, "%.1f%%", details[1]);
  for(j=2;j<14;) {
    ri_append(li, "%.1f", details[j++]);
    ri_append(li, "%.1f%%", details[j++]);
    ri_append(li, "%d%%", details[j++]);
  }

  return ri_final(li);
}
