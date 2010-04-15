/* trace.c */

#include "miscreply.h"
#include "numeric.h"
#include "../core/error.h"
#include "../irc/irc.h"
#include "../lib/irc_string.h"
#include "../nick/nick.h"
#include "../server/server.h"

#include <string.h>



/* TODO: make server module keep state of link TS ? */
/* TODO: make trace_server and trace_user function */
/* TODO: trace link,
 * loop back from target server til we find ourself, send trace links along the way
 */

/* handle remote trace request
 *
 * <source numeric> TRACE/TR <target> <target server numeric>
 *
 * cargv[0] = target
 *            as given by source, can be a nick or a server (may contain wildcards)
 * cargv[1] = target server numeric
 * 
 */
int handletracemsg(void *source, int cargc, char **cargv) {

  nick *snick;                           /* struct nick for source nick */
  nick *tnick;                           /* struct nick for target nick */
  nick **lnick;                          /* struct nick for looping local users */

  int i;                                 /* index for serverlist[] */
  int showall = 0;                       /* show all local server links */
  int opers = 0;                         /* number of opers */
  int users = 0;                         /* number of users */
  int servers = 0;                       /* number of servers */
  char *sourcenum = (char *)source;      /* source user numeric */
  char *targetnum = getmynumeric();      /* target server numeric */
  char *target;                          /* target parameter - as given by the source user */
  char *servernum;                       /* destination server numeric parameter */

  /* check parameters */
  if (cargc < 2) {
    miscreply_needmoreparams(sourcenum, "TRACE");
    return CMD_OK;
  }

  /* get the parameters */
  target = cargv[0];
  servernum = cargv[1];

  /* from a server? */
  if (IsServer(sourcenum))
    return CMD_OK;

  /* find source user */ 
  if (!(snick = miscreply_finduser(sourcenum, "TRACE")))
    return CMD_OK;

  /* TODO: trace here down to target server first? */

  /* for me */
  if (IsMeNum(servernum)) {

    /* find target user */
    if ((tnick = getnickbynick(target))) {

      /* my user? */
      if (MyUser(tnick)) {
        if (IsOper(tnick)) {
          /*
           * 204 RPL_TRACEOPERATOR "source 204 target Oper class nick[user@IP] last_parse"
           *                       "irc.netsplit.net 204 foobar Oper barfoo[fish@127.0.0.1] 0"
           *
           *  note: no info available on how long ago we last parsed something from this user -> 0 (not idle time)
           */
          send_reply(targetnum, RPL_TRACEOPERATOR, sourcenum, "Oper opers %s[%s@%s] 0", tnick->nick,
            tnick->ident[0] != '~' ? tnick->ident : "", IPtostr(tnick->p_ipaddr));
        } else {
          /*
           * 205 RPL_TRACEUSER "source 205 target User class nick[user@IP] last_parse"
           *                   "irc.netsplit.net 205 foobar User barfoo[fish@127.0.0.1] 0"
           */
          send_reply(targetnum, RPL_TRACEUSER, sourcenum, "User users %s[%s@%s] 0", tnick->nick,
            tnick->ident[0] != '~' ? tnick->ident : "", IPtostr(tnick->p_ipaddr));
        }
      }
    }
    else {

      /* target is my server,
       *   show all local server links
       */
      if (match2strings(target, myserver->content))
        showall = 1;

      /* go over all servers */
      for (i = 0; i < MAXSERVERS; i++) {
        if (serverlist[i].maxusernum == 0)   /* not linked */
          continue;
        if (!IsMeLong(serverlist[i].parent) && i != myhub)   /* not my server and not my hub */
          continue;
        servers++;
        if (!showall && !match2strings(target, serverlist[i].name->content))   /* do not show all and no match */
          continue;
        /*
         * 206 RPL_TRACESERVER "source 206 target Serv class NS NC server connected_by last_parse connected_for"
         *                     "irc.netsplit.net 206 foobar Server 2S 6C irc.netsplit.net *!user@host 34 92988"
         *
         *  note: no info available on how many servers or clients are linked through this server link -> 0S 0C
         *        no info available on who/what established the link, pretend it is by us -> *!*@myservername
         *        no info available on how long ago we last parsed something from this link -> 0
         *        no info available on how long this server has been linked -> 0
         */
        send_reply(targetnum, RPL_TRACESERVER, sourcenum, "Serv servers 0S 0C %s *!*@%s 0 0",
          serverlist[i].name->content, myserver->content);
      }

      /* show my users, show class total */
      if (showall) {

        /* loop over all local users */
        lnick = servernicks[mylongnum];
        for (i = 0; i < MAXLOCALUSER; i++) {

          if (lnick[i] == NULL)   /* no user */
            continue;

          if (!match2strings(target, lnick[i]->nick))   /* no match */
            continue;

          if (IsOper(lnick[i])) {
            opers++;
            /* 204 RPL_TRACEOPERATOR see above */
            send_reply(targetnum, RPL_TRACEOPERATOR, sourcenum, "Oper opers %s[%s@%s] 0", lnick[i]->nick,
              lnick[i]->ident[0] != '~' ? lnick[i]->ident : "", IPtostr(lnick[i]->p_ipaddr));
          } else {
            users++;
            /* TODO: perhaps do show +k and/or +X clients? */
            /* ircd does not show all local users in remote trace - might be too many users with trojan scan / nickjupes? */
            continue;
            /* 205 RPL_TRACEUSER see above */
            send_reply(targetnum, RPL_TRACEUSER, sourcenum, "User users %s[%s@%s] 0", lnick[i]->nick,
              lnick[i]->ident[0] != '~' ? lnick[i]->ident : "", IPtostr(lnick[i]->p_ipaddr));
          }
        }
        /*
         * class has no meaning here,
         * but showing the total count for users/opers/servers might be useful anyway
         *
         * 209 RPL_TRACECLASS "source 209 target Class class count"
         *                    "irc.netsplit.net 209 foobar Class users 2"
         */
        if (users)
          send_reply(targetnum, RPL_TRACECLASS, sourcenum, "Class users %d", users);
        if (opers)
          send_reply(targetnum, RPL_TRACECLASS, sourcenum, "Class opers %d", opers);
        /* should always be true - we are connected so at least 1 local server link */
        if (servers)
          send_reply(targetnum, RPL_TRACECLASS, sourcenum, "Class servers %d", servers);
      }
    }
  }

  /*
   * 262 RPL_TRACEEND "source 262 target :End of TRACE"
   *                  "irc.netsplit.net 262 foobar :End of TRACE"
   */
  send_reply(targetnum, RPL_TRACEEND, sourcenum, ":End of TRACE");

  return CMD_OK;
}
