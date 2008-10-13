#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "../core/hooks.h"
#include "../core/config.h"
#include "../core/error.h"
#include "../control/control.h"
#include "../lib/sha1.h"
#include "../lib/hmac.h"
#include "../lib/irc_string.h"
#include "../core/schedule.h"
#include "../server/server.h"
#include "../xsb/xsb.h"
#include "trusts.h"

static int syncing, synced;
static sstring *smasterserver;

static unsigned int curlineno, totallines;
static SHA1_CTX s;

void trusts_replication_createtables(void);
void trusts_replication_swap(void);
void trusts_replication_complete(int);

static int masterserver(void *source);

static void __abandonreplication(const char *fn, int line, char *error, ...) {
  va_list ap;
  char buf[512], buf2[600];

  va_start(ap, error);
  vsnprintf(buf, sizeof(buf), error, ap);
  va_end(ap);

  snprintf(buf2, sizeof(buf2), "Error replicating (function: %s, line: %d): %s", fn, line, buf);

  Error("trusts_slave", ERR_ERROR, "%s", buf2);

  syncing = 0;

  /* TODO: warn IRC */
}

#define abandonreplication(x, ...) __abandonreplication(__FUNCTION__, __LINE__, x , # __VA_ARGS__)

void trusts_replication_complete(int error) {
  if(error) {
    abandonreplication("final replication stage: error %d", error);
    return;
  }
  Error("trusts_slave", ERR_INFO, "Replication complete!");
 
  if(!trusts_loaddb()) {
    abandonreplication("couldn't load database");
    return;
  }

  synced = 1;
}

static char *extractline(char *buf, int reset, int update, int final) {
  unsigned char n = '\n';
  int chars = 0;
  unsigned int lineno, id;
  static unsigned int curid;

  if(sscanf(buf, "%u %u %n", &id, &lineno, &chars) != 2) {
    abandonreplication("bad number for sscanf result");
    return NULL;
  }

  if(chars <= 0) {
    abandonreplication("bad number of characters");
    return NULL;
  }

  if(reset && (lineno != 1)) {
    abandonreplication("bad initial line number");
    return NULL;
  }

  if(update) {
    if(reset) {
      curlineno = 2;
      curid = id;
      SHA1Init(&s);
    } else {
      /* will happen if two are sent at once, but that's ok */
      if(id != curid)
        return NULL;

      if(lineno != curlineno) {
        abandonreplication("unexpected line number (%u vs. %u)", lineno, curlineno);
        return NULL;
      }
      if(lineno > totallines) {
        abandonreplication("too many lines");
        return NULL;
      }

      curlineno++;

    }

    if(!final) {
      SHA1Update(&s, (unsigned char *)buf, strlen(buf));
      SHA1Update(&s, &n, 1);
    }
  }

  return &buf[chars];
}

/* trinit id lineno force totallines */
static int xsb_trinit(void *source, int argc, char **argv) {
  char *buf;
  unsigned int forced;

  if(!masterserver(source))
    return CMD_ERROR;

  if(argc < 1) {
    abandonreplication("bad number of args");
    return CMD_ERROR;
  }

  buf = extractline(argv[0], 1, 0, 1);
  if(!buf)
    return CMD_ERROR;

  if((sscanf(buf, "%u %u", &forced, &totallines) != 2)) {
    abandonreplication("bad number for sscanf result");
    return CMD_ERROR;
  }

  if(totallines < 2) {
    abandonreplication("bad number of lines");
    return CMD_ERROR;
  }

  if(!forced && synced)
    return CMD_OK;

  if(!extractline(argv[0], 1, 1, 0))
    return CMD_ERROR;

  trusts_replication_createtables();

  syncing = 1;

  Error("trusts_slave", ERR_INFO, "Replication in progress. . .");
  return CMD_OK;
}

/* trdata id lines type data */
static int xsb_trdata(void *source, int argc, char **argv) {
  char *buf;

  if(!syncing)
    return CMD_OK;

  if(!masterserver(source))
    return CMD_ERROR;

  if(argc < 1) {
    abandonreplication("bad number of args");
    return CMD_ERROR;
  }

  buf = extractline(argv[0], 0, 1, 0);
  if(!buf)
    return CMD_ERROR;

  if(buf[0] && (buf[1] == ' ')) {
    if(buf[0] == 'G') {
      trustgroup tg;
      if(!parsetg(&buf[2], &tg, 0)) {
        abandonreplication("bad trustgroup line: %s", buf);
        return CMD_ERROR;
      }
      trustsdb_inserttg("replication_groups", &tg);

      freesstring(tg.name);
      freesstring(tg.createdby);
      freesstring(tg.contact);
      freesstring(tg.comment);

    } else if(buf[0] == 'H') {
      unsigned int tgid;
      trusthost th;

      if(!parseth(&buf[2], &th, &tgid, 0)) {
        abandonreplication("bad trusthost line: %s", buf);
        return CMD_ERROR;
      }
      trustsdb_insertth("replication_hosts", &th, tgid);
    } else {
      abandonreplication("bad trust type: %c", buf[0]);

      return CMD_ERROR;
    }
  } else {
    abandonreplication("malformed line: %s", buf);
  }

  return CMD_OK;
}

/* trfini id lines sha */
static int xsb_trfini(void *source, int argc, char **argv) {
  char *buf, digestbuf[SHA1_DIGESTSIZE * 2 + 1];
  unsigned char digest[SHA1_DIGESTSIZE];

  if(!syncing)
    return CMD_OK;

  if(!masterserver(source))
    return CMD_ERROR;

  if(argc < 1) {
    abandonreplication("bad number of args");
    return CMD_ERROR;
  }

  buf = extractline(argv[0], 0, 1, 1);
  if(!buf)
    return CMD_ERROR;

  if((totallines + 1) != curlineno) {
    abandonreplication("wrong number of lines received: %u vs. %u", totallines, curlineno - 1);
    return CMD_ERROR;
  }

  SHA1Final(digest, &s);
  if(strcasecmp(hmac_printhex(digest, digestbuf, SHA1_DIGESTSIZE), buf)) {
    abandonreplication("digest mismatch");
    return CMD_ERROR;
  }

  Error("trusts_slave", ERR_INFO, "Data verification successful.");

  trusts_replication_swap();

  synced = 1;
  syncing = 0;

  return CMD_OK;
}

static int xsb_traddgroup(void *source, int argc, char **argv) {
  trustgroup tg, *otg;

  if(!synced)
    return CMD_OK;

  if(!masterserver(source))
    return CMD_ERROR;

  if(argc < 1) {
    abandonreplication("bad number of arguments");
    return CMD_ERROR;
  }

  if(!parsetg(argv[0], &tg, 0)) {
    abandonreplication("bad trustgroup line: %s", argv[0]);
    return CMD_ERROR;
  }

  otg = tg_copy(&tg);

  freesstring(tg.name);
  freesstring(tg.createdby);
  freesstring(tg.contact);
  freesstring(tg.comment);

  if(!otg) {
    abandonreplication("unable to add trustgroup");
    return CMD_ERROR;
  }

  return CMD_OK;
}

static int xsb_traddhost(void *source, int argc, char **argv) {
  unsigned int tgid;
  trusthost th;

  if(!synced)
    return CMD_OK;

  if(!masterserver(source))
    return CMD_ERROR;

  if(argc < 1) {
    abandonreplication("bad number of arguments");
    return CMD_ERROR;
  }

  if(!parseth(argv[0], &th, &tgid, 0)) {
    abandonreplication("bad trusthost line: %s", argv[0]);
    return CMD_ERROR;
  }

  th.group = tg_inttotg(tgid);
  if(!th.group) {
    abandonreplication("unable to lookup trustgroup");
    return CMD_ERROR;
  }

  if(!th_copy(&th)) {
    abandonreplication("unable to add trusthost");
    return CMD_ERROR;
  }

  return CMD_OK;
}

static int xsb_trdelhost(void *source, int argc, char **argv) {
  if(!synced)
    return CMD_OK;

  if(!masterserver(source))
    return CMD_ERROR;

  if(argc < 1) {
    abandonreplication("bad number of arguments");
    return CMD_ERROR;
  }

  return CMD_OK;
}

static int xsb_trdelgroup(void *source, int argc, char **argv) {
  if(!synced)
    return CMD_OK;

  if(!masterserver(source))
    return CMD_ERROR;

  if(argc < 1) {
    abandonreplication("bad number of arguments");
    return CMD_ERROR;
  }

  return CMD_OK;
}

static int loaded, masternumeric = -1;
static void *syncsched;

static int masterserver(void *source) {
  nick *np = source;
  int home = homeserver(np->numeric);

  if(home < 0)
    return 0;

  if(home != masternumeric) {
    Error("trusts_slave", ERR_WARNING, "Command from server that isn't a master: %s", serverlist[home].name->content);
    return 0;
  }

  return 1;
}

static void checksynced(void *arg) {
  if(!synced && !syncing)
    xsb_broadcast("trrequeststart", NULL, "%s", "");
}

static int trusts_cmdtrustresync(void *source, int argc, char **argv) {
  nick *np = source;

  syncing = synced = 0;

  checksynced(NULL);
  controlreply(np, "Synchronisation request sent.");

  return CMD_OK;
}

static void __serverlinked(int hooknum, void *arg) {
  int servernum = (int)(long)arg;

  if(!ircd_strcmp(serverlist[servernum].name->content, smasterserver->content)) {
    masternumeric = servernum;
    syncing = synced = 0;
    checksynced(NULL);
  }
}

void _init(void) {
  sstring *m;

  m = getconfigitem("trusts", "master");
  if(m && (atoi(m->content) != 0)) {
    Error("trusts_slave", ERR_ERROR, "Not a slave server, not loaded.");
    return;
  }

  smasterserver = getcopyconfigitem("trusts", "masterserver", "", 255);
  if(!smasterserver || !smasterserver->content || !smasterserver->content[0]) {
    Error("trusts_slave", ERR_ERROR, "No master server defined.");
    freesstring(smasterserver);
    return;
  }

  masternumeric = findserver(smasterserver->content);

  loaded = 1;

  registercontrolhelpcmd("trustresync", NO_DEVELOPER, 0, trusts_cmdtrustresync, "Usage: trustresync");

  xsb_addcommand("trinit", 1, xsb_trinit);
  xsb_addcommand("trdata", 1, xsb_trdata);
  xsb_addcommand("trfini", 1, xsb_trfini);
  xsb_addcommand("traddhost", 1, xsb_traddhost);
  xsb_addcommand("traddgroup", 1, xsb_traddgroup);
  xsb_addcommand("trdelhost", 1, xsb_trdelhost);
  xsb_addcommand("trdelgroup", 1, xsb_trdelgroup);

  registerhook(HOOK_SERVER_LINKED, __serverlinked);
  syncsched = schedulerecurring(time(NULL)+5, 0, 60, checksynced, NULL);
  
  if(trusts_fullyonline())
    checksynced(NULL);
}

void _fini(void) {
  if(!loaded)
    return;

  freesstring(smasterserver);

  deregistercontrolcmd("trustresync", trusts_cmdtrustresync);

  xsb_delcommand("trinit", xsb_trinit);
  xsb_delcommand("trdata", xsb_trdata);
  xsb_delcommand("trfini", xsb_trfini);  
  xsb_delcommand("traddhost", xsb_traddhost);
  xsb_delcommand("traddgroup", xsb_traddgroup);
  xsb_delcommand("trdelhost", xsb_trdelhost);
  xsb_delcommand("trdelgroup", xsb_trdelgroup);

  deregisterhook(HOOK_SERVER_LINKED, __serverlinked);

  deleteschedule(syncsched, checksynced, NULL);

  trusts_closedb(0);
}
