/* rping.c */

#include "miscreply.h"
#include "../irc/irc.h"
#include "../core/error.h"

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
 *            a * indicates the request is for all servers (future snircd extension?)
 * cargv[1] = target server numeric
 * cargv[2] = comment
 *
 *
 * RPING from start server to destination server:
 * <start server numeric> RPING/RI <target server numeric> <requesting user numeric> <start seconds> <start milliseconds> :<comment>
 *
 * cargv[0] = target server numeric
 *            can be a * in which case the request is for all servers (future snircd extension?)
 * cargv[1] = requesting user numeric
 * cargv[2] = start time in seconds
 * cargv[3] = start time in milliseconds
 * cargv[4] = comment
 *
 */
int handlerpingmsg(void *source, int cargc, char **cargv) {

  nick *snick;                        /* struct nick for source nick */
  long i;                             /* index for serverlist[] */
  char *sourcenum = (char *)source;   /* source numeric */
  char *server;                       /* target server parameter */
  char *destserver;                   /* destination server mask */
  char *destnum;                      /* destination server numeric */
  char *sourceoper;                   /* requesting operator numeric */
  char *time_s;                       /* time in seconds */
  char *time_us;                      /* time in milliseconds */
  char *comment;                      /* comment by client */
  struct timeval tv;                  /* get start time in seconds and milliseconds */

  /* from server */
  if (strlen(sourcenum) == 2) {

    /* check parameters */
    if (cargc < 5) {
      miscreply_needmoreparams(sourcenum, "RPING");
      return CMD_OK;
    }

    /* get the parameters */
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

    /* send RPONG to operator */
    irc_send("%s RO %s %s %s %s :%s", getmynumeric(), sourcenum,
      sourceoper, time_s, time_us, comment);
  }

  /* from user */
  else if (strlen(sourcenum) == 5) {

    /* check parameters */
    if (cargc < 3) {
      miscreply_needmoreparams(sourcenum, "RPING");
      return CMD_OK;
    }

    /* get the parameters */
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

    /* destination is me
     *   do the same as ircu in this case, nothing
     */
    else if (i == mylongnum)
      return CMD_OK;

    /* found valid destination server */
    else 
      destnum = longtonumeric(i, 2);

    /* get starttime */
    gettimeofday(&tv, NULL);

    /* send RPING to server */
    irc_send("%s RI %s %s %lu %lu :%s", getmynumeric(), destnum,
      sourcenum, tv.tv_sec, tv.tv_usec, comment);
  }

  return CMD_OK;
}
