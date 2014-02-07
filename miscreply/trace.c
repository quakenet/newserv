/* trace.c */

#include "miscreply.h"
#include "../core/error.h"
#include "../irc/irc.h"
#include "../lib/irc_string.h"
#include "../server/server.h"

#include <string.h>



/* trace_user
 *
 */
static void trace_user(char *sourcenum, nick *tnick) {
  if (IsOper(tnick)) {
    /*
     * 204 RPL_TRACEOPERATOR "source 204 target Oper class nick[user@IP] last_parse"
     *                       "irc.netsplit.net 204 foobar Oper barfoo[fish@127.0.0.1] 0"
     *
     *  note: no info available on how long ago we last parsed something from this user -> 0 (not idle time)
     */
    irc_send("%s 204 %s Oper opers %s[%s@%s] 0", getmynumeric(), sourcenum, tnick->nick,
      tnick->ident[0] != '~' ? tnick->ident : "", IPtostr(tnick->ipaddress));
  } else {
    /*
     * 205 RPL_TRACEUSER "source 205 target User class nick[user@IP] last_parse"
     *                   "irc.netsplit.net 205 foobar User barfoo[fish@127.0.0.1] 0"
     */
    irc_send("%s 205 %s User users %s[%s@%s] 0", getmynumeric(), sourcenum, tnick->nick,
      tnick->ident[0] != '~' ? tnick->ident : "", IPtostr(tnick->ipaddress));
  }
}



/* trace_server
 *   return number of matched servers
 */
static int trace_server(char *sourcenum, char *target, int doall) {

  /* do not show all and no match for my hub */
  if (!doall && !match2strings(target, serverlist[myhub].name->content))
    return 0;

  /*
   * 206 RPL_TRACESERVER "source 206 target Serv class NS NC server connected_by last_parse connected_for"
   *                     "irc.netsplit.net 206 foobar Server 2S 6C irc.netsplit.net *!user@host 34 92988"
   *
   *  note: no info available on how many servers or clients are linked through this server link -> 0S 0C
   *        no info available on who/what established the link, pretend it is by us -> *!*@myservername
   *        no info available on how long ago we last parsed something from this link -> 0
   *        no info available on how long this server has been linked -> 0
   */
  irc_send("%s 206 %s Serv servers 0S 0C %s *!*@%s 0 0", getmynumeric(), sourcenum,
    serverlist[myhub].name->content, myserver->content);

  return 1;
}



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

  nick *snick;                        /* struct nick for source nick */
  nick *tnick;                        /* struct nick for target nick */
  nick **lnick;                       /* struct nick for looping local users */
  int c;                              /* loop variable */
  int opers = 0;                      /* number of operators */
  int users = 0;                      /* number of users */
  int servers = 0;                    /* number of servers */
  int doall, wilds, dow;              /* determine what to show */
  char *sourcenum = (char *)source;   /* source user numeric */
  char *target;                       /* target parameter - as given by the source user */


  /* check parameters */
  if (cargc < 2) {
    miscreply_needmoreparams(sourcenum, "TRACE");
    return CMD_OK;
  }

  /* get the parameters */
  target = cargv[0];

  /* find source user */ 
  if (!(snick = miscreply_finduser(sourcenum, "TRACE")))
    return CMD_OK;

  /* doall, wilds, dow */
  doall = match2strings(target, myserver->content);
  wilds = strchr(target, '*') || strchr(target, '?');
  dow = wilds || doall;


  /* find target user */
  if ((tnick = getnickbynick(target))) {

    /* my user */
    if (mylongnum == homeserver(tnick->numeric))
      trace_user(sourcenum, tnick);
  }


  /* source user is an operator */
  else if (IsOper(snick)) {

    /* show servers */
    servers = trace_server(sourcenum, target, doall);

    /* do all or wilds */
    if (dow) {

      /* loop over all local users */
      lnick = servernicks[mylongnum];
      for (c = 0; c < serverlist[mylongnum].maxusernum; c++) {

        if (lnick[c] == NULL)   /* no user */
          continue;

        /* target is invisible (mode +i), target is not an operator */
        if (IsInvisible(lnick[c]) && dow && !IsOper(lnick[c]))
          continue;

        /* dont show all, do wildcards and no match */
        if (!doall && wilds && !match2strings(target, lnick[c]->nick))
          continue;

        if (IsOper(lnick[c]))
          opers++;
        else
          users++;
        trace_user(sourcenum, lnick[c]);
      }
      /*
       * class has no meaning here,
       * but showing the total count for users/opers/servers might be useful anyway
       *
       * 209 RPL_TRACECLASS "source 209 target Class class count"
       *                    "irc.netsplit.net 209 foobar Class users 2"
       */
      if (users)
        irc_send("%s 209 %s Class users %d", getmynumeric(), sourcenum, users);
      if (opers)
        irc_send("%s 209 %s Class opers %d", getmynumeric(), sourcenum, opers);
      if (servers)
        irc_send("%s 209 %s Class servers %d", getmynumeric(), sourcenum, servers);
    }
  }


  /*
   * 262 RPL_TRACEEND "source 262 target :End of TRACE"
   *                  "irc.netsplit.net 262 foobar :End of TRACE"
   */
  irc_send("%s 262 %s :End of TRACE", getmynumeric(), sourcenum);

  return CMD_OK;
}
