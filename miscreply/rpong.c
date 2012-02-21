/* rpong.c */

#include "miscreply.h"
#include "../irc/irc.h"
#include "../core/error.h"
#include "../server/server.h"

#include <stdio.h>
#include <sys/time.h>



/* handle remote rpong request
 *
 * RPONG from destination server to start server:
 * <destination server numeric> RPONG/RO <start server name> <requesting user numeric> <start seconds> <start milliseconds> :<comment>
 *
 * cargv[0] = start server name (not numeric!)
 * cargv[1] = request user numeric
 * cargv[2] = start time in seconds
 * cargv[3] = start time in milliseconds
 * cargv[4] = comment
 *
 *
 * RPONG from start server to requesting user:
 * <start server numeric> RPONG/RO <requesting user numeric> <destination server name> <ping time> :<comment>
 *
 * cargv[0] = requesting user numeric
 * cargv[1] = destination server name (the pinged server, not numeric!)
 * cargv[2] = ping time in milliseconds
 * cargv[3] = comment
 *
 */
int handlerpongmsg(void *source, int cargc, char **cargv) {

  nick *snick;                        /* struct nick for source oper nick */
  long i;                             /* index for serverlist[]  */
  char *sourcenum = (char *)source;   /* source numeric */
  char *sourceoper;                   /* requesting operator numeric */
  char *time_s;                       /* time in seconds */
  char *time_us;                      /* time in milliseconds */
  char *comment;                      /* comment by client */
  char *servername;                   /* name of source server */
  struct timeval tv;                  /* get time */
  static char ping[18];               /* elapsed time */

  /* check parameters */
  if (cargc < 4) {
    miscreply_needmoreparams(sourcenum, "RPONG");
    return CMD_OK;
  }

  /* from pinged server to source server */
  if (cargc > 4) {

    /* get the parameters */
    sourceoper = cargv[1];
    time_s = cargv[2];
    time_us = cargv[3];
    comment = cargv[4];

    /* find source server */
    if ((i = miscreply_findserver(sourcenum, "RPING")) == -1)
      return CMD_OK;
    servername = serverlist[i].name->content;

    /* find requesting oper */
    if (!(snick = miscreply_finduser(sourceoper, "RPONG")))
      return CMD_OK;

    /* get time */
    gettimeofday(&tv, NULL);

    /* calc ping */
    sprintf(ping, "%ld", (tv.tv_sec - atoi(time_s)) * 1000 + (tv.tv_usec - atoi(time_us)) / 1000);

    /* send */
    irc_send("%s RO %s %s %s :%s", getmynumeric(), sourceoper,
      servername, ping, comment);
  }

  return CMD_OK;
}
