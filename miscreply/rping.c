/* rping.c */

#include "miscreply.h"
#include "msg.h"
#include "numeric.h"
#include "../irc/irc.h"
#include "../core/error.h"
#include "../nick/nick.h"
#include "../server/server.h"

#include <stdio.h>
#include <string.h>
#include <sys/time.h>



/* handle remote rping request
 *
 * RPING from requesting user to start server:
 * <user numeric> RPING/RI <server> <target server numeric> :<comment>
 * 
 * cargv[0] = server
 *            server(mask) as given by user
 *            a * indicates the request is for all servers (possible snircd extension)
 * cargv[1] = target server numeric
 * cargv[2] = comment
 *
 *
 * RPING from start server to destination server:
 * <start server numeric> RPING/RI <target server numeric> <requesting user numeric> <start seconds> <start milliseconds> :<comment>
 *
 * cargv[0] = target server numeric
 *            can be a * in which case the request is for all servers (possible snircd extension)
 * cargv[1] = requesting user numeric
 * cargv[2] = start time in seconds
 * cargv[3] = start time in milliseconds
 * cargv[4] = comment
 *
 */
int handlerpingmsg(void *source, int cargc, char **cargv) {

  nick *snick;                                          /* struct nick for source (oper) nick */

  int i;                                                /* index for serverlist[] */
  int m;                                                /* result of myserver lookup */
  char *sourcenum = (char *)source;                     /* source numeric */
  char *targetnum = getmynumeric();                     /* target server numeric */
  char *server;                                         /* target server parameter */
  char *servername = myserver->content;                 /* servername */
  char *destserver;                                     /* destination server mask */
  char *destnum;                                        /* destination server numeric */
  char *sourceoper;                                     /* requesting oper numeric */
  char *time_s;                                         /* time in seconds */
  char *time_us;                                        /* time in milliseconds */
  char *comment;                                        /* comment by client */
  struct timeval tv;                                    /* get start time in seconds and milliseconds */

  /* from server */
  if (IsServer(sourcenum)) {

    /* check parameters */
    if (cargc < 5) {
      miscreply_needmoreparams(sourcenum, "RPING");
      return CMD_OK;
    }

    /* set the parameters */
    server = cargv[0];
    sourceoper = cargv[1];
    time_s = cargv[2];
    time_us = cargv[3];
    comment = cargv[4];

    /* find source server */
    if (miscreply_findserver(sourcenum, "RPING") == -1)
      return CMD_OK;

    /* find source user */ 
    if (!(snick = miscreply_finduser(sourceoper, "RPING")))
      return CMD_OK;

    /* request is not for * (all servers) */
    if (!(server[0] == '*' && server[1] == '\0')) {

      /* destination server not found */
      if ((i = miscreply_findservernum(sourceoper, server, "RPING")) == -1)
        return CMD_OK;

      targetnum = longtonumeric(i, 2);
      servername = serverlist[i].name->content;

      /* not me, tell user */
      if (!IsMeNum(targetnum))
        send_snotice(sourceoper, "RPING: Server %s is not a real server.", servername);
    }

    /* send */
    irc_send("%s %s %s %s %s %s :%s", targetnum, TOK_RPONG, sourcenum,
      sourceoper, time_s, time_us, comment);
  }

  /* from user */
  else if (IsUser(sourcenum)) {

    /* check parameters */
    if (cargc < 3) {
      miscreply_needmoreparams(sourcenum, "RPING");
      return CMD_OK;
    }

    /* set the parameters */
    destserver = cargv[0];
    server = cargv[1];
    comment = cargv[2];

    /* find source user */ 
    if (!(snick = miscreply_finduser(sourcenum, "RPING")))
      return CMD_OK;

    /* request is for * (all servers) */
    if ((destserver[0] == '*' && destserver[1] == '\0'))
      destnum = "*";

    /* find destination server */
    else if ((i = miscreply_findservermatch(sourcenum, destserver)) == -1)
      return CMD_OK;

    /* destination one of my servers? */   
    else if ((m = miscreply_myserver(i))) {
      servername = serverlist[i].name->content;

      /* tell user, and return RPONG with delay 0 */
      if (m == 1)
        send_snotice(sourcenum, "RPING: Server %s is me.", servername);
      else 
        send_snotice(sourcenum, "RPING: Server %s is not a real server.", servername);
      irc_send("%s %s %s %s 0 :%s", targetnum, TOK_RPONG, sourcenum, servername, comment);
      return CMD_OK;
    }

    /* found valid destination server */
    else 
      destnum = longtonumeric(i, 2);

    /* get starttime */
    gettimeofday(&tv, NULL);

    /* send */
    irc_send("%s %s %s %s %lu %lu :%s", targetnum, TOK_RPING, destnum,
      sourcenum, tv.tv_sec, tv.tv_usec, comment);
  }


  /* trouble not from server and not from user numeric? */
  else {
    Error("miscreply", ERR_WARNING,
      "RPING from odd numeric source %s (not a server and not user numeric)", sourcenum);
    return CMD_OK;
  }

  return CMD_OK;
}
