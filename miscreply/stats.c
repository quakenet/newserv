/* stats.c */

#include "miscreply.h"
#include "msg.h"
#include "numeric.h"
#include "../lib/irc_string.h"
#include "../irc/irc.h"
#include "../core/error.h"
#include "../nick/nick.h"

#include <string.h>
#include <strings.h>



/* handle remote stats request
 *
 * <source numeric> STATS/R <stats type> <target server numeric> [<extra param>]
 *
 * cargv[0] = stats type
 * cargv[1] = target server numeric
 * cargv[2] = extra param (ignored)
 *
 */
int handlestatsmsg(void *source, int cargc, char **cargv) {

  nick *snick;                                          /* struct nick for source nick */

  int i;                                                /* index for serverlist[] */
  char *sourcenum = (char *)source;                     /* source user numeric */
  char *targetnum = getmynumeric();                     /* target server numeric */
  char *statstype;                                      /* stats paramenter */
  char *servernum;                                      /* server numeric parameter */
  char *servername;                                     /* servername */

  /* check parameters */
  if (cargc < 2) {
    miscreply_needmoreparams(sourcenum, "STATS");
    return CMD_OK;
  }

  /* get the parameters */
  statstype = cargv[0];
  servernum = cargv[1];

  /* from a server? */
  if (IsServer(sourcenum))
    return CMD_OK;

  /* find source user */ 
  if (!(snick = miscreply_finduser(sourcenum, "STATS")))
    return CMD_OK;

  /* not for me */
  if (!IsMeNum(servernum)) {

    /* find the server */
    if ((i = miscreply_findservernum(sourcenum, servernum, "STATS")) == -1)
      return CMD_OK;

    targetnum = longtonumeric(i, 2);
    servername = serverlist[i].name->content;

    /* tell user */
    send_snotice(sourcenum, "STATS: Server %s is not a real server.", servername);

  }


  /* stats commands */
  if (!strcmp(statstype, "m") || !strcasecmp(statstype, "commands")) {
    stats_commands(targetnum, sourcenum);
  }


  /* stats features */
  else if (!strcasecmp(statstype, "f") ||
    !strcasecmp(statstype, "features") || !strcasecmp(statstype, "features")) {
    /*
     * 238 RPL_STATSFLINE "source 238 target F feature value"
     *                    "irc.netsplit.net 217 foobar F HIDDEN_HOST users.quakenet.org"
     */
    if (HIS_HIDDENHOST)
      send_reply(targetnum, RPL_STATSFLINE, sourcenum, "F HIDDEN_HOST %s", HIS_HIDDENHOST);
    if (HIS_SERVERNAME)
      send_reply(targetnum, RPL_STATSFLINE, sourcenum, "F HIS_SERVERNAME %s", HIS_SERVERNAME);
    if (HIS_SERVERDESC)
      send_reply(targetnum, RPL_STATSFLINE, sourcenum, "F HIS_SERVERINFO %s", HIS_SERVERDESC);
  }


  /* stats ports */
  else if (!strcasecmp(statstype, "p") || !strcasecmp(statstype, "ports")) {
    /*
     * 217 RPL_STATSPLINE "source 217 target P port connection_count flags state"
     *                    "irc.netsplit.net 217 foobar P 6667 10435 C active"
     */
    send_reply(targetnum, RPL_STATSPLINE, sourcenum, "P none 0 :0x2000");
  }


  /* stats uptime */
  else if (!strcmp(statstype, "u") || !strcasecmp(statstype, "uptime")) {
    /*
     * 242 RPL_STATSUPTIME "source 242 target :Server Up N days HH:NN:SS"
     *                     "irc.netsplit.net 217 foobar :Server Up 0 days, 0:28:56"
     */
    send_reply(targetnum, RPL_STATSUPTIME, sourcenum, ":Server Up %s", longtoduration(time(NULL)-starttime, 0));
    /*
     * 250 RPL_STATSCONN "source 250 target :Highest connection count: count (count_clients clients)"
     *                   "irc.netsplit.net 250 foobar :Highest connection count: 25001 (25000 clients)"
     */
    send_reply(targetnum, RPL_STATSCONN, sourcenum, ":Highest connection count: 10 (9 clients)");
  }


  /* tell user what stats we have */
  else {
    statstype = "*";
    send_snotice(sourcenum, "m (commands) - Message usage information.");
    send_snotice(sourcenum, "f (features) - Feature settings.");
    send_snotice(sourcenum, "p (ports) - Listening ports.");
    send_snotice(sourcenum, "u (uptime) - Current uptime & highest connection count.");
  }


  /* end of stats
   *
   * 219 RPL_ENDOFSTATS "source 219 target type :End of /STATS report"
   *                    "irc.netsplit.net 391 foobar P :End of /STATS report"
   */
  send_reply(targetnum, RPL_ENDOFSTATS, sourcenum, "%s :End of /STATS report", statstype);

  return CMD_OK;
}
