/* chanuserhash.c */

#include "channel.h"
#include "../server/server.h"
#include "../nick/nick.h"
#include "../lib/irc_string.h"
#include "../irc/irc_config.h"
#include "../parser/parser.h"
#include "../irc/irc.h"
#include "../lib/base64.h"

/*
 * rehashchannel:
 *  Make the channel hash a notch larger, to accomodate more users 
 */

void rehashchannel(channel *cp) {
  int i;
  chanuserhash *newhash;
  int newhashsize;

  /* Pick a new hash size.  If we're not making good use of the current hash, 
   * just increase size slightly, otherwise multiply size by 1.5 */

  if (cp->users->totalusers*1.6 < cp->users->hashsize) {
    newhashsize=cp->users->hashsize+1;
  } else {
    newhashsize= ((int)cp->users->hashsize*1.5);
    newhashsize |= 1;
    if (newhashsize<=cp->users->hashsize) {
      newhashsize++;
    }
  }

rehash:
  newhash=newchanuserhash(newhashsize);
  for (i=0;i<cp->users->hashsize;i++) {
    if (cp->users->content[i]!=nouser) {
      if (addnumerictochanuserhash(newhash,cp->users->content[i])) {
        /* It didn't fit.  It's clearly not our day :(
         * Increase the newhashsize by one and hope for a better fit
         * If you whine about the goto I'll just point out that the 
         * other alternative was exit(1) :) */
        newhashsize+=2;
        freechanuserhash(newhash);
        goto rehash;
      }
    }
  }
  
  /* If we got to here it means we crammed all the old stuff into the new hash */
  freechanuserhash(cp->users);
  cp->users=newhash;   
}

/*
 * addnumerictochanuserhash:
 *  Try to fit the given numeric into the channel user hash
 *
 * Returns 0 if the numeric went in, 1 if not. 
 */
    
int addnumerictochanuserhash(chanuserhash *cuh, long numeric) {
  int i,j,hash,hash2;
  
  /* Double hashing scheme; use the next higher bits of the numeric
   * for the second hash (increment) 
   *
   * TODO: discover if there is a better alternative 
   * to the fixed max search depth value */
   
  hash=(numeric&CU_NUMERICMASK);
  hash2=(hash/(cuh->hashsize))%(cuh->hashsize);
  hash=hash%(cuh->hashsize);
  
  if (hash2==0) {
    hash2=1;
  }

  for(i=0,j=hash;i<CUHASH_DEPTH && (j!=hash || i==0);i++,j=(j+hash2)%(cuh->hashsize)) {
    if (cuh->content[j]==nouser) {
      cuh->content[j]=numeric;
      cuh->totalusers++;
      return 0;
    }
  }
  
  /* Eeep, we didn't find a space */
  return 1;
}  

unsigned long *getnumerichandlefromchanhash(chanuserhash *cuh, long numeric) {
  int i, j, hash, hash2;

  hash=(numeric&CU_NUMERICMASK);
  hash2=(hash/(cuh->hashsize))%(cuh->hashsize);
  hash=hash%(cuh->hashsize);

  if (hash2==0) {
    hash2=1;
  }

  for (i=0,j=hash;i<CUHASH_DEPTH && (j!=hash || i==0);i++,j=(j+hash2)%(cuh->hashsize)) {
    if ((cuh->content[j]&CU_NUMERICMASK)==(numeric&CU_NUMERICMASK)) {
      return &(cuh->content[j]);
    }
  }
  
  return NULL;
}
