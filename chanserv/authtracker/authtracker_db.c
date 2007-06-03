/* Keeps track of current "dangling" auths.
 *
 * Every entry here corresponds to a user who has an open session in the
 * session table, but we cannot be sure that the user has actually gone. 
 * This means either (a) their server has split off or (b) we restarted and
 * found a dangling entry in the session table.
 *
 * Entries leave when either (a) the user comes back (the entry is dropped),
 * or (b) the server the user was on relinks without them, in which case we
 * close off their session.
 */ 

#include "authtracker.h"
#include "../../core/nsmalloc.h"
#include "../../server/server.h"
#include "../../irc/irc_config.h"

#include <time.h>
 
#define DANGLING_HASHSIZE	500
#define dangling_hash(x)	((x)%DANGLING_HASHSIZE)

#define	ALLOCUNIT	100

struct dangling_entry {
  unsigned int	numeric;
  unsigned long userid;
  time_t	authts;
  time_t	losttime;
  int		reason; /* AT_NETSPLIT or AT_RESTART */
  struct dangling_entry *next;
};

struct dangling_server {
  struct dangling_entry *de[DANGLING_HASHSIZE];
};

struct dangling_server *ds[MAXSERVERS];
struct dangling_entry *free_des;

static struct dangling_entry *get_de() {
  struct dangling_entry *dep;
  int i;
  
  if (free_des == NULL) {
    free_des = (struct dangling_entry *)nsmalloc(POOL_AUTHTRACKER, ALLOCUNIT * sizeof(struct dangling_entry));
    for (i=0;i<(ALLOCUNIT-1);i++) {
      free_des[i].next= &(free_des[i+1]);
    }
    free_des[ALLOCUNIT-1].next=NULL;
  }
  
  dep=free_des;
  free_des=dep->next;
  
  return dep;
}

static void free_de(struct dangling_entry *dep) {
  dep->next=free_des;
  free_des=dep;
}

void at_lostnick(unsigned int numeric, unsigned long userid, time_t accountts, time_t losttime, int reason) {
  unsigned int server=homeserver(numeric);
  unsigned int i;
  struct dangling_entry *dep;
  unsigned int thehash=dangling_hash(numeric);
  
  /* If their server doesn't have an entry, make one. */
  if (!ds[server]) {
    ds[server]=nsmalloc(POOL_AUTHTRACKER, sizeof(struct dangling_server));
    for (i=0;i<DANGLING_HASHSIZE;i++)
      ds[server]->de[i]=NULL;
  }
  
  /* Now make an entry */
  dep=get_de();
  dep->numeric=numeric;
  dep->userid=userid;
  dep->authts=accountts;
  dep->losttime=losttime;
  dep->reason=reason;
  dep->next=ds[server]->de[thehash];
  ds[server]->de[thehash]=dep;
}

/* Removes a returning user from the "dangling" tables.  Return 1 if we found it, 0 otherwise. */
int at_foundnick(unsigned int numeric, unsigned long userid, time_t accountts) {
  unsigned int server=homeserver(numeric);
  struct dangling_entry *dep, **deh;
  unsigned int thehash=dangling_hash(numeric);
  
  /* If we've not got an entry for their server we certainly won't for them. */
  if (!ds[server])
    return 0;
  
  for (deh=&(ds[server]->de[thehash]); *deh; deh=&((*deh)->next)) {
    dep=*deh;
    if ((dep->numeric == numeric) && (dep->userid==userid) && (dep->authts==accountts)) {
      /* Got it */
      *deh=dep->next;
      free_de(dep);
      return 1;
    }
  }
 
  /* Dropped through - didn't find it */ 
  return 0;
}

/* When a server is back (fully linked), any remaining dangling users on that server are definately gone. */
void at_serverback(unsigned int server) {
  int i;
  struct dangling_entry *dep, *ndep;
  
  if (!ds[server])
    return;
  
  for (i=0;i<DANGLING_HASHSIZE;i++) {
    for (dep=ds[server]->de[i];dep;dep=ndep) {
      ndep=dep->next;
      
      at_logquit(dep->userid, dep->authts, time(NULL), (dep->reason==AT_NETSPLIT)? "(netsplit)" : "(restart)");
      free_de(dep);
    }
  }
  
  nsfree(POOL_AUTHTRACKER, ds[server]);
  ds[server]=NULL;
}

void at_flushghosts() {
  int i;
  
  for (i=0;i<MAXSERVERS;i++) {
    if (serverlist[i].linkstate == LS_LINKED)
      at_serverback(i);
  }
}
