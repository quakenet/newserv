#include "chanserv.h"
#include "../lib/irc_string.h"
#include "../lib/version.h"
#include <stdio.h>
#include <string.h>

MODULE_VERSION(QVERSION);

static void cleanupdb(void *arg);
static void schedulecleanup(int hooknum, void *arg);

static unsigned int cleanupdb_active;
static DBModuleIdentifier q9cleanupdbid;

void _init() {
  q9cleanupdbid = dbgetid();

  registerhook(HOOK_CHANSERV_DBLOADED, schedulecleanup);

  if (chanservdb_ready)
    schedulecleanup(HOOK_CHANSERV_DBLOADED, NULL);
}

void _fini() {
  deleteallschedules(cleanupdb);
  dbfreeid(q9cleanupdbid);
}

static void schedulecleanup(int hooknum, void *arg) {
  /* run at 1am but only if we're more than 15m away from it, otherwise run tomorrow */

  time_t t = time(NULL);
  time_t next_run = ((t / 86400) * 86400 + 86400) + 3600;
  if(next_run - t < 900)
    next_run+=86400;
  
  schedulerecurring(next_run,0,86400,cleanupdb,NULL);
}

__attribute__ ((format (printf, 1, 2)))
static void cleanuplog(char *format, ...) {
  char buf[512];
  va_list va;

  va_start(va, format);
  vsnprintf(buf, sizeof(buf), format, va);
  va_end(va);

  cs_log(NULL, "CLEANUPDB %s", buf);
  chanservwallmessage("CLEANUPDB: %s", buf);
}

static void cleanupdb_real(DBConn *dbconn, void *arg) {
  reguser *vrup, *srup, *founder;
  regchanuser *rcup, *nrcup;
  authname *anp;
  int i,j;
  time_t t, to_age, unused_age, maxchan_age, authhistory_age;
  int expired = 0, unauthed = 0, chansvaped = 0, chansempty = 0;
  chanindex *cip, *ncip;
  regchan *rcp;
  DBResult *pgres;
  unsigned int themarker;
  unsigned int id;
  
  t = time(NULL);
  to_age = t - (CLEANUP_ACCOUNT_INACTIVE * 3600 * 24);  
  unused_age = t - (CLEANUP_ACCOUNT_UNUSED * 3600 * 24);
  maxchan_age = t - (CLEANUP_CHANNEL_INACTIVE * 3600 * 24);
  authhistory_age = t - (CLEANUP_AUTHHISTORY * 3600 * 24);

  themarker=nextauthnamemarker();
  
  if (!dbconn) {
    cleanuplog("No DB connection, aborting.");
    goto out;
  }

  pgres=dbgetresult(dbconn);
  
  if (!dbquerysuccessful(pgres)) {
    cleanuplog("DB error, aborting.");
    goto out;
  }
  
  while (dbfetchrow(pgres)) {
    id=strtoul(dbgetvalue(pgres, 0), NULL, 10);
    anp=findauthname(id);
    if (anp)
      anp->marker=themarker;
  }
  
  dbclear(pgres);

  cleanuplog("Phase 1 complete, starting phase 2 (regusers scan)...");

  for (i=0;i<REGUSERHASHSIZE;i++) {
    for (vrup=regusernicktable[i]; vrup; vrup=srup) {
      srup=vrup->nextbyname;
      if (!(anp=findauthname(vrup->ID)))
        continue; /* should maybe raise hell instead */

      /* If this user has the right marker, this means the authtracker data
       * indicates that they have been active recently */
      if (anp->marker == themarker)
        continue;

      /* HACK: don't ever delete the last user -- prevents userids being reused */
      if (vrup->ID == lastuserID)
        continue;

      if(!anp->nicks && !UHasStaffPriv(vrup) && !UIsCleanupExempt(vrup)) {
        if(vrup->lastauth && (vrup->lastauth < to_age)) {
          expired++;
          cs_log(NULL, "CLEANUPDB inactive user %s %u", vrup->username, vrup->ID);
        } else if(!vrup->lastauth && (vrup->created < unused_age)) {
          unauthed++;
          cs_log(NULL, "CLEANUPDB unused user %s %u", vrup->username, vrup->ID);
        } else {
          continue;
        }

        cs_removeuser(vrup);
      }
    }
  }

  cleanuplog("Phase 2 complete, starting phase 3 (chanindex scan)...");
    
  for (i=0;i<CHANNELHASHSIZE;i++) {
    for (cip=chantable[i];cip;cip=ncip) {
      ncip=cip->next;
      if (!(rcp=cip->exts[chanservext]))
        continue;

      /* HACK: don't ever delete the last channel -- prevents channelids being reused */
      if (rcp->ID == lastchannelID)
        continue;

      /* there's a bug here... if no joins or modes are done within the threshold
       * and someone leaves just before the cleanup then the channel will be nuked.
       */

      /* this is one possible soln but relies on cleanupdb being run more frequently than
       * the threshold:
       */ 
      /* slug: no longer required as we scan the entire network every 1h (cs_hourlyfunc) */
/*
      if(cip->channel && cs_ischannelactive(cip->channel, rcp)) {
        rcp->lastactive = t;
        if (rcp->lastcountersync < (t - COUNTERSYNCINTERVAL)) {
          csdb_updatechannelcounters(rcp);
          rcp->lastcountersync=t;
        }
      }
*/

      if(rcp->lastactive < maxchan_age) {
        /* don't remove channels with the original founder as an oper */
        founder=findreguserbyID(rcp->founder);
        if(founder && UHasOperPriv(founder))
          continue;

        cs_log(NULL, "CLEANUPDB inactive channel %s", cip->name?cip->name->content:"??");
        cs_removechannel(rcp, "Channel deleted due to lack of activity.");
        chansvaped++;
        continue;
      }
      
      /* Get rid of any dead chanlev entries */
      for (j=0;j<REGCHANUSERHASHSIZE;j++) {
        for (rcup=rcp->regusers[j];rcup;rcup=nrcup) {
          nrcup=rcup->nextbychan;
          
          if (!rcup->flags) {
            cs_log(NULL, "Removing user %s from channel %s (no flags)",rcup->user->username,rcp->index->name->content);
            csdb_deletechanuser(rcup);
            delreguserfromchannel(rcp, rcup->user);
            freeregchanuser(rcup);
          }
        }
      }

      if (cs_removechannelifempty(NULL, rcp)) {
        /* logged+parted by cs_removechannelifempty */
        chansempty++;
        continue;
      }
    }
  }
  
  cleanuplog("Phase 3 complete, starting phase 4 (history database cleanup) -- runs in the background.");
    
  csdb_cleanuphistories(authhistory_age);
  
  cleanuplog("Stats: %d accounts inactive for %d days, %d accounts weren't used within %d days, %d channels were inactive for %d days, %d channels empty.", expired, CLEANUP_ACCOUNT_INACTIVE, unauthed, CLEANUP_ACCOUNT_UNUSED, chansvaped, CLEANUP_CHANNEL_INACTIVE, chansempty);

out:
  cleanupdb_active=0;
}

void cs_cleanupdb(nick *np) {
  cleanupdb(np);
}

static void cleanupdb(void *arg) {
  unsigned int to_age;
  nick *np = (nick *)arg;
  
  to_age = time(NULL) - (CLEANUP_ACCOUNT_INACTIVE * 3600 * 24);

  if(np) {
    cleanuplog("Manually started by %s.", np->nick);
  } else {
    cleanuplog("Automatically started.");
  }

  if (cleanupdb_active) {
    cleanuplog("ABORTED! Cleanup already in progress! BUG BUG BUG!");
    return;
  }

  cleanuplog("Phase 1 started (auth history data retrieval)...");
  
  /* This query returns a single column containing the userids of all users
   * who have active sessions now, or sessions which ended in the last
   * CLEANUP_ACCOUNT_INACTIVE days.  */

  dbquery("BEGIN TRANSACTION;");

  /* increase memory for aggregate (GROUP BY) -- query can take hours if this spills to disk */
  dbquery("SET LOCAL work_mem = '512MB';");
  q9cleanup_asyncquery(cleanupdb_real, NULL,
    "SELECT userID from chanserv.authhistory WHERE disconnecttime=0 OR disconnecttime > %d GROUP BY userID;", to_age);
  dbquery("COMMIT;");

  cleanupdb_active=1;
}
