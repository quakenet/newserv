/* regexgline.c */

/* TODO:

   FUTURE: natural (sort of) language parsing
           ^^ CIDR
           
   PPA:    if multiple users match the same user@host or *@host it'll send multiple glines?!
*/

#include "regexgline.h"
#include "../lib/version.h"
#include "../dbapi/dbapi.h"
#include "../lib/stringbuf.h"
#include "../core/hooks.h"
#include "../server/server.h"
#include "../lib/strlfunc.h"
#include <stdint.h>

#define INSTANT_IDENT_GLINE  1
#define INSTANT_HOST_GLINE   2
#define INSTANT_KILL         3
#define DELAYED_IDENT_GLINE  4
#define DELAYED_HOST_GLINE   5
#define DELAYED_KILL         6

#define RESERVED_NICK_GLINE_DURATION 3600 /* 1h */

MODULE_VERSION("1.44");

typedef struct rg_glinenode {
  nick *np;
  struct rg_struct *reason;
  short punish;
  struct rg_glinenode *next;
} rg_glinenode;

typedef struct rg_glinelist {
  struct rg_glinenode *start;
  struct rg_glinenode *end;
} rg_glinelist;

typedef struct rg_delay {
  schedule *sch;
  nick *np;
  struct rg_struct *reason;
  short punish;
  struct rg_delay *next;
} rg_delay;

#define GLINE_HEADER " ID       Expires              Set by          Class    Type  Last seen (ago)      Hits(p) Hits    Reason"

rg_delay *rg_delays;

void rg_setdelay(nick *np, struct rg_struct *reason, short punish);
void rg_deletedelay(rg_delay *delay);
void rg_dodelay(void *arg);

void rg_dogline(struct rg_glinelist *gll, nick *np, struct rg_struct *rp, char *matched);
void rg_flush_schedule(void *arg);

static char *gvhost(nick *np);
typedef void (scannick_fn)(struct rg_struct *, nick *, char *, void *);
static void rg_scannick(nick *np, scannick_fn *fn, void *arg);
static void rg_gline_match(struct rg_struct *rp, nick *np, char *hostname, void *arg);

static DBModuleIdentifier dbid;
static unsigned long highestid = 0;
static int attached = 0, started = 0;

static unsigned int getrgmarker(void);

#define RESERVED_NICK_CLASS "reservednick"
/* shadowserver only reports classes[0] */
static const char *classes[] = { "drone", "proxy", "spam", "other", RESERVED_NICK_CLASS, (char *)0 };

void rg_initglinelist(struct rg_glinelist *gll);
void rg_flushglines(struct rg_glinelist *gll);

void _init(void) {
  sstring *max_casualties, *max_spew, *expiry_time, *max_per_gline;
  
  max_casualties = getcopyconfigitem("regexgline", "maxcasualties", RGStringise(RG_MAX_CASUALTIES_DEFAULT), 8);
  if(!protectedatoi(max_casualties->content, &rg_max_casualties))
    rg_max_casualties = RG_MAX_CASUALTIES_DEFAULT;
  
  freesstring(max_casualties);

  max_spew = getcopyconfigitem("regexgline", "maxspew", RGStringise(RG_MAX_SPEW_DEFAULT), 8);
  if(!protectedatoi(max_spew->content, &rg_max_spew))
    rg_max_spew = RG_MAX_SPEW_DEFAULT;
  
  freesstring(max_spew);
  
  expiry_time = getcopyconfigitem("regexgline", "expirytime", RGStringise(RG_EXPIRY_TIME_DEFAULT), 8);
  if(!protectedatoi(expiry_time->content, &rg_expiry_time))
    rg_expiry_time = RG_EXPIRY_TIME_DEFAULT;

  freesstring(expiry_time);
  
  max_per_gline = getcopyconfigitem("regexgline", "maxpergline", RGStringise(RG_MAX_PER_GLINE_DEFAULT), 8);
  if(!protectedatoi(max_per_gline->content, &rg_max_per_gline))
    rg_max_per_gline = RG_MAX_PER_GLINE_DEFAULT;

  freesstring(max_per_gline);
  
  rg_delays = NULL;

  if(dbconnected()) {
    attached = 1;
    dbid = dbgetid();
    rg_dbload();
  } else {
    Error("regexgline", ERR_STOP, "Could not connect to database.");
  }
}

static void rg_count_match(struct rg_struct *rp, nick *np, char *hostname, void *arg) {
  void **varg = (void **)arg;
  int *count = (int *)varg[0];

  (*count)++;
}

static void rg_gline_reply_match(struct rg_struct *rp, nick *np, char *hostname, void *arg) {
  void **varg = (void **)arg;

  rg_count_match(rp, np, hostname, arg);
  rg_gline_match(rp, np, hostname, varg[1]);
}

int rg_rescan(void *source, int cargc, char **cargv) {
  int gline = 0;
  int j;
  nick *np = (nick *)source, *tnp;
  void *arg[2];
  int count = 0;
  struct rg_glinelist gll;
  scannick_fn *fn;

  if(cargc > 0)
    gline = !strcmp(cargv[0], "-g");

  arg[0] = &count;

  if(gline == 0) {
    fn = rg_count_match;
  } else {
    controlreply(np, "G-line mode activated.");

    rg_initglinelist(&gll);
    arg[1] = &gll;

    fn = rg_gline_reply_match;
  }

  controlreply(np, "Beginning scan, this may take a while...");

  for(j=0;j<NICKHASHSIZE;j++)
    for(tnp=nicktable[j];tnp;tnp=tnp->next)
      rg_scannick(tnp, fn, arg);

  controlreply(np, "Scan completed, %d hits.", count);

  if(gline)
    rg_flushglines(&gll);

  return CMD_OK;
}

void _fini(void) {
  struct rg_struct *gp = rg_list, *oldgp;
  rg_delay *delay, *delaynext;
  
  if(started) {
    deregisterhook(HOOK_NICK_NEWNICK, &rg_nick);
    deregisterhook(HOOK_NICK_RENAME, &rg_rename);
    deregisterhook(HOOK_NICK_LOSTNICK, &rg_lostnick);
    deregistercontrolcmd("regexspew", rg_spew);
    deregistercontrolcmd("regexglist", rg_glist);
    deregistercontrolcmd("regexdelgline", rg_delgline);
    deregistercontrolcmd("regexgline", rg_gline);
    deregistercontrolcmd("regexidlookup", rg_idlist);
    deregistercontrolcmd("regexrescan", rg_rescan);
  }

  if(rg_delays) {
    for(delay=rg_delays;delay;delay=delaynext) {
      delaynext=delay->next;
      deleteschedule(delay->sch, rg_dodelay, delay);
      free(delay);
    }
  }

  if(rg_schedule) {
    deleteschedule(rg_schedule, &rg_checkexpiry, NULL);
    rg_schedule = NULL;
  }

  deleteallschedules(rg_flush_schedule);
  rg_flush_schedule(NULL);

  for(gp=rg_list;gp;) {
    oldgp = gp;
    gp = gp->next;
    rg_freestruct(oldgp);
  }

  if(attached) {
    dbdetach("regexgline");
    dbfreeid(dbid);
  }
}

static int ignorable_nick(nick *np) {
  if(IsOper(np) || IsService(np) || IsXOper(np) || SIsService(&serverlist[homeserver(np->numeric)]))
    return 1;
  return 0;
}

void rg_checkexpiry(void *arg) {
  struct rg_struct *rp = rg_list, *lp = NULL;
  time_t current = time(NULL);
  
  while(rp) {
    if (current >= rp->expires) {
      dbquery("DELETE FROM regexglines WHERE id = %d", rp->id);
      if (lp) {
        lp->next = rp->next;
        rg_freestruct(rp);
        rp = lp->next;
      } else {
        rg_list = rp->next;
        rg_freestruct(rp);
        rp = rg_list;
      }
    } else {
      lp = rp;
      rp = rp->next;
    }
  }
}

void rg_setdelay(nick *np, rg_struct *reason, short punish) {
  rg_delay *delay;
  delay = (rg_delay *)malloc(sizeof(rg_delay));
  
  /* Just incase */
  if(!delay) {
    killuser(NULL, np, "%s (ID: %08lx)", reason->reason->content, reason->glineid);
    return;
  }
  
  delay->np = np;
  delay->reason = reason;
  delay->punish = punish;
  delay->next = rg_delays;
  rg_delays = delay;
  
  delay->sch = scheduleoneshot(time(NULL) + (RG_MINIMUM_DELAY_TIME + (rand() % RG_MAXIMUM_RAND_TIME)), rg_dodelay, delay);
}

static void rg_shadowserver(nick *np, struct rg_struct *reason, int type) {
  char buf[1024];

  if(reason->class != classes[0]) /* drone */
    return;

  snprintf(buf, sizeof(buf), "regex-ban %lu %s!%s@%s %s %s", time(NULL), np->nick, np->ident, np->host->name->content, reason->mask->content, serverlist[homeserver(np->numeric)].name->content);

  triggerhook(HOOK_SHADOW_SERVER, (void *)buf);
}

void rg_deletedelay(rg_delay *delay) {
  rg_delay *temp, *prev;
  prev = NULL;
  for (temp=rg_delays;temp;temp=temp->next) {
    if (temp==delay) {
      if (temp==rg_delays)
        rg_delays = temp->next;
      else
        prev->next = temp->next;
      
      free(temp);
      return;
    }
    
    prev = temp;
  }
}

void rg_dodelay(void *arg) {
  rg_delay *delay = (rg_delay *)arg;
  char hostname[RG_MASKLEN];
  int hostlen, usercount = 0;
  
  /* User or regex gline no longer exists */
  if((!delay->np) || (!delay->reason)) {
    rg_deletedelay(delay);
    return;
  }
  
  hostlen = RGBuildHostname(hostname, delay->np);
  
  /* User has wisely changed nicknames */
  if(pcre_exec(delay->reason->regex, delay->reason->hint, hostname, hostlen, 0, 0, NULL, 0) < 0) {
    rg_deletedelay(delay);
    return;
  }
  
  if (delay->reason->type == DELAYED_HOST_GLINE) {
    usercount = delay->np->host->clonecount;
    snprintf(hostname, sizeof(hostname), "*@%s", IPtostr(delay->np->p_ipaddr));
  }
  
  if((delay->reason->type == DELAYED_IDENT_GLINE) || (usercount > rg_max_per_gline)) {
    nick *tnp;

    for(usercount=0,tnp=delay->np->host->nicks;tnp;tnp=tnp->nextbyhost)
      if(!ircd_strcmp(delay->np->ident, tnp->ident))
        usercount++;

    snprintf(hostname, sizeof(hostname), "%s@%s", delay->np->ident, IPtostr(delay->np->p_ipaddr));
  }
  
  if ((delay->reason->type == DELAYED_KILL) || (usercount > rg_max_per_gline)) {
    controlwall(NO_OPER, NL_HITS, "%s matched delayed kill regex %08lx (class: %s)", gvhost(delay->np), delay->reason->glineid, delay->reason->class);

    rg_shadowserver(delay->np, delay->reason, DELAYED_KILL);
    killuser(NULL, delay->np, "%s (ID: %08lx)", delay->reason->reason->content, delay->reason->glineid);
    return;
  }
  
  if (delay->reason->type == DELAYED_IDENT_GLINE) {
    controlwall(NO_OPER, NL_HITS, "%s matched delayed user@host gline regex %08lx (class: %s, hit %d user%s)", gvhost(delay->np), delay->reason->glineid, delay->reason->class, usercount, (usercount!=1)?"s":"");
  } else if (delay->reason->type == DELAYED_HOST_GLINE) {
    controlwall(NO_OPER, NL_HITS, "%s matched delayed *@host gline regex %08lx (class: %s, hit %d user%s)", gvhost(delay->np), delay->reason->glineid, delay->reason->class, usercount, (usercount!=1)?"s":"");
  } else {
    return;
  }
  
  rg_shadowserver(delay->np, delay->reason, delay->reason->type);
  irc_send("%s GL * +%s %d %jd :AUTO: %s (ID: %08lx)\r\n", mynumeric->content, hostname, rg_expiry_time, (intmax_t)time(NULL), delay->reason->reason->content, delay->reason->glineid);
  rg_deletedelay(delay);
}

void rg_initglinelist(struct rg_glinelist *gll) {
  gll->start = NULL;
  gll->end = NULL;
}

void rg_flushglines(struct rg_glinelist *gll) {
  struct rg_glinenode *nn, *pn;
  for(nn=gll->start;nn;nn=pn) {
    pn = nn->next;
    if(nn->punish == INSTANT_KILL) {
      controlwall(NO_OPER, NL_HITS, "%s matched kill regex %08lx (class: %s)", gvhost(nn->np), nn->reason->glineid, nn->reason->class);

      rg_shadowserver(nn->np, nn->reason, nn->punish);
      killuser(NULL, nn->np, "%s (ID: %08lx)", nn->reason->reason->content, nn->reason->glineid);
    } else if ((nn->punish == DELAYED_IDENT_GLINE) || (nn->punish == DELAYED_HOST_GLINE) || (nn->punish == DELAYED_KILL)) {
      rg_setdelay(nn->np, nn->reason, nn->punish);
    }
    free(nn);
  }

  rg_initglinelist(gll);
}

static void dbloaddata(DBConn *dbconn, void *arg) {
  DBResult *dbres = dbgetresult(dbconn);

  if(!dbquerysuccessful(dbres)) {
    Error("chanserv", ERR_ERROR, "Error loading DB");
    return;
  }

  if (dbnumfields(dbres) != 9) {
    Error("regexgline", ERR_ERROR, "DB format error");
    return;
  }

  while(dbfetchrow(dbres)) {
    unsigned long id, hitssaved;
    time_t lastseen;
    char *gline, *setby, *reason, *expires, *type, *class;

    id = strtoul(dbgetvalue(dbres, 0), NULL, 10);
    if(id > highestid)
      highestid = id;
    
    gline = dbgetvalue(dbres, 1);
    setby = dbgetvalue(dbres, 2);
    reason = dbgetvalue(dbres, 3);
    expires = dbgetvalue(dbres, 4);
    type = dbgetvalue(dbres, 5);
    class = dbgetvalue(dbres, 6);

    lastseen = strtoul(dbgetvalue(dbres, 7), NULL, 10);
    hitssaved = strtoul(dbgetvalue(dbres, 8), NULL, 10);

    if (!rg_newsstruct(id, gline, setby, reason, expires, type, 0, class, lastseen, hitssaved))
      dbquery("DELETE FROM regexgline.glines WHERE id = %lu", id);
  }

  dbclear(dbres);
}

static void dbloadfini(DBConn *dbconn, void *arg) {
  started = 1;
  StringBuf b;
  const char **p;
  char helpbuf[8192 * 2], allclasses[8192];

  sbinit(&b, (char *)allclasses, sizeof(allclasses));
  for(p=classes;*p;p++) {
    sbaddstr(&b, (char *)*p);
    sbaddchar(&b, ' ');
  }
  sbterminate(&b);

  snprintf(helpbuf, sizeof(helpbuf),   
                         "Usage: regexgline <regex> <duration> <type> <class> <reason>\n"
                         "Adds a new regular expression pattern.\n"
                         "Duration is represented as 3d, 3M etc.\n"
                         "Class is one of the following: %s\n"
                         "Type is an integer which represents the following:\n"
                         "1 - Instant USER@IP GLINE (igu)\n"
                         "2 - Instant *@IP GLINE (igh)\n"
                         "3 - Instant KILL (ik)\n"
                         "4 - Delayed USER@IP GLINE (dgu)\n"
                         "5 - Delayed *@IP GLINE (dgh)\n"
                         "6 - Delayed KILL (dk)\n"
                         "Note that some classes may have additional side effects (e.g. 'reservednick' also sets nick style glines).",
                         allclasses);

  registercontrolhelpcmd("regexgline", NO_OPER, 5, &rg_gline, helpbuf);
  registercontrolhelpcmd("regexdelgline", NO_OPER, 1, &rg_delgline, "Usage: regexdelgline <pattern>\nDeletes a regular expression pattern.");
  registercontrolhelpcmd("regexglist", NO_OPER, 1, &rg_glist, "Usage: regexglist <pattern>\nLists regular expression patterns.");
  registercontrolhelpcmd("regexspew", NO_OPER, 1, &rg_spew, "Usage: regexspew <pattern>\nLists users currently on the network which match the given pattern.");
  registercontrolhelpcmd("regexidlookup", NO_OPER, 1, &rg_idlist, "Usage: regexidlookup <id>\nFinds a regular expression pattern by it's ID number.");
  registercontrolhelpcmd("regexrescan", NO_OPER, 1, &rg_rescan, "Usage: regexrescan ?-g?\nRescans the net for missed clients, optionally glining matches (used for debugging).");

  registerhook(HOOK_NICK_NEWNICK, &rg_nick);
  registerhook(HOOK_NICK_RENAME, &rg_rename);
  registerhook(HOOK_NICK_LOSTNICK, &rg_lostnick);
  rg_startup();

  rg_schedule = schedulerecurring(time(NULL) + 1, 0, 1, rg_checkexpiry, NULL);
  schedulerecurring(time(NULL) + 60, 0, 60, rg_flush_schedule, NULL);
}

void rg_dbload(void) {
  dbattach("regexgline");
  dbcreatequery("CREATE TABLE regexgline.glines (id INT NOT NULL PRIMARY KEY, gline TEXT NOT NULL, setby VARCHAR(%d) NOT NULL, reason VARCHAR(%d) NOT NULL, expires INT NOT NULL, type INT NOT NULL DEFAULT 1, class TEXT NOT NULL, lastseen INT DEFAULT 0, hits INT DEFAULT 0)", ACCOUNTLEN, RG_REASON_MAX);
  dbcreatequery("CREATE TABLE regexgline.clog (host VARCHAR(%d) NOT NULL, account VARCHAR(%d) NOT NULL, event TEXT NOT NULL, arg TEXT NOT NULL, ts TIMESTAMP)", RG_MASKLEN - 1, ACCOUNTLEN);
  dbcreatequery("CREATE TABLE regexgline.glog (glineid INT NOT NULL, ts TIMESTAMP, nickname VARCHAR(%d) NOT NULL, username VARCHAR(%d) NOT NULL, hostname VARCHAR(%d) NOT NULL, realname VARCHAR(%d))", NICKLEN, USERLEN, HOSTLEN, REALLEN);

  dbloadtable("regexgline.glines", NULL, dbloaddata, dbloadfini);
}

static void rg_scannick(nick *np, scannick_fn *fn, void *arg) {
  struct rg_struct *rp;
  char hostname[RG_MASKLEN];
  int hostlen;

  if(ignorable_nick(np))
    return;

  hostlen = RGBuildHostname(hostname, np);

  for(rp=rg_list;rp;rp=rp->next) {
    if(pcre_exec(rp->regex, rp->hint, hostname, hostlen, 0, 0, NULL, 0) >= 0) {
      fn(rp, np, hostname, arg);
      break;
    }
  }
}

static void rg_gline_match(struct rg_struct *rp, nick *np, char *hostname, void *arg) {
  struct rg_glinelist *gll = (struct rg_glinelist *)arg;

  rg_dogline(gll, np, rp, hostname);
}

void rg_rename(int hooknum, void *arg) {
  void **harg = (void **)arg;
  rg_nick(hooknum, harg[0]);
}

void rg_nick(int hooknum, void *arg) {
  nick *np = (nick *)arg;
  struct rg_glinelist gll;

  rg_initglinelist(&gll);

  rg_scannick(np, rg_gline_match, &gll);

  rg_flushglines(&gll);
}

void rg_lostnick(int hooknum, void *arg) {
  nick *np = (nick *)arg;
  rg_delay *delay;
  
  /* Cleanup the delays */
  for(delay=rg_delays;delay;delay=delay->next)
    if(delay->np==np)
      delay->np = NULL;
}

int rg_gline(void *source, int cargc, char **cargv) {
  nick *np = (nick *)source, *tnp;
  time_t realexpiry;
  const char *expirybuf;
  int expiry, count, j, hostlen;
  struct rg_struct *rp;
  struct rg_glinelist gll;
  const char **p;
  
  char eemask[RG_QUERY_BUF_SIZE], eesetby[RG_QUERY_BUF_SIZE], eereason[RG_QUERY_BUF_SIZE], eeclass[RG_QUERY_BUF_SIZE];
  char hostname[RG_MASKLEN], *class, *reason, *regex, type;

  if(cargc < 5)
    return CMD_USAGE;

  type = cargv[2][0];
  if ((strlen(cargv[2]) != 1) || ((type != '1') && (type != '2') && (type != '3') && (type != '4') && (type != '5') && (type != '6'))) {
    controlreply(np, "Invalid type specified!");
    return CMD_USAGE;
  }

  regex = cargv[0];
  class = cargv[3];
  reason = cargv[4];

  for(p=classes;*p;p++)
    if(!strcasecmp(class, *p))
      break;

  if(!*p) {
    controlreply(np, "Bad class supplied.");
    return CMD_USAGE;
  }

  if (!(expiry = durationtolong(cargv[1]))) {
    controlreply(np, "Invalid duration specified!");
    return CMD_USAGE;
  }
  
  for(rp=rg_list;rp;rp=rp->next) {
    if (RGMasksEqual(rp->mask->content, regex)) {
      controlreply(np, "That regexgline already exists!");
      return CMD_ERROR;
    }
  }
  
  if (rg_sanitycheck(regex, &count)) {
    controlreply(np, "Error in expression.");
    return CMD_ERROR;
  } else if (count < 0) {
    controlreply(np, "That expression would hit too many users (%d)!", -count);  
    return CMD_ERROR;
  }
  
  realexpiry = expiry + time(NULL);
  
  dbescapestring(eemask, regex, strlen(regex));
  dbescapestring(eesetby, np->nick, strlen(np->nick));
  dbescapestring(eeclass, class, strlen(class));
  dbescapestring(eereason, reason, strlen(reason));
  
  highestid = highestid + 1;
  dbquery("INSERT INTO regexgline.glines (id, gline, setby, reason, expires, type, class, lastseen, hits) VALUES (%lu, '%s', '%s', '%s', %lu, %c, '%s', 0, 0)", highestid, eemask, eesetby, eereason, realexpiry, type, eeclass);
  rp = rg_newsstruct(highestid, regex, np->nick, reason, "", cargv[2], realexpiry, class, 0, 0);
  
  rg_initglinelist(&gll);

  for(j=0;j<NICKHASHSIZE;j++) {
    for(tnp=nicktable[j];tnp;tnp=tnp->next) {
      if(ignorable_nick(tnp))
        continue;

      hostlen = RGBuildHostname(hostname, tnp);
      if(pcre_exec(rp->regex, rp->hint, hostname, hostlen, 0, 0, NULL, 0) >= 0)
        rg_dogline(&gll, tnp, rp, hostname);
    }
  }
  
  rg_flushglines(&gll);

  expirybuf = longtoduration(expiry, 0);

  rg_logevent(np, "regexgline", "%s %d %d %s %s", regex, expiry, count, class, reason);
  controlreply(np, "Added regexgline: %s (class: %s, expires in: %s, hit %d user%s): %s", regex, class, expirybuf, count, (count!=1)?"s":"", reason);
  /* If we are using NO, can we safely assume the user is authed here and use ->authname? */
  controlwall(NO_OPER, NL_GLINES, "%s!%s@%s/%s added regexgline: %s (class: %s, expires in: %s, hit %d user%s): %s", np->nick, np->ident, np->host->name->content, np->authname, regex, class, expirybuf, count, (count!=1)?"s":"", reason);

  return CMD_OK;
}

int rg_sanitycheck(char *mask, int *count) {
  const char *error;
  char hostname[RG_MASKLEN];
  int erroroffset, hostlen, j, masklen = strlen(mask);
  pcre *regex;
  pcre_extra *hint;
  nick *np;

  if((masklen < RG_MIN_MASK_LEN) || (masklen > RG_REGEXGLINE_MAX))
    return 1;
  
  if(!(regex = pcre_compile(mask, RG_PCREFLAGS, &error, &erroroffset, NULL))) {
    Error("regexgline", ERR_WARNING, "Error compiling expression %s at offset %d: %s", mask, erroroffset, error);
    return 2;
  } else {
    hint = pcre_study(regex, 0, &error);
    if(error) {
      Error("regexgline", ERR_WARNING, "Error studying expression %s: %s", mask, error);
      pcre_free(regex);
      return 3;
    }
  }

  *count = 0;
  for(j=0;j<NICKHASHSIZE;j++) {
    for(np=nicktable[j];np;np=np->next) {
     hostlen = RGBuildHostname(hostname, np);
      if(pcre_exec(regex, hint, hostname, hostlen, 0, 0, NULL, 0) >= 0) {
        (*count)++;
      }
    }
  }

  pcre_free(regex);
  if(hint)
    pcre_free(hint);
 
  if(*count >= rg_max_casualties)
    *count = -(*count);
  
  return 0;
}

int rg_delgline(void *source, int cargc, char **cargv) {
  nick *np = (nick *)source;
  rg_delay *delay;
  struct rg_struct *rp = rg_list, *last = NULL;
  int count = 0;
  
  if(cargc < 1)
    return CMD_USAGE;
  
  rg_logevent(np, "regexdelgline", "%s", cargv[0]);
  while(rp) {
    if(RGMasksEqual(rp->mask->content, cargv[0])) {
      count++;
      
      /* Cleanup the delays */
      for(delay=rg_delays;delay;delay=delay->next)
        if(delay->reason==rp)
          delay->reason = NULL;
      
      dbquery("DELETE FROM regexgline.glines WHERE id = %d", rp->id);
      if(last) {
        last->next = rp->next;
        rg_freestruct(rp);
        rp = last->next;
      } else {
        rg_list = rp->next;
        rg_freestruct(rp);
        rp = rg_list;
      }
    } else {
      last = rp;
      rp = rp->next;
    }
  }
  if (count > 0) {
    controlreply(np, "Deleted (matched: %d).", count);
    /* If we are using NO, can we safely assume the user is authed here and use ->authname? */
    controlwall(NO_OPER, NL_GLINES, "%s!%s@%s/%s removed regexgline: %s", np->nick, np->ident, np->host->name->content, np->authname, cargv[0]);
  } else {
    controlreply(np, "No glines matched: %s", cargv[0]);
  }
  return CMD_OK;
}

int rg_idlist(void *source, int cargc, char **cargv) {
  nick *np = (nick *)source;

  if(cargc < 1) {
    return CMD_USAGE;
  } else if (strlen(cargv[0]) != 8) {
    controlreply(np, "Invalid gline id!");
    return CMD_ERROR;
  } else {
    struct rg_struct *rp;
    unsigned long id = 0;
    int i, longest = 0;
    unsigned int m;

    for(i=0;i<8;i++) {
      if(0xff == rc_hexlookup[(int)cargv[0][i]]) {
        controlreply(np, "Invalid gline id!");
        return CMD_ERROR;
      } else {
        id = (id << 4) | rc_hexlookup[(int)cargv[0][i]];
      }
    }

    m = getrgmarker();
    controlreply(np, GLINE_HEADER);
    for(rp=rg_list;rp;rp=rp->next) {
      if(id == rp->glineid) {
        rp->marker = m;
        if(rp->mask->length > longest)
          longest = rp->mask->length;
      }
    }

    for(rp=rg_list;rp;rp=rp->next)
      if(rp->marker == m)
        rg_displaygline(np, rp, longest);
    controlreply(np, "Done.");

    return CMD_OK;
  }
}

int rg_glist(void *source, int cargc, char **cargv) {
  nick *np = (nick *)source;
  struct rg_struct *rp;
  int longest = 0;

  if(cargc) {
    int erroroffset;
    pcre *regex;
    pcre_extra *hint;
    const char *error;    
    unsigned int m;

    if(!(regex = pcre_compile(cargv[0], RG_PCREFLAGS, &error, &erroroffset, NULL))) {
      controlreply(np, "Error compiling expression %s at offset %d: %s", cargv[0], erroroffset, error);
      return CMD_ERROR;
    } else {
      hint = pcre_study(regex, 0, &error);
      if(error) {
        controlreply(np, "Error studying expression %s: %s", cargv[0], error);
        pcre_free(regex);
        return CMD_ERROR;
      }
    }

    m = getrgmarker();    
    rg_logevent(np, "regexglist", "%s", cargv[0]);
    controlreply(np, GLINE_HEADER);
    for(rp=rg_list;rp;rp=rp->next) {
      if(pcre_exec(regex, hint, rp->mask->content, rp->mask->length, 0, 0, NULL, 0) >= 0) {
        rp->marker = m;
        if(rp->mask->length > longest)
          longest = rp->mask->length;
      }
    }

    for(rp=rg_list;rp;rp=rp->next)
      if(rp->marker == m)
        rg_displaygline(np, rp, longest);

    pcre_free(regex);
    if(hint)
      pcre_free(hint);
    
  } else {
    rg_logevent(np, "regexglist", "%s", "");
    controlreply(np, GLINE_HEADER);
    for(rp=rg_list;rp;rp=rp->next)
      if(rp->mask->length > longest)
        longest = rp->mask->length;

    for(rp=rg_list;rp;rp=rp->next)
      rg_displaygline(np, rp, longest);
  }

  controlreply(np, "Done.");
  return CMD_OK;
}

char *displaytype(int type) {
  char *ctype;
  static char ctypebuf[10];

  switch(type) {
    case 1:
      ctype = "igu";
      break;
    case 2:
      ctype = "igh";
      break;
    case 3:
      ctype = "ik";
      break;
    case 4:
      ctype = "dgu";
      break;
    case 5:
      ctype = "dgh";
      break;
    case 6:
      ctype = "dk";
      break;
    default:
      ctype = "??";
  }

  snprintf(ctypebuf, sizeof(ctype), "%1d:%s", type, ctype);
  return ctypebuf;
}

char *getsep(int longest) {
  static int lastlongest = -1;
  static char lenbuf[1024];

  longest = 125;
/*
  if(longest < 100)
    longest = 100;

  if(longest >= sizeof(lenbuf) - 20)
    longest = sizeof(lenbuf) - 20;
*/
  longest+=4;
  if(lastlongest == -1) {
    int i;

    for(i=0;i<sizeof(lenbuf)-1;i++)
      lenbuf[i] = '-';
    lenbuf[sizeof(lenbuf)-1] = '\0';
    lastlongest = 0;
  }

  if(lastlongest != longest) {
    lenbuf[lastlongest] = '-';
    lenbuf[longest] = '\0';
    lastlongest = longest;
  }

  return lenbuf;
}

void rg_displaygline(nick *np, struct rg_struct *rp, int longest) { /* could be a macro? I'll assume the C compiler inlines it */
  char *sep = getsep(longest);
/*   12345678 12345678901234567890 123456789012345 12345678 12345 12345678901234567890 1234567 1234567 123456
     ID       Expires              Set by          Class    Type  Last seen (ago)      Hits(s) Hits    Reason
*/

  char d[512];
  time_t t = time(NULL);

  if(rp->lastseen == 0) {
    strlcpy(d, "(never)", sizeof(d));
  } else {
    strlcpy(d, longtoduration(t - rp->lastseen, 2), sizeof(d));
  }

  controlreply(np, "%s", rp->mask->content);
  controlreply(np, " %08lx %-20s %-15s %-8s %-5s %-20s %-7lu %-7lu %s", rp->glineid, longtoduration(rp->expires - t, 2), rp->setby->content, rp->class, displaytype(rp->type), d, rp->hitssaved, rp->hits, rp->reason->content);
  controlreply(np, "%s", sep);
}

int rg_spew(void *source, int cargc, char **cargv) {
  nick *np = (nick *)source, *tnp;
  int counter = 0, erroroffset, hostlen, j;
  pcre *regex;
  pcre_extra *hint;
  const char *error;
  char hostname[RG_MASKLEN];
  int ovector[30];
  int pcreret;

  if(cargc < 1)
    return CMD_USAGE;
  
  if(!(regex = pcre_compile(cargv[0], RG_PCREFLAGS, &error, &erroroffset, NULL))) {
    controlreply(np, "Error compiling expression %s at offset %d: %s", cargv[0], erroroffset, error);
    return CMD_ERROR;
  } else {
    hint = pcre_study(regex, 0, &error);
    if(error) {
      controlreply(np, "Error studying expression %s: %s", cargv[0], error);
      pcre_free(regex);
      return CMD_ERROR;
    }
  }
  
  rg_logevent(np, "regexspew", "%s", cargv[0]);
  
  for(j=0;j<NICKHASHSIZE;j++) {
    for(tnp=nicktable[j];tnp;tnp=tnp->next) {
      hostlen = RGBuildHostname(hostname, tnp);
      pcreret = pcre_exec(regex, hint, hostname, hostlen, 0, 0, ovector, sizeof(ovector) / sizeof(int));
      if(pcreret >= 0) {
        if(counter == rg_max_spew) {
          controlreply(np, "Reached maximum spew count (%d) - aborting display.", rg_max_spew);
        } else if (counter < rg_max_spew) {
          /* 15 should be number of bolds */
          char boldbuf[RG_MASKLEN + 15], *tp, *fp, *realname = NULL;
          int boldon = 0;
          for(tp=hostname,fp=boldbuf;*tp;) {
            if(tp - hostname == ovector[0]) {
              *fp++ = '\002';
              boldon = 1;
            }
            if(tp - hostname == ovector[1]) {
              *fp++ = '\002';
              boldon = 0;
            }
            if(*tp == '\r') {
              if(boldon)
                *fp++ = '\002';
              *fp++ = '\0';
              realname = fp;
              if(boldon)
                *fp++ = '\002';
              tp++;
            } else {
              *fp++ = *tp++;
            }
          }
          if(boldon)
            *fp++ = '\002';
          *fp++ = '\0';
          controlreply(np, "%s (%s) (%dc)", boldbuf, realname, tnp->channels->cursi);
        }
        counter++;
      }
    }
  }
  controlreply(np, "Done - %d matches.", counter);
  
  pcre_free(regex);
  if(hint)
    pcre_free(hint);

  return CMD_OK;
}

void rg_startup(void) {
  int j;
  nick *np;
  struct rg_glinelist gll;

  rg_initglinelist(&gll);

  for(j=0;j<NICKHASHSIZE;j++)
    for(np=nicktable[j];np;np=np->next)
      rg_scannick(np, rg_gline_match, &gll);
  
  rg_flushglines(&gll);
}

void rg_freestruct(struct rg_struct *rp) {
  freesstring(rp->mask);
  freesstring(rp->setby);
  freesstring(rp->reason);
  pcre_free(rp->regex);
  if(rp->hint)
    pcre_free(rp->hint);
  free(rp);
}

struct rg_struct *rg_newstruct(time_t expires) {
  struct rg_struct *rp;
  
  if (time(NULL) >= expires)
    return NULL;
  
  rp = (struct rg_struct*)malloc(sizeof(struct rg_struct));
  if(rp) {
    struct rg_struct *tp, *lp;

    memset(rp, 0, sizeof(rg_struct));
    rp->expires = expires;

    for(lp=NULL,tp=rg_list;tp;lp=tp,tp=tp->next) {
      if (expires <= tp->expires) { /* <= possible, slight speed increase */
        rp->next = tp;
        if (lp) {
          lp->next = rp;
        } else {
          rg_list = rp;
        }
        break;
      }
    }
    if (!tp) {
      rp->next = NULL;
      if (lp) {
        lp->next = rp;
      } else {
        rg_list = rp;
      }        
    }
    
  }
  return rp;
}

struct rg_struct *rg_newsstruct(unsigned long id, char *mask, char *setby, char *reason, char *expires, char *type, time_t iexpires, char *class, time_t lastseen, unsigned int hitssaved) {
  struct rg_struct *newrow, *lp, *cp;
  time_t rexpires;
  char glineiddata[1024];
  const char **p;

  if (iexpires == 0) {
    int qexpires;
    if(!protectedatoi(expires, &qexpires))
      return NULL;
    rexpires = (time_t)qexpires;
  } else {
    rexpires = iexpires;
  }

  newrow = rg_newstruct(rexpires);
  
  if(newrow) {
    const char *error;
    int erroroffset;
    
    for(p=classes;*p;p++) {
      if(!strcasecmp(class, *p)) {
        newrow->class = *p;
        break;
      }
    }

    if(!*p)
      newrow->class = "unknown";

    if(!(newrow->regex = pcre_compile(mask, RG_PCREFLAGS, &error, &erroroffset, NULL))) {
      Error("regexgline", ERR_WARNING, "Error compiling expression %s at offset %d: %s", mask, erroroffset, error);
      goto dispose;
    } else {
      newrow->hint = pcre_study(newrow->regex, 0, &error);
      if(error) {
        Error("regexgline", ERR_WARNING, "Error studying expression %s: %s", mask, error);
        pcre_free(newrow->regex);
        goto dispose;
      }
    }
    
    newrow->id = id;
    newrow->hitssaved = hitssaved;
    newrow->lastseen = lastseen;

    newrow->mask = getsstring(mask, RG_REGEXGLINE_MAX);
    if(!newrow->mask) {
      Error("regexgline", ERR_WARNING, "Error allocating memory for mask!");
      goto dispose2;
    }

    newrow->setby = getsstring(setby, ACCOUNTLEN);
    if(!newrow->setby) {
      Error("regexgline", ERR_WARNING, "Error allocating memory for setby!");
      goto dispose2;
    }

    newrow->reason = getsstring(reason, RG_REASON_MAX);
    if(!newrow->reason) {
      Error("regexgline", ERR_WARNING, "Error allocating memory for reason!");
      goto dispose2;
    }

    if(!protectedatoi(type, &newrow->type))
      newrow->type = 0; /* just in case */

    snprintf(glineiddata, sizeof(glineiddata), "%s regexgline %s %s %s %d %d", mynumeric->content, mask, setby, reason, (int)iexpires, newrow->type);
    newrow->glineid = crc32(glineiddata);
  }
  
  return newrow;
  
  dispose2:
    if(newrow->mask)
      freesstring(newrow->mask);
    if(newrow->setby)
      freesstring(newrow->setby);
    if(newrow->reason)
      freesstring(newrow->reason);
    pcre_free(newrow->regex);
    if(newrow->hint)
      pcre_free(newrow->hint);    

  dispose:
    for(lp=NULL,cp=rg_list;cp;lp=cp,cp=cp->next) {
      if(newrow == cp) {
        if(lp) {
          lp->next = cp->next;
        } else {
          rg_list = cp->next;
        }
        free(newrow);
        break;
      }
    }
    return NULL;
}

int __rg_dogline(struct rg_glinelist *gll, nick *np, struct rg_struct *rp, char *matched) { /* PPA: if multiple users match the same user@host or *@host it'll send multiple glines?! */
  char hostname[RG_MASKLEN];
  int usercount = 0;
  int validdelay;

  rg_loggline(rp, np);

  if (rp->type == INSTANT_HOST_GLINE) {
    usercount = np->host->clonecount;
    snprintf(hostname, sizeof(hostname), "*@%s", IPtostr(np->p_ipaddr));
  }
  
  if ((rp->type == INSTANT_IDENT_GLINE) || (usercount > rg_max_per_gline)) {
    nick *tnp;

    for(usercount=0,tnp=np->host->nicks;tnp;tnp=tnp->nextbyhost)
      if(!ircd_strcmp(np->ident, tnp->ident))
        usercount++;

    snprintf(hostname, sizeof(hostname), "%s@%s", np->ident, IPtostr(np->p_ipaddr));
  }

  if(!strcmp(rp->class, RESERVED_NICK_CLASS))
    irc_send("%s GL * +%s!*@* %d %jd :AUTO: %s (ID: %08lx)\r\n", mynumeric->content, np->nick, RESERVED_NICK_GLINE_DURATION, (intmax_t)time(NULL), rp->reason->content, rp->glineid);

  validdelay = (rp->type == INSTANT_KILL) || (rp->type == DELAYED_IDENT_GLINE) || (rp->type == DELAYED_HOST_GLINE) || (rp->type == DELAYED_KILL);
  if (validdelay || (usercount > rg_max_per_gline)) {
    struct rg_glinenode *nn = (struct rg_glinenode *)malloc(sizeof(struct rg_glinenode));
    if(nn) {
      nn->next = NULL;
      if(gll->end) {
        gll->end->next = nn;
        gll->end = nn;
      } else {
        gll->start = nn;
        gll->end = nn;
      }

      nn->np = np;
      nn->reason = rp;
      if(!validdelay) {
        nn->punish = INSTANT_KILL;
      } else {
        nn->punish = rp->type;
      }
    }
    return usercount;
  }
  
  if (rp->type == INSTANT_IDENT_GLINE) {
    controlwall(NO_OPER, NL_HITS, "%s matched user@host gline regex %08lx (class: %s, hit %d user%s)", gvhost(np), rp->glineid, rp->class, usercount, (usercount!=1)?"s":"");
  } else if(rp->type == INSTANT_HOST_GLINE) {
    controlwall(NO_OPER, NL_HITS, "%s matched *@host gline regex %08lx (class: %s, hit %d user%s)", gvhost(np), rp->glineid, rp->class, usercount, (usercount!=1)?"s":"");
  } else {
    return 0;
  }
  
  rg_shadowserver(np, rp, rp->type);
  irc_send("%s GL * +%s %d %jd :AUTO: %s (ID: %08lx)\r\n", mynumeric->content, hostname, rg_expiry_time, (intmax_t)time(NULL), rp->reason->content, rp->glineid);
  return usercount;
}

static char *gvhost(nick *np) {
  static char buf[NICKLEN+1+USERLEN+1+HOSTLEN+1+ACCOUNTLEN+4+REALLEN+1+10];

  if(IsAccount(np)) {
    snprintf(buf, sizeof(buf), "%s!%s@%s/%s r(%s)", np->nick, np->ident, np->host->name->content, np->authname, np->realname->name->content);
  } else {
    snprintf(buf, sizeof(buf), "%s!%s@%s r(%s)", np->nick, np->ident, np->host->name->content, np->realname->name->content);
  }

  return buf;
}

static int floodprotection = 0;
static int lastfloodspam = 0;

void rg_dogline(struct rg_glinelist *gll, nick *np, struct rg_struct *rp, char *matched) {
  int t = time(NULL);

  if(t > floodprotection) {
    floodprotection = t;
  } else if((floodprotection - t) / 8 > RG_NETWORK_WIDE_MAX_GLINES_PER_8_SEC) {
    if(t > lastfloodspam + 3600) {
      channel *cp = findchannel("#twilightzone");
      if(cp)
        controlchanmsg(cp, "WARNING! REGEXGLINE DISABLED FOR AN HOUR DUE TO NETWORK WIDE LOOKING GLINE!: %d exceeded %d", (floodprotection - t) / 8, RG_NETWORK_WIDE_MAX_GLINES_PER_8_SEC);
      controlwall(NO_OPER, NL_MANAGEMENT, "WARNING! REGEXGLINE DISABLED FOR AN HOUR DUE TO NETWORK WIDE LOOKING GLINE!");
      lastfloodspam = t;
      floodprotection = t + RG_NETWORK_WIDE_MAX_GLINES_PER_8_SEC * 3600 * 8;
    }
    return;
  }

  floodprotection+=__rg_dogline(gll, np, rp, matched);
}

void rg_logevent(nick *np, char *event, char *details, ...) {
  char eeevent[RG_QUERY_BUF_SIZE], eedetails[RG_QUERY_BUF_SIZE], eemask[RG_QUERY_BUF_SIZE], eeaccount[RG_QUERY_BUF_SIZE];
  char buf[513], account[ACCOUNTLEN + 1], mask[RG_MASKLEN];
  int masklen;

  va_list va;

  if(details) {
    va_start(va, details);
    vsnprintf(buf, sizeof(buf), details, va);
    va_end(va);
  } else {
    buf[0] = '\0';
  }

  if(np) {
    if (IsAccount(np)) {
      strncpy(account, np->authname, sizeof(account) - 1);
      account[sizeof(account) - 1] = '\0';
    } else {
      account[0] = '\0';
    }
    masklen = RGBuildHostname(mask, np);
  } else {
    mask[0] = '\0';
    masklen = 0;
  }
  
  dbescapestring(eeevent, event, strlen(event));
  dbescapestring(eedetails, buf, strlen(buf));
  dbescapestring(eeaccount, account, strlen(account));
  dbescapestring(eemask, mask, masklen);
  
  dbquery("INSERT INTO regexgline.clog (host, account, event, arg, ts) VALUES ('%s', '%s', '%s', '%s', NOW())", eemask, eeaccount, eeevent, eedetails);
}

void rg_loggline(struct rg_struct *rg, nick *np) {
  char eenick[RG_QUERY_BUF_SIZE], eeuser[RG_QUERY_BUF_SIZE], eehost[RG_QUERY_BUF_SIZE], eereal[RG_QUERY_BUF_SIZE];

  rg->hits++;
  rg->hitssaved++;
  rg->lastseen = time(NULL);
  rg->dirty = 1;

  /* @paul: disabled */

  return;
  dbescapestring(eenick, np->nick, strlen(np->nick));
  dbescapestring(eeuser, np->ident, strlen(np->ident));
  dbescapestring(eehost, np->host->name->content, strlen(np->host->name->content));
  dbescapestring(eereal, np->realname->name->content, strlen(np->realname->name->content));

  dbquery("INSERT INTO regexgline.glog (glineid, nickname, username, hostname, realname, ts) VALUES (%d, '%s', '%s', '%s', '%s', NOW())", rg->id, eenick, eeuser, eehost, eereal);
}

static unsigned int getrgmarker(void) {
  static unsigned int marker = 0;

  marker++;
  if(!marker) {
    struct rg_struct *l;

    /* If we wrapped to zero, zap the marker on all hosts */
    for(l=rg_list;l;l=l->next)
      l->marker=0;
    marker++;
  }

  return marker;
}

void rg_flush_schedule(void *arg) {
  struct rg_struct *l;

  for(l=rg_list;l;l=l->next) {
    if(!l->dirty)
      continue;

    dbquery("UPDATE regexgline.glines SET lastseen = %jd, hits = %lu WHERE id = %d", (intmax_t)l->lastseen, l->hitssaved, l->id);

    l->dirty = 0;
  }
}

