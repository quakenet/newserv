/* sstring.h - Declaration of "static strings" functions */

#define COMPILING_SSTRING
#include "sstring.h"

#include "../core/hooks.h"
#include "../core/nsmalloc.h"

#include <stdio.h>

#include <assert.h>
#include <stdlib.h>
#define __USE_GNU
#include <string.h>

#ifndef USE_VALGRIND

/* List of free stuff */
sstring *freelist[SSTRING_MAXLEN+1];

/* Global variables to track allocated memory */
sstring *structmem;
char *stringmem;
int structfree;
int stringfree;

/* Statistics counters */
int getcalls;
int freecalls;
int allocstruct;
int allocstring;

/* Internal function */
void sstringstats(int hooknum, void *arg);

void initsstring() {
  int i;
  for(i=0;i<=SSTRING_MAXLEN;i++)
    freelist[i]=NULL;
    
  structfree=0;
  stringfree=0;
  structmem=NULL;
  stringmem=NULL;
  
  /* Initialise statistics counters */
  getcalls=0;
  freecalls=0;
  allocstruct=0;
  allocstring=0;
  
  registerhook(HOOK_CORE_STATSREQUEST,&sstringstats);
}

sstring *getsstring(const char *inputstr, int maxlen) {
  int i;
  sstring *retval=NULL;
  int length;
  

  /* getsstring() on a NULL pointer returns a NULL sstring.. */
  if (inputstr==NULL) {
    return NULL;
  }	     

  if (inputstr[0]=='\0') {
    return NULL;
  }

  /* Only count calls that actually did something */
  getcalls++;

  length=strlen(inputstr)+1;
  if (length>maxlen) {
    length=maxlen+1;
  }
  assert(length<=SSTRING_MAXLEN);
  
  /* Check to see if an approximately correct 
   * sized string is available */
  for(i=0;i<SSTRING_SLACK;i++) {
    if (length+i>SSTRING_MAXLEN)
      break;
      
    if (freelist[length+i]!=NULL) {
      retval=freelist[length+i];
      freelist[length+i]=retval->u.next;
      retval->u.l.alloc=(length+i);
      break;
    }  
  }
  
  /* None found, allocate a new one */
  if (retval==NULL) {
getstruct:
    if (structfree < sizeof(sstring)) {
      /* We always allocate an exact multiple of these.  
       * Therefore, if there is enough for a partial structure we broke something */
      assert(structfree==0);
     
      /* Allocate more RAM */
      allocstruct++;
      structmem=(sstring *)nsmalloc(POOL_SSTRING,SSTRING_STRUCTALLOC);
      assert(structmem!=NULL);
      structfree=SSTRING_STRUCTALLOC;
    }

    retval=structmem;
    structmem++;
    structfree-=sizeof(sstring);
   
    if (stringfree < length) {
      /* Not enough left for what we want.
       * Allocate the remainder of our chunk (if any)
       * to something and immediately free it.
       * Decrement the freecalls counter to fix the stats */ 
      if (stringfree > 0) {
        retval->content=stringmem;
        retval->u.l.alloc=stringfree;
        stringfree=0;
        freecalls--;
        freesstring(retval);
        
        /* GOTO GOTO GOTO: We need to allocate 
         * another new struct here. Goto is the cleanest
         * way to do this */
        goto getstruct;
      } else {
        /* Grab some more string space */
        allocstring++;
        stringmem=(char *)nsmalloc(POOL_SSTRING,SSTRING_DATAALLOC);
        assert(stringmem!=NULL);
        stringfree=SSTRING_DATAALLOC;
      }
    }
    
    retval->content=stringmem;
    retval->u.l.alloc=length;
    stringfree-=length;
    stringmem+=length;
  }
 
  /*
   * At this point, retval points to a valid structure which is at
   * least the right size and has the "alloc" value set correctly
   */
  
  retval->u.l.length=(length-1);
  strncpy(retval->content,inputstr,(length-1));
  retval->content[length-1]='\0';

  return retval;    
}

void freesstring(sstring *inval) {
  int alloc;
 
  
  /* Allow people to call this with a NULL value */
  if (inval==NULL)
    return;
    
  /* Only count calls that actually did something */
  freecalls++;
  
  alloc=inval->u.l.alloc;
  assert(alloc<=SSTRING_MAXLEN);
  inval->u.next=freelist[alloc];
  freelist[alloc]=inval;
}

void sstringstats(int hooknum, void *arg) {
  char buf[512];
  int i,j;
  sstring *ssp;
  long statslev=(long)arg;
  
  if (statslev>10) {
    for(i=0,j=0;i<=SSTRING_MAXLEN;i++) {
      for(ssp=freelist[i];ssp;ssp=ssp->u.next)
        j++;
    }
  
    snprintf(buf,512,"SString : %7d get calls, %7d free calls, %7d struct allocs, %7d string allocs, %7d strings free",getcalls,freecalls,allocstruct,allocstring,j);
    triggerhook(HOOK_CORE_STATSREPLY,(void *)buf);
  }
}

#else /* USE_VALGRIND */
void initsstring() {
}

sstring *getsstring(const char *inputstr, int maxlen) {
  size_t len = strlen(inputstr);
  sstring *s;

  s=(sstring *)nsmalloc(POOL_SSTRING,sizeof(sstring));
  s->u.l.length = strlen(inputstr);
  s->content=(char *)nsmalloc(POOL_SSTRING,s->u.l.length + 1);

  memcpy(s->content, inputstr, len + 1);

  return s;
}

void freesstring(sstring *inval) {
  if(!inval)
    return;

  nsfree(POOL_SSTRING,inval->content);
  nsfree(POOL_SSTRING,inval);
}
#endif

int sstringcompare(sstring *ss1, sstring *ss2) {
  if (ss1->u.l.length != ss2->u.l.length)
    return -1;
  
  return strncmp(ss1->content, ss2->content, ss1->u.l.length);
}

