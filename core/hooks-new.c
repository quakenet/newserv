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
  int dirty;
  Hook *head;
} HookHead;

static HookHead hooks[HOOKMAX];
static int dirtyhooks[HOOKMAX];
static int dirtyhookcount;

unsigned int hookqueuelength = 0;

static void collectgarbage(HookHead *h);
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

  if (!hookqueuelength && hooknum!=HOOK_CORE_ENDOFHOOKSQUEUE) {
    triggerhook(HOOK_CORE_ENDOFHOOKSQUEUE, 0);

    for(i=0;i<dirtyhookcount;i++) {
      collectgarbage(&hooks[dirtyhooks[i]]);
    }
    dirtyhookcount=0;
  }
}

static void markdirty(int hook) {
  if(hooks[hook].dirty)
    return;

  hooks[hook].dirty = 1;
  dirtyhooks[dirtyhookcount++] = hook;
}

static void collectgarbage(HookHead *h) {
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
}
