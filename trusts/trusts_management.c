#include <stdio.h>
#include <string.h>
#include "../control/control.h"
#include "../lib/version.h"
#include "../lib/irc_string.h"
#include "../lib/strlfunc.h"
#include "../core/config.h"
#include "../core/schedule.h"
#include "../irc/irc.h"
#include "../lib/stringbuf.h"
#include "../control/control.h"
#include "../control/control_policy.h"
#include "trusts.h"

MODULE_VERSION("");

static void registercommands(int, void *);
static void deregistercommands(int, void *);

typedef int (*trustmodificationfn)(void *, char *arg, nick *, int);

struct trustmodification {
  char name[50];
  trustmodificationfn fn;
};

static int trusts_cmdtrustadd(void *source, int cargc, char **cargv) {
  trustgroup *tg;
  nick *sender = source;
  char *host;
  struct irc_in_addr ip;
  unsigned char bits;
  trusthost *th, *superset, *subset;

  if(cargc < 2)
    return CMD_USAGE;

  tg = tg_strtotg(cargv[0]);
  if(!tg) {
    controlreply(sender, "Couldn't look up trustgroup.");
    return CMD_ERROR;
  }

  host = cargv[1];
  if(!ipmask_parse(host, &ip, &bits)) {
    controlreply(sender, "Invalid host.");
    return CMD_ERROR;
  }

  if(!is_normalized_ipmask(&ip, bits)) {
    controlreply(sender, "Invalid IP Mask.");
    return CMD_ERROR;
  }

  /* Don't allow non-developers to add trusts for large subnets or modify protected groups. */
  if (!noperserv_policy_command_permitted(NO_DEVELOPER, sender)) {
    int minbits = irc_in_addr_is_ipv4(&ip)?TRUST_MIN_UNPRIVILEGED_BITS_IPV4:TRUST_MIN_UNPRIVILEGED_BITS_IPV6;
    if(bits < minbits) {
      controlreply(sender, "You don't have the necessary privileges to add a subnet larger than /%d.", irc_bitlen(&ip, minbits));
      return CMD_ERROR;
    }

    if(tg->flags & TRUST_PROTECTED) {
      controlreply(sender, "You don't have the necessary privileges to modify a protected trust group.");
      return CMD_ERROR;
    }
  }

  /* OKAY! Lots of checking here!
   *
   * Need to check:
   *   - exact same host isn't already covered by given group (reject if it is)
   *   - host doesn't already exist exactly already (reject if it does)
   *   - host is more specific than an existing one (warn if it is, fix up later)
   *   - host is less specific than an existing one (warn if it is, don't need to do anything special)
   */

  for(th=tg->hosts;th;th=th->next) {
    if(ipmask_check(&ip, &th->ip, th->bits) && th->bits == bits) {
      controlreply(sender, "This host (or part of it) is already covered in the given group.");
      return CMD_ERROR;
    }
  }

  if(th_getbyhostandmask(&ip, bits)) {
    controlreply(sender, "This host already exists in another group with the same mask.");
    return CMD_ERROR;
  }

  /* this function will set both to NULL if it's equal, hence the check above */
  th_getsuperandsubsets(&ip, bits, &superset, &subset);
  if(superset) {
    /* a superset exists for us, we will be more specific than one existing host */

    controlreply(sender, "Note: This host already exists in another group, but this new host will override it as it has a smaller prefix.");
  }
  if(subset) {
    /* a subset of us exists, we will be less specific than some existing hosts */

    controlreply(sender, "Note: This host already exists in at least one other group, the new host has a larger prefix and therefore will not override those hosts.");
  }

  th = th_new(tg, host);
  if(!th) {
    controlreply(sender, "An error occured adding the host to the group.");
    return CMD_ERROR;
  }

  controlreply(sender, "Host added.");
  triggerhook(HOOK_TRUSTS_ADDHOST, th);

  controlwall(NO_OPER, NL_TRUSTS, "%s TRUSTADD'ed host %s to group '%s'", controlid(sender), host, th->group->name->content);
  trustlog(tg, sender->authname, "Added host '%s'.", host);

  return CMD_OK;
}

static int trusts_cmdtrustgroupadd(void *source, int cargc, char **cargv) {
  nick *sender = source;
  char *name, *contact, *comment, createdby[ACCOUNTLEN + 2];
  long howmany, maxperident, enforceident;
  trustgroup *tg, itg;
  int override, flags;

  if(cargc < 5)
    return CMD_USAGE;

  override = noperserv_policy_command_permitted(NO_DEVELOPER, sender);

  name = cargv[0];
  howmany = strtol(cargv[1], NULL, 10);
  if(!override && (!howmany || (howmany > MAXTRUSTEDFOR))) {
    controlreply(sender, "Bad value maximum number of clients.");
    return CMD_ERROR;
  }

  maxperident = strtol(cargv[2], NULL, 10);
  if(maxperident < 0 || (maxperident > MAXPERIDENT)) {
    controlreply(sender, "Bad value for max per ident.");
    return CMD_ERROR;
  }

  if(cargv[3][0] != '1' && cargv[3][0] != '0') {
    controlreply(sender, "Bad value for enforce ident (use 0 or 1).");
    return CMD_ERROR;
  }
  enforceident = cargv[3][0] == '1';

  contact = cargv[4];

  if(cargc < 6) {
    comment = "(no comment)";
  } else {
    comment = cargv[5];
  }

  /* don't allow #id or id forms */
  if((name[0] == '#') || strtol(name, NULL, 10)) {
    controlreply(sender, "Invalid trustgroup name.");
    return CMD_ERROR;
  }

  tg = tg_strtotg(name);
  if(tg) {
    controlreply(sender, "A group with that name already exists.");
    return CMD_ERROR;
  }

  snprintf(createdby, sizeof(createdby), "#%s", sender->authname);

  flags = 0;

  if(maxperident > 0)
    flags |= TRUST_RELIABLE_USERNAME;

  if(enforceident)
    flags |= TRUST_ENFORCE_IDENT;

  itg.trustedfor = howmany;
  itg.flags = flags;
  itg.maxperident = maxperident;
  itg.expires = 0;
  itg.createdby = getsstring(createdby, CREATEDBYLEN);
  itg.contact = getsstring(contact, CONTACTLEN);
  itg.comment = getsstring(comment, COMMENTLEN);
  itg.name = getsstring(name, TRUSTNAMELEN);

  if(itg.createdby && itg.contact && itg.comment && itg.name) {
    tg = tg_new(&itg);
  } else {
    tg = NULL;
  }

  freesstring(itg.createdby);
  freesstring(itg.comment);
  freesstring(itg.name);
  freesstring(itg.contact);

  if(!tg) {
    controlreply(sender, "An error occured adding the trustgroup.");
    return CMD_ERROR;
  }

  controlreply(sender, "Group added.");
  triggerhook(HOOK_TRUSTS_ADDGROUP, tg);

  controlwall(NO_OPER, NL_TRUSTS, "%s TRUSTGROUPADD'ed '%s'", controlid(sender), tg->name->content);
  trustlog(tg, sender->authname, "Created trust group '%s' (ID #%d): howmany=%d, enforceident=%d, maxperident=%d, "
    "createdby=%s, contact=%s, comment=%s",
    tg->name->content, tg->id, howmany, enforceident, maxperident, createdby, contact, comment);

  return CMD_OK;
}

static int trusts_cmdtrustgroupdel(void *source, int cargc, char **cargv) {
  trustgroup *tg;
  nick *sender = source;

  if(cargc < 1)
    return CMD_USAGE;

  tg = tg_strtotg(cargv[0]);
  if(!tg) {
    controlreply(sender, "Couldn't look up trustgroup.");
    return CMD_ERROR;
  }

  /* Don't allow non-developers to delete protected groups. */
  if (!noperserv_policy_command_permitted(NO_DEVELOPER, sender)) {
    if(tg->flags & TRUST_PROTECTED) {
      controlreply(sender, "You don't have the necessary privileges to modify a protected trust group.");
      return CMD_ERROR;
    }
  }

  if(tg->hosts) {
    controlreply(sender, "Delete all hosts before deleting the group.");
    return CMD_ERROR;
  }

  controlwall(NO_OPER, NL_TRUSTS, "%s TRUSTGROUPDEL'ed '%s'.", controlid(sender), tg->name->content);
  trustlog(tg, sender->authname, "Deleted group '%s'.", tg->name->content);

  triggerhook(HOOK_TRUSTS_DELGROUP, tg);
  tg_delete(tg);
  controlreply(sender, "Group deleted.");

  return CMD_OK;
}

static int trusts_cmdtrustdel(void *source, int cargc, char **cargv) {
  trustgroup *tg;
  trusthost *th;
  struct irc_in_addr ip;
  unsigned char bits;
  nick *sender = source;
  char *host;

  if(cargc < 2)
    return CMD_USAGE;

  tg = tg_strtotg(cargv[0]);
  if(!tg) {
    controlreply(sender, "Couldn't look up trustgroup.");
    return CMD_ERROR;
  }

  host = cargv[1];
  if(!ipmask_parse(host, &ip, &bits)) {
    controlreply(sender, "Invalid IP/mask.");
    return CMD_ERROR;
  }

  /* Don't allow non-developers to remove trusts for large subnets or modify protected groups. */
  if (!noperserv_policy_command_permitted(NO_DEVELOPER, sender)) {
    int minbits = irc_in_addr_is_ipv4(&ip)?TRUST_MIN_UNPRIVILEGED_BITS_IPV4:TRUST_MIN_UNPRIVILEGED_BITS_IPV6;
    if(bits < minbits) {
      controlreply(sender, "You don't have the necessary privileges to remove a subnet larger than /%d.", irc_bitlen(&ip, minbits));
      return CMD_ERROR;
    }

    if(tg->flags & TRUST_PROTECTED) {
      controlreply(sender, "You don't have the necessary privileges to modify a protected trust group.");
      return CMD_ERROR;
    }
  }

  for(th=tg->hosts;th;th=th->next)
    if(ipmask_check(&ip, &th->ip, th->bits) && th->bits == bits)
      break;

  if(!th) {
    controlreply(sender, "Couldn't find that host in that group.");
    return CMD_ERROR;
  }

  triggerhook(HOOK_TRUSTS_DELHOST, th);
  th_delete(th);
  controlreply(sender, "Host deleted.");

  controlwall(NO_OPER, NL_TRUSTS, "%s TRUSTDEL'ed %s from group '%s'.", controlid(sender), host, tg->name->content);
  trustlog(tg, sender->authname, "Removed host '%s'.", host);

  return CMD_OK;
}

static int modifycomment(void *arg, char *comment, nick *source, int override) {
  trustgroup *tg = arg;
  sstring *n = getsstring(comment, COMMENTLEN);
  if(!n)
    return 0;

  freesstring(tg->comment);
  tg->comment = n;

  return 1;
}

static int modifycontact(void *arg, char *contact, nick *source, int override) {
  trustgroup *tg = arg;
  sstring *n = getsstring(contact, CONTACTLEN);
  if(!n)
    return 0;

  freesstring(tg->contact);
  tg->contact = n;

  return 1;
}

static int modifytrustedfor(void *arg, char *num, nick *source, int override) {
  trustgroup *tg = arg;
  long trustedfor = strtol(num, NULL, 10);

  if(trustedfor < 0) {
    controlreply(source, "The clone limit must not be negative.");
    return 0;
  }

  if(!override) {
    if (trustedfor == 0) {
      controlreply(source, "You don't have the necessary privileges to set an unlimited clone limit.");
      return 0;
    }

    if (trustedfor > MAXTRUSTEDFOR) {
      controlreply(source, "You don't have the necessary privileges to set the clone limit to a value higher than %d.", MAXTRUSTEDFOR);
      return 0;
    }
  }

  tg->trustedfor = trustedfor;

  return 1;
}

static int modifymaxperident(void *arg, char *num, nick *source, int override) {
  trustgroup *tg = arg;
  long maxperident = strtol(num, NULL, 10);

  if(maxperident < 0) {
    controlreply(source, "Ident limit must not be negative.");
    return 0;
  }

  if(maxperident > MAXPERIDENT) {
    controlreply(source, "Ident limit must not be higher than %d. Consider setting it to 0 (unlimited) instead.", MAXPERIDENT);
    return 0;
  }

  tg->maxperident = maxperident;

  return 1;
}

static int modifyenforceident(void *arg, char *num, nick *source, int override) {
  trustgroup *tg = arg;

  if(num[0] == '1') {
    tg->flags |= TRUST_ENFORCE_IDENT;
  } else if(num[0] == '0') {
    tg->flags &= ~TRUST_ENFORCE_IDENT;
  } else {
    return 0;
  }

  return 1;
}

static int modifyreliableusername(void *arg, char *num, nick *source, int override) {
  trustgroup *tg = arg;

  if(num[0] == '1') {
    tg->flags |= TRUST_RELIABLE_USERNAME;
  } else if(num[0] == '0') {
    tg->flags &= ~TRUST_RELIABLE_USERNAME;
  } else {
    return 0;
  }

  return 1;
}

static int modifyunthrottle(void *arg, char *num, nick *source, int override) {
  trustgroup *tg = arg;

  if(num[0] == '1') {
    tg->flags |= TRUST_UNTHROTTLE;
  } else if(num[0] == '0') {
    tg->flags &= ~TRUST_UNTHROTTLE;
  } else {
    return 0;
  }

  return 1;
}

static int modifyexpires(void *arg, char *expires, nick *source, int override) {
  trustgroup *tg = arg;
  int howlong = durationtolong(expires);

  if((howlong < 0) || (howlong > MAXDURATION)) {
    controlreply(source, "Duration cannot be negative or greater than %s (use 0 instead if you don't want the group to expire).", longtoduration(MAXDURATION, 0));
    return 0;
  }

  if(howlong)
    tg->expires = getnettime() + howlong;
  else
    tg->expires = 0; /* never */

  return 1;
}

static int modifycleanup(void *arg, char *num, nick *source, int override) {
  trustgroup *tg = arg;

  if(!override) {
    controlreply(source, "You don't have the necessary privileges to modify this attribute.");
    return 0;
  }

  if(num[0] == '1') {
    tg->flags &= ~TRUST_NO_CLEANUP;
  } else if(num[0] == '0') {
    tg->flags |= TRUST_NO_CLEANUP;
  } else {
    return 0;
  }

  return 1;
}

static int modifyprotected(void *arg, char *num, nick *source, int override) {
  trustgroup *tg = arg;

  if(!override) {
    controlreply(source, "You don't have the necessary privileges to modify this attribute.");
    return 0;
  }

  if(num[0] == '1') {
    tg->flags |= TRUST_PROTECTED;
  } else if(num[0] == '0') {
    tg->flags &= ~TRUST_PROTECTED;
  } else {
    return 0;
  }

  return 1;
}

static int modifymaxpernode(void *arg, char *num, nick *source, int override) {
  trusthost *th = arg;
  int maxpernode = strtol(num, NULL, 10);

  if(maxpernode < 0) {
    controlreply(source, "Node limit must not be negative.");
    return 0;
  }

  if(maxpernode>MAXPERNODE) {
    controlreply(source, "Node limit must not be higher than %d. Consider setting it to 0 (unlimited) instead.", MAXPERNODE);
    return 0;
  } 
 
  th->maxpernode = maxpernode;

  return 1;
}

static int modifynodebits(void *arg, char *num, nick *source, int override) {
  trusthost *th = arg;
  int nodebits = strtol(num, NULL, 10);

  if(nodebits < 0) {
    controlreply(source, "Node bits must not be negative.");
    return 0;
  }

  if(irc_in_addr_is_ipv4(&th->ip))
    nodebits += 96;

  if(nodebits > 128) {
    controlreply(source, "Node bits is invalid.");
    return 0;
  }

  if(!override) {
    int minbits = irc_in_addr_is_ipv4(&th->ip)?TRUST_MIN_UNPRIVILEGED_NODEBITS_IPV4:TRUST_MIN_UNPRIVILEGED_NODEBITS_IPV6;

    if(nodebits < minbits) {
      controlreply(source, "You don't have the necessary privileges to set node bits to a subnet larger than /%d.", irc_bitlen(&th->ip, minbits));
      return 0;
    }
  }

  if(nodebits<th->bits) {
    controlreply(source, "Node bits must be smaller or equal to the trusted CIDR's subnet size.");
    return 0;
  }

  th->nodebits = nodebits;

  return 1;
}

static array trustgroupmods_a;
static struct trustmodification *trustgroupmods;
static array trusthostmods_a;
static struct trustmodification *trusthostmods;

static int trusts_cmdtrustgroupmodify(void *source, int cargc, char **cargv) {
  trustgroup *tg;
  nick *sender = source;
  char *what, *to;
  int i, override;

  if(cargc < 3)
    return CMD_USAGE;

  tg = tg_strtotg(cargv[0]);
  if(!tg) {
    controlreply(sender, "Couldn't look up trustgroup.");
    return CMD_ERROR;
  }

  what = cargv[1];
  to = cargv[2];

  override = noperserv_policy_command_permitted(NO_DEVELOPER, sender);

  /* Don't allow non-developers to modify protected groups. */
  if (!override && tg->flags & TRUST_PROTECTED) {
    controlreply(sender, "You don't have the necessary privileges to modify a protected trust group.");
    return CMD_ERROR;
  }

  for(i=0;i<trustgroupmods_a.cursi;i++) {
    if(!strcmp(what, trustgroupmods[i].name)) {
      if(!(trustgroupmods[i].fn)(tg, to, sender, override)) {
        controlreply(sender, "An error occured changing that property, check the syntax.");
        return CMD_ERROR;
      }
      break;
    }
  }

  if(i == trustgroupmods_a.cursi)
    return CMD_USAGE;

  triggerhook(HOOK_TRUSTS_MODIFYGROUP, tg);
  tg_update(tg);
  controlreply(sender, "Group modified.");

  controlwall(NO_OPER, NL_TRUSTS, "%s TRUSTMODIFIED'ed group '%s' (field: %s, value: %s)", controlid(sender), tg->name->content, what, to);
  trustlog(tg, sender->authname, "Modified %s: %s", what, to);

  return CMD_OK;
}

static int trusts_cmdtrusthostmodify(void *source, int cargc, char **cargv) {
  trustgroup *tg;
  trusthost *th;
  nick *sender = source;
  char *what, *to;
  int i, override;
  struct irc_in_addr ip;
  unsigned char bits;

  if(cargc < 4)
    return CMD_USAGE;

  tg = tg_strtotg(cargv[0]);
  if(!tg) {
    controlreply(sender, "Couldn't look up trustgroup.");
    return CMD_ERROR;
  }

  if(!ipmask_parse(cargv[1], &ip, &bits)) {
    controlreply(sender, "Invalid host.");
    return CMD_ERROR;
  }

  /* Don't allow non-developers to modify trusts for large subnets or modify protected groups. */
  if (!noperserv_policy_command_permitted(NO_DEVELOPER, sender)) {
    int minbits = irc_in_addr_is_ipv4(&ip)?TRUST_MIN_UNPRIVILEGED_BITS_IPV4:TRUST_MIN_UNPRIVILEGED_BITS_IPV6;
    if(bits < minbits) {
      controlreply(sender, "You don't have the necessary privileges to modify a subnet larger than /%d.", irc_bitlen(&ip, minbits));
      return CMD_ERROR;
    }

    if(tg->flags & TRUST_PROTECTED) {
      controlreply(sender, "You don't have the necessary privileges to modify a protected trust group.");
      return CMD_ERROR;
    }
  }

  th = th_getbyhostandmask(&ip, bits);

  if(!th || th->group != tg) {
    controlreply(sender, "Host does not belong to the specified group.");
    return CMD_ERROR;
  }

  what = cargv[2];
  to = cargv[3];

  override = noperserv_policy_command_permitted(NO_DEVELOPER, sender);

  for(i=0;i<trusthostmods_a.cursi;i++) {
    if(!strcmp(what, trusthostmods[i].name)) {
      if(!(trusthostmods[i].fn)(th, to, sender, override)) {
        controlreply(sender, "An error occured changing that property, check the syntax.");
        return CMD_ERROR;
      }
      break;
    }
  }

  if(i == trusthostmods_a.cursi)
    return CMD_USAGE;

  triggerhook(HOOK_TRUSTS_MODIFYHOST, th);
  th_update(th);
  controlreply(sender, "Host modified.");

  controlwall(NO_OPER, NL_TRUSTS, "%s TRUSTMODIFIED'ed host '%s' in group '%s' (field: %s, value: %s)", controlid(sender), CIDRtostr(ip, bits), tg->name->content, what, to);
  trustlog(tg, sender->authname, "Modified %s for host '%s': %s", what, CIDRtostr(ip, bits), to);

  return CMD_OK;
}

static int trusts_cmdtrustlog(void *source, int cargc, char **cargv) {
  nick *sender = source;
  char *name;
  int groupid;
  long limit = 0;

  if(cargc < 1)
    return CMD_USAGE;

  if(cargc>1)
    limit = strtol(cargv[1], NULL, 10);

  if(limit==0)
    limit = 100;

  name = cargv[0];

  if (name[0] == '#') {
    groupid = strtol(name + 1, NULL, 10);
    trustlogspewid(sender, groupid, limit);
  } else {
    trustlogspewname(sender, name, limit);
  }

  return CMD_OK;
}

static int trusts_cmdtrustloggrep(void *source, int cargc, char **cargv) {
  nick *sender = source;
  char *pattern;
  long limit = 0;

  if(cargc < 1)
    return CMD_USAGE;

  pattern = cargv[0];

  if(cargc>1)
    limit = strtol(cargv[1], NULL, 10);

  if(limit==0)
    limit = 100;

  trustloggrep(sender, pattern, limit);

  return CMD_OK;
}

static int trusts_cmdtrustcomment(void *source, int cargc, char **cargv) {
  nick *sender = source;
  trustgroup *tg = NULL;
  char *name, *comment;

  if(cargc < 2)
    return CMD_USAGE;

  name = cargv[0];
  comment = cargv[1];

  if(strlen(comment)>TRUSTLOGLEN) {
    controlreply(sender, "Your comment is too long (max: %d characters).", TRUSTLOGLEN);
    return CMD_OK;
  }

  tg = tg_strtotg(name);

  if(!tg) {
    controlreply(sender, "Invalid trust group name or ID.");
    return CMD_OK;
  }

  controlwall(NO_OPER, NL_TRUSTS, "%s TRUSTCOMMENT'ed group '%s': %s", controlid(sender), tg->name->content, comment);
  trustlog(tg, sender->authname, "Comment: %s", comment);

  return CMD_OK;
}

static void cleanuptrusts(void *arg);

static int trusts_cmdtrustcleanup(void *source, int cargc, char **cargv) {
  cleanuptrusts(source);

  controlreply(source, "Done.");

  return CMD_OK;
}


static int commandsregistered;

static void registercommands(int hooknum, void *arg) {
  static char tgmhelp[512], thmhelp[512];
  char validfields[512];
  StringBuf b;
  int i;

  if(commandsregistered)
    return;
  commandsregistered = 1;

  registercontrolhelpcmd("trustgroupadd", NO_OPER, 6, trusts_cmdtrustgroupadd, "Usage: trustgroupadd <name> <howmany> <maxperident> <enforceident> <contact> ?comment?");
  registercontrolhelpcmd("trustadd", NO_OPER, 2, trusts_cmdtrustadd, "Usage: trustadd <#id|name|id> <host>");
  registercontrolhelpcmd("trustgroupdel", NO_OPER, 1, trusts_cmdtrustgroupdel, "Usage: trustgroupdel <#id|name|id>");
  registercontrolhelpcmd("trustdel", NO_OPER, 2, trusts_cmdtrustdel, "Usage: trustdel <#id|name|id> <ip/mask>");

  sbinit(&b, validfields, sizeof(validfields));
  for(i=0;i<trustgroupmods_a.cursi;i++) {
    if(i > 0)
      sbaddstr(&b, ", ");
    sbaddstr(&b, trustgroupmods[i].name);
  }
  sbterminate(&b);

  snprintf(tgmhelp, sizeof(tgmhelp), "Usage: trustgroupmodify <#id|name|id> <field> <new value>\nModifies a trust group.\nValid fields: %s", validfields);
  registercontrolhelpcmd("trustgroupmodify", NO_OPER, 3, trusts_cmdtrustgroupmodify, tgmhelp);

  sbinit(&b, validfields, sizeof(validfields));
  for(i=0;i<trusthostmods_a.cursi;i++) {
    if(i > 0)
      sbaddstr(&b, ", ");
    sbaddstr(&b, trusthostmods[i].name);
  }
  sbterminate(&b);

  snprintf(thmhelp, sizeof(thmhelp), "Usage: trusthostmodify <#id|name|id> <host> <field> <new value>\nModifies a trust host\nValid fields: %s", validfields);
  registercontrolhelpcmd("trusthostmodify", NO_OPER, 4, trusts_cmdtrusthostmodify, thmhelp);

  registercontrolhelpcmd("trustlog", NO_OPER, 2, trusts_cmdtrustlog, "Usage: trustlog <#id|name> ?limit?\nShows log for the specified trust group.");
  registercontrolhelpcmd("trustloggrep", NO_OPER, 2, trusts_cmdtrustloggrep, "Usage trustloggrep <pattern> ?limit?\nShows maching log entries.");
  registercontrolhelpcmd("trustcomment", NO_OPER, 2, trusts_cmdtrustcomment, "Usage: trustcomment <#id|name> <comment>\nLogs a comment for a trust.");
  registercontrolhelpcmd("trustcleanup", NO_DEVELOPER, 0, trusts_cmdtrustcleanup, "Usage: trustcleanup\nCleans up unused trusts.");
}

static void deregistercommands(int hooknum, void *arg) {
  if(!commandsregistered)
    return;
  commandsregistered = 0;

  deregistercontrolcmd("trustgroupadd", trusts_cmdtrustgroupadd);
  deregistercontrolcmd("trustadd", trusts_cmdtrustadd);
  deregistercontrolcmd("trustgroupdel", trusts_cmdtrustgroupdel);
  deregistercontrolcmd("trustdel", trusts_cmdtrustdel);
  deregistercontrolcmd("trustgroupmodify", trusts_cmdtrustgroupmodify);
  deregistercontrolcmd("trusthostmodify", trusts_cmdtrusthostmodify);
  deregistercontrolcmd("trustlog", trusts_cmdtrustlog);
  deregistercontrolcmd("trustloggrep", trusts_cmdtrustloggrep);
  deregistercontrolcmd("trustcomment", trusts_cmdtrustcomment);
  deregistercontrolcmd("trustcleanup", trusts_cmdtrustcleanup);
}

static int loaded;

#define _ms_(x) (struct trustmodification){ .name = # x, .fn = modify ## x }
#define MSGROUP(x) { int slot = array_getfreeslot(&trustgroupmods_a); trustgroupmods = (struct trustmodification *)trustgroupmods_a.content; memcpy(&trustgroupmods[slot], &_ms_(x), sizeof(struct trustmodification)); }
#define MSHOST(x) { int slot = array_getfreeslot(&trusthostmods_a); trusthostmods = (struct trustmodification *)trusthostmods_a.content; memcpy(&trusthostmods[slot], &_ms_(x), sizeof(struct trustmodification)); }

static void setupmods(void) {
  MSGROUP(expires);
  MSGROUP(enforceident);
  MSGROUP(reliableusername);
  MSGROUP(maxperident);
  MSGROUP(contact);
  MSGROUP(comment);
  MSGROUP(trustedfor);
  MSGROUP(cleanup);
  MSGROUP(protected);
  MSGROUP(unthrottle);

  MSHOST(maxpernode);
  MSHOST(nodebits);
}

static int cleanuptrusts_active;

static void cleanuptrusts(void *arg) {
  unsigned int now, to_age;
  nick *np = (nick *)arg;
  trustgroup *tg;
  trusthost *th;
  int thcount = 0, tgcount = 0;
  int i;
  flag_t noticelevel;
  array expiredths, expiredtgs;

  now = getnettime();
  to_age = now - (CLEANUP_TH_INACTIVE * 3600 * 24);

  if(np) {
    noticelevel = NL_TRUSTS;
    controlwall(NO_OPER, noticelevel, "CLEANUPTRUSTS: Manually started by %s.", np->nick);
  } else {
    noticelevel = NL_CLEANUP;
    controlwall(NO_OPER, noticelevel, "CLEANUPTRUSTS: Automatically started.");
  }

  if (cleanuptrusts_active) {
    controlwall(NO_OPER, NL_TRUSTS, "CLEANUPTRUSTS: ABORTED! Cleanup already in progress! BUG BUG BUG!");
    return;
  }

  cleanuptrusts_active=1;

  array_init(&expiredtgs, sizeof(trustgroup *));

  for(tg=tglist;tg;tg=tg->next) {
    array_init(&expiredths, sizeof(trusthost *));

    if(tg->flags & TRUST_NO_CLEANUP)
      continue;

    for(th=tg->hosts;th;th=th->next) {
      if((th->count == 0 && th->created < to_age && th->lastseen < to_age) || (tg->expires && tg->expires < now)) {
        int pos = array_getfreeslot(&expiredths);
        ((trusthost **)(expiredths.content))[pos] = th;
      }       
    }

    for(i=0;i<expiredths.cursi;i++) {
      const char *cidrstr;

      th = ((trusthost **)(expiredths.content))[i];
      triggerhook(HOOK_TRUSTS_DELHOST, th);

      cidrstr = CIDRtostr(th->ip, th->bits);
      trustlog(tg, "cleanuptrusts", "Removed host '%s' because it was unused for %d days.", cidrstr, CLEANUP_TH_INACTIVE);

      th_delete(th);

      thcount++;
    }

    if(!tg->hosts) {
      int pos = array_getfreeslot(&expiredtgs);
      ((trustgroup **)(expiredtgs.content))[pos] = tg;
    }
  }

  for(i=0;i<expiredtgs.cursi;i++) {
    tg = ((trustgroup **)(expiredtgs.content))[i];
    triggerhook(HOOK_TRUSTS_DELGROUP, tg);
    trustlog(tg, "cleanuptrusts", "Deleted group '%s' because it had no hosts left.", tg->name->content);
    tg_delete(tg);
    tgcount++;
  }

  controlwall(NO_OPER, noticelevel, "CLEANUPTRUSTS: Removed %d trust hosts (inactive for %d days) and %d empty trust groups.", thcount, CLEANUP_TH_INACTIVE, tgcount);

  cleanuptrusts_active=0;
}

static void schedulecleanup(int hooknum, void *arg) {
  /* run at 1am but only if we're more than 15m away from it, otherwise run tomorrow */

  time_t t = time(NULL);
  time_t next_run = ((t / 86400) * 86400 + 86400) + 3600;
  if(next_run - t < 900)
    next_run+=86400;

  schedulerecurring(next_run,0,86400,cleanuptrusts,NULL);
}

void _init(void) {
  sstring *m;

  array_init(&trustgroupmods_a, sizeof(struct trustmodification));
  array_init(&trusthostmods_a, sizeof(struct trustmodification));
  setupmods();

  m = getconfigitem("trusts", "master");
  if(!m || (atoi(m->content) != 1)) {
    Error("trusts_management", ERR_ERROR, "Not a master server, not loaded.");
    return;
  }

  loaded = 1;

  registerhook(HOOK_TRUSTS_DB_LOADED, registercommands);
  registerhook(HOOK_TRUSTS_DB_LOADED, schedulecleanup);
  registerhook(HOOK_TRUSTS_DB_CLOSED, deregistercommands);

  if(trustsdbloaded)
    registercommands(0, NULL);
}

void _fini(void) {
  array_free(&trustgroupmods_a);
  array_free(&trusthostmods_a);

  if(!loaded)
    return;

  deregisterhook(HOOK_TRUSTS_DB_LOADED, registercommands);
  deregisterhook(HOOK_TRUSTS_DB_CLOSED, deregistercommands);

  deregistercommands(0, NULL);

  deleteallschedules(cleanuptrusts);
}
