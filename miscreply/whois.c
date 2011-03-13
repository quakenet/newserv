/* whois.c */

#include "miscreply.h"
#include "../irc/irc.h"
#include "../core/error.h"
#include "../core/hooks.h"
#include "../server/server.h"

#include <string.h>



/* do whois
 *
 */
static void do_whois(char *sourcenum, nick *snick, nick *tnick) {

  void *nicks[3];   /* nick array for whois channels hook */


  /* user
   *
   * 311 RPL_WHOISUSER "source 311 target nick user host * :realname"
   *                   "irc.netsplit.net 311 foobar bar foo host.com * :foobar"
   */
  /* sethost (usermode +h) */
  if (IsSetHost(tnick)) {
  	irc_send("%s 311 %s %s %s %s * :%s", getmynumeric(), sourcenum, tnick->nick,
      tnick->shident->content, tnick->sethost->content, tnick->realname->name->content);
  /* account and hiddenhost (usermode +rx) */
  } else if (IsAccount(tnick) && IsHideHost(tnick)) {
    irc_send("%s 311 %s %s %s %s.%s * :%s", getmynumeric(), sourcenum, tnick->nick,
      tnick->ident, tnick->authname, HIS_HIDDENHOST, tnick->realname->name->content);
  /* not hidden */
  } else {
    irc_send("%s 311 %s %s %s %s * :%s", getmynumeric(), sourcenum, tnick->nick,
      tnick->ident, tnick->host->name->content, tnick->realname->name->content);
  }


  /* channels
   *   show channels only when
   *      target user is not a service (usermode +k) AND
   *      target user is not hidechan (usermode +n)
   *      OR user WHOIS'ing himself
   */
  if ((!IsService(tnick) && !IsHideChan(tnick)) || snick == tnick) {
    nicks[0] = (char *)snick;
    nicks[1] = (char *)tnick;
    nicks[2] = sourcenum;
    triggerhook(HOOK_NICK_WHOISCHANNELS, nicks);
  }


  /* server
   *
   * 312 RPL_WHOISSERVER "source 312 target nick server :description"
   *                     "irc.netsplit.net 312 foobar barfoo *.netsplit.net :Netsplit Server"
   */
  /* show server and description when
   *   user WHOIS'ing himself, OR to an IRC Operator, OR when HIS_SERVER is 0
   */
  if (snick == tnick || IsOper(snick) || !HIS_SERVER) {
  	 irc_send("%s 312 %s %s %s :%s", getmynumeric(), sourcenum, tnick->nick,
      serverlist[homeserver(tnick->numeric)].name->content,
      serverlist[homeserver(tnick->numeric)].description->content);
  /* show HIS server and description */
  } else {
  	irc_send("%s 312 %s %s %s :%s", getmynumeric(), sourcenum, tnick->nick,
     HIS_SERVERNAME,
     HIS_SERVERDESC);
  }


  /* target user is away */
  if ((tnick)->away) {
    /* 
     * 301 RPL_AWAY "source 301 target nick :awaymsg"
     *              "irc.netsplit.net 301 foobar barfoo :afk."
     */
    irc_send("%s 301 %s %s :%s", getmynumeric(), sourcenum, tnick->nick, tnick->away->content);
  }


  /* target user is an IRC Operator */
  if (IsOper(tnick)) {
    /* 
     * 313 RPL_WHOISOPERATOR "source 313 target nick :is an IRC Operator"
     *                       "irc.netsplit.net 313 foobar barfoo :is an IRC Operator"
     */
    irc_send("%s 313 %s %s :is an IRC Operator", getmynumeric(), sourcenum, tnick->nick);

    /* source user is an IRC Operator, show opername if we have it */
    if (IsOper(snick) && (tnick->opername && strcmp(tnick->opername->content, "-")))
      /* 
       * 343 RPL_WHOISOPERNAME "source 343 target nick opername :is opered as"
       *                       "irc.netsplit.net 343 foobar barfoo fish :is opered as"
       *
       *  note: snircd extension
       */
      irc_send("%s 343 %s %s %s :is opered as", getmynumeric(), sourcenum, tnick->nick, tnick->opername->content);
  }



  /* target user got an account (usermode +r) */
  if (IsAccount(tnick)) {
    /* 
     * 330 RPL_WHOISACCOUNT "source 330 target nick account :is authed as"
     *                      "irc.netsplit.net 330 foobar barfoo fish :is authed as"
     *
     *   note: ircu shows "is logged in as" as text
     */
    irc_send("%s 330 %s %s %s :is authed as", getmynumeric(), sourcenum, tnick->nick, tnick->authname);
  }



  /* target user got a sethost (usermode +h) or has a hiddenhost (usermode +rx) 
   *   show real host to user WHOIS'ing himself and to an IRC Operator
   */
  if ((snick == tnick || IsOper(snick)) && (IsSetHost(tnick) || (IsAccount(tnick) && IsHideHost(tnick)) )) {
    /* 
     * 338 RPL_WHOISACTUALLY "source 338 target nick user@host IP :Actual user@host, Actual IP"
     *                       "irc.netsplit.net 338 foobar barfoo foobar@localhost 127.0.0.1 :Actual user@host, Actual IP"
     */
    irc_send("%s 338 %s %s %s@%s %s :Actual user@host, Actual IP", getmynumeric(), sourcenum, tnick->nick,
      tnick->ident, tnick->host->name->content, IPtostr(tnick->p_ipaddr));
  }



  /* my user,
   *   AND not a service (usermode +k), AND not hiding idle time (usermode +I)
   *   show idle and signon time
   */
  if (mylongnum == homeserver(tnick->numeric) && !IsService(tnick) && !IsHideIdle(tnick)) {
    /* 
     * 317 RPL_WHOISIDLE "source 317 target nick idle signon :seconds idle, signon time"
     *                   "irc.netsplit.net 317 foobar barfoo 5 1084458353"
     */
    irc_send("%s 317 %s %s %ld %ld :seconds idle, signon time", getmynumeric(), sourcenum, tnick->nick,
      (getnettime() - tnick->timestamp) % (((tnick->numeric + tnick->timestamp) % 983) + 7), tnick->timestamp);
  }
}



/* handle remote whois request
 *
 * <source numeric> WHOIS/W <target server numeric> <nick>
 *
 * cargv[0] = target server numeric
 * cargv[1] = nick as given by source
 *            comma separated list of one or more nicks, may contain wildcards
 *            ircu does not do wildcard matches for remote whois, we dont either
 *
 */
int handlewhoismsg(void *source, int cargc, char **cargv) {

  nick *snick;                        /* struct nick for source nick */
  nick *tnick;                        /* struct nick for target nick */
  char *sourcenum = (char *)source;   /* source user numeric */
  char nicks[BUFSIZE];                /* nick parameter */
  char *nick;                         /* loop nick */

  /* check parameters */
  if (cargc < 2) {
    miscreply_needmoreparams(sourcenum, "WHOIS");
    return CMD_OK;
  }

  /* get the parameters */
  strcpy(nicks, cargv[1]);

  /* find source user */ 
  if (!(snick = miscreply_finduser(sourcenum, "WHOIS")))
    return CMD_OK;

  /* go through the list of nicks */
  for (nick = strtok(nicks, ","); nick != NULL; nick = strtok(NULL, ",")) {

    /* find the target */
    if (!(tnick = getnickbynick(nick))) {
      /*
       * 401 ERR_NOSUCHNICK "source 401 target nick :No such nick"
       *                    "irc.netsplit.net 391 foobar barfoo :No such nick"
       */
      irc_send("%s 401 %s %s :No such nick", getmynumeric(), sourcenum, nick);
    }

    else
      do_whois(sourcenum, snick, tnick);
  }


  /* end of
   *
   * 318 RPL_ENDOFWHOIS "source 318 target nick :End of /WHOIS list."
   *                    "irc.netsplit.net 318 foobar barfoo :End of /WHOIS list."
   */
  irc_send("%s 318 %s %s :End of /WHOIS list.", getmynumeric(), sourcenum, cargv[1]);

  return CMD_OK;
}
