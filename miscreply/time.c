/* time.c */

#include "miscreply.h"
#include "numeric.h"
#include "../irc/irc.h"
#include "../core/error.h"
#include "../nick/nick.h"

#include <stdio.h>
#include <string.h>



/*
 *  Return the current date in the following format:
 *          "Day Month DD YYYY -- HH:MM +/-HH:MM"
 *     e.g. "Saturday March 27 2010 -- 19:03 +01:00"
 *    where the last part is the timezone offset
 *
 */
char *timelongdate(void)
{
  static char buf[64];
  unsigned int off;
  struct tm *tmp;
  time_t gt, lt;
  size_t n;
  char c;
 
  lt = time(NULL);
 
  tmp = localtime(&lt);
 
  if (tmp == NULL)
    return "";
 
  n = strftime(buf, sizeof(buf) - 8, "%A %B %e %Y -- %H:%M", tmp);
 
  if (!n)
    return "";
 
  tmp = gmtime(&lt);
 
  if (tmp == NULL)
    return "";
 
  gt = mktime(tmp);
 
  if (gt > lt) {
    c = '-';
    off = (unsigned int) (gt - lt);
  } else {
    c = '+';
    off = (unsigned int) (lt - gt);
  }
 
  off = (off / 60) % 6000;
  sprintf(&buf[n], " %c%02u:%02u", c, off / 60, off % 60);
 
  return buf;
}



/* handle remote time request
 *
 * <source numeric> TIME/TI <target server numeric>
 *
 * cargv[0] = target server numeric
 *            can be a * in which case the request is for all servers (snircd extension)
 * 
 */
int handletimemsg(void *source, int cargc, char **cargv) {

  nick *snick;                                          /* struct nick for source nick */

  int i;                                                /* index for serverlist[] */
  long unsigned int timestamp = getnettime();           /* current nettime */
  long unsigned int offset = timestamp - time(NULL);    /* offset nettime to system clock */
  char *sourcenum = (char *)source;                     /* source user numeric */
  char *targetnum = getmynumeric();                     /* target server numeric */
  char *servernum;                                      /* server numeric parameter */
  char *servername = myserver->content;                 /* servername */

  /* check parameters */
  if (cargc < 1) {
    miscreply_needmoreparams(sourcenum, "TIME");
    return CMD_OK;
  }

  /* get the parameter */
  servernum = cargv[0];

  /* from a server? */
  if (IsServer(sourcenum))
    return CMD_OK;

  /* find source user */ 
  if (!(snick = miscreply_finduser(sourcenum, "TIME")))
    return CMD_OK;

  /* request is not me and not for * (all servers) */
  if (!IsMeNum(servernum) && !(servernum[0] == '*' && servernum[1] == '\0')) {

    /* find the server */
    if ((i = miscreply_findservernum(sourcenum, servernum, "TIME")) == -1)
      return CMD_OK;

    targetnum = longtonumeric(i, 2);
    servername = serverlist[i].name->content;

    /* tell user */
    send_snotice(sourcenum, "TIME: Server %s is not a real server.", servername);
  }

  /*
   * 391 RPL_TIME "source 391 target servername timestamp offset :longdate"
   *              "irc.netsplit.net 391 foobar irc.netsplit.net 1269713493 0 :Saturday March 27 2010 -- 19:11 +01:00"
   */
  send_reply(targetnum, RPL_TIME, sourcenum, "%s %lu %lu :%s", servername, timestamp, offset, timelongdate());

  return CMD_OK;
}
