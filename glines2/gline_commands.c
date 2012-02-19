#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "../control/control.h"
#include "../nick/nick.h"
#include "../localuser/localuserchannel.h"
#include "../core/hooks.h"
#include "../server/server.h"
#include "../parser/parser.h"
#include "../core/schedule.h"
#include "../lib/array.h"
#include "../lib/base64.h"
#include "../lib/irc_string.h"
#include "../lib/splitline.h"

#include "gline.h"

void _init() {
  registercontrolhelpcmd("glstats",NO_OPER,0,&gline_glstats,"Usage: glstats.");
}

void _fini() {
  deregistercontrolcmd("glstats", gline_glstats);
}

int gline_glstats(void* source, int cargc, char** cargv) {
  nick* sender = (nick*)source;
  gline* g;
  gline* sg;
  time_t curtime = time(0);
  int glinecount = 0, hostglinecount = 0, ipglinecount = 0, badchancount = 0, rnglinecount = 0;
  int deactivecount =0, activecount = 0;

  for (g = glinelist; g; g = sg) {
    sg = g->next;

    if (g->lifetime <= curtime)
      continue;
 
    if (g->flags & GLINE_ACTIVE) {
      activecount++;
    } else {
      deactivecount++;
    }

    if(g->flags & GLINE_IPMASK)
      ipglinecount++;
    else if (g->flags & (GLINE_HOSTMASK | GLINE_HOSTEXACT))
      hostglinecount++;
    else if (g->flags & GLINE_REALNAME)
      rnglinecount++;
    else if (g->flags & GLINE_BADCHAN)
      badchancount++;
    glinecount++;
  }

  controlreply(sender, "Total G-Lines set: %d", glinecount);
  controlreply(sender, "Hostmask G-Lines:  %d", hostglinecount);
  controlreply(sender, "IPMask G-Lines:    %d", ipglinecount);
  controlreply(sender, "Channel G-Lines:   %d", badchancount);
  controlreply(sender, "Realname G-Lines:  %d", rnglinecount);

  controlreply(sender, "Active G-Lines:    %d", activecount);
  controlreply(sender, "De-Active G-Lines: %d", deactivecount);

  /* TODO show top 10 creators here */
  /* TODO show unique creators count */
  /* TODO show glines per create %8.1f", ccount?((float)gcount/(float)ccount):0 */
  return CMD_OK;
}

