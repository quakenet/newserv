/* hooks.c */

#include "hooks.h"
#include <assert.h>
#include "../core/error.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define HF_CONTIGUOUS 0x01

typedef struct Hook {
  HookCallback callback;
  long priority;
  short flags;
  struct Hook *next;
} Hook;

typedef struct HookHead {
  int count;
  int alive;
  int dirty;
  Hook *head;
} HookHead;

static HookHead hooks[HOOKMAX];
static int dirtyhooks[HOOKMAX];
static int dirtyhookcount;

unsigned int hookqueuelength = 0;

static void defraghook(HookHead *h);
static void markdirty(int hook);

void inithooks() {
}

int registerhook(int hooknum, HookCallback callback) {
  return registerpriorityhook(hooknum, callback, PRIORITY_DEFAULT);
}

int registerpriorityhook(int hooknum, HookCallback callback, long priority) {
  Hook *hp, *pred, *n;
  HookHead *h;

  if(hooknum>HOOKMAX)
    return 1;

  if(hookqueuelength > 0)
    Error("core", ERR_WARNING, "Attempting to register hook %d inside a hook queue: %p", hooknum, callback);

  for(pred=NULL,h=&hooks[hooknum],hp=h->head;hp;hp=hp->next) {
    if(hp->callback==callback)
      return 1;
    if(priority>=hp->priority)
      pred=hp;
  }

  n = malloc(sizeof(Hook));
  n->priority = priority;
  n->callback = callback;
  n->flags = 0;

  if(!pred) {
    n->next = h->head;
    h->head = n;
  } else {
    n->next = pred->next;
    pred->next = n;
  }

  if(h->count > 0)
    markdirty(hooknum);

  h->count++;
  h->alive++;

  return 0;
}

int deregisterhook(int hooknum, HookCallback callback) {
  Hook *hp;
  HookHead *h;

  if (hooknum>HOOKMAX)
    return 1;
    
  if(hookqueuelength > 0)
    Error("core", ERR_WARNING, "Attempting to deregister hook %d inside a hook queue: %p", hooknum, callback);

  for(h=&hooks[hooknum],hp=h->head;hp;hp=hp->next) {
    if(hp->callback==callback) {
      markdirty(hooknum);
      hp->callback = NULL;
      h->alive--;
      return 0;
    }
  }

  return 1;
}
  
void triggerhook(int hooknum, void *arg) {
  int i;
  Hook *hp;

  if (hooknum>HOOKMAX)
    return;
    
  hookqueuelength++;
  for(hp=hooks[hooknum].head;hp;hp=hp->next) {
    if(hp->callback)
      (hp->callback)(hooknum, arg);
  }
  hookqueuelength--;

  for(i=0;i<dirtyhookcount;i++) {
    defraghook(&hooks[dirtyhooks[i]]);
  }
  dirtyhookcount=0;

  if (!hookqueuelength && hooknum!=HOOK_CORE_ENDOFHOOKSQUEUE)
    triggerhook(HOOK_CORE_ENDOFHOOKSQUEUE, 0);
}

static void markdirty(int hook) {
  if(hooks[hook].dirty)
    return;

  hooks[hook].dirty = 1;
  dirtyhooks[dirtyhookcount++] = hook;
}

/* Non compacting version
static void defraghook(HookHead *h) {
  Hook *hp, *pp, *np;

  for(pp=NULL,hp=h->head;hp;hp=np) {
    np=hp->next;
    if(hp->callback==NULL) {
      free(hp);
    } else {
      if(pp==NULL) {
        h->head = hp;
      } else {
        pp->next = hp;
      }
      pp = hp;
    }
  }
  if(!pp) {
    h->head = NULL;
  } else {
    pp->next = NULL;
  }

  h->dirty = 0;
  h->count = h->alive;
}
*/

static void defraghook(HookHead *h) {
  Hook *hp, *pp, *np;
  Hook *n, *nstart;
  Hook *contig = NULL;

  if(h->alive != 0) {
    nstart = n = malloc(sizeof(Hook) * h->alive);
  } else {
    nstart = n = NULL;
  }

  for(pp=NULL,hp=h->head;hp;hp=np) {
    np=hp->next;
    if(!contig && (hp->flags & HF_CONTIGUOUS))
      contig = hp;

    if(hp->callback==NULL) {
      if(!(hp->flags & HF_CONTIGUOUS))
        free(hp);
    } else {
      memcpy(n, hp, sizeof(Hook));

      n->flags|=HF_CONTIGUOUS;

      if(pp!=NULL)
        pp->next = n;
      pp = n++;
    }
  }

  if(contig)
    free(contig);

  assert(!(nstart && !pp) && !(!nstart && pp));

  h->head = nstart;
  if(pp)
    pp->next = NULL;

  h->dirty = 0;
  h->count = h->alive;
}
