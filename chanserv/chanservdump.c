/* chanservdump.c */

#include <stdio.h>
#include <string.h>

#include "chanserv.h"

struct lastjoin {
  unsigned int chanid;
  unsigned int userid;
  time_t lastjoin;
};

int dumplastjoindata(const char *filename) {
  FILE *fp;
  chanindex *cip;
  regchan *rcp;
  regchanuser *rcup;
  int i,j,total=0;
  struct lastjoin lj;

  Error("chanserv",ERR_INFO,"Dumping last join data.");  

  if (!(fp=fopen(filename,"w"))) {
    Error("chanserv",ERR_ERROR,"Error opening lastjoin dump file.");
    return 1;
  }
    
  for (i=0;i<CHANNELHASHSIZE;i++) {
    for (cip=chantable[i];cip;cip=cip->next) {
      if (!(rcp=cip->exts[chanservext]))
	continue;
      
      lj.chanid=rcp->ID;
      for (j=0;j<REGCHANUSERHASHSIZE;j++) {	
	for (rcup=rcp->regusers[j];rcup;rcup=rcup->nextbychan) {
	  lj.userid=rcup->user->ID;
	  lj.lastjoin=rcup->usetime;
	  if (!fwrite(&lj,sizeof(struct lastjoin),1,fp)) {
	    Error("chanserv",ERR_ERROR,"Error saving lastjoin data.");
	    fclose(fp);
	    return 1;
	  }
	  total++;
	}
      }
    }
  }
  
  Error("chanserv",ERR_INFO,"Dumped %d last join records to file",total);
  
  fclose(fp);
  
  return 0;
}

int readlastjoindata(const char *filename) {
  FILE *fp;
  reguser *rup=NULL;
  chanindex *cip;
  regchan *rcp=NULL;
  regchan **allchans;
  regchanuser *rcup=NULL;
  unsigned int lastuser=0;
  struct lastjoin lj;
  int i;
  int badcount=0,total=0;

  if (!(fp=fopen(filename,"r"))) {
    Error("chanserv",ERR_ERROR,"Error opening lastjoin dump file.");
    return 1;
  }

  /* Set up the allchans and allusers arrays */
  allchans=(regchan **)malloc((lastchannelID+1)*sizeof(regchan *));
  memset(allchans,0,(lastchannelID+1)*sizeof(regchan *));
  for (i=0;i<CHANNELHASHSIZE;i++) {
    for (cip=chantable[i];cip;cip=cip->next) {
      if ((rcp=cip->exts[chanservext]))
	allchans[rcp->ID]=cip->exts[chanservext];
    }
  }  
  
  while (fread(&lj, sizeof(struct lastjoin), 1, fp)) {
    total++;
    if (lj.userid != lastuser) {
      rup=findreguserbyID(lj.userid);
      lastuser=lj.userid;
    }

    if (lj.chanid > lastchannelID)
      rcp=NULL;
    else
      rcp=allchans[lj.chanid];

    if (!rup || !rcp) {
      badcount++;
      continue;
    }

    if (!(rcup=findreguseronchannel(rcp, rup))) {
      badcount++;
      continue;
    }

    rcup->usetime=lj.lastjoin;
  }

  Error("chanserv",ERR_INFO,"Retrieved %d last join entries from file (%d bad entries)",total,badcount);

  fclose(fp);
  free(allchans);

  return 0;
}
      
