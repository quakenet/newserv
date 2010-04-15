/* admin.c */

#include "miscreply.h"
#include "numeric.h"
#include "../irc/irc.h"
#include "../core/error.h"
#include "../nick/nick.h"

#include <string.h>



/* TODO: grab admin info from conf */

/* handle remote admin request
 *
 * <source numeric> ADMIN/AD <target server numeric>
 *
 * cargv[0] = target server numeric
 * 
 */
int handleadminmsg(void *source, int cargc, char **cargv) {

  nick *snick;                                          /* struct nick for source nick */

  int i;                                                /* index for serverlist[] */
  char *sourcenum = (char *)source;                     /* source user numeric */
  char *targetnum = getmynumeric();                     /* target server numeric */
  char *servernum;                                      /* server numeric parameter */
  char *servername = myserver->content;                 /* servername */

  /* check parameters */
  if (cargc < 1) {
    miscreply_needmoreparams(sourcenum, "ADMIN");
    return CMD_OK;
  }

  /* get the parameter */
  servernum = cargv[0];

  /* from a server? */
  if (IsServer(sourcenum))
    return CMD_OK;

  /* find source user */ 
  if (!(snick = miscreply_finduser(sourcenum, "ADMIN")))
    return CMD_OK;

  /* not for me */
  if (!IsMeNum(servernum)) {

    /* find the server */
    if ((i = miscreply_findservernum(sourcenum, servernum, "ADMIN")) == -1)
      return CMD_OK;

    targetnum = longtonumeric(i, 2);
    servername = serverlist[i].name->content;

    /* tell user */
    send_snotice(sourcenum, "ADMIN: Server %s is not a real server.", servername);
    /*
     * 423 ERR_NOADMININFO "source 423 target server :No administrative info available"
     *                     "irc.netsplit.net 423 foobar irc.netsplit.net :No administrative info available"
     */
    send_reply(targetnum, ERR_NOADMININFO, sourcenum, "%s :No administrative info available", servername);
  }

  /* for me */
  else {

    /*
     * 256 RPL_ADMINME "source 256 target :Administrative info about server"
     *                 "irc.netsplit.net 256 foobar :Administrative info about irc.netsplit.net"
     */
    send_reply(targetnum, RPL_ADMINME, sourcenum, ":Administrative info about %s", servername);

    /*
     * 257 RPL_ADMINLOC1 "source 257 target :text"
     *                   "irc.netsplit.net 257 foobar :The NetSplit IRC Network"
     */
    send_reply(targetnum, RPL_ADMINLOC1, sourcenum, ":The NetSplit IRC Network");

    /*
     * 258 RPL_ADMINLOC2 "source 258 target :text"
     *                   "irc.netsplit.net 258 foobar :NetSplit IRC Server"
     */
    send_reply(targetnum, RPL_ADMINLOC2, sourcenum, ":NetSplit IRC Server");

    /*
     * 259 RPL_ADMINEMAIL "source 259 target :text"
     *                    "irc.netsplit.net 259 foobar :IRC Admins <mail@host>"
     */
    send_reply(targetnum, RPL_ADMINEMAIL, sourcenum, ":IRC Admins <mail@host>");
  }

  return CMD_OK;
}
