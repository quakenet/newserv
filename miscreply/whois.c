/* whois.c */

#include "miscreply.h"
#include "msg.h"
#include "numeric.h"
#include "../irc/irc.h"
#include "../core/error.h"
#include "../core/hooks.h"
#include "../nick/nick.h"

#include <string.h>



/* handle remote whois request
 *
 * <source numeric> WHOIS/W <target server numeric> <nick>
 *
 * cargv[0] = target server numeric
 * cargv[1] = nick as given by source
 *            comma separated list of one or more nicks, may contain wildcards
 *            multiple nicks and wildcards currently not supported here
 *
 */
int handlewhoismsg(void *source, int cargc, char **cargv) {

  nick *snick;                                          /* struct nick for source nick */
  nick *tnick;                                          /* struct nick for target nick */
  nick *nicks[2];                                       /* struct nick for whois channels trigger */

  char *sourcenum = (char *)source;                     /* source user numeric */
  char *targetnum = getmynumeric();                     /* target server numeric */
  char *servernum;                                      /* server numeric parameter */
  char *nick;                                           /* nick parameter */


  /* check parameters */
  if (cargc < 2) {
    miscreply_needmoreparams(sourcenum, "WHOIS");
    return CMD_OK;
  }

  /* get the parameters */
  servernum = cargv[0];
  nick = cargv[1];

  /* request not for my server
   *
   *   if newserv can have clients on its downlinks,
   *   this will need to be changed,
   *   else remote whois on those users wont work
   */
  if (!IsMeNum(servernum))
    return CMD_OK;

  /* from a server? */
  if (IsServer(sourcenum))
    return CMD_OK;

  /* find source user */ 
  if (!(snick = miscreply_finduser(sourcenum, "WHOIS")))
    return CMD_OK;

  /* find the target */
  if (!(tnick = getnickbynick(nick))) {
    /*
     * 401 ERR_NOSUCHNICK "source 401 target nick :No such nick"
     *                    "irc.netsplit.net 391 foobar barfoo :No such nick"
     */
    send_reply(targetnum, ERR_NOSUCHNICK, sourcenum, "%s :No such nick", nick);

  } else {


    /* user
     *
     * 311 RPL_WHOISUSER "source 311 target nick user host * :realname"
     *                   "irc.netsplit.net 311 foobar bar foo host.com * :foobar"
     */
    /* sethost (mode +h) */
    if (IsSetHost(tnick)) {
      send_reply(targetnum, RPL_WHOISUSER, sourcenum, "%s %s %s * :%s", tnick->nick,
        tnick->shident->content, tnick->sethost->content, tnick->realname->name->content);
    /* account and hiddenhost (mode +rx) */
    } else if (HasHiddenHost(tnick)) {
      send_reply(targetnum, RPL_WHOISUSER, sourcenum, "%s %s %s.%s * :%s", tnick->nick,
        tnick->ident, tnick->authname, HIS_HIDDENHOST, tnick->realname->name->content);
    /* not hidden */
    } else {
      send_reply(targetnum, RPL_WHOISUSER, sourcenum, "%s %s %s * :%s", tnick->nick,
        tnick->ident, tnick->host->name->content, tnick->realname->name->content);
    }


    /* TODO: these checks are in handlewhoischannels in channel/channelhandlers.c also .. */
    /* TODO: move whois channels here? */
    /* channels
     *   show channels only when
     *      target user is not a service (mode +k) AND
     *      target user is not hidechan (mode +n)
     *      OR user WHOIS'ing himself
     */
    if ((!IsService(tnick) && !IsHideChan(tnick)) || snick == tnick) {
      nicks[0] = snick;
      nicks[1] = tnick;
      triggerhook(HOOK_NICK_WHOISCHANNELS, nicks);
    }


    /* server
     *
     * 312 RPL_WHOISSERVER "source 312 target nick server :description"
     *                     "irc.netsplit.net 312 foobar barfoo *.netsplit.net :Netsplit Server"
     */
    /* show server and description when
     *   user WHOIS'ing himself, OR to an operator, OR when HIS_SERVER is not set
     */
    if (snick == tnick || IsOper(snick) || !HIS_SERVER) {
      send_reply(targetnum, RPL_WHOISSERVER, sourcenum, "%s %s :%s", tnick->nick,
        serverlist[homeserver(tnick->numeric)].name->content,
        serverlist[homeserver(tnick->numeric)].description->content);
    /* show HIS server and description */
    } else {
      send_reply(targetnum, RPL_WHOISSERVER, sourcenum, "%s %s :%s", tnick->nick,
        HIS_SERVERNAME,
        HIS_SERVERDESC);
    }


    /* target user is away */
    if (IsAway(tnick)) {
      /* 
       * 301 RPL_AWAY "source 301 target nick :awaymsg"
       *              "irc.netsplit.net 301 foobar barfoo :afk."
       */
      send_reply(targetnum, RPL_AWAY, sourcenum, "%s :%s", tnick->nick, tnick->away->content);
    }


    /* target user is an operator */
    if (IsOper(tnick)) {
      /* 
       * 313 RPL_WHOISOPERATOR "source 313 target nick :is an IRC Operator"
       *                       "irc.netsplit.net 313 foobar barfoo :is an IRC Operator"
       */
      send_reply(targetnum, RPL_WHOISOPERATOR, sourcenum, "%s :is an IRC Operator", tnick->nick);

      /* source user is an operator, show opername if we have it */
      if (IsOper(snick) && HasOperName(tnick))
        /* 
         * 343 RPL_WHOISOPERNAME "source 343 target nick opername :is opered as"
         *                       "irc.netsplit.net 343 foobar barfoo fish :is opered as"
         *
         *  note: snircd extension
         */
        send_reply(targetnum, RPL_WHOISOPERNAME, sourcenum, "%s %s :is opered as", tnick->nick, tnick->opername->content);
    }



    /* target user got an account (mode +r) */
    if (IsAccount(tnick)) {
      /* 
       * 330 RPL_WHOISACCOUNT "source 330 target nick account :is authed as"
       *                      "irc.netsplit.net 330 foobar barfoo fish :is authed as"
       *
       *   note: ircu shows "is logged in as" as text
       */      
      send_reply(targetnum, RPL_WHOISACCOUNT, sourcenum, "%s %s :is authed as", tnick->nick, tnick->authname);
    }



    /* target user got a sethost (mode +h) or has a hiddenhost (mode +rx) 
     *   show real host to user WHOIS'ing himself and to an operator
     */
    if ((snick == tnick || IsOper(snick)) && (IsSetHost(tnick) || HasHiddenHost(tnick))) {
      /* 
       * 338 RPL_WHOISACTUALLY "source 338 target nick user@host IP :Actual user@host, Actual IP"
       *                       "irc.netsplit.net 338 foobar barfoo foobar@localhost 127.0.0.1 :Actual user@host, Actual IP"
       */
      send_reply(targetnum, RPL_WHOISACTUALLY, sourcenum, "%s %s@%s %s :Actual user@host, Actual IP", tnick->nick,
        tnick->ident, tnick->host->name->content, IPtostr(tnick->p_ipaddr));
    }



    /* target user is paranoid */
    if (IsParanoid(tnick) && !IsOper(snick) && snick != tnick) {
      send_snotice(longtonumeric(tnick->numeric, 5), "whois: %s performed a /WHOIS on you.", snick->nick);
    }



    /* target user is myuser, AND not a service (mode +k), AND not hiding idle time (mode +I)
     *   show idle and signon time
     */
    if (MyUser(tnick) && !IsService(tnick) && !IsHideIdle(tnick)) {
      /* 
       * 317 RPL_WHOISIDLE "source 317 target nick idle signon :seconds idle, signon time"
       *                   "irc.netsplit.net 317 foobar barfoo 5 1084458353"
       */
      send_reply(targetnum, RPL_WHOISIDLE, sourcenum, "%s %ld %ld :seconds idle, signon time", tnick->nick,
         (getnettime() - tnick->timestamp) % (((tnick->numeric + tnick->timestamp) % 983) + 7), tnick->timestamp);
    }
  }


  /* end of
   *
   * 318 RPL_ENDOFWHOIS "source 318 target nick :End of /WHOIS list."
   *                    "irc.netsplit.net 318 foobar barfoo :End of /WHOIS list."
   */
  send_reply(targetnum, RPL_ENDOFWHOIS, sourcenum, "%s :End of /WHOIS list.", nick);

  return CMD_OK;
}
