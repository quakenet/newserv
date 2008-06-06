/* regexgline.c */

/* TODO:

   FUTURE: natural (sort of) language parsing
           ^^ CIDR
           
   PPA:    if multiple users match the same user@host or *@host it'll send multiple glines?!
*/

#include "regexgline.h"
#include "../lib/version.h"

MODULE_VERSION("");

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

rg_delay *rg_delays;

void rg_setdelay(nick *np, struct rg_struct *reason, short punish);
void rg_deletedelay(rg_delay *delay);
void rg_dodelay(void *arg);

void rg_dogline(struct rg_glinelist *gll, nick *np, struct rg_struct *rp, char *matched);

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

  if(!rg_dbconnect()) {
    rg_dbload();
    
    registercontrolhelpcmd("regexgline", NO_OPER, 4, &rg_gline,
                           "Usage: regexgline <regex> <duration> <type> <reason>\n"
                           "Adds a new regular expression pattern.\n"
                           "Duration is represented as 3d, 3M etc.\n"
                           "Type is an integer which represents the following:\n"
                           "1 - Instant USER@IP GLINE\n"
                           "2 - Instant *@IP GLINE\n"
                           "3 - Instant KILL\n"
                           "4 - Delayed USER@IP GLINE\n"
                           "5 - Delayed *@IP GLINE\n"
                           "6 - Delayed KILL");
    registercontrolhelpcmd("regexdelgline", NO_OPER, 1, &rg_delgline, "Usage: regexdelgline <pattern>\nDeletes a regular expression pattern.");
    registercontrolhelpcmd("regexglist", NO_OPER, 1, &rg_glist, "Usage: regexglist <pattern>\nLists regular expression patterns.");
    registercontrolhelpcmd("regexspew", NO_OPER, 1, &rg_spew, "Usage: regexspew <pattern>\nLists users currently on the network which match the given pattern.");
    registercontrolhelpcmd("regexidlookup", NO_OPER, 1, &rg_idlist, "Usage: regexidlookup <id>\nFinds a regular expression pattern by it's ID number.");
    
    registerhook(HOOK_NICK_NEWNICK, &rg_nick);
    registerhook(HOOK_NICK_RENAME, &rg_nick);
    registerhook(HOOK_NICK_LOSTNICK, &rg_lostnick);
    rg_startup();
    
    rg_schedule = schedulerecurring(time(NULL) + 1, 0, 1, rg_checkexpiry, NULL);
  }
}

void _fini(void) {
  struct rg_struct *gp = rg_list, *oldgp;
  rg_delay *delay, *delaynext;
  
  deregisterhook(HOOK_NICK_NEWNICK, &rg_nick);
  deregisterhook(HOOK_NICK_RENAME, &rg_nick);
  deregisterhook(HOOK_NICK_LOSTNICK, &rg_lostnick);
  deregistercontrolcmd("regexspew", rg_spew);
  deregistercontrolcmd("regexglist", rg_glist);
  deregistercontrolcmd("regexdelgline", rg_delgline);
  deregistercontrolcmd("regexgline", rg_gline);
  deregistercontrolcmd("regexidlookup", rg_idlist);
  
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
  
  for(gp=rg_list;gp;) {
    oldgp = gp;
    gp = gp->next;
    rg_freestruct(oldgp);
  }
  
  if(rg_sqlconnected)
    rg_sqldisconnect();
}

void rg_checkexpiry(void *arg) {
  struct rg_struct *rp = rg_list, *lp = NULL;
  time_t current = time(NULL);
  
  while(rp) {
    if (current >= rp->expires) {
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
  
  if (delay->reason->type == 5) {
    usercount = delay->np->host->clonecount;
    snprintf(hostname, sizeof(hostname), "*@%s", IPtostr(delay->np->p_ipaddr));
  }
  
  if((delay->reason->type == 4) || (usercount > rg_max_per_gline)) {
    nick *tnp;

    for(usercount=0,tnp=delay->np->host->nicks;tnp;tnp=tnp->nextbyhost)
      if(!ircd_strcmp(delay->np->ident, tnp->ident))
        usercount++;

    snprintf(hostname, sizeof(hostname), "%s@%s", delay->np->ident, IPtostr(delay->np->p_ipaddr));
  }
  
  if ((delay->reason->type == 6) || (usercount > rg_max_per_gline)) {
    if (IsAccount(delay->np)) {
      controlwall(NO_OPER, NL_HITS, "%s!%s@%s/%s matched delayed kill regex %08lx", delay->np->nick, delay->np->ident, delay->np->host->name->content, delay->np->authname, delay->reason->glineid);
    } else {
      controlwall(NO_OPER, NL_HITS, "%s!%s@%s matched delayed kill regex %08lx", delay->np->nick, delay->np->ident, delay->np->host->name->content, delay->reason->glineid);
    }
    killuser(NULL, delay->np, "%s (ID: %08lx)", delay->reason->reason->content, delay->reason->glineid);
    return;
  }
  
  if ((delay->reason->type <= 3) || (delay->reason->type >= 6))
    return;
  
  if (delay->reason->type == 4) {
    if (IsAccount(delay->np)) {
      controlwall(NO_OPER, NL_HITS, "%s!%s@%s/%s matched delayed user@host gline regex %08lx (hit %d user%s)", delay->np->nick, delay->np->ident, delay->np->host->name->content, delay->np->authname, delay->reason->glineid, usercount, (usercount!=1)?"s":"");
    } else {
      controlwall(NO_OPER, NL_HITS, "%s!%s@%s matched delayed user@host gline regex %08lx (hit %d user%s)", delay->np->nick, delay->np->ident, delay->np->host->name->content, delay->reason->glineid, usercount, (usercount!=1)?"s":"");
    }
  } else {
    if (IsAccount(delay->np)) {
      controlwall(NO_OPER, NL_HITS, "%s!%s@%s/%s matched delayed *@host gline regex %08lx (hit %d user%s)", delay->np->nick, delay->np->ident, delay->np->host->name->content, delay->np->authname, delay->reason->glineid, usercount, (usercount!=1)?"s":"");
    } else {
      controlwall(NO_OPER, NL_HITS, "%s!%s@%s matched delayed *@host gline regex %08lx (hit %d user%s)", delay->np->nick, delay->np->ident, delay->np->host->name->content, delay->reason->glineid, usercount, (usercount!=1)?"s":"");
    }
  }
  
  irc_send("%s GL * +%s %d %d :AUTO: %s (ID: %08lx)\r\n", mynumeric->content, hostname, rg_expiry_time, time(NULL), delay->reason->reason->content, delay->reason->glineid);
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
    if(nn->punish == 3) {
      if ( IsAccount(nn->np) ) {
        controlwall(NO_OPER, NL_HITS, "%s!%s@%s/%s matched kill regex %08lx", nn->np->nick, nn->np->ident, nn->np->host->name->content, nn->np->authname, nn->reason->glineid);
      } else {
        controlwall(NO_OPER, NL_HITS, "%s!%s@%s matched kill regex %08lx", nn->np->nick, nn->np->ident, nn->np->host->name->content, nn->reason->glineid);
      }
      
      killuser(NULL, nn->np, "%s (ID: %08lx)", nn->reason->reason->content, nn->reason->glineid);
    } else if ((nn->punish == 4) || (nn->punish == 5) || (nn->punish == 6)) {
      rg_setdelay(nn->np, nn->reason, nn->punish);
    }
    free(nn);
  }

  rg_initglinelist(gll);
}

int rg_dbconnect(void) {
  sstring *dbhost, *dbusername, *dbpassword, *dbdatabase, *dbport;
  
  dbhost = getcopyconfigitem("regexgline", "dbhost", "localhost", HOSTLEN);
  dbusername = getcopyconfigitem("regexgline", "dbusername", "regexgline", 20);
  dbpassword = getcopyconfigitem("regexgline", "dbpassword", "moo", 20);
  dbdatabase = getcopyconfigitem("regexgline", "dbdatabase", "regexgline", 20);
  dbport = getcopyconfigitem("regexgline", "dbport", "3306", 8);

  if(rg_sqlconnect(dbhost->content, dbusername->content, dbpassword->content, dbdatabase->content, strtol(dbport->content, NULL, 10))) {
    Error("regexgline", ERR_FATAL, "Cannot connect to database host!");
    return 1; /* PPA: splidge: do something here 8]! */
  } else {
    rg_sqlconnected = 1;
  }
  
  freesstring(dbhost);
  freesstring(dbusername);
  freesstring(dbpassword);
  freesstring(dbdatabase);
  freesstring(dbport);

  return 0;
}

int rg_dbload(void) {
  rg_sqlquery("CREATE TABLE regexglines (id INT(10) PRIMARY KEY AUTO_INCREMENT, gline TEXT NOT NULL, setby VARCHAR(%d) NOT NULL, reason VARCHAR(%d) NOT NULL, expires BIGINT NOT NULL, type TINYINT(4) NOT NULL DEFAULT 1)", ACCOUNTLEN, RG_REASON_MAX);
  rg_sqlquery("CREATE TABLE regexlogs (id INT(10) PRIMARY KEY AUTO_INCREMENT, host VARCHAR(%d) NOT NULL, account VARCHAR(%d) NOT NULL, event TEXT NOT NULL, arg TEXT NOT NULL, ts TIMESTAMP)", RG_MASKLEN - 1, ACCOUNTLEN);
  rg_sqlquery("CREATE TABLE regexglinelog (id INT(10) PRIMARY KEY AUTO_INCREMENT, glineid INT(10) NOT NULL, ts TIMESTAMP, nickname VARCHAR(%d) NOT NULL, username VARCHAR(%d) NOT NULL, hostname VARCHAR(%d) NOT NULL, realname VARCHAR(%d))", NICKLEN, USERLEN, HOSTLEN, REALLEN);
  
  if(!rg_sqlquery("SELECT id, gline, setby, reason, expires, type FROM regexglines")) {
    rg_sqlresult res;
    if((res = rg_sqlstoreresult())) {
      rg_sqlrow row;
      while((row = rg_sqlgetrow(res))) {
        if (!rg_newsstruct(row[0], row[1], row[2], row[3], row[4], row[5], 0, 0))
          rg_sqlquery("DELETE FROM regexglines WHERE id = %s", row[0]);
      }
      rg_sqlfree(res);
    }
  }    
  
  return 0;
}

void rg_nick(int hooknum, void *arg) {
  nick *np = (nick *)arg;
  struct rg_struct *rp;
  char hostname[RG_MASKLEN];
  int hostlen;
  struct rg_glinelist gll;

  rg_initglinelist(&gll);

  hostlen = RGBuildHostname(hostname, np);

  if(IsOper(np) || IsService(np) || IsXOper(np))
    return;

  for(rp=rg_list;rp;rp=rp->next) {
    if(pcre_exec(rp->regex, rp->hint, hostname, hostlen, 0, 0, NULL, 0) >= 0) {
      rg_dogline(&gll, np, rp, hostname);
      break;
    }
  }

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
  
  char eemask[RG_QUERY_BUF_SIZE], eesetby[RG_QUERY_BUF_SIZE], eereason[RG_QUERY_BUF_SIZE];
  char hostname[RG_MASKLEN];

  if(cargc < 4)
    return CMD_USAGE;
  
  if ((strlen(cargv[2]) != 1) || ((cargv[2][0] != '1') && (cargv[2][0] != '2') && (cargv[2][0] != '3') && (cargv[2][0] != '4') && (cargv[2][0] != '5') && (cargv[2][0] != '6'))) {
    controlreply(np, "Invalid type specified!");
    return CMD_USAGE;
  }

  if (!(expiry = durationtolong(cargv[1]))) {
    controlreply(np, "Invalid duration specified!");
    return CMD_USAGE;
  }
  
  for(rp=rg_list;rp;rp=rp->next) {
    if (RGMasksEqual(rp->mask->content, cargv[0])) {
      controlreply(np, "That regexp gline already exists!");
      return CMD_ERROR;
    }
  }
  
  if (rg_sanitycheck(cargv[0], &count)) {
    controlreply(np, "Error in expression.");
    return CMD_ERROR;
  } else if (count < 0) {
    controlreply(np, "That expression would hit too many users (%d)!", -count);  
    return CMD_ERROR;
  }
  
  realexpiry = expiry + time(NULL);
  
  rg_sqlescape_string(eemask, cargv[0], strlen(cargv[0]));
  rg_sqlescape_string(eesetby, np->nick, strlen(np->nick));
  rg_sqlescape_string(eereason, cargv[3], strlen(cargv[3]));
  
  rg_sqlquery("INSERT INTO regexglines (gline, setby, reason, expires, type) VALUES ('%s', '%s', '%s', %d, %s)", eemask, eesetby, eereason, realexpiry, cargv[2]);
  if (!rg_sqlquery("SELECT LAST_INSERT_ID()")) {
    rg_sqlresult res;
    if((res = rg_sqlstoreresult())) {
      rg_sqlrow row;
      row = rg_sqlgetrow(res);
      if (row) {
        int id = atoi(row[0]);
        if(id == 0) {
          rp = NULL;
        } else {
          rp = rg_newsstruct(row[0], cargv[0], np->nick, cargv[3], "", cargv[2], realexpiry, 0);
        }
        rg_sqlfree(res);
        if(!rp) {
          rg_sqlquery("DELETE FROM regexglines WHERE gline = '%s' AND setby = '%s' AND reason = '%s' AND expires = %d AND type = %s ORDER BY ID DESC LIMIT 1", eemask, eesetby, eereason, realexpiry, cargv[2]);
          controlreply(np, "Error allocating - regexgline NOT ADDED.");
          return CMD_ERROR;
        }
      } else {
        rg_sqlfree(res);
        rg_sqlquery("DELETE FROM regexglines WHERE gline = '%s' AND setby = '%s' AND reason = '%s' AND expires = %d AND type = %s ORDER BY ID DESC LIMIT 1", eemask, eesetby, eereason, realexpiry, cargv[2]);
        controlreply(np, "Error selecting ID from database - regexgline NOT ADDED.");
        return CMD_ERROR;
      }
    } else {
      rg_sqlquery("DELETE FROM regexglines WHERE gline = '%s' AND setby = '%s' AND reason = '%s' AND expires = %d AND type = %s ORDER BY ID DESC LIMIT 1", eemask, eesetby, eereason, realexpiry, cargv[2]);
      controlreply(np, "Error fetching ID from database - regexgline NOT ADDED.");
      return CMD_ERROR;
    }
  } else {
    rg_sqlquery("DELETE FROM regexglines WHERE gline = '%s' AND setby = '%s' AND reason = '%s' AND expires = %d AND type = %s ORDER BY ID DESC LIMIT 1", eemask, eesetby, eereason, realexpiry, cargv[2]);
    controlreply(np, "Error executing query - regexgline NOT ADDED.");
    return CMD_ERROR;
  }
  
  rg_initglinelist(&gll);

  for(j=0;j<NICKHASHSIZE;j++) {
    for(tnp=nicktable[j];tnp;tnp=tnp->next) {
      if(IsOper(tnp) || IsService(tnp) || IsXOper(tnp))
        continue;

      hostlen = RGBuildHostname(hostname, tnp);
      if(pcre_exec(rp->regex, rp->hint, hostname, hostlen, 0, 0, NULL, 0) >= 0)
        rg_dogline(&gll, tnp, rp, hostname);
    }
  }
  
  rg_flushglines(&gll);

  expirybuf = longtoduration(expiry, 0);

  rg_logevent(np, "regexgline", "%s %d %d %s", cargv[0], expiry, count, cargv[3]);
  controlreply(np, "Added regexgline: %s (expires in: %s, hit %d user%s): %s", cargv[0], expirybuf, count, (count!=1)?"s":"", cargv[3]);
  /* If we are using NO, can we safely assume the user is authed here and use ->authname? */
  controlwall(NO_OPER, NL_GLINES, "%s!%s@%s/%s added regexgline: %s (expires in: %s, hit %d user%s): %s", np->nick, np->ident, np->host->name->content, np->authname, cargv[0], expirybuf, count, (count!=1)?"s":"", cargv[3]);

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
      
      rg_sqlquery("DELETE FROM regexglines WHERE id = %d", rp->id);
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
    int i;

    for(i=0;i<8;i++) {
      if(0xff == rc_hexlookup[(int)cargv[0][i]]) {
        controlreply(np, "Invalid gline id!");
        return CMD_ERROR;
      } else {
        id = (id << 4) | rc_hexlookup[(int)cargv[0][i]];
      }
    }

    controlreply(np, "Mask                      Expires              Set by          Type Reason");
    for(rp=rg_list;rp;rp=rp->next)
      if(id == rp->glineid)
        rg_displaygline(np, rp);
    controlreply(np, "Done.");

    return CMD_OK;
  }
}

int rg_glist(void *source, int cargc, char **cargv) {
  nick *np = (nick *)source;
  struct rg_struct *rp;

  if(cargc) {
    int erroroffset;
    pcre *regex;
    pcre_extra *hint;
    const char *error;    
    
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
    
    rg_logevent(np, "regexglist", "%s", cargv[0]);
    controlreply(np, "Mask                      Expires              Set by          Type Reason");
    for(rp=rg_list;rp;rp=rp->next)
      if(pcre_exec(regex, hint, rp->mask->content, rp->mask->length, 0, 0, NULL, 0) >= 0)
        rg_displaygline(np, rp);
    
    pcre_free(regex);
    if(hint)
      pcre_free(hint);
    
  } else {
    rg_logevent(np, "regexglist", "");
    controlreply(np, "Mask                      Expires              Set by          Type Reason");
    for(rp=rg_list;rp;rp=rp->next)
      rg_displaygline(np, rp);
  }

  controlreply(np, "Done.");
  return CMD_OK;
}

void rg_displaygline(nick *np, struct rg_struct *rp) { /* could be a macro? I'll assume the C compiler inlines it */
  controlreply(np, "%-25s %-20s %-15s %-4d %s", rp->mask->content, longtoduration(rp->expires - time(NULL), 0), rp->setby->content, rp->type, rp->reason->content);
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

int rg_sqlconnect(char *dbhost, char *dbuser, char *dbpass, char *db, unsigned int port) {
  mysql_init(&rg_sql);
  if(!mysql_real_connect(&rg_sql, dbhost, dbuser, dbpass, db, port, NULL, 0))
    return -1;
  return 0;
}

void rg_sqldisconnect(void) {
  mysql_close(&rg_sql);
}

void rg_sqlescape_string(char *dest, char *source, size_t length) {
  if(length >= RG_QUERY_BUF_SIZE)
    length = RG_QUERY_BUF_SIZE - 1;

  mysql_real_escape_string(&rg_sql, dest, source, length);
}

int rg_sqlquery(char *format, ...) {
  char rg_sqlquery[RG_QUERY_BUF_SIZE];
  va_list va;
  
  va_start(va, format);
  vsnprintf(rg_sqlquery, sizeof(rg_sqlquery), format, va);
  va_end(va);

  return mysql_query(&rg_sql, rg_sqlquery);
}

rg_sqlresult rg_sqlstoreresult(void) {
  return mysql_store_result(&rg_sql);
}

rg_sqlrow rg_sqlgetrow(rg_sqlresult res) {
  return mysql_fetch_row(res);
}

void rg_sqlfree(rg_sqlresult res) {
  mysql_free_result(res);
}

void rg_startup(void) {
  int j, hostlen;
  nick *np;
  struct rg_struct *rp;
  struct rg_glinelist gll;
  char hostname[RG_MASKLEN];

  rg_initglinelist(&gll);

  for(j=0;j<NICKHASHSIZE;j++) {
    for(np=nicktable[j];np;np=np->next) {
      if(IsOper(np) || IsService(np) || IsXOper(np))
        continue;
      hostlen = RGBuildHostname(hostname, np);
      for(rp=rg_list;rp;rp=rp->next) {
        if(pcre_exec(rp->regex, rp->hint, hostname, hostlen, 0, 0, NULL, 0) >= 0) {
          rg_dogline(&gll, np, rp, hostname);
          break;
        }
      }
    }
  }
  
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
    rp->id = 0;
    rp->mask = NULL;
    rp->setby = NULL;
    rp->reason = NULL;
    rp->expires = expires;
    rp->type = 0;
    rp->regex = NULL;
    rp->hint = NULL;

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

struct rg_struct *rg_newsstruct(char *id, char *mask, char *setby, char *reason, char *expires, char *type, time_t iexpires, int iid) {
  struct rg_struct *newrow, *lp, *cp;
  time_t rexpires;
  char glineiddata[1024];
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
    
    if (!iid) {
      if(!protectedatoi(id, &newrow->id))
        goto dispose2;
    } else {
      newrow->id = iid;
    }
    
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

  rg_loggline(rp, np);

  if (rp->type == 2) {
    usercount = np->host->clonecount;
    snprintf(hostname, sizeof(hostname), "*@%s", IPtostr(np->p_ipaddr));
  }
  
  if ((rp->type == 1) || (usercount > rg_max_per_gline)) {
    nick *tnp;

    for(usercount=0,tnp=np->host->nicks;tnp;tnp=tnp->nextbyhost)
      if(!ircd_strcmp(np->ident, tnp->ident))
        usercount++;

    snprintf(hostname, sizeof(hostname), "%s@%s", np->ident, IPtostr(np->p_ipaddr));
  }

  if ((rp->type >= 3) || (usercount > rg_max_per_gline)) {
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
      
      if (rp->type <= 3) {
        nn->punish = 3;
      } else {
        nn->punish = rp->type;
      }
    }
    return usercount;
  }
  
  if ((rp->type <= 0) || (rp->type >= 3))
    return 0;
  
  if (rp->type == 1) {
    if (IsAccount(np)) {
      controlwall(NO_OPER, NL_HITS, "%s!%s@%s/%s matched user@host gline regex %08lx (hit %d user%s)", np->nick, np->ident, np->host->name->content, np->authname, rp->glineid, usercount, (usercount!=1)?"s":"");
    } else {
      controlwall(NO_OPER, NL_HITS, "%s!%s@%s matched user@host gline regex %08lx (hit %d user%s)", np->nick, np->ident, np->host->name->content, rp->glineid, usercount, (usercount!=1)?"s":"");
    }
  } else {
    if (IsAccount(np)) {
      controlwall(NO_OPER, NL_HITS, "%s!%s@%s/%s matched *@host gline regex %08lx (hit %d user%s)", np->nick, np->ident, np->host->name->content, np->authname, rp->glineid, usercount, (usercount!=1)?"s":"");
    } else {
      controlwall(NO_OPER, NL_HITS, "%s!%s@%s matched *@host gline regex %08lx (hit %d user%s)", np->nick, np->ident, np->host->name->content, rp->glineid, usercount, (usercount!=1)?"s":"");
    }
  }
  
  irc_send("%s GL * +%s %d %d :AUTO: %s (ID: %08lx)\r\n", mynumeric->content, hostname, rg_expiry_time, time(NULL), rp->reason->content, rp->glineid);
  return usercount;
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

  return;
  va_list va;
    
  va_start(va, details);
  vsnprintf(buf, sizeof(buf), details, va);
  va_end(va);
  
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
  
  rg_sqlescape_string(eeevent, event, strlen(event));
  rg_sqlescape_string(eedetails, buf, strlen(buf));
  rg_sqlescape_string(eeaccount, event, strlen(account));
  rg_sqlescape_string(eemask, mask, masklen);
  
  rg_sqlquery("INSERT INTO regexlogs (host, account, event, arg) VALUES ('%s', '%s', '%s', '%s')", eemask, eeaccount, eeevent, eedetails);
}

void rg_loggline(struct rg_struct *rg, nick *np) {
  char eenick[RG_QUERY_BUF_SIZE], eeuser[RG_QUERY_BUF_SIZE], eehost[RG_QUERY_BUF_SIZE], eereal[RG_QUERY_BUF_SIZE];

  return;
  rg_sqlescape_string(eenick, np->nick, strlen(np->nick));
  rg_sqlescape_string(eeuser, np->ident, strlen(np->ident));
  rg_sqlescape_string(eehost, np->host->name->content, strlen(np->host->name->content));
  rg_sqlescape_string(eereal, np->realname->name->content, strlen(np->realname->name->content));

  rg_sqlquery("INSERT INTO regexglinelog (glineid, nickname, username, hostname, realname) VALUES (%d, '%s', '%s', '%s', '%s')", rg->id, eenick, eeuser, eehost, eereal);
}
