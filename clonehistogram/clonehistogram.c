#include <string.h>

#include "../control/control.h"
#include "../lib/irc_string.h"
#include "../localuser/localuserchannel.h"

int ch_clonehistogram(void *source, int cargc, char **cargv);

#define MAX_CLONES 5

void _init() {
  registercontrolhelpcmd("clonehistogram", NO_OPER, 1, ch_clonehistogram, "Usage: clonehistogram <hostmask>\nShows the distribution of user clone counts of a given mask.");
}

void _fini () {
  deregistercontrolcmd("clonehistogram", ch_clonehistogram);
}

void histoutput(nick *np, int clonecount, int amount, int total) {
  char max[51];
  float percentage = ((float)amount / (float)total) * 100;

  if(percentage > 1)
    memset(max, '#', (int)(percentage / 2));
  max[(int)(percentage / 2)] = '\0';

  controlreply(np, "%s%d %06.2f%% %s", (clonecount<1)?">":"=", abs(clonecount), percentage, max);
}

int ch_clonehistogram(void *source, int cargc, char **cargv) {
  nick *np = (nick *)source;
  int count[MAX_CLONES + 1], j, total = 0, totalusers = 0;
  host *hp;
  char *pattern;

  if(cargc < 1)
    return CMD_USAGE;

  pattern = cargv[0];

  memset(count, 0, sizeof(count));

  for (j=0;j<HOSTHASHSIZE;j++)
    for(hp=hosttable[j];hp;hp=hp->next)
      if (match2strings(pattern, hp->name->content)) {
        total++;
        totalusers+=hp->clonecount;

        if(hp->clonecount && (hp->clonecount > MAX_CLONES)) {
          count[0]++;
        } else {
          count[hp->clonecount]++;
        }
      }

  if(total == 0) {
    controlreply(np, "No hosts matched.");
  } else {
    for(j=1;j<=MAX_CLONES;j++)
      histoutput(np, j, count[j], total);

    histoutput(np, -MAX_CLONES, count[0], total);

    controlreply(np, "%d hosts/%d users matched.", total, totalusers);
  }

  controlreply(np, "Done.");
  return CMD_OK;
}

