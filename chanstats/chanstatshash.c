
#include "chanstats.h"
#include "../lib/irc_string.h"
#include "../irc/irc_config.h"

#define chanstatshash(x) (irc_crc32i(x)%CHANSTATSHASHSIZE)

/*
 * findchanstats:
 *  Returns a chanstats * for the given channel name.
 *  Creates if necessary.
 */

chanstats *findchanstats(const char *channame) {
  chanstats *csp;
  unsigned int hash;
  
  hash=chanstatshash(channame);
  
  for (csp=chanstatstable[hash];csp;csp=csp->next) {
    if (!ircd_strcmp(channame,csp->name->content)) {
      return csp;
    }
  }
  
  /* It didn't match, create a new one */
  csp=getchanstats();
  memset(csp,0,sizeof(chanstats));
  csp->name=getsstring(channame,CHANNELLEN);
  
  csp->next=chanstatstable[hash];
  chanstatstable[hash]=csp;
  
  return csp;
}

/*
 * findchanstatsifexists:
 *  Returns a chanstats * for the given channel name.
 *  Returns NULL if there are no stats.
 */

chanstats *findchanstatsifexists(const char *channame) {
  chanstats *csp;
  unsigned int hash;
  
  hash=chanstatshash(channame);
  
  for (csp=chanstatstable[hash];csp;csp=csp->next) {
    if (!ircd_strcmp(channame,csp->name->content)) {
      return csp;
    }
  }
  
  return NULL;
}

  
  
