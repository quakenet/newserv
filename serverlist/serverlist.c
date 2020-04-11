#include "../core/schedule.h"
#include "../irc/irc.h"
#include "../lib/irc_string.h"
#include "../localuser/localuserchannel.h"
#include "../control/control.h"
#include "../usercount/usercount.h"
#include "../lib/version.h"
#include "../serverlist/serverlist.h"
#include "../core/config.h"
#include "../lib/strlfunc.h"

MODULE_VERSION("")

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/time.h>
#include <pcre.h>

int cmd_serverlist(void *sender, int cargc, char **cargv);
void serverlist_hook_newserver(int hook, void *arg);
void serverlist_hook_lostserver(int hook, void *arg);
int serverlist_versionreply(void *source, int cargc, char **cargv);
void serverlist_pingservers(void *arg);
int serverlist_rpong(void *source, int cargc, char **cargv);

const flag servertypeflags[] = {
  { 'c', SERVERTYPEFLAG_CLIENT_SERVER },
  { 'h', SERVERTYPEFLAG_HUB },
  { 's', SERVERTYPEFLAG_SERVICE },
  { 'Q', SERVERTYPEFLAG_CHANSERV },
  { 'S', SERVERTYPEFLAG_SPAMSCAN },
  { 'X', SERVERTYPEFLAG_CRITICAL_SERVICE },
  { '\0', 0 }
};

struct {
  int used;
  int lag;
  sstring *version1;
  sstring *version2;
  flag_t type;
} serverinfo[MAXSERVERS];

void serverlist_doversion(void);

static sstring *s_server, *q_server;
static pcre *service_re, *hub_re, *not_client_re;

static pcre *compilefree(sstring *re) {
  const char *err;
  int erroffset;
  pcre *r;

  r = pcre_compile(re->content, 0, &err, &erroffset, NULL);
  if(r == NULL) {
    Error("serverlist", ERR_WARNING, "Unable to compile RE %s (offset: %d, reason: %s)", re->content, erroffset, err);
    freesstring(re);
    return NULL;
  }

  freesstring(re);
  return r;
}

void _init(void) {
  registercontrolhelpcmd("serverlist",NO_OPER,1,&cmd_serverlist,"Usage: serverlist [pattern]\nShows all currently connected servers");
  /* hooks for serverlist */
  registerhook(HOOK_SERVER_NEWSERVER, &serverlist_hook_newserver);
  registerhook(HOOK_SERVER_LOSTSERVER, &serverlist_hook_lostserver);
  int i;

  q_server = getcopyconfigitem("serverlist", "q_server", "CServe.quakenet.org", HOSTLEN);
  s_server = getcopyconfigitem("serverlist", "s_server", "services2.uk.quakenet.org", HOSTLEN);

  service_re = compilefree(getcopyconfigitem("serverlist", "service_re", "^services\\d*\\..*$", 256));
  hub_re = compilefree(getcopyconfigitem("serverlist", "hub_re", "^hub\\d*\\..*$", 256));
  not_client_re = compilefree(getcopyconfigitem("serverlist", "not_client_re", "^(testserv\\d*\\.).*$", 256));

  for (i = 0; i < MAXSERVERS; i++) {
    if (serverlist[i].linkstate == LS_LINKED)
      serverinfo[i].used = 1;
    else
      serverinfo[i].used = 0;

    serverinfo[i].lag = -1;
    serverinfo[i].version1 = NULL;
    serverinfo[i].version2 = NULL;
    serverinfo[i].type = getservertype(&serverlist[i]);
  }
  registernumerichandler(351, &serverlist_versionreply, 2);
  registerserverhandler("RO", &serverlist_rpong, 4);

  schedulerecurring(time(NULL)+1, 0, 60, &serverlist_pingservers, NULL);
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

  freesstring(q_server);
  freesstring(s_server);
  pcre_free(service_re);
  pcre_free(hub_re);
  pcre_free(not_client_re);
}

int cmd_serverlist(void *sender, int cargc, char **cargv) {
  nick *np = (nick*)sender;
  int a, i, ucount, acount, scount;
  char lagstr[20];
  char buf[512];

  controlreply(np, "%-7s %-35s %5s/%5s/%-7s %-7s %-7s %-20s %-8s %-20s", "Numeric", "Hostname", "ECl", "Cl", "MaxCl", "Flags", "Type", "Connected for", "Lag", "Version");

  scount = acount = 0;

  for (i = 0; i < MAXSERVERS; i++) {
    if (serverlist[i].linkstate == LS_LINKED && (cargc < 1 || match2strings(cargv[0], serverlist[i].name->content))) {
      ucount = 0;

      for (a = 0; a <= serverlist[i].maxusernum; a++)
        if (servernicks[i][a] != NULL)
          ucount++;

      acount += ucount;
      scount++;

      if (serverinfo[i].lag == -1)
        strcpy(lagstr, "-");
      else
        snprintf(lagstr, sizeof(lagstr), "%d", serverinfo[i].lag);

      strlcpy(buf, printflags(serverinfo[i].type, servertypeflags), sizeof(buf));

      controlreply(np, "%-7d %-35s %5d/%5d/%-7d %-7s %-7s %-20s %-8s %-20s - %s", i, serverlist[i].name->content,
            servercount[i], ucount, serverlist[i].maxusernum,
            printflags(serverlist[i].flags, smodeflags), buf,
            longtoduration(getnettime() - serverlist[i].ts, 0),
            lagstr,
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

  serverinfo[num].used = 1;
  serverinfo[num].version1 = NULL;
  serverinfo[num].version2 = NULL;
  serverinfo[num].type = getservertype(&serverlist[num]);

  if (mynick == NULL) /* bleh this is buggy... doesn't do it when mynick appears */
    return;

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

flag_t getservertype(server *server) {
  flag_t result = 0;
  char *server_name;
  int server_len;

  if(server == NULL || server->name == NULL)
    return 0;

  server_name = server->name->content;
  server_len = server->name->length;

  if(server->flags & SMODE_SERVICE)
    result|=SERVERTYPEFLAG_SERVICE;

  if(!strcmp(server_name, q_server->content)) {
    result|=SERVERTYPEFLAG_CHANSERV;
    result|=SERVERTYPEFLAG_CRITICAL_SERVICE;
  } else if(!strcmp(server_name, s_server->content)) {
    result|=SERVERTYPEFLAG_SPAMSCAN;
    result|=SERVERTYPEFLAG_CRITICAL_SERVICE;
  } else {
    if(service_re && (pcre_exec(service_re, NULL, server_name, server_len, 0, 0, NULL, 0) >= 0)) {
      /* matches service re */
      if((server->flags & SMODE_SERVICE) == 0) {
        /* is not a service */
        Error("serverlist", ERR_WARNING, "Non-service server (%s) matched service RE.", server_name);
      }
    } else {
      /* does not match service re */
      if((server->flags & SMODE_SERVICE) != 0) {
        result|=SERVERTYPEFLAG_SERVICE;
        Error("serverlist", ERR_WARNING, "Service server (%s) that does not match service RE.", server_name);
      }
    }
  }

  if(hub_re && (pcre_exec(hub_re, NULL, server_name, server_len, 0, 0, NULL, 0) >= 0)) {
    if((server->flags & SMODE_HUB) != 0) {
      result|=SERVERTYPEFLAG_HUB;
    } else {
      Error("serverlist", ERR_WARNING, "Server matched hub re but isn't a hub (%s).", server_name);
    }
  }

  if(not_client_re && (pcre_exec(not_client_re, NULL, server_name, server_len, 0, 0, NULL, 0) >= 0)) {
    /* noop */
  } else if(result == 0) {
    result|=SERVERTYPEFLAG_CLIENT_SERVER;
  }

  return result;
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
