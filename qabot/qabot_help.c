#include <stdio.h>
#include <time.h>

#include "../nick/nick.h"
#include "../localuser/localuserchannel.h"
#include "../core/hooks.h"
#include "../core/schedule.h"
#include "../lib/array.h"
#include "../lib/base64.h"
#include "../lib/irc_string.h"
#include "../lib/splitline.h"

#include "qabot.h"

int qabot_showhelp(nick* np, char* arg) {
  if (!ircd_strcmp(arg, "showcommands")) {
    sendnoticetouser(qabot_nick, np, "Syntax: showcommands");
    sendnoticetouser(qabot_nick, np, "Display commands the bot provides.");
  }
  else if (!ircd_strcmp(arg, "help")) {
    sendnoticetouser(qabot_nick, np, "Syntax: help <command>");
    sendnoticetouser(qabot_nick, np, "View help for a particular command.");
  }
  else if (!ircd_strcmp(arg, "hello")) {
    sendnoticetouser(qabot_nick, np, "Syntax: hello");
    sendnoticetouser(qabot_nick, np, "Create the initial user account on the bot.");
  }
  else if (!ircd_strcmp(arg, "save")) {
    sendnoticetouser(qabot_nick, np, "Syntax: save");
    sendnoticetouser(qabot_nick, np, "Save the user and bot databases.");
  }
  else if (!ircd_strcmp(arg, "listbots")) {
    sendnoticetouser(qabot_nick, np, "Syntax: listbots");
    sendnoticetouser(qabot_nick, np, "List currently added bots.");
  }
  else if (!ircd_strcmp(arg, "listusers")) {
    sendnoticetouser(qabot_nick, np, "Syntax: listusers");
    sendnoticetouser(qabot_nick, np, "List currently added users.");
  }
  else if (!ircd_strcmp(arg, "showbot")) {
    sendnoticetouser(qabot_nick, np, "Syntax: showbot <nick>");
    sendnoticetouser(qabot_nick, np, "Show information about a particular bot.");
  }
  else if (!ircd_strcmp(arg, "addbot")) {
    sendnoticetouser(qabot_nick, np, "Syntax: addbot <nick> <user> <host> <public chan> <question chan> <staff chan>");
    sendnoticetouser(qabot_nick, np, "Add a new bot.");
  }
  else if (!ircd_strcmp(arg, "delbot")) {
    sendnoticetouser(qabot_nick, np, "Syntax: delbot <nick>");
    sendnoticetouser(qabot_nick, np, "Delete a bot.");
  }
  else if (!ircd_strcmp(arg, "adduser")) {
    sendnoticetouser(qabot_nick, np, "Syntax: adduser <nick|#authname> <flags>");
    sendnoticetouser(qabot_nick, np, "Add a user. Flags may consist of:");
    sendnoticetouser(qabot_nick, np, "+a: administrator");
    sendnoticetouser(qabot_nick, np, "+d: developer");
    sendnoticetouser(qabot_nick, np, "+s: staff");
  }
  else if (!ircd_strcmp(arg, "deluser")) {
    sendnoticetouser(qabot_nick, np, "Syntax: deluser <nick|#authname>");
    sendnoticetouser(qabot_nick, np, "Delete a user.");
  }
  else if (!ircd_strcmp(arg, "changelev")) {
    sendnoticetouser(qabot_nick, np, "Syntax: changelev <nick|#authname> <flags>");
    sendnoticetouser(qabot_nick, np, "Change a user's level. Flags may consist of:");
    sendnoticetouser(qabot_nick, np, "+a: administrator");
    sendnoticetouser(qabot_nick, np, "+d: developer");
    sendnoticetouser(qabot_nick, np, "+s: staff");
  }
  else if (!ircd_strcmp(arg, "whois")) {
    sendnoticetouser(qabot_nick, np, "Syntax: whois <nick|#authname>");
    sendnoticetouser(qabot_nick, np, "Display information about a particular user.");
  }
  else if (!ircd_strcmp(arg, "status")) {
    sendnoticetouser(qabot_nick, np, "Syntax: status");
    sendnoticetouser(qabot_nick, np, "Display some status information.");
  }
  else {
    sendnoticetouser(qabot_nick, np, "No such command.");
    return CMD_ERROR;
  }
  
  return CMD_OK;
}
