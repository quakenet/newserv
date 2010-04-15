/* links.c */

#include "miscreply.h"
#include "numeric.h"
#include "../core/error.h"
#include "../irc/irc.h"
#include "../lib/irc_string.h"
#include "../nick/nick.h"

#include <string.h>



/* TODO: should it be long intead of int ? */
/* hops
 *
 */
int hops(int numeric) {
  int c = 1;

  /* it is me */
  if (IsMeLong(numeric))
    return 0;

  /* while parent server is not me */
  while (!IsMeLong(serverlist[numeric].parent)) {
    numeric = serverlist[numeric].parent;
    c++;
  }

  return c;
}



/* TODO: make server module keep state of hop ? */
/* TODO: make server module keep state of protocol ? */
/* TODO: make .parent for own server myself ? */

/* handle remote links request
 *
 * <source numeric> LINKS/LI <target server numeric> <mask>
 *
 * cargv[0] = target server numeric
 * cargv[1] = mask
 *            as given by the source to match against server names
 * 
 */
int handlelinksmsg(void *source, int cargc, char **cargv) {

  nick *snick;                                          /* struct nick for source nick */

  int i;                                                /* index for serverlist[] */
  char *sourcenum = (char *)source;                     /* source user numeric */
  char *targetnum = getmynumeric();                     /* target server numeric */
  char *servernum;                                      /* server numeric parameter */
  char *mask;                                           /* server mask */
  char *servername = myserver->content;                 /* servername */

  /* check parameters */
  if (cargc < 2) {
    miscreply_needmoreparams(sourcenum, "LINKS");
    return CMD_OK;
  }

  /* get the parameters */
  servernum = cargv[0];
  mask = cargv[1];

  /* from a server? */
  if (IsServer(sourcenum))
    return CMD_OK;

  /* find source user */ 
  if (!(snick = miscreply_finduser(sourcenum, "LINKS")))
    return CMD_OK;

  /* request is not for me */
  if (!IsMeNum(servernum)) {

    /* find the server */
    if ((i = miscreply_findservernum(sourcenum, servernum, "LINKS")) == -1)
      return CMD_OK;

    targetnum = longtonumeric(i, 2);
    servername = serverlist[i].name->content;

    /* tell user */
    send_snotice(sourcenum, "LINKS: Server %s is not a real server.", servername);
  }

  /* request for me, show links */
  else {

    /* go over all servers */
    for (i = 0; i < MAXSERVERS; i++) {

      /* not linked */
      if (serverlist[i].maxusernum == 0)
        continue;

      /* no match */
      if (!match2strings(mask, serverlist[i].name->content))
        continue;

      /*
       * 364 RPL_LINKS "source 364 target server uplink :hops protocol description"
       *               "irc.netsplit.net 364 foobar hub.netsplit.net irc.netsplit.net :2 P10 NetSplit IRC Server"
       */
      send_reply(targetnum, RPL_LINKS, sourcenum, "%s %s :%d P10 %s",
        serverlist[i].name->content,
        serverlist[IsMeLong(i) ? i : serverlist[i].parent].name->content,
        hops(i),
        serverlist[i].description->content);
    }
  }

  /*
   * 365 RPL_ENDOFLINKS "source 365 target mask :End of /LINKS list."
   *                    "irc.netsplit.net 365 foobar * :End of /LINKS list."
   */
  send_reply(targetnum, RPL_ENDOFLINKS, sourcenum, "%s :End of /LINKS list.", mask);

  return CMD_OK;
}
