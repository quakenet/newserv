#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "../lib/version.h"
#include "../control/control.h"
#include "../lib/irc_string.h"
#include "../lib/splitline.h"
#include "../lib/strlfunc.h"
#include "../core/nsmalloc.h"
#include "../irc/irc.h"
#include "../localuser/localuser.h"
#include "../localuser/localuserchannel.h"
#include "glines.h"
#include "../trusts/trusts.h"

MODULE_VERSION("");

static void registercommands(int, void *);
static void deregistercommands(int, void *);

static int glines_cmdblock(void *source, int cargc, char **cargv) {
  nick *sender = source;
  nick *target;
  int hits, duration;
  char *reason;
  char creator[32];
  glinebuf gbuf;

  if (cargc < 3)
    return CMD_USAGE;

  target = getnickbynick(cargv[0]);

  if (!target) {
    controlreply(sender, "Sorry, couldn't find that user.");
    return CMD_ERROR;
  }

  duration = durationtolong(cargv[1]);

  if (duration <= 0) {
    controlreply(sender, "Invalid duration specified.");
    return CMD_ERROR;
  }

  if (duration > MAXUSERGLINEDURATION) {
    controlreply(sender, "Sorry, glines may not last longer than %s.", longtoduration(MAXUSERGLINEDURATION, 0));
    return CMD_ERROR;
  }

  reason = cargv[2];

  if (strlen(reason) < MINUSERGLINEREASON) {
    controlreply(sender, "Please specify a proper gline reason.");
    return CMD_ERROR;
  }

  snprintf(creator, sizeof(creator), "#%s", sender->authname);

  glinebufinit(&gbuf, 1);
  glinebufaddbynick(&gbuf, target, 0, creator, reason, getnettime() + duration, getnettime(), getnettime() + duration);
  glinebufcounthits(&gbuf, &hits, NULL);
  glinebufflush(&gbuf, 1);

  controlwall(NO_OPER, NL_GLINES, "%s BLOCK'ed user '%s!%s@%s' for %s with reason '%s' (%d hits)", controlid(sender), target->nick, target->ident, target->host->name->content, longtoduration(duration, 0), reason, hits);

  controlreply(sender, "Done.");

  return CMD_OK;
}

static int glines_cmdgline(void *source, int cargc, char **cargv) {
  nick *sender = source;
  int duration, users, channels;
  char *mask, *reason, *pos;
  char creator[32];
  int coff, sanitychecks, operlimit;
  glinebuf gbuf;
#if SNIRCD_VERSION < 140
  gline *gl;
#endif

  if (cargc < 1)
    return CMD_USAGE;

  coff = 0;
  sanitychecks = 1;
  operlimit = 1;
    
  if (cargv[0][0] == '-') {
    coff = 1;
    
    for (pos = &(cargv[0][1]); *pos; pos++) {
      switch (*pos) {
	case 'S':
	  sanitychecks = 0;
	  break;
	case 'l':
	  operlimit = 0;
	  break;
	default:
	  controlreply(sender, "Invalid flag specified: %c", *pos);
	  return CMD_ERROR;
      }
    }
  }

  if (cargc < 3 + coff)
    return CMD_USAGE;

  mask = cargv[coff];

  duration = durationtolong(cargv[coff + 1]);

  if (duration <= 0) {
    controlreply(sender, "Invalid duration specified.");
    return CMD_ERROR;
  }

  if (duration > MAXUSERGLINEDURATION) {
    controlreply(sender, "Sorry, glines may not last longer than %s.", longtoduration(MAXUSERGLINEDURATION, 0));
    return CMD_ERROR;
  }

  rejoinline(cargv[coff + 2], cargc - coff - 2);
  reason = cargv[coff + 2];

  if (strlen(reason) < MINUSERGLINEREASON) {
    controlreply(sender, "Please specify a proper gline reason.");
    return CMD_ERROR;
  }

#if SNIRCD_VERSION < 140
  gl = findgline(mask);

  if (gl) {
     /* warn opers that they can't modify this gline */
    if (gl->flags & GLINE_ACTIVE) {
      controlreply(sender, "Active G-Line already exists on %s - unable to modify", mask);
      return CMD_ERROR;
    }

    controlreply(sender, "Reactivating existing gline on %s", mask);
  }
#endif

  snprintf(creator, sizeof(creator), "#%s", sender->authname);

  glinebufinit(&gbuf, 1);

  if (!glinebufadd(&gbuf, mask, creator, reason, getnettime() + duration, getnettime(), getnettime() + duration)) {
    controlreply(sender, "Invalid G-Line mask.");
    return CMD_ERROR;
  }

  glinebufcounthits(&gbuf, &users, &channels);

  if (operlimit) {  
    if (channels > MAXUSERGLINECHANNELHITS) {
      glinebufabandon(&gbuf);
      controlreply(sender, "G-Line on '%s' would hit %d channels. Limit is %d. Not setting G-Line.", mask, channels, MAXUSERGLINECHANNELHITS);
      return CMD_ERROR;
    } else if (users > MAXUSERGLINEUSERHITS) {
      glinebufabandon(&gbuf);
      controlreply(sender, "G-Line on '%s' would hit %d users. Limit is %d. Not setting G-Line.", mask, users, MAXUSERGLINEUSERHITS);
      return CMD_ERROR;
    }
  }

  if (sanitychecks && glinebufsanitize(&gbuf) > 0) {
    glinebufabandon(&gbuf);
    controlreply(sender, "G-Line failed sanity checks. Not setting G-Line.");
    return CMD_ERROR;
  }
  
  glinebufflush(&gbuf, 1);

  controlwall(NO_OPER, NL_GLINES, "%s GLINE'd mask '%s' for %s with reason '%s' (hits %d users/%d channels)",
    controlid(sender), mask, longtoduration(duration, 0), reason, users, channels);

  controlreply(sender, "Done.");

  return CMD_OK;
}

static int glines_cmdglinesimulate(void *source, int cargc, char **cargv) {
  nick *sender = source;
  char *mask;
  glinebuf gbuf;
  int users, channels;
  char creator[32];

  if (cargc < 1)
    return CMD_USAGE;

  mask = cargv[0];

  snprintf(creator, sizeof(creator), "#%s", sender->authname);

  glinebufinit(&gbuf, 0);

  if (!glinebufadd(&gbuf, mask, creator, "Simulate", getnettime(), getnettime(), getnettime())) {
    glinebufabandon(&gbuf);
    controlreply(sender, "Invalid G-Line mask.");
    return CMD_ERROR;
  }

  glinebufcounthits(&gbuf, &users, &channels);
  glinebufabandon(&gbuf);

  controlreply(sender, "G-Line on '%s' would hit %d users/%d channels.", mask, users, channels);

  return CMD_OK;
}

static int glines_cmdsmartgline(void *source, int cargc, char **cargv) {
  nick *sender = source;
  char *origmask;
  char mask[512];
  char *p, *user, *host;
  struct irc_in_addr ip;
  unsigned char bits;
  int hits, duration;
  char *reason;
  char creator[32];
  glinebuf gbuf;

  if (cargc < 3)
    return CMD_USAGE;

  origmask = cargv[0];

  if (origmask[0] == '#' || origmask[0] == '&') {
    controlreply(sender, "Please use \"gline\" for badchans and regex glines.");
    return CMD_ERROR;
  }

  duration = durationtolong(cargv[1]);

  if (duration <= 0) {
    controlreply(sender, "Invalid duration specified.");
    return CMD_ERROR;
  }

  if (duration > MAXUSERGLINEDURATION) {
    controlreply(sender, "Sorry, glines may not last longer than %s.", longtoduration(MAXUSERGLINEDURATION, 0));
    return CMD_ERROR;
  }

  reason = cargv[2];

  if (strlen(reason) < MINUSERGLINEREASON) {
    controlreply(sender, "Please specify a proper gline reason.");
    return CMD_ERROR;
  }

  strncpy(mask, origmask, sizeof(mask));

  if (strchr(mask, '!')) {
    controlreply(sender, "Use \"gline\" to place nick glines.");
    return CMD_ERROR;
  }

  p = strchr(mask, '@');

  if (!p) {
    controlreply(sender, "Mask must contain a username (e.g. user@ip).");
    return CMD_ERROR;
  }

  user = mask;
  host = p + 1;
  *p = '\0';

  if (strchr(user, '*') || strchr(user, '?')) {
    controlreply(sender, "Usernames may not contain wildcards.");
    return CMD_ERROR;
  }

  if (!ipmask_parse(host, &ip, &bits)) {
    controlreply(sender, "Invalid CIDR mask.");
    return CMD_ERROR;
  }

  snprintf(creator, sizeof(creator), "#%s", sender->authname);

  glinebufinit(&gbuf, 1);
  glinebufaddbyip(&gbuf, user, &ip, 128, 0, creator, reason, getnettime() + duration, getnettime(), getnettime() + duration);
  glinebufcounthits(&gbuf, &hits, NULL);
  glinebufflush(&gbuf, 1);

  controlwall(NO_OPER, NL_GLINES, "%s GLINE'd mask '%s' for %s with reason '%s' (%d hits)",
    controlid(sender), cargv[0], longtoduration(duration, 0), reason, hits);

  controlreply(sender, "Done.");

  return CMD_OK;
}

static int glines_cmdungline(void *source, int cargc, char **cargv) {
  nick *sender = source;
  gline *gl;

  if (cargc < 1)
    return CMD_USAGE;

  gl = findgline(cargv[0]);

  if (!gl) {
    controlreply(sender, "No such G-Line.");
    return CMD_ERROR;
  }

  if (!(gl->flags & GLINE_ACTIVE)) {
    controlreply(sender, "G-Line was already deactivated.");
    return CMD_ERROR;
  }

  gline_deactivate(gl, 0, 1);

  controlwall(NO_OPER, NL_GLINES, "%s UNGLINE'd mask '%s'", controlid(sender), cargv[0]);

  controlreply(sender, "G-Line deactivated.");

  return CMD_OK;
}

static int glines_cmdclearchan(void *source, int cargc, char **cargv) {
  nick *sender = source;
  channel *cp;
  nick *np;
  char *reason = "Clearing channel.";
  int mode, duration, i, slot, hits;
  array victims;
  char creator[32];
  glinebuf gbuf;

  if (cargc < 2)
    return CMD_USAGE;

  cp = findchannel(cargv[0]);

  if (!cp) {
    controlreply(sender, "Couldn't find that channel.");
    return CMD_ERROR;
  }

  if (strcmp(cargv[1], "kick") == 0)
    mode = 0;
  else if (strcmp(cargv[1], "kill") == 0)
    mode = 1;
  else if (strcmp(cargv[1], "gline") == 0)
    mode = 2;
  else if (strcmp(cargv[1], "glineall") == 0)
    mode = 3;
  else
    return CMD_USAGE;

  if (mode == 0 || mode == 1) {
    if (cargc >= 3) {
      rejoinline(cargv[2], cargc - 2);
      reason = cargv[2];
    }
  } else {
    if (cargc < 3)
      return CMD_USAGE;

    duration = durationtolong(cargv[2]);

    if (duration <= 0) {
      controlreply(sender, "Invalid duration specified.");
      return CMD_ERROR;
    }

    if (duration > MAXUSERGLINEDURATION) {
      controlreply(sender, "Sorry, glines may not last longer than %s.", longtoduration(MAXUSERGLINEDURATION, 0));
      return CMD_ERROR;
    }

    if (cargc >= 4)
      reason = cargv[3];
  }

  array_init(&victims, sizeof(nick *));

  /* we need to make a list of the channel users here because
   * kicking/killing them will affect their position in the channel's
   * user list. */
  for (i = 0; i < cp->users->hashsize; i++) {
    if (cp->users->content[i] == nouser)
      continue;

    np = getnickbynumeric(cp->users->content[i]);

    if (!np)
      continue;

    if (IsService(np) || IsOper(np) || NickOnServiceServer(np))
      continue;

    slot = array_getfreeslot(&victims);
    (((nick **)victims.content)[slot]) = np;
  }

  snprintf(creator, sizeof(creator), "#%s", sender->authname);

  glinebufinit(&gbuf, 1);

  for (i = 0; i < victims.cursi; i++) {
    np = ((nick **)victims.content)[i];

    switch (mode) {
      case 0:
        localkickuser(NULL, cp, np, reason);
        break;
      case 1:
        killuser(NULL, np, "%s", reason);
        break;
      case 2:
        if (IsAccount(np))
          break;
        /* fall through */
      case 3:
        glinebufaddbynick(&gbuf, np, 0, creator, reason, getnettime() + duration, getnettime(), getnettime() + duration);
        break;
      default:
        assert(0);
    }
  }

  glinebufcounthits(&gbuf, &hits, NULL);
  glinebufflush(&gbuf, 1);

  array_free(&victims);

  if (mode == 0 || mode == 1)
    controlwall(NO_OPER, NL_GLINES, "%s CLEARCHAN'd channel '%s' with mode '%s' and reason '%s'",
      controlid(sender), cp->index->name->content, cargv[1], reason);
  else
    controlwall(NO_OPER, NL_GLINES, "%s CLEARCHAN'd channel '%s' with mode '%s', duration %s and reason '%s' (%d hits)",
      controlid(sender), cp->index->name->content, cargv[1], longtoduration(duration, 0), reason, hits);

  controlreply(sender, "Done.");

  return CMD_OK;
}

static int glines_cmdtrustgline(void *source, int cargc, char **cargv) {
  nick *sender = source;
  trustgroup *tg;
  trusthost *th;
  int duration, hits;
  char *reason;
  char mask[512];
  char creator[32];
  glinebuf gbuf;

  if (cargc < 4)
    return CMD_USAGE;

  tg = tg_strtotg(cargv[0]);

  if (!(tg->flags & TRUST_RELIABLE_USERNAME)) {
    controlreply(sender, "Sorry, that trust group does not have the \"reliable username\" flag.");
    return CMD_ERROR;
  }

  duration = durationtolong(cargv[2]);

  if (duration <= 0 || duration > MAXUSERGLINEDURATION) {
    controlreply(sender, "Sorry, glines may not last longer than %s.", longtoduration(MAXUSERGLINEDURATION, 0));
    return CMD_ERROR;
  }

  reason = cargv[3];

  if (strlen(reason) < MINUSERGLINEREASON) {
    controlreply(sender, "Please specify a proper gline reason.");
    return CMD_ERROR;
  }

  snprintf(creator, sizeof(creator), "#%s", sender->authname);

  glinebufinit(&gbuf, 0);

  for(th = tg->hosts; th; th = th->next) {
    snprintf(mask, sizeof(mask), "*!%s@%s", cargv[1], trusts_cidr2str(&th->ip, th->bits));
    glinebufadd(&gbuf, mask, creator, reason, getnettime() + duration, getnettime(), getnettime() + duration);
  }

  glinebufcounthits(&gbuf, &hits, NULL);
  glinebufflush(&gbuf, 1);

  controlwall(NO_OPER, NL_GLINES, "%s TRUSTGLINE'd user '%s' on trust group '%s' for %s with reason '%s' (%d hits)",
    controlid(sender), cargv[1], tg->name->content, longtoduration(duration, 0), reason, hits);

  controlreply(sender, "Done.");

  return CMD_OK;
}

static int glines_cmdtrustungline(void *source, int cargc, char **cargv) {
  nick *sender = source;
  trustgroup *tg;
  trusthost *th;
  char mask[512];
  gline *gl;
  int count;

  if (cargc < 2)
    return CMD_USAGE;

  tg = tg_strtotg(cargv[0]);

  if (!(tg->flags & TRUST_RELIABLE_USERNAME)) {
    controlreply(sender, "Sorry, that trust group does not have the \"reliable username\" flag.");
    return CMD_ERROR;
  }

  count = 0;

  for (th = tg->hosts; th; th = th->next) {
    snprintf(mask, sizeof(mask), "*!%s@%s", cargv[1], trusts_cidr2str(&th->ip, th->bits));

    gl = findgline(mask);

    if (gl && (gl->flags & GLINE_ACTIVE)) {
      gline_deactivate(gl, 0, 1);
      count++;
    }
  }

  controlwall(NO_OPER, NL_GLINES, "%s TRUSTUNGLINE'd user '%s' on trust group '%s' (%d G-Lines deactivated)",
    controlid(sender), cargv[1], tg->name->content, count);

  controlreply(sender, "Done.");

  return CMD_OK;
}

static int glines_cmdglstats(void *source, int cargc, char **cargv) {
  nick *sender = (nick*)source;
  gline *gl, *next;
  time_t curtime = getnettime();
  int glinecount = 0, hostglinecount = 0, ipglinecount = 0, badchancount = 0, rnglinecount = 0;
  int deactivecount = 0, activecount = 0;

  for (gl = glinelist; gl; gl = next) {
    next = gl->next;

    if (gl->lifetime <= curtime) {
      removegline(gl);
      continue;
    } else if (gl->expire <= curtime) {
      gl->flags &= ~GLINE_ACTIVE;
    }

    if (gl->flags & GLINE_ACTIVE) {
      activecount++;
    } else {
      deactivecount++;
    }

    if (gl->flags & GLINE_IPMASK)
      ipglinecount++;
    else if (gl->flags & GLINE_HOSTMASK)
      hostglinecount++;
    else if (gl->flags & GLINE_REALNAME)
      rnglinecount++;
    else if (gl->flags & GLINE_BADCHAN)
      badchancount++;
    glinecount++;
  }

  controlreply(sender, "Total G-Lines set: %d", glinecount);
  controlreply(sender, "Hostmask G-Lines:  %d", hostglinecount);
  controlreply(sender, "IPMask G-Lines:    %d", ipglinecount);
  controlreply(sender, "Channel G-Lines:   %d", badchancount);
  controlreply(sender, "Realname G-Lines:  %d", rnglinecount);

  controlreply(sender, "Active G-Lines:    %d", activecount);
  controlreply(sender, "Inactive G-Lines:  %d", deactivecount);

  /* TODO show top 10 creators here */
  /* TODO show unique creators count */
  /* TODO show glines per create %8.1f", ccount?((float)gcount/(float)ccount):0 */
  return CMD_OK;
}

static int glines_cmdglist(void *source, int cargc, char **cargv) {
  nick *sender = (nick *)source;
  gline *gl, *next;
  time_t curtime = time(NULL);
  int flags = 0;
  char *mask;
  int count = 0;
  int limit = 500;
  char tmp[250];

  if (cargc < 1 || (cargc == 1 && cargv[0][0] == '-')) {
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

  mask = cargv[0];

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

  gline *searchgl = makegline(mask);

  for (gl = glinelist; gl; gl = next) {
    next = gl->next;

    if (gl->lifetime <= curtime) {
      removegline(gl);
      continue;
    } else if (gl->expire <= curtime) {
      gl->flags &= ~GLINE_ACTIVE;
    }

    if (!(gl->flags & GLINE_ACTIVE)) {
      if (!(flags & GLIST_INACTIVE)) {
        continue;
      }
    }

    if (flags & GLIST_REALNAME) {
      if (!(gl->flags & GLINE_REALNAME))
        continue;
      if (flags & GLIST_EXACT) {
        if (!glineequal(searchgl, gl)) {
          continue;
        }
      } else if (flags & GLIST_FIND) {
        if (!gline_match_mask(searchgl, gl)) {
          continue;
        }
      }
    } else {
      if (gl->flags & GLINE_REALNAME)
        continue;

      if (flags & GLIST_REASON) {
        if (flags & GLIST_EXACT) {
          if (!gl->reason || ircd_strcmp(mask, gl->reason->content) != 0)
            continue;
        } else if (flags & GLIST_FIND) {
          if (!gl->reason || match(gl->reason->content, mask))
            continue;
        } else if (!gl->reason || match(mask, gl->reason->content))
            continue;
      } else if (flags & GLIST_OWNER) {
        if (flags & GLIST_EXACT) {
          if (!gl->creator || ircd_strcmp(mask, gl->creator->content) != 0)
            continue;
        } else if (flags & GLIST_FIND) {
          if (!gl->creator || match(gl->creator->content, mask))
            continue;
        } else if (!gl->creator || match(mask, gl->creator->content))
          continue;
      } else {
        if (flags & GLIST_EXACT) {
          if (!glineequal(searchgl, gl)) {
            continue;
          }
        } else if (flags & GLIST_FIND) {
          if (!gline_match_mask(searchgl, gl)) {
            continue;
          }
        }
      }
    }

    if (count == limit && !(flags & GLIST_COUNT))
      controlreply(sender, "More than %d matches, list truncated.", limit);

    count++;

    if (!(flags & GLIST_COUNT) && count < limit) {
      snprintf(tmp, 249, "%s", glinetostring(gl));
      controlreply(sender, "%s%-49s %-19s %-25s %s",
        (gl->flags & GLINE_ACTIVE) ? "+" : "-",
        tmp,
        (gl->flags & GLINE_ACTIVE) ? (char*)longtoduration(gl->expire - curtime, 0) : "<inactive>",
        gl->creator ? gl->creator->content : "",
        gl->reason ? gl->reason->content : "");
    }
  }

  controlreply(sender, "%s%d G-Line%s found.", (flags & GLIST_COUNT) ? "" : "End of list - ", count, count == 1 ? "" : "s");

  return CMD_OK;
}

static int glines_cmdcleanupglines(void *source, int cargc, char **cargv) {
  nick *sender = source;
  gline **pnext, *gl;
  int count;
  
  count = 0;
  
  for (pnext = &glinelist; *pnext; pnext = &((*pnext)->next)) {
    gl = *pnext;
    
    if (!(gl->flags & GLINE_ACTIVE)) {
      gline_destroy(gl, 0, 1);
      count++;
    }
    
    if (!*pnext)
      break;
  }
  
  controlwall(NO_OPER, NL_GLINES, "%s CLEANUPGLINES'd %d G-Lines.",
    controlid(sender), count);
  
  controlreply(sender, "Done.");
  
  return CMD_OK;
}

static int glines_cmdsyncglines(void *source, int cargc, char **cargv) {
  nick *sender = source;
  gline *gl;
  int count;

  count = 0;

  for (gl = glinelist; gl; gl = gl->next) {
    gline_propagate(gl);
    count++;
  }
  
  controlwall(NO_OPER, NL_GLINES, "%s SYNCGLINE'd %d G-Lines.",
    controlid(sender), count);

  controlreply(sender, "Done.");

  return CMD_OK;
}

static int glines_cmdsaveglines(void *source, int cargc, char **cargv) {
  nick *sender = source;
  int count;

  count = glstore_save();

  if (count < 0)
    controlreply(sender, "An error occured while saving G-Lines.");
  else
    controlreply(sender, "Saved %d G-Line%s.", count, (count == 1) ? "" : "s");

  return CMD_OK;
}

static int glines_cmdloadglines(void *source, int cargc, char **cargv) {
  nick *sender = source;
  int count;

  count = glstore_load();

  if (count < 0)
    controlreply(sender, "An error occured while loading the G-Lines file.");
  else
    controlreply(sender, "Loaded %d G-Line%s.", count, (count == 1) ? "" : "s");

  return CMD_OK;
}

static int commandsregistered;

static void registercommands(int hooknum, void *arg) {
  if (commandsregistered)
    return;
  commandsregistered = 1;

  registercontrolhelpcmd("block", NO_OPER, 3, glines_cmdblock, "Usage: block <nick> <duration> <reason>\nSets a gline using an appropriate mask given the user's nickname.");
  registercontrolhelpcmd("gline", NO_OPER, 4, glines_cmdgline, "Usage: gline ?flags? <mask> <duration> <reason>\nSets a gline. Flags can be one or more of:\n-S - bypass sanity checks\n-l - bypass hit limits");
  registercontrolhelpcmd("glinesimulate", NO_OPER, 1, glines_cmdglinesimulate, "Usage: glinesimulate <mask>\nSimulates what happens when a gline is set.");
  registercontrolhelpcmd("smartgline", NO_OPER, 3, glines_cmdsmartgline, "Usage: smartgline <user@host> <duration> <reason>\nSets a gline. Automatically adjusts the mask depending on whether the specified mask is trusted.");
  registercontrolhelpcmd("ungline", NO_OPER, 1, glines_cmdungline, "Usage: ungline <mask>\nDeactivates a gline.");
  registercontrolhelpcmd("clearchan", NO_OPER, 4, glines_cmdclearchan, "Usage: clearchan <#channel> <how> <duration> ?reason?\nClears a channel.\nhow can be one of:\nkick - Kicks users.\nkill - Kills users.\ngline - Glines non-authed users (using an appropriate mask).\nglineall - Glines users.\nDuration is only valid when glining users. Reason defaults to \"Clearing channel.\".");
  registercontrolhelpcmd("trustgline", NO_OPER, 4, glines_cmdtrustgline, "Usage: trustgline <#id|name> <user> <duration> <reason>\nSets a gline on the specified username for each host in the specified trust group. The username may contain wildcards.");
  registercontrolhelpcmd("trustungline", NO_OPER, 2, glines_cmdtrustungline, "Usage: trustungline <#id|name> <user>\nRemoves a gline that was previously set with trustgline.");
  registercontrolhelpcmd("glstats", NO_OPER, 0, glines_cmdglstats, "Usage: glstat\nShows statistics about G-Lines.");
  registercontrolhelpcmd("glist", NO_OPER, 2, glines_cmdglist, "Usage: glist [-flags] <mask>\nLists matching G-Lines.\nValid flags are:\n-c: Count G-Lines.\n-f: Find G-Lines active on <mask>.\n-x: Find G-Lines matching <mask> exactly.\n-R: Find G-lines on realnames.\n-o: Search for glines by owner.\n-r: Search for glines by reason.\n-i: Include inactive glines.");
  registercontrolhelpcmd("cleanupglines", NO_OPER, 0, glines_cmdcleanupglines, "Usage: cleanupglines\nDestroys all deactivated G-Lines.");
  registercontrolhelpcmd("syncglines", NO_DEVELOPER, 0, glines_cmdsyncglines, "Usage: syncglines\nSends all G-Lines to all other servers.");
  registercontrolhelpcmd("loadglines", NO_DEVELOPER, 0, glines_cmdloadglines, "Usage: loadglines\nForce load of glines.");
  registercontrolhelpcmd("saveglines", NO_DEVELOPER, 0, glines_cmdsaveglines, "Usage: saveglines\nForce save of glines.");
}

static void deregistercommands(int hooknum, void *arg) {
  if (!commandsregistered)
    return;
  commandsregistered = 0;

  deregistercontrolcmd("block", glines_cmdblock);
  deregistercontrolcmd("gline", glines_cmdgline);
  deregistercontrolcmd("glinesimulate", glines_cmdglinesimulate);
  deregistercontrolcmd("smartgline", glines_cmdsmartgline);
  deregistercontrolcmd("ungline", glines_cmdungline);
  deregistercontrolcmd("clearchan", glines_cmdclearchan);
  deregistercontrolcmd("trustgline", glines_cmdtrustgline);
  deregistercontrolcmd("trustungline", glines_cmdtrustungline);
  deregistercontrolcmd("glstats", glines_cmdglstats);
  deregistercontrolcmd("glist", glines_cmdglist);
  deregistercontrolcmd("cleanupglines", glines_cmdcleanupglines);
  deregistercontrolcmd("syncglines", glines_cmdsyncglines);
  deregistercontrolcmd("loadglines", glines_cmdloadglines);
  deregistercontrolcmd("saveglines", glines_cmdsaveglines);
}

void _init(void) {
  registerhook(HOOK_TRUSTS_DB_LOADED, registercommands);
  registerhook(HOOK_TRUSTS_DB_CLOSED, deregistercommands);

  if (trustsdbloaded)
    registercommands(0, NULL);
}

void _fini(void) {
  deregisterhook(HOOK_TRUSTS_DB_LOADED, registercommands);
  deregisterhook(HOOK_TRUSTS_DB_CLOSED, deregistercommands);

  deregistercommands(0, NULL);
}
