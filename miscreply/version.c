/* version.c */

#include "miscreply.h"
#include "../irc/irc.h"
#include "../core/error.h"



/* handle remote version request
 *
 * <source numeric> VERSION/V <target server numeric>
 *
 * cargv[0] = target server numeric
 *            can be a * in which case the request is for all servers (snircd extension)
 * 
 */
int handleversionmsg(void *source, int cargc, char **cargv) {

  nick *snick;                        /* struct nick for source nick */
  char *sourcenum = (char *)source;   /* source user numeric */

  /* check parameters */
  if (cargc < 1) {
    miscreply_needmoreparams(sourcenum, "VERSION");
    return CMD_OK;
  }

  /* find source user */ 
  if (!(snick = miscreply_finduser(sourcenum, "VERSION")))
    return CMD_OK;

  /*
   * 351 RPL_VERSION "source 351 target version server :info"
   *                 "irc.netsplit.net 351 foobar u2.10.12.12+snircd(1.3.4a). irc.netsplit.net :B96ADHMRU6"
   */
  irc_send("%s 351 %s newserv%s %s :Newserv IRC Service", getmynumeric(), sourcenum, MISCREPLY_VERSION, myserver->content);

  return CMD_OK;
}
