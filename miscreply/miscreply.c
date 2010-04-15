/* miscreply.c */


#include "miscreply.h"
#include "msg.h"
#include "numeric.h"
#include "../core/error.h"
#include "../irc/irc.h"
#include "../lib/irc_string.h"
#include "../lib/version.h"
#include "../nick/nick.h"

#include <stdarg.h>
#include <stdio.h>

/* module version */
MODULE_VERSION("0.0.1");

/* TODO: what module name? miscreply remotereply .. ? */
/* TODO: what includes are required ? */
/* TODO: add proper descriptions, maybe readme file ? */
/* TODO: use serve instead of accessing serverlist[] directly ? */
/* TODO: default.c handle there all remote requests we dont support ? */
/* TODO: how to handle remote requests not for ourself but for other servers we have (juped ones or whatever) ? */
/* TODO: check requests we receive are for me or my servers ? */
/* TODO: remote requests:
 *       ASLL CHECK CONNECT INFO LUSERS MOTD NAMES UPING WHOWAS
 *       ADMIN LINKS PRIVS RPING RPONG STATS TRACE TIME VERSION WHOIS (done)
 *       GLINE JUPE SETTIME (already handled in other places)
 */



/* init */
void _init() {
  
  /* add server handlers */
  registerserverhandler(TOK_ADMIN, &handleadminmsg, 1);      /* ADMIN   */
  registerserverhandler(TOK_LINKS, &handlelinksmsg, 2);      /* LINKS   */
  registerserverhandler(TOK_PRIVS, &handleprivsmsg, 1);      /* PRIVS   */
  registerserverhandler(TOK_RPING, &handlerpingmsg, 3);      /* RPING   */
  registerserverhandler(TOK_RPONG, &handlerpongmsg, 4);      /* RPONG   */
  registerserverhandler(TOK_STATS, &handlestatsmsg, 2);      /* STATS   */
  registerserverhandler(TOK_TIME, &handletimemsg, 1);        /* TIME    */
  registerserverhandler(TOK_TRACE, &handletracemsg, 2);      /* TRACE   */
  registerserverhandler(TOK_VERSION, &handleversionmsg, 1);  /* VERSION */
  registerserverhandler(TOK_WHOIS, &handlewhoismsg, 2);      /* WHOIS   */

}



/* fini */
void _fini() {

  /* remove server handlers */
  deregisterserverhandler(TOK_ADMIN, &handleadminmsg);      /* ADMIN   */
  deregisterserverhandler(TOK_LINKS, &handlelinksmsg);      /* LINKS   */
  deregisterserverhandler(TOK_PRIVS, &handleprivsmsg);      /* PRIVS   */
  deregisterserverhandler(TOK_RPING, &handlerpingmsg);      /* RPING   */
  deregisterserverhandler(TOK_RPONG, &handlerpongmsg);      /* RPONG   */
  deregisterserverhandler(TOK_STATS, &handlestatsmsg);      /* STATS   */
  deregisterserverhandler(TOK_TIME, &handletimemsg);        /* TIME    */
  deregisterserverhandler(TOK_TRACE, &handletracemsg);      /* TRACE   */
  deregisterserverhandler(TOK_VERSION, &handleversionmsg);  /* VERSION */
  deregisterserverhandler(TOK_WHOIS, &handlewhoismsg);      /* WHOIS   */

}



/* needmoreparams
 * return error to source user and log if we did not get enough parameters
 */
void miscreply_needmoreparams(char *sourcenum, char *command) {
  /*
   * 461 ERR_NEEDMOREPARAMS "source 461 target command :Not enough parameters"
   *                        "irc.netsplit.net 461 foobar JOIN :Not enough parameters"
   */
  send_reply(getmynumeric(), ERR_NEEDMOREPARAMS, sourcenum, "%s :Not enough parameters", command);
  Error("miscreply", ERR_WARNING, "%s request without enough parameters from %s", command, sourcenum);
}



/* findserver
 * return error to log if source server numeric cannot be found
 * returns server longnumeric
 * returns -1 when not found
 */
int miscreply_findserver(char *sourcenum, char *command) {
  int i = numerictolong(sourcenum, 2);

  if (serverlist[i].maxusernum == 0) {
    Error("miscreply", ERR_WARNING, "%s request from unknown server numeric %s", command, sourcenum);
    return -1;
  }

  return i;
}



/* findservernum
 * return error to source user and log if server numeric cannot be found
 * returns server longnumeric
 * returns -1 when not found
 */
int miscreply_findservernum(char *sourcenum, char *servernum, char *command) {
  int i = numerictolong(servernum, 2);

  if (serverlist[i].maxusernum == 0) {
    /*
     * 402 RPL_NOSUCHSERVER "source 402 target * :Server has disconnected"
     *                      "irc.netsplit.net 402 foobar * :Server has disconnected"
     */
    send_reply(getmynumeric(), ERR_NOSUCHSERVER, sourcenum, "* :Server has disconnected");
    Error("miscreply", ERR_WARNING, "%s request for unknown server numeric %s from numeric %s", command, servernum, sourcenum);
    return -1;
  }

  return i;
}



/* findservermatch
 * return error to source user if no servers matching the servermask can be found
 * returns server longnumeric
 * returns -1 when not found
 */
int miscreply_findservermatch(char *sourcenum, char *servermask) {
  int i;

  /* go over all servers */
  for (i = 0; i < MAXSERVERS; i++) {
    if (serverlist[i].maxusernum == 0)   /* not linked */
      continue;
    if (!match2strings(servermask, serverlist[i].name->content))   /* no match */
      continue;
    return i;
  }

  /*
   * 402 RPL_NOSUCHSERVER "source 402 target server :No such server"
   *                      "irc.netsplit.net 402 foobar hub* :No such server"
   */
  send_reply(getmynumeric(), ERR_NOSUCHSERVER, sourcenum, "%s :No such server", servermask);

  return -1;
}



/* finduser 
 * return error to log if user numeric cannot be found 
 */
nick *miscreply_finduser(char *sourcenum, char *command) {
  nick *nick;

  if (!(nick = getnickbynumericstr(sourcenum)))
    Error("miscreply", ERR_WARNING, "%s request from unknown user numeric %s", command, sourcenum);

  return nick;
}



/* TODO: should it be long intead of int ? */
/* myserver
 *
 * return 2 if the server is one of my downlinks (juped servers?),
 * return 1 if the server is me, else
 * return 0 
 */
int miscreply_myserver(int numeric) {

  /* it is me */
  if (IsMeLong(numeric))
    return 1;

  /* while parent server is not me */
  while (!IsMeLong(serverlist[numeric].parent))
    numeric = serverlist[numeric].parent;

  /* numeric is my hub,
   *   so the start server is behind it,
   *   and not one of mine
   */
  if (numeric == myhub)
    return 0;

  return 2;
}



/* send_reply
 *
 * send server numeric reply to user
 *
 */
void send_reply(char *sourcenum, int numeric, char *targetnum, char *pattern, ...) {
  char buf[BUFSIZE];
  va_list val;

  /* negative numeric? */
  if (numeric < 0)
    return;

  va_start(val, pattern);
  vsnprintf(buf, sizeof(buf), pattern, val);
  va_end(val);

  /*
   * Reserve numerics 000-099 for server-client connections where the client
   * is local to the server. If any server is passed a numeric in this range
   * from another server then it is remapped to 100-199. -avalon
   *
   *  from ircu ircd/numeric.h
   */
  /* just in case we are about to send a numeric in the reserved range, remap it */
  if (numeric < 100)
    numeric =+ 100;

  irc_send("%s %d %s %s", sourcenum, numeric, targetnum, buf);
}



/* send_snotice
 *
 * send server notice to user
 *
 */
void send_snotice(char *targetnum, char *pattern, ...) {
  char buf[BUFSIZE];
  va_list val;

  va_start(val, pattern);
  vsnprintf(buf, sizeof(buf), pattern, val);
  va_end(val);

  irc_send("%s %s %s :%s", getmynumeric(), TOK_NOTICE, targetnum, buf);
}
