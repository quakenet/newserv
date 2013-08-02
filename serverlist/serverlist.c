/* Some utterly useless dog */

#include "../core/schedule.h"
#include "../irc/irc.h"
#include "../lib/irc_string.h"
#include "../localuser/localuserchannel.h"
#include "../control/control.h"
#include "../usercount/usercount.h"
#include "../lib/version.h"

MODULE_VERSION("")

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/time.h>

int cmd_serverlist(void *sender, int cargc, char **cargv);
void serverlist_hook_newserver(int hook, void *arg);
void serverlist_hook_lostserver(int hook, void *arg);
int serverlist_versionreply(void *source, int cargc, char **cargv);
void serverlist_pingservers(void *arg);
int serverlist_rpong(void *source, int cargc, char **cargv);

struct {
  int used;
  time_t ts;
  int lag;
  sstring *version1;
  sstring *version2;
} serverinfo[MAXSERVERS];

void serverlist_doversion(void);


void _init(void) {
  registercontrolhelpcmd("serverlist",NO_OPER,1,&cmd_serverlist,"Usage: serverlist [pattern]\nShows all currently connected servers");
  /* hooks for serverlist */
  registerhook(HOOK_SERVER_NEWSERVER, &serverlist_hook_newserver);
  registerhook(HOOK_SERVER_LOSTSERVER, &serverlist_hook_lostserver);
  int i;

  for (i = 0; i < MAXSERVERS; i++) {
    if (serverlist[i].linkstate == LS_LINKED)
      serverinfo[i].used = 1;
    else
      serverinfo[i].used = 0;

    serverinfo[i].ts = getnettime();
    serverinfo[i].lag = -1;
    serverinfo[i].version1 = NULL;
    serverinfo[i].version2 = NULL;
  }
  registernumerichandler(351, &serverlist_versionreply, 2);
  registerserverhandler("RO", &serverlist_rpong, 4);

  schedulerecurring(time(NULL)+1, 0, 10, &serverlist_pingservers, NULL);
}

void _fini(void) {
  int i;
  for (i = 0; i < MAXSERVERS; i++) {
    if (serverinfo[i].used) {
      freesstring(serverinfo[i].version1);
      freesstring(serverinfo[i].version2);
    }
  }
  deregisternumerichandler(351, &serverlist_versionreply);
  deregisterserverhandler("RO", &serverlist_rpong);

  deregisterhook(HOOK_SERVER_NEWSERVER, &serverlist_hook_newserver);
  deregisterhook(HOOK_SERVER_LOSTSERVER, &serverlist_hook_lostserver);

  deregistercontrolcmd("serverlist",cmd_serverlist);

  deleteschedule(NULL, &serverlist_pingservers, NULL);
}

int cmd_serverlist(void *sender, int cargc, char **cargv) {
  nick *np = (nick*)sender;
  int a, i, ucount, acount, scount;

  controlreply(np, "%-7s %-30s %5s/%5s/%-5s %-7s %-16s %-10s %-20s", "Numeric", "Hostname", "ECl", "Cl", "MaxCl", "Flags", "Connected for", "Lag", "Version");

  scount = acount = 0;

  for (i = 0; i < MAXSERVERS; i++) {
    if (serverlist[i].linkstate == LS_LINKED && (cargc < 1 || match2strings(cargv[0], serverlist[i].name->content))) {
      ucount = 0;

      for (a = 0; a <= serverlist[i].maxusernum; a++)
        if (servernicks[i][a] != NULL)
          ucount++;

      acount += ucount;
      scount++;

      controlreply(np, "%-7d %-30s %5d/%5d/%-5d %-7s %-16s %-10d %-20s - %s", i, serverlist[i].name->content,
            servercount[i], ucount, serverlist[i].maxusernum,
            printflags(serverlist[i].flags, smodeflags),
            longtoduration(getnettime() - serverinfo[i].ts, 0),
            serverinfo[i].lag,
            serverinfo[i].version1 ? serverinfo[i].version1->content : "Unknown",
            serverinfo[i].version2 ? serverinfo[i].version2->content : "Unknown");
    }
  }

  controlreply(np, "--- End of list. %d users and %d servers on the net.", acount, scount);

  /* update version info for next time */
  serverlist_doversion();

  return CMD_OK;
}

int serverlist_versionreply(void *source, int cargc, char **cargv) {
  int num;

  if (cargc < 6)
    return CMD_OK;

  num = numerictolong(cargv[0], 2);

  if (serverinfo[num].used) {
    freesstring(serverinfo[num].version1);
    freesstring(serverinfo[num].version2);

    serverinfo[num].version1 = getsstring(cargv[3], strlen(cargv[3]));
    serverinfo[num].version2 = getsstring(cargv[5], strlen(cargv[5]));
  }
  return CMD_OK;
}

void serverlist_doversion(void) {
  int i;
  char *num1, *numeric;

  if (mynick == NULL)
    return;

  for (i = 0; i < MAXSERVERS; i++) {
    if (serverlist[i].linkstate == LS_LINKED && serverinfo[i].version1 == NULL) {
      numeric = longtonumeric(mynick->numeric,5);
      num1 = (char*)malloc(strlen(numeric) + 1); /* bleh.. longtonumeric() is using static variables */
      strcpy(num1, numeric);

      irc_send("%s V :%s", num1, longtonumeric(i,2));
      free(num1);
    }
  }
}

void serverlist_hook_newserver(int hook, void *arg) {
  char *num1, *numeric;
  long num = (long)arg;

  if (mynick == NULL)
    return;

  serverinfo[num].used = 1;
  serverinfo[num].ts = getnettime();
  serverinfo[num].version1 = NULL;
  serverinfo[num].version2 = NULL;

  numeric = longtonumeric(mynick->numeric,5);
  num1 = (char*)malloc(strlen(numeric) + 1); /* bleh.. longtonumeric() is using static variables */
  strcpy(num1, numeric);

  irc_send("%s V :%s", num1, longtonumeric(num,2));

  free(num1);
}

void serverlist_hook_lostserver(int hook, void *arg) {
  long num = (long)arg;

  serverinfo[num].used = 0;
  freesstring(serverinfo[num].version1);
  freesstring(serverinfo[num].version2);
}


void serverlist_pingservers(void *arg) {
  int i;
  server *from, *to;
  char fnum[10], tnum[10];
  struct timeval tv;

  if (!mynick)
    return;

  for(i=0;i<MAXSERVERS;i++) {
    to = &serverlist[i];

    if (to->parent == -1)
      continue;

    from = &serverlist[to->parent];

    if (to->linkstate != LS_LINKED || from->linkstate != LS_LINKED)
      continue;

    strcpy(fnum, longtonumeric(to->parent,2));
    strcpy(tnum, longtonumeric(i, 2));

    if (from->parent == -1) { /* Are we the source? */
      (void) gettimeofday(&tv, NULL);
      irc_send("%s RI %s %s %jd %jd :RP", mynumeric->content, tnum, longtonumeric(mynick->numeric,5), (intmax_t)tv.tv_sec, (intmax_t)tv.tv_usec);
    } else {
      strcpy(fnum, longtonumeric(to->parent,2));
      strcpy(tnum, longtonumeric(i, 2));
      irc_send("%s RI %s %s :RP", longtonumeric(mynick->numeric,5), to->name->content, fnum);
    }
  }
}

int serverlist_rpong(void *source, int cargc, char **cargv) {
  int to, lag;
  struct timeval tv;

  if (cargc < 4)
    return CMD_ERROR;

  if (cargc == 5) { /* From target to source server */
    if (strcmp(cargv[4], "RP") != 0)
      return CMD_OK;

    to = numerictolong(source, 2);

    (void) gettimeofday(&tv, NULL);
    lag = (tv.tv_sec - strtoull(cargv[2], NULL, 10)) * 1000 + (tv.tv_usec - strtoull(cargv[3], NULL, 10)) / 1000;
  } else { /* From source server to client */
    if (strcmp(cargv[3], "RP") != 0)
      return CMD_OK;

    to = findserver(cargv[1]);

    if (to == -1)
      return CMD_ERROR;

    lag = atoi(cargv[2]);
  }

  serverinfo[to].lag = lag;

  return CMD_OK;
}

