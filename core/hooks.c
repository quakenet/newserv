/* hooks.c */

#include "hooks.h"
#include <assert.h>
#include "../lib/array.h"

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

  i=array_getfreeslot(&hooks[hooknum]);
  hcbs=(HookCallback *)(hooks[hooknum].content);
  hcbs[i]=callback;
  
  return 0;
}

int deregisterhook(int hooknum, HookCallback callback) {
  int i;
  HookCallback *hcbs;
  
  if (hooknum>HOOKMAX)
    return 1;
    
  hcbs=(HookCallback *)(hooks[hooknum].content);

  for(i=0;i<hooks[hooknum].cursi;i++)
    if(hcbs[i]==callback) {
      array_delslot(&(hooks[hooknum]),i);
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
    (hcbs[i])(hooknum, arg);
  }
  hookqueuelength--;
  
  if (!hookqueuelength && hooknum!=HOOK_CORE_ENDOFHOOKSQUEUE)
    triggerhook(HOOK_CORE_ENDOFHOOKSQUEUE, 0);
}
  
