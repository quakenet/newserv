/*
 * chanservhash.c:
 *  Handles insertion and retrieval of various data structures
 *  from their respective hashes.  Hopefully it's obvious from 
 *  the name of each function what it does.
 */

#include <string.h>

#include "chanserv.h"
#include "../lib/irc_string.h"

reguser *regusernicktable[REGUSERHASHSIZE];

#define regusernickhash(x)  ((crc32i(x))%REGUSERHASHSIZE)

void chanservhashinit() {
  memset(regusernicktable,0,REGUSERHASHSIZE*sizeof(reguser *));
}

void addregusertohash(reguser *rup) {
  unsigned int hash;
  authname *anp;
  
  hash=regusernickhash(rup->username);

  rup->nextbyname=regusernicktable[hash];
  regusernicktable[hash]=rup;

  anp=findorcreateauthname(rup->ID);
  anp->exts[chanservaext]=rup;
}

reguser *findreguserbynick(const char *nick) {
  unsigned int hashnick;
  reguser *rup;

  hashnick=regusernickhash(nick);
  for (rup=regusernicktable[hashnick];rup;rup=rup->nextbyname) {
    if (!ircd_strcmp(nick,rup->username)) {
      return rup;
    }
  }

  /* Not found */
  return NULL;
}

reguser *findreguserbyID(unsigned int ID) {
  authname *anp;

  anp=findauthname(ID);
  if (anp)
    return (reguser *)anp->exts[chanservaext];
  else
    return NULL;
}

/*
 * findreguser:
 *  This function does the standard "nick or #user" lookup.
 *  If "sender" is not NULL, a suitable error message will
 *  be sent if the lookup fails.
 */

reguser *findreguser(nick *sender, const char *str) {
  reguser *rup;
  nick *np;

  if (!str || !*str)
    return NULL;

  if (*str=='#') {
    if (str[1]=='\0') {
      if (sender)
      	chanservstdmessage(sender, QM_UNKNOWNUSER, str);
      return NULL;
    }
    if (!(rup=findreguserbynick(str+1)) && sender)
      chanservstdmessage(sender, QM_UNKNOWNUSER, str);
  } else {
    if (!(np=getnickbynick(str))) {
      if (sender)
      	chanservstdmessage(sender, QM_UNKNOWNUSER, str);
      return NULL;
    }
    if (!(rup=getreguserfromnick(np)) && sender)
      chanservstdmessage(sender, QM_USERNOTAUTHED, str);
  }

  if (rup && (UIsSuspended(rup) || (rup->status & QUSTAT_DEAD))) {
    chanservstdmessage(sender, QM_USERHASBADAUTH, rup->username);
    return NULL;
  }

  return rup;
}
    
/*
 * findreguserbyemail()
 */
reguser *findreguserbyemail(const char *email) {
  int i;
  reguser *rup;

  for (i=0;i<REGUSERHASHSIZE;i++) {
    for (rup=regusernicktable[i];rup;rup=rup->nextbyname) {
      if (!ircd_strcmp(email,rup->email->content)) {
        return rup;
      }
    }
  } 
  /* Not found */
  return NULL;
}

void removereguserfromhash(reguser *rup) {
  unsigned int hash;
  reguser **ruh;
  authname *anp;
  
  int found;

  hash=regusernickhash(rup->username);
  
  found=0;
  for (ruh=&(regusernicktable[hash]);*ruh;ruh=&((*ruh)->nextbyname)) {
    if (*ruh==rup) {
      *ruh=(rup->nextbyname);
      found=1;
      break;
    }
  }
  
  if (found==0) {
    Error("chanserv",ERR_ERROR,"Unable to remove reguser %s from nickhash",
	  rup->username);
  }

  anp=findauthname(rup->ID);
  
  if (anp) {
    anp->exts[chanservaext]=NULL;
    releaseauthname(anp);
  } else {
    Error("chanserv",ERR_ERROR,"Unable to remove reguser %s from ID hash",rup->username);
  }  
}

void addregusertochannel(regchanuser *rcup) {
  rcup->nextbyuser=(rcup->user->knownon);
  rcup->nextbychan=(rcup->chan->regusers[(rcup->user->ID)%REGCHANUSERHASHSIZE]);

  rcup->user->knownon=rcup;
  rcup->chan->regusers[(rcup->user->ID)%REGCHANUSERHASHSIZE]=rcup;
}

regchanuser *findreguseronchannel(regchan *rcp, reguser *rup) {
  regchanuser *rcup;

  for (rcup=rcp->regusers[(rup->ID)%REGCHANUSERHASHSIZE];rcup;rcup=rcup->nextbychan) {
    if (rcup->user==rup) {
      return rcup;
    }
  }
  
  /* Not found */
  return NULL;
}

void delreguserfromchannel(regchan *rcp, reguser *rup) {
  regchanuser **rcuh;
  int found=0;

  for (rcuh=&(rcp->regusers[(rup->ID)%REGCHANUSERHASHSIZE]);*rcuh;
       rcuh=&((*rcuh)->nextbychan)) {
    if ((*rcuh)->user==rup) {
      /* Found the user */
      freesstring((*rcuh)->info);
      (*rcuh)->info=NULL;
      *rcuh=(*rcuh)->nextbychan;
      found=1;
      break;
    }
  }

  if (found==0) {
    Error("chanserv",ERR_ERROR,"Unable to remove user %s from channel %s",
	  rup->username,rcp->index->name->content);
    return;
  }

  for (rcuh=&(rup->knownon);*rcuh;rcuh=&((*rcuh)->nextbyuser)) {
    if ((*rcuh)->chan==rcp) {
      /* Found the channel */
      *rcuh=(*rcuh)->nextbyuser;
      return;
    }
  }

  Error("chanserv",ERR_ERROR,"Unable to remove channel %s from user %s",
	rcp->index->name->content,rup->username);
}
