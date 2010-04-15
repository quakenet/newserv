/* version.c */

#include "miscreply.h"
#include "numeric.h"
#include "../irc/irc.h"
#include "../core/error.h"
#include "../nick/nick.h"

#include <string.h>



/* TODO: grab version info from conf or elsewhere */
/* TODO: grab configuration string from elsewhere? */

/* handle remote version request
 *
 * <source numeric> VERSION/V <target server numeric>
 *
 * cargv[0] = target server numeric
 *            can be a * in which case the request is for all servers (snircd extension)
 * 
 */
int handleversionmsg(void *source, int cargc, char **cargv) {

  nick *snick;                                          /* struct nick for source nick */

  int i;                                                /* index for serverlist[] */
  char *sourcenum = (char *)source;                     /* source user numeric */
  char *targetnum = getmynumeric();                     /* target server numeric */
  char *servernum;                                      /* server numeric parameter */
  char *servername = myserver->content;                 /* servername */
  char *version = "newserv2.10.12.";                    /* version */
  char *info = "B0HM6";                                 /* configuration string */

  /* check parameters */
  if (cargc < 1) {
    miscreply_needmoreparams(sourcenum, "VERSION");
    return CMD_OK;
  }

  /* get the parameter */
  servernum = cargv[0];

  /* from a server? */
  if (IsServer(sourcenum))
    return CMD_OK;

  /* find source user */ 
  if (!(snick = miscreply_finduser(sourcenum, "VERSION")))
    return CMD_OK;

  /* request is not for me and not for * (all servers) */
  if (!IsMeNum(servernum) && !(servernum[0] == '*' && servernum[1] == '\0')) {

    /* find the server */
    if ((i = miscreply_findservernum(sourcenum, servernum, "VERSION")) == -1)
      return CMD_OK;

    targetnum = longtonumeric(i, 2);
    servername = serverlist[i].name->content;

    /* tell user */
    send_snotice(sourcenum, "VERSION: Server %s is not a real server.", servername);
  }

  /*
   * 351 RPL_VERSION "source 351 target version server :info"
   *                 "irc.netsplit.net 351 foobar u2.10.12.12+snircd(1.3.4a). irc.netsplit.net :B128AHMU6"
   */
  send_reply(targetnum, RPL_VERSION, sourcenum, "%s %s :%s", version, servername, info);

  return CMD_OK;
}
