#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include "../core/hooks.h"
#include "../irc/irc.h"
#include "../localuser/localuser.h"
#include "../localuser/localuserchannel.h"
#include "../nick/nick.h"

nick *fishbot_nickname, *spamscan_nickname, *fishjoin_nickname;

void fishjoin_handlemessages(nick *target, int messagetype, void **args)
{
    switch ( messagetype )
    {
        case LU_KILLED:
            fishjoin_nickname = NULL;
        default:
        break;
    }
}

int fishjoin_isreportable(channel *nc)
{
unsigned long *userhand;

    if ( !nc || !spamscan_nickname )
        return 0;
        
    userhand = getnumerichandlefromchanhash(nc->users, spamscan_nickname->numeric);
    
    if ( userhand == NULL )
        return 0;
    else
        return 1;
}

void fishjoin_handlejoin(int hooknum, void* arg)
{
void** args = (void**)arg;
channel *spamchannel;
	
	if ( fishjoin_nickname && fishbot_nickname && spamscan_nickname && fishbot_nickname == (nick *)args[1] && fishjoin_isreportable((channel *)args[0]) )
	{
		spamchannel = findchannel("#qnet.fishbot");
		
		if ( spamchannel )
			sendmessagetochannel(fishjoin_nickname, spamchannel, "fishbot joined %s", ((channel *)args[0])->index->name->content);
	}
}

void fishjoin_handlekick(int hooknum, void* arg)
{
void** args = (void**)arg;
channel *spamchannel;
	
	if ( fishjoin_nickname && fishbot_nickname && spamscan_nickname && fishbot_nickname == (nick *)args[1] && fishjoin_isreportable((channel *)args[0]) )
	{
		spamchannel = findchannel("#qnet.fishbot");
		
		if ( spamchannel )
			sendmessagetochannel(fishjoin_nickname, spamchannel, "fishbot kicked from %s by %s, Reason: %s", ((channel *)args[0])->index->name->content, args[2] ? ((nick *)args[2])->nick : "???", args[3] ? (char *)args[3] : "???");
	}
}

void fishjoin_handlenewnick(int hooknum, void* arg)
{
channel *spamchannel;

	if ( !fishbot_nickname && !strcasecmp(((nick *)arg)->nick, "fishbot") )
	{
		if ( fishjoin_nickname )
		{
			spamchannel = findchannel("#qnet.fishbot");
			
			if ( spamchannel )
				sendmessagetochannel(fishjoin_nickname, spamchannel, "fishbot has returned");
		}
		
		fishbot_nickname = (nick *)arg;
	}
	
	if ( !spamscan_nickname && !strcasecmp(((nick *)arg)->nick, "S") )
	{
		if ( fishjoin_nickname )
		{
			spamchannel = findchannel("#qnet.fishbot");
			
			if ( spamchannel )
				sendmessagetochannel(fishjoin_nickname, spamchannel, "S has returned");
		}
		
		spamscan_nickname = (nick *)arg;
	}
}

void fishjoin_handlelostnick(int hooknum, void* arg)
{
channel *spamchannel;

	if ( fishbot_nickname && fishbot_nickname == (nick *)arg )
	{
		if ( fishjoin_nickname )
		{
			spamchannel = findchannel("#qnet.fishbot");
			
			if ( spamchannel )
				sendmessagetochannel(fishjoin_nickname, spamchannel, "fishbot has quit");
		}
		
		fishbot_nickname = NULL;
	}
	
	if ( spamscan_nickname && spamscan_nickname == (nick *)arg )
	{
		if ( fishjoin_nickname )
		{
			spamchannel = findchannel("#qnet.fishbot");
			
			if ( spamchannel )
				sendmessagetochannel(fishjoin_nickname, spamchannel, "S has quit");
		}
		
		spamscan_nickname = NULL;
	}
}

void _init()
{
	fishjoin_nickname = registerlocaluser("F", "TheFBot","fishbotsfriend.quakenet.org", "fishbot's friend", NULL, UMODE_SERVICE, &fishjoin_handlemessages);
	registerhook(HOOK_CHANNEL_JOIN, &fishjoin_handlejoin);
	registerhook(HOOK_CHANNEL_KICK, &fishjoin_handlekick);
	registerhook(HOOK_NICK_NEWNICK, &fishjoin_handlenewnick);
	registerhook(HOOK_NICK_LOSTNICK, &fishjoin_handlelostnick);
	fishbot_nickname = getnickbynick("fishbot");
	spamscan_nickname = getnickbynick("S");
}

void _fini()
{
	if ( fishjoin_nickname )
	{
		deregisterlocaluser(fishjoin_nickname, NULL);
		fishjoin_nickname = NULL;
	}
	
	deregisterhook(HOOK_CHANNEL_JOIN, &fishjoin_handlejoin);
	deregisterhook(HOOK_CHANNEL_KICK, &fishjoin_handlekick);
	deregisterhook(HOOK_NICK_NEWNICK, &fishjoin_handlenewnick);
	deregisterhook(HOOK_NICK_LOSTNICK, &fishjoin_handlelostnick);
}
