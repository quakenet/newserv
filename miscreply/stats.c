/* stats.c */

#include "miscreply.h"
#include "../lib/irc_string.h"
#include "../irc/irc.h"
#include "../core/error.h"

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

  nick *snick;                        /* struct nick for source nick */
  char *sourcenum = (char *)source;   /* source user numeric */
  char *statstype;                    /* stats paramenter */

  /* check parameters */
  if (cargc < 2) {
    miscreply_needmoreparams(sourcenum, "STATS");
    return CMD_OK;
  }

  /* get the parameters */
  statstype = cargv[0];

  /* find source user */ 
  if (!(snick = miscreply_finduser(sourcenum, "STATS")))
    return CMD_OK;


  /* stats commands */
  if (!strcmp(statstype, "m") || !strcasecmp(statstype, "commands")) {
    stats_commands(sourcenum);
  }


  /* stats features */
  else if (!strcasecmp(statstype, "f") ||
    !strcasecmp(statstype, "features") || !strcasecmp(statstype, "featuresall")) {
    /*
     * 238 RPL_STATSFLINE "source 238 target F feature value"
     *                    "irc.netsplit.net 217 foobar F HIDDEN_HOST users.quakenet.org"
     */
    if (HIS_HIDDENHOST)
      irc_send("%s 238 %s F HIDDEN_HOST %s", getmynumeric(), sourcenum, HIS_HIDDENHOST);
    if (HIS_SERVERNAME)
      irc_send("%s 238 %s F HIS_SERVERNAME %s", getmynumeric(), sourcenum, HIS_SERVERNAME);
    if (HIS_SERVERDESC)
      irc_send("%s 238 %s F HIS_SERVERINFO %s", getmynumeric(), sourcenum, HIS_SERVERDESC);
  }


  /* stats ports
   *   NOTE: operserv uses this for lag check prior to sending out SETTIME
   *         as long as operserv is used, newserv must reply to STATS p
   */
  else if (!strcasecmp(statstype, "p") || !strcasecmp(statstype, "ports")) {
    /*
     * 217 RPL_STATSPLINE "source 217 target P port connection_count flags state"
     *                    "irc.netsplit.net 217 foobar P 6667 10435 C active"
     */
    irc_send("%s 217 %s P none 0 :0x2000", getmynumeric(), sourcenum);
  }


  /* stats uptime */
  else if (!strcmp(statstype, "u") || !strcasecmp(statstype, "uptime")) {
    /*
     * 242 RPL_STATSUPTIME "source 242 target :Server Up N days HH:NN:SS"
     *                     "irc.netsplit.net 217 foobar :Server Up 0 days, 0:28:56"
     */
    irc_send("%s 242 %s :Server Up %s", getmynumeric(), sourcenum, longtoduration(time(NULL)-starttime, 0));
    /*
     * 250 RPL_STATSCONN "source 250 target :Highest connection count: count (count_clients clients)"
     *                   "irc.netsplit.net 250 foobar :Highest connection count: 25001 (25000 clients)"
     */
    irc_send("%s 250 %s :Highest connection count: 10 (9 clients)", getmynumeric(), sourcenum);
  }


  /* tell user what stats we have */
  else {
    statstype = "*";
    /* NOTICE/O */
    irc_send("%s O %s :m (commands) - Message usage information.", getmynumeric(), sourcenum);
    irc_send("%s O %s :f (features) - Feature settings.", getmynumeric(), sourcenum);
    irc_send("%s O %s :p (ports) - Listening ports.", getmynumeric(), sourcenum);
    irc_send("%s O %s :u (uptime) - Current uptime & highest connection count.", getmynumeric(), sourcenum);
  }


  /* end of stats
   *
   * 219 RPL_ENDOFSTATS "source 219 target type :End of /STATS report"
   *                    "irc.netsplit.net 391 foobar P :End of /STATS report"
   */
  irc_send("%s 219 %s %s :End of /STATS report", getmynumeric(), sourcenum, statstype);

  return CMD_OK;
}
