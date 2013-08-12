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

static int parse_gline_flags(nick *sender, const char *flagparam, int *overridesanity, int *overridelimit, int *simulate, int *chase, int *coff) {
  const char *pos;

  *coff = 0;
  *overridesanity = 0;
  *overridelimit = 0;
  *simulate = 0;

  if (chase)
    *chase = 0;

  if (flagparam[0] == '-') {
    *coff = 1;

    for (pos = flagparam + 1; *pos; pos++) {
      switch (*pos) {
	case 'f':
          *overridesanity = 1;
	  break;
	case 'l':
          *overridelimit = 1;
	  break;
	case 'S':
	  *simulate = 1;
	  break;
        case 'c':
          if (!chase)
            goto invalid;

          *chase = 1;
          break;
	default:
          goto invalid;
      }
    }
  }

  return 1;

invalid:
  controlreply(sender, "Invalid flag specified: %c", *pos);
  return 0;
}

static int glines_cmdblock(void *source, int cargc, char **cargv) {
  nick *sender = source;
  nick *target, *wnp;
  whowas *ww;
  int hits, duration, id;
  int coff, overridesanity, overridelimit, simulate, chase;
  char *reason;
  char creator[128];
  glinebuf gbuf;
  int ownww;

  if (cargc < 1)
    return CMD_USAGE;

  if (!parse_gline_flags(sender, cargv[0], &overridesanity, &overridelimit, &simulate, &chase, &coff))
    return CMD_ERROR;

  if (cargc < 3 + coff)
    return CMD_USAGE;

  duration = durationtolong(cargv[coff + 1]);

  if (duration <= 0) {
    controlreply(sender, "Invalid duration specified.");
    return CMD_ERROR;
  }

  target = getnickbynick(cargv[coff]);

  if (!target) {
    ww = whowas_chase(cargv[coff], 1800);
   
    if (!ww) {
      controlreply(sender, "Sorry, couldn't find that user.");
      return CMD_ERROR;
    }
   
    ownww = 0;

    controlreply(sender, "Found matching whowas record:");
    controlreply(sender, "%s", whowas_format(ww));
  } else {
    ww = whowas_fromnick(target, 1);
    ownww = 1;
  }

  wnp = &ww->nick;

  if (sender != target && (IsService(wnp) || IsOper(wnp) || NickOnServiceServer(wnp))) {
    controlreply(sender, "Target user '%s' is an oper or a service. Not setting G-Lines.", wnp->nick);
    return CMD_ERROR;
  }

  rejoinline(cargv[coff + 2], cargc - coff - 2);
  reason = cargv[coff + 2];

  snprintf(creator, sizeof(creator), "#%s", sender->authname);

  glinebufinit(&gbuf, 0);
  glinebufcommentv(&gbuf, "BLOCK", cargc + coff - 1, cargv);
  glinebufaddbywhowas(&gbuf, ww, 0, creator, reason, getnettime() + duration, getnettime(), getnettime() + duration);

  glinebufspew(&gbuf, sender);

  if (!glinebufchecksane(&gbuf, sender, overridesanity, overridelimit)) {
    glinebufabort(&gbuf);
    if (ownww)
      whowas_free(ww);
    controlreply(sender, "G-Lines failed sanity checks. Not setting G-Lines.");
    return CMD_ERROR;
  }

  if (simulate) {
    glinebufabort(&gbuf);
    if (ownww)
      whowas_free(ww);
    controlreply(sender, "Simulation complete. Not setting G-Lines.");
    return CMD_ERROR;
  }

  glinebufcounthits(&gbuf, &hits, NULL);
  id = glinebufcommit(&gbuf, 1);

  controlwall(NO_OPER, NL_GLINES, "%s BLOCK'ed user '%s!%s@%s' for %s with reason '%s' (%d hits)", controlid(sender),
              wnp->nick, wnp->ident, wnp->host->name->content,
              longtoduration(duration, 0), reason, hits);

  if (ownww)
    whowas_free(ww);

  controlreply(sender, "Done. G-Line transaction ID: %d", id);

  return CMD_OK;
}

static int glines_cmdgline(void *source, int cargc, char **cargv) {
  nick *sender = source;
  int duration, users, channels, id;
  char *mask, *reason;
  char creator[128];
  int coff, overridesanity, overridelimit, simulate;
  glinebuf gbuf;
#if SNIRCD_VERSION < 140
  gline *gl;
#endif /* SNIRCD_VERSION */

  if (cargc < 1)
    return CMD_USAGE;

  if (!parse_gline_flags(sender, cargv[0], &overridesanity, &overridelimit, &simulate, NULL, &coff))
    return CMD_ERROR;

  if (cargc < 3 + coff)
    return CMD_USAGE;

  mask = cargv[coff];

  duration = durationtolong(cargv[coff + 1]);

  if (duration <= 0) {
    controlreply(sender, "Invalid duration specified.");
    return CMD_ERROR;
  }

  rejoinline(cargv[coff + 2], cargc - coff - 2);
  reason = cargv[coff + 2];

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
#endif /* SNIRCD_VERSION */

  snprintf(creator, sizeof(creator), "#%s", sender->authname);

  glinebufinit(&gbuf, 0);
  glinebufcommentv(&gbuf, "GLINE", cargc + coff - 1, cargv);

  if (!glinebufadd(&gbuf, mask, creator, reason, getnettime() + duration, getnettime(), getnettime() + duration)) {
    controlreply(sender, "Invalid G-Line mask.");
    return CMD_ERROR;
  }

  glinebufspew(&gbuf, sender);

  if (!glinebufchecksane(&gbuf, sender, overridesanity, overridelimit)) {
    glinebufabort(&gbuf);
    controlreply(sender, "G-Lines failed sanity checks. Not setting G-Lines.");
    return CMD_ERROR;
  }

  if (simulate) {
    glinebufabort(&gbuf);
    controlreply(sender, "Simulation complete. Not setting G-Lines.");
    return CMD_ERROR;
  }

  glinebufcounthits(&gbuf, &users, &channels);
  id = glinebufcommit(&gbuf, 1);

  controlwall(NO_OPER, NL_GLINES, "%s GLINE'd mask '%s' for %s with reason '%s' (hits %d users/%d channels)",
    controlid(sender), mask, longtoduration(duration, 0), reason, users, channels);

  controlreply(sender, "Done. G-Line transaction ID: %d", id);

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
  int coff, overridesanity, overridelimit, simulate, id;
  char *reason;
  char creator[128];
  glinebuf gbuf;

  if (cargc < 1)
    return CMD_USAGE;

  if (!parse_gline_flags(sender, cargv[0], &overridesanity, &overridelimit, &simulate, NULL, &coff))
    return CMD_ERROR;

  if (cargc < 3 + coff)
    return CMD_USAGE;

  origmask = cargv[coff];

  if (origmask[0] == '#' || origmask[0] == '&' || origmask[0] == '$') {
    controlreply(sender, "Please use \"gline\" for badchan or realname glines.");
    return CMD_ERROR;
  }

  duration = durationtolong(cargv[coff + 1]);

  if (duration <= 0) {
    controlreply(sender, "Invalid duration specified.");
    return CMD_ERROR;
  }

  rejoinline(cargv[coff + 2], cargc - coff - 2);
  reason = cargv[coff + 2];

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

  glinebufinit(&gbuf, 0);
  glinebufcommentv(&gbuf, "SMARTGLINE", cargc + coff - 1, cargv);
  glinebufaddbyip(&gbuf, user, &ip, 128, 0, creator, reason, getnettime() + duration, getnettime(), getnettime() + duration);

  glinebufspew(&gbuf, sender);

  if (!glinebufchecksane(&gbuf, sender, overridesanity, overridelimit)) {
    glinebufabort(&gbuf);
    controlreply(sender, "G-Lines failed sanity checks. Not setting G-Lines.");
    return CMD_ERROR;
  }

  if (simulate) {
    glinebufabort(&gbuf);
    controlreply(sender, "Simulation complete. Not setting G-Lines.");
    return CMD_ERROR;
  }

  glinebufcounthits(&gbuf, &hits, NULL);
  id = glinebufcommit(&gbuf, 1);

  controlwall(NO_OPER, NL_GLINES, "%s SMARTGLINE'd mask '%s' for %s with reason '%s' (%d hits)",
    controlid(sender), cargv[0], longtoduration(duration, 0), reason, hits);

  controlreply(sender, "Done. G-Line transaction ID: %d", id);

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

static int glines_cmddestroygline(void *source, int cargc, char **cargv) {
  nick *sender = source;
  gline *gl;

  if (cargc < 1)
    return CMD_USAGE;

  gl = findgline(cargv[0]);

  if (!gl) {
    controlreply(sender, "No such G-Line.");
    return CMD_ERROR;
  }

  gline_destroy(gl, 0, 1);

  controlwall(NO_OPER, NL_GLINES, "%s DESTROYGLINE'd mask '%s'", controlid(sender), cargv[0]);

  controlreply(sender, "G-Line destroyed.");

  return CMD_OK;
}

static int glines_cmdclearchan(void *source, int cargc, char **cargv) {
  nick *sender = source;
  channel *cp;
  nick *np;
  char *reason = "Clearing channel.";
  int mode, duration, i, slot, hits, id;
  int coff, overridesanity, overridelimit, simulate;
  array victims;
  char creator[128];
  glinebuf gbuf;

  if (cargc < 1)
    return CMD_USAGE;

  if (!parse_gline_flags(sender, cargv[0], &overridesanity, &overridelimit, &simulate, NULL, &coff))
    return CMD_ERROR;

  if (cargc < 2 + coff)
    return CMD_USAGE;

  cp = findchannel(cargv[coff]);

  if (!cp) {
    controlreply(sender, "Couldn't find that channel.");
    return CMD_ERROR;
  }

  if (strcmp(cargv[coff + 1], "kick") == 0)
    mode = 0;
  else if (strcmp(cargv[coff + 1], "kill") == 0)
    mode = 1;
  else if (strcmp(cargv[coff + 1], "gline") == 0)
    mode = 2;
  else if (strcmp(cargv[coff + 1], "glineall") == 0)
    mode = 3;
  else
    return CMD_USAGE;

  if (mode == 0 || mode == 1) {
    if (cargc >= 3) {
      rejoinline(cargv[coff + 2], cargc - coff - 2);
      reason = cargv[coff + 2];
    }
  } else {
    if (cargc < 3 + coff)
      return CMD_USAGE;

    duration = durationtolong(cargv[coff + 2]);

    if (duration <= 0) {
      controlreply(sender, "Invalid duration specified.");
      return CMD_ERROR;
    }

    if (cargc >= 4 + coff) {
      rejoinline(cargv[coff + 3], cargc - coff - 3);
      reason = cargv[coff + 3];
    }
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

  glinebufinit(&gbuf, 0);
  glinebufcommentv(&gbuf, "CLEARCHAN", cargc + coff - 1, cargv);

  for (i = 0; i < victims.cursi; i++) {
    np = ((nick **)victims.content)[i];

    switch (mode) {
      case 0:
	if (simulate)
	  controlreply(sender, "user: %s!%s@%s r(%s)", np->nick, np->ident, np->host->name->content, np->realname->name->content);
	else
	  localkickuser(NULL, cp, np, reason);

        break;
      case 1:
	if (simulate)
	  controlreply(sender, "user: %s!%s@%s r(%s)", np->nick, np->ident, np->host->name->content, np->realname->name->content);
	else
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

  if (mode != 0 && mode != 1) {
    glinebufspew(&gbuf, sender);

    if (!glinebufchecksane(&gbuf, sender, overridesanity, overridelimit)) {
      glinebufabort(&gbuf);
      controlreply(sender, "G-Line failed sanity checks. Not setting G-Line.");
      return CMD_ERROR;
    }
  }

  if (simulate) {
    glinebufabort(&gbuf);
    controlreply(sender, "Simulation complete. Not clearing channel.");
    return CMD_ERROR;
  }

  glinebufmerge(&gbuf);
  glinebufcounthits(&gbuf, &hits, NULL);
  id = glinebufcommit(&gbuf, 1);

  array_free(&victims);

  if (mode == 0 || mode == 1) {
    controlwall(NO_OPER, NL_GLINES, "%s CLEARCHAN'd channel '%s' with mode '%s' and reason '%s'",
      controlid(sender), cp->index->name->content, cargv[1], reason);
    controlreply(sender, "Done.");
  } else {
    controlwall(NO_OPER, NL_GLINES, "%s CLEARCHAN'd channel '%s' with mode '%s', duration %s and reason '%s' (%d hits)",
      controlid(sender), cp->index->name->content, cargv[1], longtoduration(duration, 0), reason, hits);
    controlreply(sender, "Done. G-Line transaction ID: %d", id);
  }

  return CMD_OK;
}

static int glines_cmdtrustgline(void *source, int cargc, char **cargv) {
  nick *sender = source;
  trustgroup *tg;
  trusthost *th;
  int duration, hits;
  int coff, overridesanity, overridelimit, simulate, id;
  char *reason;
  char mask[512];
  char creator[128];
  glinebuf gbuf;

  if (cargc < 1)
    return CMD_USAGE;

  if (!parse_gline_flags(sender, cargv[0], &overridesanity, &overridelimit, &simulate, NULL, &coff))
    return CMD_ERROR;

  if (cargc < 4 + coff)
    return CMD_USAGE;

  tg = tg_strtotg(cargv[coff]);

  if (!(tg->flags & TRUST_RELIABLE_USERNAME)) {
    controlreply(sender, "Sorry, that trust group does not have the \"reliable username\" flag.");
    return CMD_ERROR;
  }

  duration = durationtolong(cargv[coff + 2]);

  if (duration <= 0) {
    controlreply(sender, "Invalid duration specified.");
    return CMD_ERROR;
  }

  rejoinline(cargv[coff + 3], cargc - coff - 3);
  reason = cargv[coff + 3];

  snprintf(creator, sizeof(creator), "#%s", sender->authname);

  glinebufinit(&gbuf, 0);
  glinebufcommentv(&gbuf, "TRUSTGLINE", cargc + coff - 1, cargv);

  for(th = tg->hosts; th; th = th->next) {
    snprintf(mask, sizeof(mask), "*!%s@%s", cargv[1], CIDRtostr(th->ip, th->bits));
    glinebufadd(&gbuf, mask, creator, reason, getnettime() + duration, getnettime(), getnettime() + duration);
  }

  glinebufspew(&gbuf, sender);

  if (!glinebufchecksane(&gbuf, sender, overridesanity, overridelimit)) {
    glinebufabort(&gbuf);
    controlreply(sender, "G-Line failed sanity checks. Not setting G-Line.");
    return CMD_ERROR;
  }

  if (simulate) {
    glinebufabort(&gbuf);
    controlreply(sender, "Simulation complete. Not setting G-Lines.");
    return CMD_ERROR;
  }

  glinebufcounthits(&gbuf, &hits, NULL);
  id = glinebufcommit(&gbuf, 1);

  controlwall(NO_OPER, NL_GLINES, "%s TRUSTGLINE'd user '%s' on trust group '%s' for %s with reason '%s' (%d hits)",
    controlid(sender), cargv[1], tg->name->content, longtoduration(duration, 0), reason, hits);

  controlreply(sender, "Done. G-Line transaction ID: %d", id);

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
    snprintf(mask, sizeof(mask), "*!%s@%s", cargv[1], CIDRtostr(th->ip, th->bits));

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
  char expirestr[250], idstr[250];

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
    controlreply(sender, "%-50s %-19s %-15s %-25s %s", "Mask:", "Expires in:", "Transaction ID:", "Creator:", "Reason:");

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
        if (!glineequal(searchgl, gl))
          continue;
      } else if (flags & GLIST_FIND) {
        if (!gline_match_mask(gl, searchgl))
          continue;
      } else {
        if (!match2strings(mask, glinetostring(gl)))
          continue;
      }
    } else {
      if (gl->flags & GLINE_REALNAME)
        continue;

      if (flags & GLIST_REASON) {
        if (flags & GLIST_EXACT) {
          if (!gl->reason || ircd_strcmp(mask, gl->reason->content) != 0)
            continue;
        } else if (flags & GLIST_FIND) {
          if (!gl->reason || !match2strings(gl->reason->content, mask))
            continue;
        } else if (!gl->reason || !match2strings(mask, gl->reason->content))
            continue;
      } else if (flags & GLIST_OWNER) {
        if (flags & GLIST_EXACT) {
          if (!gl->creator || ircd_strcmp(mask, gl->creator->content) != 0)
            continue;
        } else if (flags & GLIST_FIND) {
          if (!gl->creator || !match2strings(gl->creator->content, mask))
            continue;
        } else if (!gl->creator || !match2strings(mask, gl->creator->content))
          continue;
      } else {
        if (flags & GLIST_EXACT) {
          if (!glineequal(searchgl, gl))
            continue;
        } else if (flags & GLIST_FIND) {
          if (!gline_match_mask(gl, searchgl))
            continue;
        } else {
          if (!match2strings(mask, glinetostring(gl)))
            continue;
        }
      }
    }

    if (count == limit && !(flags & GLIST_COUNT))
      controlreply(sender, "More than %d matches, list truncated.", limit);

    count++;

    if (!(flags & GLIST_COUNT) && count < limit) {
      snprintf(expirestr, sizeof(expirestr), "%s", glinetostring(gl));
      snprintf(idstr, sizeof(idstr), "%d", gl->glinebufid);
      controlreply(sender, "%s%-49s %-19s %-15s %-25s %s",
        (gl->flags & GLINE_ACTIVE) ? "+" : "-",
        expirestr,
        (gl->flags & GLINE_ACTIVE) ? (char*)longtoduration(gl->expire - curtime, 0) : "<inactive>",
        gl->glinebufid ? idstr : "",
        gl->creator ? gl->creator->content : "",
        gl->reason ? gl->reason->content : "");
    }
  }

  controlreply(sender, "%s%d G-Line%s found.", (flags & GLIST_COUNT) ? "" : "End of list - ", count, count == 1 ? "" : "s");

  return CMD_OK;
}

static int glines_cmdglinelog(void *source, int cargc, char **cargv) {
  nick *sender = source;
  glinebuf *gbl;
  gline *gl;
  int i, id, count;
  char timebuf[30];
  
  id = 0;
  
  if (cargc > 0) {
    id = atoi(cargv[0]);
    
    if (id == 0) {
      controlreply(sender, "Invalid log ID.");
      return CMD_ERROR;
    }
  }
  
  controlreply(sender, "Time                 ID         G-Lines    User Hits      Channel Hits     Comment");
  
  for (i = 0; i < MAXGLINELOG; i++) {
    gbl = glinebuflog[(i + glinebuflogoffset + 1) % MAXGLINELOG];
    
    if (!gbl)
      continue;
    
    if (id == 0 || gbl->id == id) {
      count = 0;
      
      for (gl = gbl->glines; gl; gl = gl->next)
	count++;

      strftime(timebuf, sizeof(timebuf), "%d/%m/%y %H:%M:%S", localtime((gbl->amend) ? &gbl->amend : &gbl->commit));
      strncat(timebuf, (gbl->amend) ? "*" : " ", sizeof(timebuf));
      controlreply(sender, "%-20s %-10d %-10d %-15d %-15d %s", timebuf, gbl->id, count, gbl->userhits, gbl->channelhits, gbl->comment ? gbl->comment->content : "(no comment)");
    }

    if (id != 0 && gbl->id == id) {
      glinebufspew(gbl, sender);
      controlreply(sender, "Done.");
      return CMD_OK;
    }
  }
  
  if (id == 0) {
    controlreply(sender, "Done.");
  } else {
    controlreply(sender, "Log entry for ID %d not found.", id);
  }

  return CMD_OK;
}

static int glines_cmdglineundo(void *source, int cargc, char **cargv) {
  nick *sender = source;
  int id;
  
  if (cargc < 1)
    return CMD_USAGE;

  id = atoi(cargv[0]);
  
  if (id == 0 || !glinebufundo(id)) {
    controlreply(sender, "Invalid log ID.");
    return CMD_ERROR;
  }

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

static int glines_cmdcleanupglines(void *source, int cargc, char **cargv) {
  nick *sender = source;
  gline **pnext, *gl;
  int count;
  time_t now;
  
  count = 0;
  time(&now);
  
  for (pnext = &glinelist; *pnext;) {
    gl = *pnext;
    
    /* Remove inactivate glines that have been last changed more than a week ago */
    if (!(gl->flags & GLINE_ACTIVE) && gl->lastmod < now - 7 * 24 * 60 * 60) {
      gline_destroy(gl, 0, 1);
      count++;
    } else {
      pnext = &((*pnext)->next);
    }
    
    if (!*pnext)
      break;
  }
  
  controlwall(NO_OPER, NL_GLINES, "%s CLEANUPGLINES'd %d G-Lines.",
    controlid(sender), count);
  
  controlreply(sender, "Done.");
  
  return CMD_OK;
}

static int commandsregistered;

static void registercommands(int hooknum, void *arg) {
  if (commandsregistered)
    return;
  commandsregistered = 1;

  registercontrolhelpcmd("block", NO_OPER, 4, glines_cmdblock, "Usage: block ?flags? <nick> <duration> <reason>\nSets a gline using an appropriate mask given the user's nickname.\nFlags can be one or more of:\n-f - bypass sanity checks\n-l - bypass hit limits\n-S - simulate who the glines would hit\n-c - chase nick across quits/kills/nick changes");
  registercontrolhelpcmd("gline", NO_OPER, 4, glines_cmdgline, "Usage: gline ?flags? <mask> <duration> <reason>\nSets a gline.\nFlags can be one or more of:\n-f - bypass sanity checks\n-l - bypass hit limits\n-S - simulate who the glines would hit");
  registercontrolhelpcmd("smartgline", NO_OPER, 4, glines_cmdsmartgline, "Usage: smartgline ?flags? <user@host> <duration> <reason>\nSets a gline. Automatically adjusts the mask depending on whether the specified mask is trusted.\nFlags can be one or more of:\n-f - bypass sanity checks\n-l - bypass hit limits\n-S - simulate who the glines would hit");
  registercontrolhelpcmd("ungline", NO_OPER, 1, glines_cmdungline, "Usage: ungline <mask>\nDeactivates a gline.");
  registercontrolhelpcmd("destroygline", NO_DEVELOPER, 1, glines_cmddestroygline, "Usage: destroygline <mask>\nDestroys a gline.");
  registercontrolhelpcmd("clearchan", NO_OPER, 5, glines_cmdclearchan, "Usage: clearchan ?flags? <#channel> <how> <duration> ?reason?\nClears a channel.\nhow can be one of:\nkick - Kicks users.\nkill - Kills users.\ngline - Glines non-authed users (using an appropriate mask).\nglineall - Glines users.\nDuration is only valid when glining users. Reason defaults to \"Clearing channel.\".\nFlags (for glines) can be one or more of:\n-f - bypass sanity checks\n-l - bypass hit limits\n-S - simulate who the glines would hit");
  registercontrolhelpcmd("trustgline", NO_OPER, 5, glines_cmdtrustgline, "Usage: trustgline ?flags? <#id|name> <user> <duration> <reason>\nSets a gline on the specified username for each host in the specified trust group. The username may contain wildcards.\nFlags can be one or more of:\n-f - bypass sanity checks\n-l - bypass hit limits\n-S - simulate who the glines would hit");
  registercontrolhelpcmd("trustungline", NO_OPER, 2, glines_cmdtrustungline, "Usage: trustungline <#id|name> <user>\nRemoves a gline that was previously set with trustgline.");
  registercontrolhelpcmd("glstats", NO_OPER, 0, glines_cmdglstats, "Usage: glstat\nShows statistics about G-Lines.");
  registercontrolhelpcmd("glist", NO_OPER, 2, glines_cmdglist, "Usage: glist [-flags] <mask>\nLists matching G-Lines.\nValid flags are:\n-c: Count G-Lines.\n-f: Find G-Lines active on <mask>.\n-x: Find G-Lines matching <mask> exactly.\n-R: Find G-lines on realnames.\n-o: Search for glines by owner.\n-r: Search for glines by reason.\n-i: Include inactive glines.");
  registercontrolhelpcmd("glinelog", NO_OPER, 1, glines_cmdglinelog, "Usage: glinelog ?id?\nShows information about previous gline transactions.");
  registercontrolhelpcmd("glineundo", NO_OPER, 1, glines_cmdglineundo, "Usage: glineundo ?id?\nUndoes a gline transaction.");
  registercontrolhelpcmd("syncglines", NO_DEVELOPER, 0, glines_cmdsyncglines, "Usage: syncglines\nSends all G-Lines to all other servers.");
  registercontrolhelpcmd("cleanupglines", NO_DEVELOPER, 0, glines_cmdcleanupglines, "Usage: cleanupglines\nDestroys all deactivated G-Lines.");
}

static void deregistercommands(int hooknum, void *arg) {
  if (!commandsregistered)
    return;
  commandsregistered = 0;

  deregistercontrolcmd("block", glines_cmdblock);
  deregistercontrolcmd("gline", glines_cmdgline);
  deregistercontrolcmd("smartgline", glines_cmdsmartgline);
  deregistercontrolcmd("ungline", glines_cmdungline);
  deregistercontrolcmd("destroygline", glines_cmddestroygline);
  deregistercontrolcmd("clearchan", glines_cmdclearchan);
  deregistercontrolcmd("trustgline", glines_cmdtrustgline);
  deregistercontrolcmd("trustungline", glines_cmdtrustungline);
  deregistercontrolcmd("glstats", glines_cmdglstats);
  deregistercontrolcmd("glist", glines_cmdglist);
  deregistercontrolcmd("glinelog", glines_cmdglinelog);
  deregistercontrolcmd("glineundo", glines_cmdglineundo);
  deregistercontrolcmd("syncglines", glines_cmdsyncglines);
  deregistercontrolcmd("cleanupglines", glines_cmdcleanupglines);
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
