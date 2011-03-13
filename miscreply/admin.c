/* admin.c */

#include "miscreply.h"
#include "../irc/irc.h"
#include "../core/error.h"



/* handle remote admin request
 *
 * <source numeric> ADMIN/AD <target server numeric>
 *
 * cargv[0] = target server numeric
 * 
 */
int handleadminmsg(void *source, int cargc, char **cargv) {

  nick *snick;                        /* struct nick for source nick */
  char *sourcenum = (char *)source;   /* source user numeric */

  /* check parameters */
  if (cargc < 1) {
    miscreply_needmoreparams(sourcenum, "ADMIN");
    return CMD_OK;
  }

  /* find source user */ 
  if (!(snick = miscreply_finduser(sourcenum, "ADMIN")))
    return CMD_OK;

  /*
   * 256 RPL_ADMINME "source 256 target :Administrative info about server"
   *                 "irc.netsplit.net 256 foobar :Administrative info about irc.netsplit.net"
   */
  irc_send("%s 256 %s :Administrative info about %s", getmynumeric(), sourcenum, myserver->content);

  /*
   * 257 RPL_ADMINLOC1 "source 257 target :text"
   *                   "irc.netsplit.net 257 foobar :Located at someplace"
   */
  irc_send("%s 257 %s :%s", getmynumeric(), sourcenum, admin1->content);

  /*
   * 258 RPL_ADMINLOC2 "source 258 target :text"
   *                   "irc.netsplit.net 258 foobar :NetSplit IRC Server"
   */
  irc_send("%s 258 %s :%s", getmynumeric(), sourcenum, admin2->content);

  /*
   * 259 RPL_ADMINEMAIL "source 259 target :text"
   *                    "irc.netsplit.net 259 foobar :IRC Admins <mail@host>"
   */
  irc_send("%s 259 %s :%s", getmynumeric(), sourcenum, admin3->content);

  return CMD_OK;
}
