/* privs.c */

#include "miscreply.h"
#include "numeric.h"
#include "../irc/irc.h"
#include "../core/error.h"
#include "../nick/nick.h"

#include <string.h>



/* handle remote privs request
 *
 * <source numeric> PRIVS/PR <target user numeric>
 *
 * cargv[0] = target user numeric
 *            in the client protocol it is a space separated list of one or more nicks
 *            but in P10 it is always one target user numeric
 * 
 */
int handleprivsmsg(void *source, int cargc, char **cargv) {

  nick *snick;                                                  /* struct nick for source nick */
  nick *tnick;                                                  /* struct nick for target nick */

  char *sourcenum = (char *)source;                             /* source user numeric */
  char *targetnum = getmynumeric();                             /* target server numeric */
  char *privs = "IDDQD IDKFA IDCLIP IDDT IDCHOPPERS";           /* privileges */

  /*
   * cheat codes from DOOM I & II
   *
   * IDDQD      - god mode
   * IDKFA      - maximum ammo
   * IDCLIP     - walk through walls 
   * IDBEHOLD   - toggle selected powerup
   * IDDT       - show entire map, show all monsters
   * IDCHOPPERS - get chainsaw
   *
   */

  /* check parameters */
  if (cargc < 1) {
    miscreply_needmoreparams(sourcenum, "PRIVS");
    return CMD_OK;
  }

  /* get the parameter */
  targetnum = cargv[0];

  /* from a server? */
  if (IsServer(sourcenum))
    return CMD_OK;

  /* find source user */ 
  if (!(snick = miscreply_finduser(sourcenum, "PRIVS")))
    return CMD_OK;

  /* find target user */
  if (!(tnick = getnickbynumericstr(targetnum))) {
    Error("miscreply", ERR_WARNING,
      "PRIVS request from %s [%s] for unknown numeric %s",
      snick->nick, sourcenum, targetnum);
    return CMD_OK;
  }

  /* get numeric of target's server */
  targetnum = longtonumeric(homeserver((tnick)->numeric), 2);

  if (!IsOper(tnick))
    privs = "";

  /* send the privs */
  /*
   * 270 RPL_PRIVS "source 270 target nick :privs"
   *               "irc.netsplit.net 270 foobar barfoo :CHAN_LIMIT SHOW_INVIS SHOW_ALL_INVIS KILL"
   */
  send_reply(targetnum, RPL_PRIVS, sourcenum, "%s :%s", tnick->nick, privs);

  return CMD_OK;
}
