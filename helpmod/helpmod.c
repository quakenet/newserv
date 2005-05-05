#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

#include "helpmod_entries.h"
#include "helpmod_user.h"
#include "helpmod_alias.h"

#include "../localuser/localuser.h"
#include "../localuser/localuserchannel.h"
#include "../control/control.h"
#include "../channel/channel.h"
#include "../nick/nick.h"
#include "../core/config.h"
#include "../core/schedule.h"

#define HELPMOD_VERSION "1.05"

helpmod_entry helpmod_base;
helpmod_user helpmod_users;
alias_tree aliases;
nick *helpmodnick;
char helpmod_db[128] = "helpmod/default";
long helpmod_usage;

void helpreply(nick *target, char *message, ... ) {
  char buf[512];
  va_list va;

  if (helpmodnick==NULL) {
    return;
  }

  va_start(va,message);
  vsnprintf(buf,512,message,va);
  va_end(va);

  sendmessagetouser(helpmodnick,target,"%s",buf);
}

void helpmod_send_help(nick* target, helpmod_user hlp_user)
{
    int i;
    helpmod_usage++;
    for (i=0;i<hlp_user->state->text_lines;i++)
	helpreply(target, "%s", hlp_user->state->text[i]->content);
    for (i=0;i<hlp_user->state->option_count;i++)
	helpreply(target, "%d) %s", i+1, hlp_user->state->options[i]->description->content);
    if (!hlp_user->state->option_count)
    {
	helpreply(target, "This concludes the help for this topic, if you want to, you can restart the service with the 'help' command, or return to the previous entry by selecting 0");
    }
}

void helpmod_cmd_help (nick* sender, char* arg)
{
    helpmod_user hlp_user = helpmod_get_user(sender->numeric);
    int hlp_target;
    int i;

    if (!helpmod_base)
    {
	helpreply(sender, "The help service is not available at this time, please try again later");
	return;
    }

    if (!arg)
    {
	hlp_user->state = helpmod_base;
	helpmod_send_help(sender, hlp_user);
        return;
    }
    else
	if (!sscanf(arg, "%d", &hlp_target))
	{
            helpmod_entry tmp;
	    for(i=0;i<strlen(arg);i++)
		if (isspace(arg[i]))
		{
		    arg[i] = 0x00;
		    break;
		}
            tmp = helpmod_get_alias(arg);
	    if (!tmp)
		helpreply(sender, "Invalid value. Either use 'help' to restart or give an integer as a valid selection");
	    else
	    {
		hlp_user->state = tmp;
		helpmod_send_help(sender, hlp_user);
	    }
	    return;
	}

    hlp_target--;
    if (!helpmod_valid_selection(hlp_user->state, hlp_target))
    {
	if (!hlp_user->state->option_count)
	    helpreply(sender, "There are no more options to choose from, you can restart the service by giving the 'help' command or select 0 to return to previous entry");
	else if (!hlp_user->state->parent)
            helpreply(sender, "Bad selection, please enter your selection as an integer from %d to %d", 1, hlp_user->state->option_count);
	else
	    helpreply(sender, "Bad selection, please enter your selection as an integer from %d to %d, selecting 0 will take you to the previous entry", 1, hlp_user->state->option_count);
	return;
    }

    hlp_user->state = helpmod_make_selection(hlp_user->state, hlp_target);
    helpmod_send_help(sender, hlp_user);

    return;
}

void helpmod_cmd_stats (nick *sender)
{
    helpreply(sender, "Amount of users:       %9ld", helpmod_user_count());
    helpreply(sender, "          entries:     %9ld", helpmod_entry_count(helpmod_base));
    helpreply(sender, "          aliases:     %9ld", helpmod_alias_count(aliases));
    helpreply(sender, "          service use: %9ld", helpmod_usage);
    helpreply(sender, "Using database: %s", helpmod_db);
    return;
}

void helpmod_cmd_load (void *sender, char* arg)
{
    FILE *tmp_file;
    char buf[128] = "helpmod/default";

    if (!arg)
	helpreply(sender, "Warning: No config specified, using default");
    else
    {
	if (strlen(arg) >= 100)
	    return;
	else
	    sprintf(buf, "helpmod/%s", arg);
    }

    if (!(tmp_file = fopen(buf, "rt")))
    {
	helpreply(sender, "File %s not found", buf);
	return;
    }
    else
	fclose(tmp_file);

    helpmod_clear_aliases(&aliases);
    helpmod_clear_all_entries();
    helpmod_init_entry(&helpmod_base);
    helpmod_clear_users();
    helpmod_load_entries(buf);
    strcpy(helpmod_db, buf);
    helpreply(sender, "New config loaded and system reset");
}

void helpmod_cmd_showcommands (nick *sender)
{
    helpreply(sender,"HelpMod %s Commands", HELPMOD_VERSION);
    helpreply(sender,"help      Primary command, gives automated help to lusers");
    helpreply(sender,"stats     Shows some service statistics");
    helpreply(sender,"aliases   Gives a simple list of aliases currently in use");
    helpreply(sender,"load      Loads a new help config");
    return;
}

void helpmod_cmd_aliases (nick *sender)
{
    char buf[512];
    int i = 0;
    void helpmod_list_aliases(alias_tree node)
    {
	if (i > 256)
	{
	    helpreply(sender, buf);
	    i = 0;
	}
	if (!node)
	    return;
        sprintf(buf+i,"%.200s ",node->name->content);
	i+=(1+strlen(node->name->content));
        helpmod_list_aliases(node->left);
	helpmod_list_aliases(node->right);
    }
    helpmod_list_aliases(aliases);
    if (i)
	helpreply(sender, buf);
}

void helpmodmessagehandler(nick *sender, int messagetype, void **args)
{
    char* first_arg;
    char* second_arg = NULL;
    nick *target;
    int i, useless_var;

    if (messagetype != LU_PRIVMSG)
	return;

    target = getnickbynick((char*)args[0]);
    first_arg = args[1];

    for (i=0;i<strlen(args[1]);i++)
	if (((char*)args[1])[i] == ' ')
	{
	    second_arg = args[1] + i + 1;
	    ((char*)args[1])[i] = 0x00;
	    break;
	}
    if (!strcmp(first_arg, "help"))
    {
	helpmod_cmd_help(target, second_arg);
        return;
    }
    if (sscanf(first_arg, "%d", &useless_var))
    {
	helpmod_cmd_help(target, first_arg);
	return;
    }
    if (!IsOper(target))
    {
        helpreply(target, "To access the help service, just type 'help'");
	return;
    }
    if (!strcmp(first_arg, "stats"))
        helpmod_cmd_stats(target);
    else if (!strcmp(first_arg, "load"))
	helpmod_cmd_load(target, second_arg);
    else if (!strcmp(first_arg,"aliases"))
        helpmod_cmd_aliases(target);
    else if (!strcmp(first_arg, "showcommands"))
	helpmod_cmd_showcommands(target);
    else if (!strcmp(first_arg, "help"))
        helpmod_cmd_help(target, first_arg);
    else
	helpreply(target, "Unknown command, see 'showcommands'");
}

void helpconnect(void) {
    channel *cp;
    /* helpmod offers no channel features, it's merely a decoration */
    char *helpmod_chans[] = {"#twilightzone", "#help", "#feds", "#qnet.help"};
    int i,helpmod_chan_count = 4;

    helpmodnick=registerlocaluser("H",
				  "help",
				  "quakenet.org",
				  "NewServ HelpMod, /msg H help",
				  "H",
				  UMODE_DEAF|UMODE_OPER|UMODE_ACCOUNT,&helpmodmessagehandler);

    helpmod_init_entry(&helpmod_base);
    helpmod_init_users();
    helpmod_load_entries(NULL);
    /* join channels */
    for (i=0;i < helpmod_chan_count;i++)
    {
	cp = findchannel(helpmod_chans[i]);
	if (!cp)
	    localcreatechannel(helpmodnick, helpmod_chans[i]);
	else
	{
	    localjoinchannel(helpmodnick, cp);
	    localgetops(helpmodnick, cp);
	}
    }
}


void _init()
{
    helpmod_usage = 0;
    helpmod_base = NULL;
    aliases = NULL;
    scheduleoneshot(time(NULL)+1,(ScheduleCallback)&helpconnect,NULL);
    schedulerecurring(time(NULL)+1,0,300,(ScheduleCallback)&helpmod_clear_inactives,NULL);
}

void _fini()
{
    deregisterlocaluser(helpmodnick, "Module unloaded");
    deleteallschedules((ScheduleCallback)&helpconnect);
    helpmod_clear_aliases(&aliases);
    helpmod_clear_all_entries();
    helpmod_clear_users();
}
