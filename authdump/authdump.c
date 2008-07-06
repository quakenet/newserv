#include <stdio.h>

#include "../core/schedule.h"
#include "../authext/authext.h"
#include "../nick/nick.h"
#include "../lib/version.h"

MODULE_VERSION("");

static void *authdumpsched;

static void doauthdump(void *arg) {
  authname *a;
  nick *np;
  int i;
  FILE *fp = fopen("authdump/authdump.txt.1", "w");

  if(!fp)
    return;

  for(i=0;i<AUTHNAMEHASHSIZE;i++) {
    for(a=authnametable[i];a;a=a->next) {
      np = a->nicks;
      if(!np)
        continue;

      /* grossly inefficient */
      fprintf(fp, "%s %lu", np->authname, np->auth->userid);
      for(;np;np=np->nextbyauthname)
        fprintf(fp, " %s", np->nick);

      fprintf(fp, "\n");
    }
  }

  fclose(fp);

  rename("authdump/authdump.txt.1", "authdump/authdump.txt");
} 

void _init() {
  authdumpsched = (void *)schedulerecurring(time(NULL), 0, 300, &doauthdump, NULL);
}

void _fini() {
  deleteschedule(authdumpsched, &doauthdump, NULL);
}
