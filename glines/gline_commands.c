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

#include "glines.h"
#include "../lib/version.h"

MODULE_VERSION("")

int gline_glist(void* source, int cargc, char** cargv);
int gline_glstats(void* source, int cargc, char** cargv);
int gline_ungline(void* source, int cargc, char** cargv);
int gline_saveglines(void* source, int cargc, char** cargv);

void _init() {
  registercontrolhelpcmd("glstats",NO_OPER,0,&gline_glstats,"Usage: glstats.");
  registercontrolhelpcmd("glist",NO_OPER,2,&gline_glist,"Usage: glist.");
  registercontrolhelpcmd("ungline",NO_OPER,1,&gline_ungline,"Usage: ungline.");
  
  registercontrolhelpcmd("saveglines",NO_OPER,0,&gline_saveglines,"Usage: saveglines");

}

void _fini() {
  deregistercontrolcmd("glstats", gline_glstats);
  deregistercontrolcmd("glist", gline_glist);
  deregistercontrolcmd("ungline", gline_ungline);

  deregistercontrolcmd("saveglines", gline_saveglines);
}

int gline_glstats(void* source, int cargc, char** cargv) {
  nick* sender = (nick*)source;
  gline* g;
  gline* sg;
  time_t curtime = getnettime();
  int glinecount = 0, hostglinecount = 0, ipglinecount = 0, badchancount = 0, rnglinecount = 0;
  int deactivecount =0, activecount = 0;

  for (g = glinelist; g; g = sg) {
    sg = g->next;

    if (g->lifetime <= curtime) {
      removegline(g);
      continue;
    } else if (g->expires <= curtime) {
      g->flags &= ~GLINE_ACTIVE; 
    }
 
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

int gline_glist(void* source, int cargc, char** cargv) {
  nick* sender = (nick*)source;
  gline* g;
  gline* sg;
  time_t curtime = time(0);
  int flags = 0;
  char* mask = cargv[0];
  int count = 0;
  int limit = 500;
  char tmp[250];

  if ((cargc < 1) || ((cargc == 1) && (cargv[0][0] == '-'))) {
    controlreply(sender, "Syntax: glist [-flags] <mask>");
    controlreply(sender, "Valid flags are:");
    controlreply(sender, "-c: Count G-Lines.");
    controlreply(sender, "-f: Find G-Lines active on <mask>.");
    controlreply(sender, "-x: Find G-Lines matching <mask> exactly.");
    controlreply(sender, "-R: Find G-lines on realnames.");
    controlreply(sender, "-o: Search for glines by owner.");
    controlreply(sender, "-r: Search for glines by reason.");  
    controlreply(sender, "-i: Include inactive glines.");
    return CMD_ERROR;
  }

  if (cargc > 1) {
    char* ch = cargv[0];

    for (; *ch; ch++)
      switch (*ch) {
      case '-':
        break;

      case 'c':
        flags |= GLIST_COUNT;
        break;

      case 'f':
        flags |= GLIST_FIND;
        break;

      case 'x':
        flags |= GLIST_EXACT;
        break;

      case 'r':
        flags |= GLIST_REASON;
        break;
      case 'o':
        flags |= GLIST_OWNER;
        break;

      case 'R':
        flags |= GLIST_REALNAME;
        break;

      case 'i':
        flags |= GLIST_INACTIVE;
        break;

      default:
        controlreply(sender, "Invalid flag '%c'.", *ch);
        return CMD_ERROR;
      }

      mask = cargv[1];
  } else {
    mask = cargv[0];
  }

  if ((flags & (GLIST_EXACT|GLIST_FIND)) == (GLIST_EXACT|GLIST_FIND)) {
    controlreply(sender, "You cannot use -x and -f flags together.");
    return CMD_ERROR;
  }

  if (!(flags & GLIST_COUNT))
    controlreply(sender, "%-50s %-19s %-25s %s", "Mask:", "Expires in:", "Creator:", "Reason:");

  gline* searchgl = makegline(mask); 

  for (g = glinelist; g; g = sg) {
    sg = g->next;

    if (g->lifetime <= curtime) {
      removegline(g);
      continue;
    } else if (g->expires <= curtime) {
      g->flags &= ~GLINE_ACTIVE;
    }

    if (!(g->flags & GLINE_ACTIVE)) {
      if(!(flags & GLIST_INACTIVE)) {
        continue;
      }
    }
    
    if (flags & GLIST_REALNAME) {
      if(!(g->flags & GLINE_REALNAME))
        continue;
      if (flags & GLIST_EXACT) {
        if (!glineequal( searchgl, g )) {
          continue;
        }
      } else if (flags & GLIST_FIND) {
        if (!gline_match_mask( searchgl, g )) {
          continue;
        }
      }
    } else {
      if (g->flags & GLINE_REALNAME)
        continue;

      if (flags & GLIST_REASON) {
        if (flags & GLIST_EXACT) {
          if (g->reason && ircd_strcmp(mask, g->reason->content))
            continue;
        } else if ((flags & GLIST_FIND)) {    
          if (g->reason && match(g->reason->content, mask))
            continue;
        } else if (g->reason && match(mask, g->reason->content))
            continue;
      } else if (flags & GLIST_OWNER) {
        if (flags & GLIST_EXACT) {
          if ( g->creator && ircd_strcmp(mask, g->creator->content))
            continue;
        } else if ((flags & GLIST_FIND)) {
          if (  g->creator && match( g->creator->content, mask))
            continue;
        } else if ( g->creator && match(mask, g->creator->content))
          continue;
      } else { 
        if (flags & GLIST_EXACT) {
          if (!glineequal( searchgl, g )) {
            continue; 
          }
        } else if (flags & GLIST_FIND) {
          if (!gline_match_mask( searchgl, g )) {
            continue;
          }
        }
      }
    }

    if (count == limit && !(flags & GLIST_COUNT))
      controlreply(sender, "More than %d matches, list truncated.", limit);

    count++;

    if (!(flags & GLIST_COUNT) && (count < limit)) {
      snprintf(tmp, 249, "%s", glinetostring(g)); 
      controlreply(sender, "%s%-50s %-19s %-25s %s", g->flags & GLINE_ACTIVE ? "+" : "-", tmp, g->flags & GLINE_ACTIVE ? (char*)longtoduration(g->expires - curtime, 0) : "<inactive>",
            g->creator ? g->creator->content : "", g->reason ? g->reason->content : "");
    }
  }

  controlreply(sender, "%s%d G-Line%s found.", (flags & GLIST_COUNT) ? "" : "End of list - ", count, count == 1 ? "" : "s");

  return CMD_OK;
}

int gline_ungline(void* source, int cargc, char** cargv) {
  nick* sender = (nick*)source;
  gline* g;

  if (cargc < 1) {
    controlreply(sender, "Syntax: ungline <mask>");
    controlreply(sender, "Where <mask> is the exact G-Line to remove. Wildcard removes are not supported.");
    return CMD_ERROR;
  }

  if (!(g = gline_find(cargv[0]))) {
    controlreply(sender, "No such G-Line.");
    return CMD_ERROR;
  }

  gline_deactivate(g, 0, 1);

  controlreply(sender, "G-Line deactivated.");

  return CMD_OK;
}

int gline_saveglines(void* source, int cargc, char** cargv) {
  nick* sender = (nick*)source;
  FILE* fp;
  gline* g;
  gline* sg;
  char buf[512];
  int gc = 0;
  time_t curtime = time(0);

  snprintf(buf, 511, "logs/glines.%ld", curtime);
  if (!(fp = fopen(buf, "w"))) {
    controlreply(sender, "Failed to open file.");
    return CMD_ERROR;
  }

  for (g = glinelist; g; g = sg) {
    sg = g->next;

    if (g->lifetime <= curtime) {
      removegline(g);
      continue;
    } else if (g->expires <= curtime) {
      g->flags &= ~GLINE_ACTIVE;
    }

    fprintf(fp, "%s\n", glinetostring(g));

    gc++;
  }

  fclose(fp);

  controlreply(sender, "Saved %d G-Line%s to %s.", gc, gc == 1 ? "" : "s", buf);

  return CMD_OK;
}

