#include "trusts.h"

trustgroup_t *trustgroupidtable[TRUSTS_HASH_GROUPSIZE];
trustgroup_t *trustgroupnametable[TRUSTS_HASH_GROUPSIZE];
trusthost_t *trusthostidtable[TRUSTS_HASH_HOSTSIZE];
trusthost_t *trusthostgroupidtable[TRUSTS_HASH_HOSTSIZE];
trustgroupidentcount_t *trustgroupidentcounttable[TRUSTS_HASH_IDENTSIZE];

void trusts_hash_init() {
  memset(trustgroupidtable,0,sizeof(trustgroupidtable));
  memset(trustgroupnametable,0,sizeof(trustgroupnametable));
  memset(trusthostidtable,0,sizeof(trusthostidtable));
  memset(trusthostgroupidtable,0,sizeof(trusthostgroupidtable));
}

void trusts_hash_fini() {
}

void trusts_addtrusthosttohash(trusthost_t *newhost)
{
  unsigned int hash;
  hash = trusts_gettrusthostidhash(newhost->id);
  newhost->nextbyid = trusthostidtable[hash];
  trusthostidtable[hash] = newhost;

  hash = trusts_gettrusthostgroupidhash(newhost->trustgroup->id);
  newhost->nextbygroupid = trusthostgroupidtable[hash];
  trusthostgroupidtable[hash] = newhost;
}

void trusts_removetrusthostfromhash(trusthost_t *t)
{
  trusthost_t **tgh;
  int found = 0;
  for(tgh=&(trusthostidtable[trusts_gettrusthostidhash(t->id)]);*tgh;tgh=(trusthost_t **)&((*tgh)->nextbyid)) {
    if((*tgh)==t) {
      (*tgh)=(trusthost_t *)t->nextbyid;
      found = 1;
      break;
    }
  }
  if(!found)
    Error("trusts",ERR_ERROR,"Unable to remove trusthost id %lu from hashtable", t->id);
  found = 0;
  for(tgh=&(trusthostgroupidtable[trusts_gettrusthostgroupidhash(t->trustgroup->id)]);*tgh;tgh=(trusthost_t **)&((*tgh)->nextbygroupid)) {
    if((*tgh)==t) {
      (*tgh)=(trusthost_t *)t->nextbygroupid;
      found = 1;
      break;
    }
  }
  if(!found)
    Error("trusts",ERR_ERROR,"Unable to remove trusthost groupid %lu from hashtable", t->trustgroup->id);
}

void trusts_addtrustgrouptohash(trustgroup_t *newgroup)
{
  unsigned int hash;
  hash = trusts_gettrustgroupidhash(newgroup->id);
  newgroup->nextbyid = trustgroupidtable[hash];
  trustgroupidtable[hash] = newgroup;
}

trustgroup_t* findtrustgroupbyid(int id) {
  trustgroup_t* tl;
 
  for(tl=trustgroupidtable[trusts_gettrustgroupidhash(id)]; tl; tl = (trustgroup_t *)tl->nextbyid) { 
    if(tl->id == id) {
      return tl;
    }
  }
  return NULL;
}

trustgroup_t* findtrustgroupbyownerid(int ownerid) {
  trustgroup_t* tg;
  int i;

  for ( i = 0; i < TRUSTS_HASH_GROUPSIZE ; i++ ) {
    for ( tg = trustgroupidtable[i]; tg; tg = tg -> nextbyid ) {
      if(tg->ownerid == ownerid) {
        return tg;
      }
    }
  }
  return NULL;
}


void trusts_removetrustgroupfromhash(trustgroup_t *t)
{
  trustgroup_t **tg;
  int found = 0;
  for(tg=&(trustgroupidtable[trusts_gettrustgroupidhash(t->id)]);*tg;tg=(trustgroup_t **)&((*tg)->nextbyid)) {
    if((*tg)==t) {
      (*tg)=(trustgroup_t *)t->nextbyid;
      found = 1;
      break;
    }
  }
  if (!found)
    Error("trusts",ERR_ERROR,"Unable to remove trustgroup ID %lu from hashtable",t->id);
}

void trusts_addtrustgroupidenttohash(trustgroupidentcount_t *newident)
{
  unsigned int hash;
  hash = trusts_gettrustgroupidenthash(newident->ident->content);
  newident->next = trustgroupidentcounttable[hash];
  trustgroupidentcounttable[hash] = newident;
}

void trusts_removetrustgroupidentfromhash(trustgroupidentcount_t *t)
{
  trustgroupidentcount_t **thi;
  for(thi=&(trustgroupidentcounttable[trusts_gettrustgroupidenthash(t->ident->content)]);*thi;thi=(trustgroupidentcount_t **)&((*thi)->next)) {
    if((*thi)==t) {
      (*thi)=(trustgroupidentcount_t *)t->next;
      return; 
    }
  }
  Error("trusts",ERR_ERROR,"Unable to remove trustgroup ident %s from group %lu from hashtable",t->ident->content, t->trustgroup->id);
}

trustgroupidentcount_t* findtrustgroupcountbyident(char *ident, trustgroup_t *t) {
  trustgroupidentcount_t* tgi;

  for(tgi=trustgroupidentcounttable[trusts_gettrustgroupidenthash(ident)]; tgi; tgi = (trustgroupidentcount_t *)tgi->next) {
    if(tgi->trustgroup == t) {
      if(ircd_strcmp(tgi->ident->content,ident)==0) {
        return tgi;
      }
    }
  }
  return NULL;
}

