/* sstring.h - Declaration of "static strings" functions */

#define COMPILING_SSTRING
#include "sstring.h"

#include "../core/hooks.h"
#include "../core/nsmalloc.h"
#include "../core/error.h"
#include "../lib/irc_string.h"

#include <stdio.h>

#include <assert.h>
#include <stdlib.h>
#define __USE_GNU
#include <string.h>

/* List of free stuff */
static sstring *freelist[SSTRING_MAXLEN+1];
static sstring *sshash[SSTRING_HASHSIZE];

/* Global variables to track allocated memory */
static char *ssmem;
static int ssmemfree;

/* Statistics counters */
static int getcalls;
static int freecalls;
static int allocs;

/* Internal functions */
static void sstringstats(int hooknum, void *arg);
static void salloc(void);

#ifndef USE_VALGRIND

#define sunprotect(x)
#define sunprotectb(x)
#define sprotect(x)

#else

#define __USE_MISC

#include <sys/mman.h>
static void *mblock;
struct mblock_list {
  void *block;
  struct mblock_list *next;
};

static void *mblock_head;

#define sunprotectb(x) mprotect(x, SSTRING_ALLOC, PROT_READ|PROT_WRITE);
#define sunprotect(x) sunprotectb((x)->block);
#define sprotect(x) mprotect((x)->block, SSTRING_ALLOC, PROT_READ);

#ifndef MAP_ANON
#define MAP_ANON MAP_ANONYMOUS
#endif

#endif /* USE_VALGRIND */

void initsstring() {
  int i;
  for(i=0;i<=SSTRING_MAXLEN;i++)
    freelist[i]=NULL;
  
  for(i=0;i<SSTRING_HASHSIZE;i++)
    sshash[i]=NULL;  

  ssmemfree=0;  
  ssmem=NULL;
    
  /* Initialise statistics counters */
  getcalls=0;
  freecalls=0;
  allocs=0;
  
  registerhook(HOOK_CORE_STATSREQUEST,&sstringstats);
}

#ifndef USE_VALGRIND
void finisstring() {
  nsfreeall(POOL_SSTRING);
}

static void salloc(void) {
  ssmem=(char *)nsmalloc(POOL_SSTRING, SSTRING_ALLOC);
  ssmemfree=SSTRING_ALLOC;
}
#endif /* USE_VALGRIND */

sstring *findsstring(const char *str) {
  unsigned int hash=crc32(str)%SSTRING_HASHSIZE;
  sstring *ss;
  
  for (ss=sshash[hash];ss;ss=ss->next)
    if (!strcmp(str, ss->content))
      return ss;
  
  return NULL;
}

void sstring_enhash(sstring *ss) {
  unsigned int hash=crc32(ss->content)%SSTRING_HASHSIZE;
  
  ss->next=sshash[hash];
  sshash[hash]=ss;
}

void sstring_dehash(sstring *ss) {
  unsigned int hash=crc32(ss->content)%SSTRING_HASHSIZE;
  sstring *ssh, *ssp;
  
  for (ssh=sshash[hash],ssp=NULL; ssh; ssp=ssh,ssh=ssh->next) {
    if (ssh==ss) {
      if (!ssp) {
        sshash[hash]=ss->next;
      } else {
#ifndef USE_VALGRIND
        ssp->next=ss->next;
#else
        if (ssp->block!=ss->block) {
          sunprotect(ssp) {
            ssp->next=ss->next;
          } sprotect(ssp);
        } else {
          ssp->next=ss->next;
        }
      }
#endif
      return;
    }
  }
  
  Error("sstring",ERR_WARNING,"sstring_dehash(): Unable to find string (ref=%d, length=%d) in hash: %s",ss->refcount,ss->length,ss->content);
}

sstring *getsstring(const char *inputstr, int maxlen) {
  int i;
  sstring *retval=NULL;
  int length, foreignblock;
  char strbuf[SSTRING_MAXLEN];

  /* getsstring() on a NULL pointer returns a NULL sstring.. */
  if (inputstr==NULL) {
    return NULL;
  }	     

  if (inputstr[0]=='\0') {
    return NULL;
  }
  
  if (maxlen>SSTRING_MAX) {
    Error("sstring", ERR_ERROR, "Attempt to allocate overlength string (maxlen=%d)",maxlen);
    return NULL;
  }

  /* Only count calls that actually did something */
  getcalls++;

  /* Make a copy of the desired string to make things easier later */
  for (length=0;length<maxlen;length++) {
    strbuf[length]=inputstr[length];
    if (!strbuf[length])
      break;
  }
  strbuf[length]='\0';
  
  length++;

  /* If it's hashed this is easy */
  if ((retval=findsstring(strbuf))) {
    sunprotect(retval) {
      retval->refcount++;
    } sprotect(retval);

    return retval;
  }

  foreignblock=0;  
  /* Check to see if an approximately correct 
   * sized string is available */
  for(i=0;i<SSTRING_SLACK;i++) {
    if (length+i>SSTRING_MAXLEN)
      break;
      
    if (freelist[length+i]!=NULL) {
      retval=freelist[length+i];
      freelist[length+i]=retval->next;
      sunprotect(retval);
      foreignblock=1;

      retval->alloc=(length+i);
      break;
    }  
  }
  
  /* None found, allocate a new one */
  if (retval==NULL) {
    /* Check for free memory */
    if (ssmemfree < (sizeof(sstring)+length)) {
      /* Not enough for us - turn the remaining memory into a free string for later */
      if (ssmemfree>sizeof(sstring)) {
        retval=(sstring *)ssmem;
        sunprotectb(mblock);
        retval->block=mblock;
        retval->alloc=(ssmemfree-sizeof(sstring));
        retval->refcount=0;
        freesstring(retval);
      }
  
      allocs++;     
      salloc();
    } else {
      sunprotectb(mblock);
    }
    
    retval=(sstring *)ssmem;
    ssmem+=(sizeof(sstring)+length);
    ssmemfree-=(sizeof(sstring)+length);
    
    retval->alloc=length;

    /* If there's a fragment left over, lump it into this allocation */
    if (ssmemfree < (sizeof(sstring)+1)) {
      retval->alloc += ssmemfree;
      ssmemfree=0;
      ssmem=NULL;
    }
  }    
 
  /*
   * At this point, retval points to a valid structure which is at
   * least the right size and has the "alloc" value set correctly
   */
  
  retval->length=(length-1);
  strcpy(retval->content,strbuf);
  retval->refcount=1;
  
#ifdef USE_VALGRIND 
  if(!foreignblock)
    retval->block = mblock;
#endif

  sstring_enhash(retval);
  sprotect(retval);

  return retval;    
}

void freesstring(sstring *inval) {
  int alloc;
  
  /* Allow people to call this with a NULL value */
  if (inval==NULL)
    return;
    
  /* Only count calls that actually did something */
  freecalls++;
  
  if (inval->refcount)
    sunprotect(inval);

  if (inval->refcount > 1) {
    inval->refcount--;
    sprotect(inval);
    return;
  }

  /* If refcount==0 it wasn't hashed, or protected */
  if (inval->refcount)
    sstring_dehash(inval);

  alloc=inval->alloc;
  assert(alloc<=SSTRING_MAXLEN);
  inval->next=freelist[alloc];
  freelist[alloc]=inval;
  sprotect(inval);
}

void sstringstats(int hooknum, void *arg) {
  char buf[512];
  int i,j;
  sstring *ssp;
  long statslev=(long)arg;
  
  if (statslev>10) {
    for(i=0,j=0;i<=SSTRING_MAXLEN;i++) {
      for(ssp=freelist[i];ssp;ssp=ssp->next)
        j++;
    }
  
    snprintf(buf,512,"SString : %7d get calls, %7d free calls, %7d allocs, %7d strings free",getcalls,freecalls,allocs,j);
    triggerhook(HOOK_CORE_STATSREPLY,(void *)buf);
  }
}

int sstringcompare(sstring *ss1, sstring *ss2) {
  if (ss1->length != ss2->length)
    return -1;
  
  return strncmp(ss1->content, ss2->content, ss1->length);
}

#ifdef USE_VALGRIND
void finisstring() {
  struct mblock_list *c, *n;
  for (c=mblock_head;c;c=n) {
    n=c->next;
    munmap(c->block, SSTRING_ALLOC);
  }
}

static void salloc(void) {
  struct mblock_list *n;
  mblock=mmap((void *)0, SSTRING_ALLOC, PROT_WRITE|PROT_READ, MAP_PRIVATE|MAP_ANON, -1, 0);

  n=(struct mblock_list *)mblock;
  n->block = mblock;
  n->next = mblock_head;
  mblock_head = n;

  ssmem=(char *)mblock + sizeof(struct mblock_list);
  ssmemfree=SSTRING_ALLOC-sizeof(struct mblock_list);
}
#endif /* USE_VALGRIND */
