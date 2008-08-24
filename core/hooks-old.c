/* hooks.c */

#include "hooks.h"
#include "error.h"
#include "../lib/array.h"
#include <assert.h>
#include <stdlib.h>

array hooks[HOOKMAX];
unsigned int hookqueuelength;

void inithooks() {
  int i;
  
  hookqueuelength=0;
  for (i=0;i<HOOKMAX;i++) {
    array_init(&(hooks[i]),sizeof(HookCallback));
    array_setlim1(&(hooks[i]),2);
    array_setlim2(&(hooks[i]),2);
  }
}

int registerhook(int hooknum, HookCallback callback) {
  int i;
  HookCallback *hcbs;
  
  if (hooknum>HOOKMAX)
    return 1;
    
  hcbs=(HookCallback *)(hooks[hooknum].content);
  for(i=0;i<hooks[hooknum].cursi;i++)
    if(hcbs[i]==callback)
      return 1;

  /* If there is a previously blanked slot, go in there */
  for(i=0;i<hooks[hooknum].cursi;i++) {
    if(hcbs[i]==NULL) {
      hcbs[i]=callback;
      return 0;
    }
  }

  i=array_getfreeslot(&hooks[hooknum]);
  hcbs=(HookCallback *)(hooks[hooknum].content);
  hcbs[i]=callback;
  
  return 0;
}

int registerpriorityhook(int hooknum, HookCallback callback, long priority) {
  Error("core", ERR_WARNING, "Priority hook system not loaded.");

  return registerhook(hooknum, callback);
}

int deregisterhook(int hooknum, HookCallback callback) {
  int i;
  HookCallback *hcbs;
  
  if (hooknum>HOOKMAX)
    return 1;
    
  hcbs=(HookCallback *)(hooks[hooknum].content);

  for(i=0;i<hooks[hooknum].cursi;i++)
    if(hcbs[i]==callback) {
      /* If there is an ongoing callback, don't actually delete from the
       * array in case THIS hook is active */
      if (hookqueuelength) {
        hcbs[i]=NULL;
      } else {
        array_delslot(&(hooks[hooknum]),i);
      }
      return 0;
    }

  return 1;
}
  
void triggerhook(int hooknum, void *arg) {
  int i;
  HookCallback *hcbs;
  
  if (hooknum>HOOKMAX)
    return;
    
  hookqueuelength++;
  hcbs=(HookCallback *)(hooks[hooknum].content);
  for (i=0;i<hooks[hooknum].cursi;i++) {
    if (hcbs[i])
      (hcbs[i])(hooknum, arg);
  }
  hookqueuelength--;
  
  if (!hookqueuelength && hooknum!=HOOK_CORE_ENDOFHOOKSQUEUE)
    triggerhook(HOOK_CORE_ENDOFHOOKSQUEUE, 0);
}
