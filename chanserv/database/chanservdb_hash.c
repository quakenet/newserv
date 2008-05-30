/*
 * chanservhash.c:
 *  Handles insertion and retrieval of various data structures
 *  from their respective hashes.  Hopefully it's obvious from 
 *  the name of each function what it does.
 */

#include <string.h>
#include <strings.h>

#include "../chanserv.h"
#include "../../lib/irc_string.h"

reguser *regusernicktable[REGUSERHASHSIZE];

maildomain *maildomainnametable[MAILDOMAINHASHSIZE];
maildomain *maildomainIDtable[MAILDOMAINHASHSIZE];

#define regusernickhash(x)  ((crc32i(x))%REGUSERHASHSIZE)
#define maildomainnamehash(x)   ((crc32i(x))%MAILDOMAINHASHSIZE)
#define maildomainIDhash(x)     ((x)%MAILDOMAINHASHSIZE)

void chanservhashinit() {
  memset(regusernicktable,0,REGUSERHASHSIZE*sizeof(reguser *));
  memset(maildomainnametable,0,MAILDOMAINHASHSIZE*sizeof(maildomain *));
  memset(maildomainIDtable,0,MAILDOMAINHASHSIZE*sizeof(maildomain *));
}

void addregusertohash(reguser *rup) {
  unsigned int hash;
  authname *anp;
  
  hash=regusernickhash(rup->username);

  rup->nextbyname=regusernicktable[hash];
  regusernicktable[hash]=rup;

  anp=findorcreateauthname(rup->ID, rup->username);
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
 * findreguserbyemail()
 */
reguser *findreguserbyemail(const char *email) {
  int i;
  reguser *rup;

  for (i=0;i<REGUSERHASHSIZE;i++) {
    for (rup=regusernicktable[i];rup;rup=rup->nextbyname) {
      if (rup->email && !strcasecmp(email,rup->email->content)) {
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

void addmaildomaintohash(maildomain *mdp) {
  unsigned int hash;

  hash=maildomainnamehash(mdp->name->content);

  mdp->nextbyname=maildomainnametable[hash];
  maildomainnametable[hash]=mdp;

  hash=maildomainIDhash(mdp->ID);

  mdp->nextbyID=maildomainIDtable[hash];
  maildomainIDtable[hash]=mdp;
}

maildomain *findmaildomainbyID(unsigned int ID) {
  unsigned int hash;
  maildomain *mdp;

  hash=maildomainIDhash(ID);
  for (mdp=maildomainIDtable[hash]; mdp; mdp=mdp->nextbyID)
    if (mdp->ID==ID)
      return mdp;

  return NULL;
}

maildomain *findmaildomainbydomain(char *domain) {
  unsigned int hash;
  maildomain *mdp;

  hash=maildomainnamehash(domain);
  for (mdp=maildomainnametable[hash]; mdp; mdp=mdp->nextbyname)
    if (!strcasecmp(mdp->name->content, domain))
      return mdp;

  return NULL;
}

maildomain *findnearestmaildomain(char *domain) {
  maildomain *m = findmaildomainbydomain(domain);
  char *p;

  if(!m && (p=strchr(domain, '.')))
    return findnearestmaildomain(++p);

  return m;
}

maildomain *findmaildomainbyemail(char *email) {
  char *domain;

  if (!(domain=strchr(email, '@')))
    domain=email;
  else
    domain++;

  return findmaildomainbydomain(domain);
}

maildomain *findorcreatemaildomain(char *email) {
  unsigned int hash;
  char *domain,*pdomain;
  maildomain *mdp;

  if (!(domain=strchr(email, '@')))
    domain=email;
  else
    domain++;

  if( domain[0] == '.' )
    domain++;

  hash=maildomainnamehash(domain);
  for (mdp=maildomainnametable[hash]; mdp; mdp=mdp->nextbyname)
    if (!strcasecmp(mdp->name->content, domain))
      return mdp;

  mdp=getmaildomain();
  mdp->ID=0;
  mdp->name=getsstring(domain, EMAILLEN);
  mdp->count=0;
  mdp->limit=0;
  mdp->actlimit=MD_DEFAULTACTLIMIT;
  mdp->flags=MDFLAG_DEFAULT;
  mdp->users=NULL;
  addmaildomaintohash(mdp);

  pdomain=strchr(domain, '.');

  if(pdomain) {
    pdomain++;
    mdp->parent = findorcreatemaildomain(pdomain);
  } else {
    mdp->parent = NULL;
  }

  return mdp;
}

void removemaildomainfromhash(maildomain *mdp) {
  unsigned int hash;
  maildomain **mdh;
  int found;

  hash=maildomainnamehash(mdp->name->content);

  found=0;
  for (mdh=&(maildomainnametable[hash]);*mdh;mdh=&((*mdh)->nextbyname)) {
    if (*mdh==mdp) {
      *mdh=(mdp->nextbyname);
      found=1;
      break;
    }
  }

  if (found==0) {
    Error("chanserv",ERR_ERROR,"Unable to remove maildomain %s from namehash",
          mdp->name->content);
  }

  hash=maildomainIDhash(mdp->ID);
  found=0;

  for (mdh=&(maildomainIDtable[hash]);*mdh;mdh=&((*mdh)->nextbyID)) {
    if (*mdh==mdp) {
      *mdh=(mdp->nextbyID);
      found=1;
      break;
    }
  }

  if (found==0) {
    Error("chanserv",ERR_ERROR,"Unable to remove maildomain %s from ID hash",mdp->name->content);
  }
}

void addregusertomaildomain(reguser *rup, maildomain *mdp) {
  maildomain *smdp;
  rup->nextbydomain=mdp->users;
  mdp->users=rup;
  for(smdp=mdp;smdp;smdp=smdp->parent) {
    smdp->count++;
  }
}

void delreguserfrommaildomain(reguser *rup, maildomain *mdp) {
  maildomain *smdp, *nmdp;
  reguser *ruh, *pruh=NULL;
  int found=0;

  if(!mdp)
    return;

  for (ruh=mdp->users; ruh; ruh=ruh->nextbydomain) {
    if (ruh==rup) {
      /* Found the user */
      if (pruh)
        pruh->nextbydomain=ruh->nextbydomain;
      else
        mdp->users=ruh->nextbydomain;
      found=1;
      break;
    }
    pruh=ruh;
  }

  if (found==0) {
    Error("chanserv",ERR_ERROR,"Unable to remove user %s from maildomain %s",
          rup->username, mdp->name->content);
    return;
  }

  /* Free it from all the parent domains, cleaning up as we go.. */
  for(smdp=mdp;smdp;smdp=nmdp) {
    nmdp=smdp->parent;

    /* Keep it if there are users left or we're remembering something about it. */    
    if (--smdp->count || (smdp->flags != MDFLAG_DEFAULT) || (smdp->actlimit != MD_DEFAULTACTLIMIT))
      continue;
    
    removemaildomainfromhash(smdp);
    freesstring(smdp->name);
    freemaildomain(smdp);
  }

  rup->domain=NULL;
  freesstring(rup->localpart);
  rup->localpart=NULL;
}

