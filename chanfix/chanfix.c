/* shroud's chanfix */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "chanfix.h"
#include "../splitlist/splitlist.h"
#include "../localuser/localuserchannel.h"
#include "../core/schedule.h"
#include "../core/error.h"
#include "../nick/nick.h"
#include "../lib/irc_string.h"
#include "../control/control.h"

/* control's nick */
extern nick *mynick;

int cfext;
int cfnext;
int failedinit;

/* user accessible commands */
int cfcmd_debug(void *source, int cargc, char **cargv);
int cfcmd_debughistogram(void *source, int cargc, char **cargv);
int cfcmd_debugsample(void *source, int cargc, char **cargv);
int cfcmd_debugexpire(void *source, int cargc, char **cargv);
int cfcmd_chanopstat(void *source, int cargc, char **cargv);
int cfcmd_chanoplist(void *source, int cargc, char **cargv);
int cfcmd_chanfix(void *source, int cargc, char **cargv);
int cfcmd_showregs(void *source, int cargc, char **cargv);
int cfcmd_requestop(void *source, int cargc, char **cargv);
int cfcmd_save(void *source, int cargc, char **cargv);
int cfcmd_load(void *source, int cargc, char **cargv);

/* scheduled events */
void cfsched_dosample(void *arg);
void cfsched_doexpire(void *arg);
void cfsched_dosave(void *arg);

/* hooks */
void cfhook_autofix(int hook, void *arg);
void cfhook_statsreport(int hook, void *arg);
void cfhook_auth(int hook, void *arg);
void cfhook_lostnick(int hook, void *arg);

/* helper functions */
regop *cf_createregop(nick *np, chanindex *cip);
void cf_deleteregop(chanindex *cip, regop *ro);
unsigned long cf_gethash(nick *np, int type);

int cf_storechanfix(void);
int cf_loadchanfix(void);
void cf_free(void);

#define min(a,b) ((a > b) ? b : a)

void _init() {
  cfext = registerchanext("chanfix");
  cfnext = registernickext("chanfix");

  if (cfext < 0 || cfnext < 0) {
    Error("chanfix", ERR_ERROR, "Couldn't register channel and/or nick extension");
    failedinit = 1;
  } else {
    schedulerecurring(time(NULL), 0, CFSAMPLEINTERVAL, &cfsched_dosample, NULL);
    schedulerecurring(time(NULL), 0, CFEXPIREINTERVAL, &cfsched_doexpire, NULL);
    schedulerecurring(time(NULL), 0, CFAUTOSAVEINTERVAL, &cfsched_dosave, NULL);

    registercontrolcmd("cfdebug", 10, 1, &cfcmd_debug);
    registercontrolcmd("cfhistogram", 10, 1, &cfcmd_debughistogram);
#if CFDEBUG
    registercontrolcmd("cfsample", 10, 1, &cfcmd_debugsample);
    registercontrolcmd("cfexpire", 10, 1, &cfcmd_debugexpire);
#endif
    registercontrolcmd("chanopstat", 10, 1, &cfcmd_chanopstat);
    registercontrolcmd("chanoplist", 10, 1, &cfcmd_chanoplist);

//    registercontrolcmd("chanfix", 10, 1, &cfcmd_chanfix);
    registercontrolcmd("showregs", 10, 1, &cfcmd_showregs);
#if CFDEBUG
    /* should we disable this in the 'final' build? */
//    registercontrolcmd("requestop", 0, 2, &cfcmd_requestop);
#endif
    registercontrolcmd("cfsave", 10, 0, &cfcmd_save);
    registercontrolcmd("cfload", 10, 0, &cfcmd_load);

#if CFAUTOFIX
    registerhook(HOOK_CHANNEL_DEOPPED, &cfhook_autofix);
    registerhook(HOOK_CHANNEL_PART, &cfhook_autofix);
    registerhook(HOOK_CHANNEL_KICK, &cfhook_autofix);
    registerhook(HOOK_CHANNEL_JOIN, &cfhook_autofix);
#endif

    registerhook(HOOK_CORE_STATSREQUEST, &cfhook_statsreport);
    registerhook(HOOK_NICK_ACCOUNT, &cfhook_auth);
    registerhook(HOOK_NICK_LOSTNICK, &cfhook_lostnick);

    cf_loadchanfix();

    failedinit = 0;
  }
}

void _fini() {
  int i;
  nick *nip;

  if (failedinit == 0) {
    deleteschedule(NULL, &cfsched_dosample, NULL);
    deleteschedule(NULL, &cfsched_doexpire, NULL);
    deleteschedule(NULL, &cfsched_dosave, NULL);

    cf_storechanfix();

    cf_free();

    for (i=0; i<NICKHASHSIZE; i++)
      for (nip=nicktable[i]; nip; nip=nip->next)
        free(nip->exts[cfnext]);

    releasechanext(cfext);
    releasenickext(cfnext);

    deregistercontrolcmd("cfdebug", &cfcmd_debug);
    deregistercontrolcmd("cfhistogram", &cfcmd_debughistogram);
#if CFDEBUG
    deregistercontrolcmd("cfsample", &cfcmd_debugsample);
    deregistercontrolcmd("cfexpire", &cfcmd_debugexpire);
#endif
    deregistercontrolcmd("chanopstat", &cfcmd_chanopstat);
    deregistercontrolcmd("chanoplist", &cfcmd_chanoplist);
//    deregistercontrolcmd("chanfix", &cfcmd_chanfix);
    deregistercontrolcmd("showregs", &cfcmd_showregs);
#if CFDEBUG
//    deregistercontrolcmd("requestop", &cfcmd_requestop);
#endif
    deregistercontrolcmd("cfsave", &cfcmd_save);
    deregistercontrolcmd("cfload", &cfcmd_load);

#if CFAUTOFIX
    deregisterhook(HOOK_CHANNEL_DEOPPED, &cfhook_autofix);
    deregisterhook(HOOK_CHANNEL_PART, &cfhook_autofix);
    deregisterhook(HOOK_CHANNEL_KICK, &cfhook_autofix);
    deregisterhook(HOOK_CHANNEL_JOIN, &cfhook_autofix);
#endif

    deregisterhook(HOOK_CORE_STATSREQUEST, &cfhook_statsreport);
    deregisterhook(HOOK_NICK_ACCOUNT, &cfhook_auth);
    deregisterhook(HOOK_NICK_LOSTNICK, &cfhook_lostnick);
  }
}

int cfcmd_debug(void *source, int cargc, char **cargv) {
  nick *np = (nick*)source;
  chanindex *cip;
  chanfix *cf;
  regop *ro;
  int i;

  if (cargc < 1) {
    controlreply(np, "Syntax: cfdebug <#channel>");

    return CMD_ERROR;
  }

  cip = findchanindex(cargv[0]);

  if (cip == NULL) {
    controlreply(np, "No such channel.");

    return CMD_ERROR;
  } else
    controlreply(np, "Found channel %s. Retrieving chanfix information...", cargv[0]);

  cf = cip->exts[cfext];

  if (cf == NULL) {
    controlreply(np, "No chanfix information for %s", cargv[0]);

    return CMD_ERROR;
  } else
    controlreply(np, "Found chanfix information. Dumping...");

  for (i=0;i<cf->regops.cursi;i++) {
    ro = ((regop**)cf->regops.content)[i];

    controlreply(np, "%d. type: %s hash: 0x%x lastopped: %d uh: %s score: %d",
                 i + 1, ro->type == CFACCOUNT ? "CFACCOUNT" : "CFHOST", ro->hash,
                 ro->lastopped, ro->uh ? ro->uh->content : "(unknown)", ro->score);
  }

  controlreply(np, "Done.");

  return CMD_OK;
}

int cfcmd_debughistogram(void *source, int cargc, char **cargv) {
  nick *np = (nick*)source;
  int i,a,score;
  chanindex* cip;
  chanfix *cf;
  char buf[400];
  int histogram[10001]; /* 625 (lines) * 16 (columns/line) + 1 (for histogram[0]) */

  for (i = 0; i < 10001; i++)
    histogram[i] = 0;

  for (i=0; i<CHANNELHASHSIZE; i++) {
    for (cip=chantable[i]; cip; cip=cip->next) {
      if ((cf = cip->exts[cfext]) != NULL) {
        for (a=0;a<cf->regops.cursi;a++) {
          score = ((regop**)cf->regops.content)[a]->score;

          if (score < 10001)
            histogram[score]++;
        }
      }
    }
  }

  controlreply(np, "--- Histogram of chanfix scores");

  for (i = 1; i < 10001; i += 16) {
    buf[0] = '\0';

    for (a = 0; a < 16; a++) {
      if (a != 0)
        strcat(buf, " ");

      sprintf(buf+strlen(buf),"%d", histogram[i+a]);
    }

    controlreply(np, "%6d: %s", i, buf);
  }

  controlreply(np, "--- End of histogram");

  return CMD_OK;
}

int cfcmd_debugsample(void *source, int cargc, char **cargv) {
  cfsched_dosample(NULL);

  controlreply((nick*)source, "Done.");

  return CMD_OK;
}

int cfcmd_debugexpire(void *source, int cargc, char **cargv) {
  cfsched_doexpire(NULL);

  controlreply((nick*)source, "Done.");

  return CMD_OK;
}

/* used for qsorting int arrays */
int cmpint(const void *a, const void *b) {
  int p = *(int*)a;
  int q = *(int*)b;

  if (p > q)
    return -1;
  else if (p < q)
    return 1;
  else
    return 0;
}

/* used for qsorting regop* arrays */
int cmpregop(const void *a, const void *b) {
  regop *p = *(regop**)a;
  regop *q = *(regop**)b;

  if (p->score > q->score)
    return -1;
  else if (p->score < q->score)
    return 1;
  else
    return 0;
}

int cf_getsortedregops(chanfix *cf, int max, regop **list) {
  int i;

  if (cf == NULL)
    return 0;

  qsort(cf->regops.content, cf->regops.cursi, sizeof(regop*), cmpregop);

  for (i = 0; i < min(max, cf->regops.cursi); i++) {
    list[i] = ((regop**)cf->regops.content)[i];
  }

  return i;
}

int cfcmd_chanopstat(void *source, int cargc, char **cargv) {
  nick *np = (nick*)source;
  nick *np2;
  channel *cp;
  chanfix *cf;
  regop *ro;
  regop *rolist[10];
  int i, a, count;
  int *scores;
  char buf[400];

  if (cargc < 1) {
    controlreply(np, "Syntax: chanopstat <#channel>");

    return CMD_ERROR;
  }

  cp = findchannel(cargv[0]);

  if (cp == NULL) {
    controlreply(np, "No such channel.");

    return CMD_ERROR;
  }

  cf = cp->index->exts[cfext];

  if (cf == NULL) {
    controlreply(np, "No chanfix information for %s", cargv[0]);

    return CMD_ERROR;
  }

  /* known ops */
  count = cf_getsortedregops(cf, 10, rolist);

  buf[0] = '\0';

  for (i=0;i<count;i++) {
    if (i != 0)
      strcat(buf, " ");

    sprintf(buf+strlen(buf),"%d", rolist[i]->score);
  }

  controlreply(np, "Scores of \"best ops\" on %s are: %s", cargv[0], buf);

  /* current ops */
  scores = (int*)malloc(sizeof(int) * cp->users->hashsize);

  i = 0;

  for (a=0;a<cp->users->hashsize;a++) {
    if ((cp->users->content[a] != nouser) && (cp->users->content[a] & CUMODE_OP)) {
      np2 = getnickbynumeric(cp->users->content[a]);

      ro = cf_findregop(np2, cp->index, CFACCOUNT | CFHOST);

      if (ro)
        scores[i++] = ro->score;
    }
  }

  qsort(scores, i, sizeof(int), &cmpint);

  buf[0] = '\0';

  for (a=0;a<min(i,20);a++) {
    if (scores[a] == 0)
      break;

    if (a != 0)
      strcat(buf, " ");

    sprintf(buf+strlen(buf),"%d", scores[a]);
  }

  free(scores);

  controlreply(np, "Scores of current ops on %s are: %s", cargv[0], buf);

  controlreply(np, "Done.");

  return CMD_OK;
}

int cfcmd_chanoplist(void *source, int cargc, char **cargv) {
  nick *np = (nick*)source;
  nick *np2;
  chanindex *cip;
  chanfix *cf;
  regop *rolist[50];
  int i,a,count;
  time_t ct;
  const char* cdate;
  char* date;
  unsigned long *hand;

  if (cargc < 1) {
    controlreply(np, "Syntax: chanoplist <#channel>");

    return CMD_ERROR;
  }

  cip = findchanindex(cargv[0]);

  if (cip == NULL) {
    controlreply(np, "No such channel.");

    return CMD_ERROR;
  }

  cf = cip->exts[cfext];

  if (cf == NULL) {
    controlreply(np, "No chanfix information for %s", cargv[0]);

    return CMD_ERROR;
  }

  count = cf_getsortedregops(cf, 50, rolist);

  controlreply(np, "Pos Score Type    User/Last seen");

  for (i=0;i<count;i++) {
    np2 = cf_findnick(rolist[i], cip);

    if (np2 != NULL) {
      hand = getnumerichandlefromchanhash(cip->channel->users, np2->numeric);

      /* hand should be non-null in all cases */
      if (hand != NULL)
        controlreply(np, "%3d %5d %-7s %1s%-16s %s@%s (%s)", i + 1, rolist[i]->score,
                     (rolist[i]->type == CFACCOUNT) ? "account" : "host",
                     (*hand & CUMODE_OP) ? "@" : "", np2->nick, np2->ident,
                     np2->host->name->content, np2->realname->name->content);
    } else {
      ct = rolist[i]->lastopped;

      cdate = ctime(&ct);

      date = (char*)malloc(strlen(cdate) + 1);
      strcpy(date, cdate);

      for (a=0;a<strlen(date);a++) {
        if (date[a] == '\n') {
          date[a] = '\0';
          break;
        }
      }

      controlreply(np, "%3d %5d %-7s %1s%-16s %s Last seen: %s", i + 1, rolist[i]->score,
                   (rolist[i]->type == CFACCOUNT) ? "account" : "host",
                   "", "!NONE!",
                   rolist[i]->uh ? rolist[i]->uh->content : "!UNKNOWN!", date);

      free(date);
    }
  }

  controlreply(np, "Done.");

  return CMD_OK;
}

int cfcmd_chanfix(void *source, int cargc, char **cargv) {
  nick *np = (nick*)source;
  channel *cp;
  int ret;

  if (cargc < 1) {
    controlreply(np, "Syntax: chanfix <#channel>");

    return CMD_ERROR;
  }

  cp = findchannel(cargv[0]);

  if (cp == NULL) {
    controlreply(np, "No such channel.");

    return CMD_ERROR;
  }

  if (sp_countsplitservers() > 0) {
    controlreply(np, "Chanfix cannot be used during a netsplit.");

    return CMD_ERROR;
  }

  ret = cf_fixchannel(cp);

  switch (ret) {
    case CFX_FIXED:
      controlreply(np, "Channel was fixed.");
      break;
    case CFX_NOCHANFIX:
      controlreply(np, "Channel cannot be fixed: no chanfix information");
      break;
    case CFX_FIXEDFEWOPS:
      controlreply(np, "Channel was fixed but only a few ops could be found and reopped.");
      break;
    default:
      /* oops */
      break;
  }

  return CMD_OK;
}

int cfcmd_showregs(void *source, int cargc, char **cargv) {
  nick *np = (nick*)source;
  nick *np2;
  chanfix *cf;
  channel *cp;
  int a, i, count, ops;
  regop *rolist[50];

  if (cargc < 1) {
    controlreply(np, "Syntax: showregs <#channel>");

    return CMD_ERROR;
  }

  cp = findchannel(cargv[0]);

  if (cp == NULL) {
    controlreply(np, "No such channel.");

    return CMD_ERROR;
  }

  cf = cp->index->exts[cfext];

  if (cf == NULL) {
    controlreply(np, "No chanfix information for %s", cargv[0]);

    return CMD_ERROR;
  }

  count = 0;

  for(a=0;a<cp->users->hashsize;a++) {
    if(cp->users->content[a] != nouser) {
      np2 = getnickbynumeric(cp->users->content[a]);

      if (IsService(np2)) {
        controlreply(np, "%s (service)", np2->nick);
        count++;
      }
    }
  }

  /* now get a sorted list of regops */
  ops = cf_getsortedregops(cf, 50, rolist);

  /* and show some of them */
  for (i=0;i<ops;i++) {
    if (count >= CFMAXOPS || rolist[i]->score < rolist[0]->score / 2 || rolist[i]->score < CFMINSCORE)
      break;

    np2 = cf_findnick(rolist[i], cp->index);

    if (np2) {
      controlreply(np, "%s (%s)", np2->nick, rolist[i]->type == CFACCOUNT ? "account" : "host");

      count++;
    }
  }

  controlreply((nick*)source, "--- End of list: %d users listed", count);

  return CMD_OK;
}

int cfcmd_requestop(void *source, int cargc, char **cargv) {
  nick *np = (nick*)source;
  nick *user = np;
  channel *cp;
  int ret, a;
  unsigned long *hand;
  modechanges changes;

  if (cargc < 1) {
    controlreply(np, "Syntax: requestop <#channel> [nick]");

    return CMD_ERROR;
  }

  cp = findchannel(cargv[0]);

  if (cp == NULL) {
    controlreply(np, "No such channel.");

    return CMD_ERROR;
  }

  if (cargc > 1) {
    user = getnickbynick(cargv[1]);

    if (!user) {
      controlreply(np, "No such user.");

      return CMD_ERROR;
    }
  }

  hand = getnumerichandlefromchanhash(cp->users, user->numeric);

  if (hand == NULL) {
    controlreply(np, "User %s is not on channel %s.", user->nick, cargv[0]);

    return CMD_ERROR;
  }

  for (a=0;a<cp->users->hashsize;a++) {
    if ((cp->users->content[a] != nouser) && (cp->users->content[a] & CUMODE_OP)) {
      controlreply(np, "There are ops on channel %s. This command can only be"
                   " used if there are no ops.", cargv[0]);

      return CMD_ERROR;
    }
  }

  if (sp_countsplitservers() > 0) {
    controlreply(np, "One or more servers are currently split. Wait until the"
                 " netsplit is over and try again.");

    return CMD_ERROR;
  }

  if (cf_wouldreop(user, cp)) {
    localsetmodeinit(&changes, cp, mynick);
    localdosetmode_nick(&changes, user, MC_OP);
    localsetmodeflush(&changes, 1);

    controlreply(np, "Chanfix opped you on the specified channel.");
  } else {
    ret = cf_fixchannel(cp);

    if (ret == CFX_NOUSERSAVAILABLE)
      controlreply(np, "Chanfix knows regular ops for that channel. They will"
                   " be opped when they return.");
    else
      controlreply(np, "Chanfix has opped known ops for that channel.");
  }

  return CMD_OK;
}

int cfcmd_save(void *source, int cargc, char **cargv) {
  nick *np = (nick*)source;
  int count;

  count = cf_storechanfix();

  controlreply(np, "%d chanfix records saved.", count);

  return CMD_OK;
}

void cf_free(void) {
  int a, i;
  chanfix *cf;
  chanindex *cip;

  /* free old stuff */
  for (i=0; i<CHANNELHASHSIZE; i++) {
    for (cip=chantable[i]; cip; cip=cip->next) {
      if ((cf = cip->exts[cfext]) != NULL) {
        for (a=0;a<cf->regops.cursi;a++) {
          freesstring(((regop**)cf->regops.content)[a]->uh);
          free(((regop**)cf->regops.content)[a]);
        }

        array_free(&(((chanfix*)cip->exts[cfext])->regops));
      }

      free(cip->exts[cfext]);
      cip->exts[cfext] = NULL;
    }
  }
}

int cfcmd_load(void *source, int cargc, char **cargv) {
  nick *np = (nick*)source;
  int count;

  count = cf_loadchanfix();

  controlreply(np, "%d chanfix records loaded.", count);

  return CMD_OK;
}

int cf_hasauthedcloneonchan(nick *np, channel *cp) {
  nick *jp;
  unsigned long *hand;

  for (jp = np->host->nicks; jp; jp = jp->nextbyhost) {
    if (!IsAccount(jp) || jp->numeric == np->numeric)
      continue;

    hand = getnumerichandlefromchanhash(cp->users, jp->numeric);

    if (hand && (*hand & CUMODE_OP) && strcmp(np->ident, jp->ident) == 0)
      return 1;
  }

  return 0;
}

void cfsched_dosample(void *arg) {
  int i,a,now,cfscore,cfnewro,cfuhost,diff;
  channel *cp;
  chanindex *cip;
  nick *np;
  chanfix *cf;
  regop *ro, *roh;
  struct timeval start;
  struct timeval end;
  char buf[USERLEN+1+HOSTLEN+1];

  now = getnettime();

  cfuhost = cfscore = cfnewro = 0;

  if (sp_countsplitservers() > CFMAXSPLITSERVERS)
    return;

  gettimeofday(&start, NULL);

  for (i=0; i<CHANNELHASHSIZE; i++) {
    for (cip=chantable[i]; cip; cip=cip->next) {
      cf = (chanfix*)cip->exts[cfext];
      cp = cip->channel;

      if (!cp || cp->users->totalusers < CFMINUSERS)
        continue;

      for (a=0;a<cp->users->hashsize;a++) {
        if ((cp->users->content[a] != nouser) && (cp->users->content[a] & CUMODE_OP)) {
          np = getnickbynumeric(cp->users->content[a]);

#if !CFDEBUG
          if (IsService(np))
            continue;
#endif

          roh = ro = cf_findregop(np, cip, CFACCOUNT | CFHOST);

          if ((ro == NULL || (IsAccount(np) && ro->type == CFHOST)) &&
              !cf_hasauthedcloneonchan(np, cp)) {
            ro = cf_createregop(np, cip);
            cfnewro++;
          }

          /* lastopped == now if the user has clones, we obviously
           * don't want to give them points in this case */
          if (ro && ro->lastopped != now) {
            if (ro->type != CFHOST || !cf_hasauthedcloneonchan(np, cp)) {
              ro->score++;
              cfscore++;
            }

            /* merge any matching CFHOST records */
            if (roh && roh->type == CFHOST && ro->type == CFACCOUNT) {
              /* hmm */
              ro->score += roh->score;

              cf_deleteregop(cip, roh);
            }

            /* store the user's account/host if we have to */
            if (ro->uh == NULL && ro->score >= CFMINSCOREUH) {
              if (ro->type == CFACCOUNT)
                ro->uh = getsstring(np->authname, ACCOUNTLEN);
              else {
                strcpy(buf, np->ident);
                strcat(buf, "@");
                strcat(buf, np->host->name->content);

                roh->uh = getsstring(buf, USERLEN+1+HOSTLEN);
              }

              cfuhost++;
            }

            ro->lastopped = now;
          }
        }
      }
    }
  }

  cp = findchannel("#qnet.chanfix");

  if (cp) {
    gettimeofday(&end, NULL);

    diff = (end.tv_sec * 1000 + end.tv_usec / 1000) -
           (start.tv_sec * 1000 + start.tv_usec / 1000);

    sendmessagetochannel(mynick, cp, "sampled chanfix scores, assigned %d new"
                         " points, %d new regops, %d user@hosts added, deltaT: %dms", cfscore,
                         cfnewro, cfuhost, diff);
  }
}

void cfsched_doexpire(void *arg) {
  channel *cp;
  chanindex *cip;
  chanindex *ncip;
  chanfix *cf;
  int i,a,cfscore,cfregop,cffreeuh,diff;
  regop **rolist;
  regop *ro;
  struct timeval start;
  struct timeval end;
  time_t currenttime;

  cffreeuh = cfscore = cfregop = 0;

  gettimeofday(&start, NULL);
  currenttime=getnettime();

  for (i=0; i<CHANNELHASHSIZE; i++) {
    for (cip=chantable[i]; cip; cip=cip->next) {
      cf = (chanfix*)cip->exts[cfext];

      if (cf) {
        rolist = (regop**)cf->regops.content;

        for (a=0;a<cf->regops.cursi;a++) {
          ro = rolist[a];

          if ((currenttime - ro->lastopped > 2 * CFSAMPLEINTERVAL) && ro->score) {
            ro->score--;
            cfscore++;
          }

          if (ro->score < CFMINSCOREUH && ro->uh) {
            freesstring(ro->uh);
            ro->uh = NULL;

            cffreeuh++;
          }

          if (ro->score == 0 || ro->lastopped < currenttime - CFREMEMBEROPS) {
            cf_deleteregop(cip, ro);
            cfregop++;
          }
        }
      }
    }
  }

  /* stolen from channel/channelindex.c */
  for (i=0;i<CHANNELHASHSIZE;i++) {
    for (cip=chantable[i];cip;cip=ncip) {
      /* CAREFUL: deleting items from chains you're walking is bad */
      ncip=cip->next;

      /* try to delete it if there's no chanfix* pointer */
      if (cip->exts[cfext] == NULL)
        releasechanindex(cip);
    }
  }

  cp = findchannel("#qnet.chanfix");

  if (cp) {
    gettimeofday(&end, NULL);

    diff = (end.tv_sec * 1000 + end.tv_usec / 1000) -
           (start.tv_sec * 1000 + start.tv_usec / 1000);

    sendmessagetochannel(mynick, cp, "expired chanfix scores, purged %d points,"
                         " scrapped %6d regops, %d user@hosts freed, deltaT: %dms", cfscore,
                         cfregop, cffreeuh, diff);
  }

}

void cfsched_dosave(void *arg) {
  cf_storechanfix();
}

#if CFAUTOFIX
void cfhook_autofix(int hook, void *arg) {
  int a, count;
  void **args = (void**)arg;
  channel *cp;

  if (hook == HOOK_CHANNEL_DEOPPED || hook == HOOK_CHANNEL_PART ||
      hook == HOOK_CHANNEL_KICK || hook == HOOK_CHANNEL_JOIN) {
    cp = args[0];

    if (sp_countsplitservers() > 0)
      return;

    for(a=0;a<cp->users->hashsize;a++) {
      if (cp->users->content[a] != nouser) {
        count++;

        if (cp->users->content[a] & CUMODE_OP)
          return;
      }
    }

    /* don't fix small channels.. it's inaccurate and
     * they could just cycle the channel */
    if (count < 4)
      return;

    cf_fixchannel((channel*)args[0]);
  }
}
#endif

void cfhook_statsreport(int hook, void *arg) {
  char buf[300];
  int i,a,rc,mc,memory;
  chanindex *cip;
  nick *nip;
  chanfix *cf;

  if ((int)arg > 2) {
    memory = rc = mc = 0;

    for (i=0; i<CHANNELHASHSIZE; i++) {
      for (cip=chantable[i]; cip; cip=cip->next) {
        if ((cf = cip->exts[cfext]) != NULL) {
          for (a=0;a<cf->regops.cursi;a++) {
            memory += sizeof(regop) + sizeof(regop*);

            if (((regop**)cf->regops.content)[a]->uh)
              memory += sizeof(sstring) + strlen(((regop**)cf->regops.content)[a]->uh->content) + 1;

            rc++;
          }

          memory += sizeof(chanfix);

          mc++;
        }
      }
    }

    for (i=0; i<NICKHASHSIZE; i++) {
      for (nip=nicktable[i]; nip; nip=nip->next) {
        if (nip->exts[cfnext])
          memory += sizeof(int);
      }
    }

    sprintf(buf, "Chanfix : %6d registered ops, %9d monitored channels. %9d"
            " kbytes of memory used", rc, mc, memory / 1024);
    triggerhook(HOOK_CORE_STATSREPLY, buf);
  }
}

void cfhook_auth(int hook, void *arg) {
  nick *np = (nick*)arg;

  /* Invalidate the user's hash */
  free(np->exts[cfnext]);

  np->exts[cfnext] = NULL;
  
  /* Calculate the new hash */
  cf_gethash(np, CFACCOUNT);
}

void cfhook_lostnick(int hook, void *arg) {
  nick *np = (nick*)arg;

  free(np->exts[cfnext]);
}

/* Returns the hash of a specific user (np), type can be either CFACCOUNT,
   CFHOST or both (binary or'd values). cf_gethash will also cache the user's
   hash in a nick extension */
unsigned long cf_gethash(nick *np, int type) {
  char buf[USERLEN+1+HOSTLEN+1];
  unsigned long hash;

  if (IsAccount(np) && type & CFACCOUNT) {
    if (np->exts[cfnext] == NULL) {
      np->exts[cfnext] = (int*)malloc(sizeof(int));
      *(int*)np->exts[cfnext] = crc32(np->authname);
    }

    return *(int*)np->exts[cfnext];
  } else if (type == CFACCOUNT)
    return 0; /* this should not happen */

  if (type & CFHOST) {
    if (!IsAccount(np) && np->exts[cfnext])
      return *(int*)np->exts[cfnext];
    else {
      strcpy(buf, np->ident);
      strcat(buf, "@");
      strcat(buf, np->host->name->content);
      hash = crc32(buf);

      /* if the user is not authed, update the hash */
      if (!IsAccount(np)) {
        np->exts[cfnext] = (int*)malloc(sizeof(int));

        *(int*)np->exts[cfnext] = hash;
      }

      return hash;
    }
  }

  return 0; /* should not happen */
}

/* This seems to be a lot faster than using sprintf */
int cf_equhost(const char *uhost, const char *user, const char *host) {
  char *p = strchr(uhost, '@');

  /* We assume the uhost contains a @ - which it should do in all cases */
  /*  if (!p)
      return 0; */

  if (ircd_strncmp(uhost, user, p - uhost) == 0 && ircd_strcmp(p + 1, host) == 0)
    return 1;
  else
    return 0;
}

/* Why do we actually store the users' real hosts/accounts instead of hashes?
 * - it allows operators to see the hosts/accounts in 'chanoplist' even if the
 *   users are not online
 * - it avoids hash collisions (could be avoided with md5/sha1/etc.)
 */
int cf_cmpregopnick(regop *ro, nick *np) {
  if (ro->uh != NULL) {
    if (ro->type == CFACCOUNT && IsAccount(np))
      return (ro->hash == cf_gethash(np, CFACCOUNT) &&
              strcmp(ro->uh->content, np->authname) == 0);
    else {
      return (ro->hash == cf_gethash(np, CFHOST) &&
              cf_equhost(ro->uh->content, np->ident, np->host->name->content));
    }
  } else {
    if (ro->type == CFACCOUNT && IsAccount(np))
      return (ro->hash == cf_gethash(np, CFACCOUNT));
    else
      return (ro->hash == cf_gethash(np, CFHOST));
  }
}

nick *cf_findnick(regop *ro, chanindex *cip) {
  chanfix *cf = cip->exts[cfext];
  channel *cp = cip->channel;
  nick *np2;
  int a;

  if (cf == NULL || cp == NULL)
    return NULL;

  if (ro->type == CFACCOUNT) {
    for(a=0;a<cp->users->hashsize;a++) {
      if(cp->users->content[a] != nouser) {
        np2 = getnickbynumeric(cp->users->content[a]);

        if (!IsAccount(np2))
          continue;

        if (cf_cmpregopnick(ro, np2))
          return np2;
      }
    }
  }

  if (ro->type == CFHOST) {
    for(a=0;a<cp->users->hashsize;a++) {
      if(cp->users->content[a] != nouser) {
        np2 = getnickbynumeric(cp->users->content[a]);

        if (cf_cmpregopnick(ro, np2))
          return np2;
      }
    }
  }

  return NULL;
}

regop *cf_findregop(nick *np, chanindex *cip, int type) {
  chanfix *cf = cip->exts[cfext];
  regop *ro;
  int i, ty;

  if (cf == NULL)
    return NULL;

  if (IsAccount(np) && type & CFACCOUNT)
    ty = CFACCOUNT;
  else
    ty = CFHOST;

  for (i=0;i<cf->regops.cursi;i++) {
    ro = ((regop**)cf->regops.content)[i];

    if (ro->type == ty && cf_cmpregopnick(ro, np))
      return ro;
  }

  /* try using the uhost if we didn't find a user with the right account */
  if (ty == CFACCOUNT && type & CFHOST)
    return cf_findregop(np, cip, CFHOST);
  else
    return NULL;

  return NULL;
}

regop *cf_createregop(nick *np, chanindex *cip) {
  chanfix *cf = cip->exts[cfext];
  int slot, type;
  regop **rolist;

  if (cf == NULL) {
    cf = (chanfix*)malloc(sizeof(chanfix));
    cf->index = cip;

    array_init(&(cf->regops), sizeof(regop*));

    cip->exts[cfext] = cf;
  }

  slot = array_getfreeslot(&(cf->regops));

  rolist = (regop**)cf->regops.content;

  rolist[slot] = (regop*)malloc(sizeof(regop));

  if (IsAccount(np))
    type = CFACCOUNT;
  else
    type = CFHOST;

  rolist[slot]->type = type;
  rolist[slot]->hash = cf_gethash(np, type);
  rolist[slot]->uh = NULL;
  rolist[slot]->lastopped = 0;
  rolist[slot]->score = 0;

  return rolist[slot];
}

void cf_deleteregop(chanindex *cip, regop *ro) {
  chanfix *cf = cip->exts[cfext];
  int a;

  if (cf == NULL)
    return;

  for (a=0;a<cf->regops.cursi;a++) {
    if (((regop**)cf->regops.content)[a] == ro) {
      freesstring(((regop**)cf->regops.content)[a]->uh);
      free(((regop**)cf->regops.content)[a]);
      array_delslot(&(cf->regops), a);
    }
  }

  /* get rid of chanfix* if there are no more regops */
  if (cf->regops.cursi == 0) {
    array_free(&(cf->regops));
    free(cf);
    cip->exts[cfext] = NULL;

    /* we could try to free the chanindex* here
       but that would make cfsched_dosample a lot more
       complicated */
  }
}

int cf_fixchannel(channel *cp) {
  int a,i;
  nick *np;
  modechanges changes;
  regop *rolist[50];
  int count,ops;
  chanfix *cf = cp->index->exts[cfext];

  if (cf == NULL)
    return CFX_NOCHANFIX;

  localsetmodeinit(&changes, cp, mynick);

  count = 0;

  /* reop services first and deop other users */
  for(a=0;a<cp->users->hashsize;a++) {
    if(cp->users->content[a] != nouser) {
      np = getnickbynumeric(cp->users->content[a]);

      if (IsService(np)) {
        localdosetmode_nick(&changes, np, MC_OP);
        count++;
      } else
        localdosetmode_nick(&changes, np, MC_DEOP);
    }
  }

  /* don't reop users if we've already opped some services */
#if !CFDEBUG
  if (count > 0)
    return count;
#endif

  /* now get a sorted list of regops */
  ops = cf_getsortedregops(cf, 50, rolist);

  /* and op some of them */
  for (i=0;i<ops;i++) {
    if (count >= CFMAXOPS || rolist[i]->score < rolist[0]->score / 2)
      break;

    if (rolist[i]->score < CFMINSCORE)
      continue;

    np = cf_findnick(rolist[i], cp->index);

    /* only if it's not a service, so we don't screw up 'count' */
    if (np && !IsService(np)) {
      localdosetmode_nick(&changes, np, MC_OP);

      count++;
    }
  }

  localsetmodeflush(&changes, 1);

  irc_send("%s WA :cf_fixchannel(%s) done: %d client(s) opped.",
           longtonumeric(mynick->numeric,5), cp->index->name->content, count);

  if (count == CFMAXOPS)
    return CFX_FIXED;
  else if (count == 0)
    return CFX_NOUSERSAVAILABLE;
  else
    return CFX_FIXEDFEWOPS;
}

int cf_storechanfix(void) {
  FILE *cfdata;
  regop *ro;
  chanfix *cf;
  chanindex *cip;
  char srcfile[300];
  char dstfile[300];
  int a, i, count = 0;

  snprintf(dstfile, 300, "%s.%d", CFSTORAGE, CFSAVEFILES);
  unlink(dstfile);

  for (i = CFSAVEFILES; i > 0; i--) {
    snprintf(srcfile, 300, "%s.%i", CFSTORAGE, i - 1);
    snprintf(dstfile, 300, "%s.%i", CFSTORAGE, i);
    rename(srcfile, dstfile);
  }

  snprintf(srcfile, 300, "%s.0", CFSTORAGE);
  cfdata = fopen(srcfile, "w");

  if (cfdata == NULL)
    return 0;

  for (i=0; i<CHANNELHASHSIZE; i++) {
    for (cip=chantable[i]; cip; cip=cip->next) {
      if ((cf = cip->exts[cfext]) != NULL) {
        for (a=0;a<cf->regops.cursi;a++) {
          ro = ((regop**)cf->regops.content)[a];

          if (ro->uh)
            fprintf(cfdata, "%s %lu %lu %lu %lu %s\n", cip->name->content,
                    (unsigned long)ro->type, (unsigned long)ro->hash,
                    (unsigned long)ro->lastopped, (unsigned long)ro->score,
                    ro->uh->content);
          else
            fprintf(cfdata, "%s %lu %lu %lu %lu\n", cip->name->content,
                    (unsigned long)ro->type, (unsigned long)ro->hash,
                    (unsigned long)ro->lastopped, (unsigned long)ro->score);
          count++;
        }
      }
    }
  }

  fclose(cfdata);

  return count;
}

/* channel type hash lastopped score host */
int cf_parseline(char *line) {
  chanindex *cip;
  chanfix *cf;
  int count;
  int slot;
  char chan[CHANNELLEN+1];
  char host[USERLEN+1+HOSTLEN+1];
  unsigned long type,hash,lastopped,score;
  regop **rolist;

  count = sscanf(line, "%s %lu %lu %lu %lu %s", chan, &type, &hash, &lastopped, &score, host);

  if (count < 5)
    return 0; /* invalid chanfix record */

  cip = findorcreatechanindex(chan);

  cf = cip->exts[cfext];

  if (cf == NULL) {
    cf = (chanfix*)malloc(sizeof(chanfix));
    cf->index = cip;

    array_init(&(cf->regops), sizeof(regop*));

    cip->exts[cfext] = cf;
  }

  slot = array_getfreeslot(&(cf->regops));

  rolist = (regop**)cf->regops.content;

  rolist[slot] = (regop*)malloc(sizeof(regop));

  rolist[slot]->type = type;
  rolist[slot]->hash = hash;
  rolist[slot]->lastopped = lastopped;
  rolist[slot]->score = score;

  if (count >= 6 && strchr(host, '@') != NULL)
    rolist[slot]->uh = getsstring(host, USERLEN+1+HOSTLEN);
  else
    rolist[slot]->uh = NULL;

  return 1;
}

int cf_loadchanfix(void) {
  char line[4096];
  FILE *cfdata;
  char srcfile[300];
  int count;

  cf_free();

  snprintf(srcfile, 300, "%s.0", CFSTORAGE);
  cfdata = fopen(srcfile, "r");

  if (cfdata == NULL)
    return 0;

  count = 0;

  while (!feof(cfdata)) {
    if (fgets(line, sizeof(line), cfdata) == NULL)
      break;

    if (line[strlen(line) - 1] == '\n')
      line[strlen(line) - 1] = '\0';

    if (line[strlen(line) - 1] == '\r')
      line[strlen(line) - 1] = '\0';

    if (line[0] != '\0') {
      if (cf_parseline(line))
        count++;
    }
  }

  fclose(cfdata);

  return count;
}

/* functions for users of this module */
chanfix *cf_findchanfix(chanindex *cip) {
  return cip->exts[cfext];
}

int cf_wouldreop(nick *np, channel *cp) {
  int i,topscore = 0;
  regop **rolist;
  regop *ro;
  chanfix *cf = cp->index->exts[cfext];

  if (cf == NULL)
    return 1; /* too bad, we can't do anything about it */

  ro = cf_findregop(np, cp->index, CFACCOUNT | CFHOST);

  if (ro == NULL)
    return 0;

  rolist = (regop**)cf->regops.content;

  for (i=0; i<cf->regops.cursi; i++)
    if (rolist[i]->score > topscore)
      topscore = rolist[i]->score;

  if (ro->score > topscore / 2 && ro->score > CFMINSCORE)
    return 1;
  else
    return 0;
}
