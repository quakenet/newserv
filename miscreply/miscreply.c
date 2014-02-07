/* miscreply.c */

#include "miscreply.h"
#include "../core/error.h"
#include "../core/config.h"
#include "../irc/irc.h"
#include "../lib/irc_string.h"
#include "../lib/version.h"
#include "../nick/nick.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* module version */
MODULE_VERSION(MISCREPLY_VERSION);



/*
 * This module handles the following remote IRC commands
 *
 *  ADMIN
 *  PRIVS
 *  RPING
 *  RPONG
 *  STATS
 *  TRACE
 *  TIME
 *  VERSION 
 *  WHOIS
 *
 */



/* admin info */
sstring *admin1;
sstring *admin2;
sstring *admin3;



/* init */
void _init() {

  /* get the admin info from the config
   *   mix in some references to Quake and Doom in the defaults
   */
  admin1 = getcopyconfigitem("miscreply", "admin1", "Located at the Union Aerospace Corp. facility, Stroggos", BUFSIZE);
  admin2 = getcopyconfigitem("miscreply", "admin2", "Network IRC Service", BUFSIZE);
  admin3 = getcopyconfigitem("miscreply", "admin3", "No administrative info available", BUFSIZE);

  /* add server handlers */
  registerserverhandler("AD", &handleadminmsg, 1);   /* ADMIN   */
  registerserverhandler("PR", &handleprivsmsg, 1);   /* PRIVS   */
  registerserverhandler("RI", &handlerpingmsg, 3);   /* RPING   */
  registerserverhandler("RO", &handlerpongmsg, 4);   /* RPONG   */
  registerserverhandler("R",  &handlestatsmsg, 2);   /* STATS   */
  registerserverhandler("TI", &handletimemsg, 1);    /* TIME    */
  registerserverhandler("TR", &handletracemsg, 2);   /* TRACE   */
  registerserverhandler("V",  &handleversionmsg, 1); /* VERSION */
  registerserverhandler("W",  &handlewhoismsg, 2);   /* WHOIS   */

}



/* fini */
void _fini() {

  /* remove server handlers */
  deregisterserverhandler("AD", &handleadminmsg);   /* ADMIN   */
  deregisterserverhandler("PR", &handleprivsmsg);   /* PRIVS   */
  deregisterserverhandler("RI", &handlerpingmsg);   /* RPING   */
  deregisterserverhandler("RO", &handlerpongmsg);   /* RPONG   */
  deregisterserverhandler("R",  &handlestatsmsg);   /* STATS   */
  deregisterserverhandler("TI", &handletimemsg);    /* TIME    */
  deregisterserverhandler("TR", &handletracemsg);   /* TRACE   */
  deregisterserverhandler("V",  &handleversionmsg); /* VERSION */
  deregisterserverhandler("W",  &handlewhoismsg);   /* WHOIS   */

}



/* from_server
 *   if valid server numeric return 1
 *   else return 0 and log error
 */
static int from_server(char *sourcenum, char *command) {

   /* valid server numeric */
  if (strlen(sourcenum) == 2)
    return 1;

  Error("miscreply", ERR_WARNING, "Received %s request from non-server numeric %s", command, sourcenum);
  return 0;
}



/* from_user
 *   if valid user numeric return 1
 *   else return 0 and log error
 */
static int from_user(char *sourcenum, char *command) {

  /* valid user numeric */
  if (strlen(sourcenum) == 5)
    return 1;

  Error("miscreply", ERR_WARNING, "Received %s request from non-user numeric %s", command, sourcenum);
  return 0;
}



/* needmoreparams
 *   reply error to source user
 *   log error
 */
void miscreply_needmoreparams(char *sourcenum, char *command) {
  /*
   * 461 ERR_NEEDMOREPARAMS "source 461 target command :Not enough parameters"
   *                        "irc.netsplit.net 461 foobar JOIN :Not enough parameters"
   */
  irc_send("%s 461 %s %s :Not enough parameters", getmynumeric(), sourcenum, command);
  Error("miscreply", ERR_WARNING, "Received %s request without enough parameters from %s", command, sourcenum);
}



/* findserver
 *   if found return server longnumeric
 *   else return -1 and log error
 */
long miscreply_findserver(char *sourcenum, char *command) {
  long i = numerictolong(sourcenum, 2);

  /* check source is a valid server numeric */
  if (!from_server(sourcenum, command))
    return -1;

  if (serverlist[i].linkstate != LS_INVALID)
    return i;

  Error("miscreply", ERR_WARNING, "Received %s request from unknown server numeric %s", command, sourcenum);
  return -1;
}



/* findservermatch
 *   if found return server longnumeric
 *   else return -1 and reply error to source user
 */
long miscreply_findservermatch(char *sourcenum, char *servermask) {
  long i;

  /* go over all servers */
  for (i = 0; i < MAXSERVERS; i++) {
    if (serverlist[i].linkstate == LS_INVALID)   /* not linked */
      continue;
    if (!match2strings(servermask, serverlist[i].name->content))   /* no match */
      continue;
    return i;
  }

  /*
   * 402 RPL_NOSUCHSERVER "source 402 target server :No such server"
   *                      "irc.netsplit.net 402 foobar hub* :No such server"
   */
  irc_send("%s 402 %s %s :No such server", getmynumeric(), sourcenum, servermask);

  return -1;
}



/* finduser
 *   if found return nick for source numeric
 *   else return NULL and log error
 */
nick *miscreply_finduser(char *sourcenum, char *command) {
  nick *nick;

  /* check source is a valid user numeric */
  if (!from_user(sourcenum, command))
    return NULL;

  if ((nick = getnickbynumericstr(sourcenum)))
    return nick;

  Error("miscreply", ERR_WARNING, "Received %s request from unknown user numeric %s", command, sourcenum);
  return NULL;
}
