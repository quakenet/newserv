#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "../channel/channel.h"
#include "../control/control.h"
#include "../core/config.h"
#include "../localuser/localuser.h"
#include "../localuser/localuserchannel.h"
#include "../nick/nick.h"
#include "../irc/irc.h"

typedef struct spawnuser_user
{
    nick *user;
    struct spawnuser_user *next;
} spawnuser_user;

spawnuser_user *spawnuser_users;

void spawnuser_handlemessages(nick *target, int messagetype, void **args)
{
spawnuser_user *spawnuser;

    switch ( messagetype )
    {
        case LU_KILLED:
            for ( spawnuser = spawnuser_users; spawnuser; spawnuser = spawnuser->next )
            {
                if ( target == spawnuser->user )
                {
                    Error("spawnuser", ERR_WARNING, "A SpawnUser got killed");
                    spawnuser->user = NULL;
                    break;
                }
            }
        default:
            break;
    }
}

void _init()
{
char nickbuf[NICKLEN + 1], hostbuf[HOSTLEN + 1];
int i;
spawnuser_user *spawnuser;
channel *spawnchannel;

    spawnuser_users = NULL;
    
    for ( i = 0; i < 3000; i++ )
    {
        snprintf(nickbuf, NICKLEN, "SU-%s-%d", mynick->nick, i);
        nickbuf[NICKLEN] = '\0';
        snprintf(hostbuf, HOSTLEN, "SpawnUser-%s.netsplit.net", mynick->nick);
        hostbuf[HOSTLEN] = '\0';
        spawnuser = malloc(sizeof(spawnuser_user));
        spawnuser->user = registerlocaluser(nickbuf, "SpawnUser", hostbuf, "SpawnUser", NULL, UMODE_INV, &spawnuser_handlemessages);
        
        spawnchannel = findchannel("#twilightzone");
        
        if ( !spawnchannel )
            localcreatechannel(spawnuser->user, "#twilightzone");
        else
            localjoinchannel(spawnuser->user, spawnchannel);
        
        spawnuser->next = spawnuser_users;
        spawnuser_users = spawnuser;
    }
}

void _fini()
{
spawnuser_user *temp_spawnuser;

    while ( spawnuser_users )
    {
        temp_spawnuser = spawnuser_users;
        spawnuser_users = temp_spawnuser->next;
        
        if ( temp_spawnuser->user )
            deregisterlocaluser(temp_spawnuser->user, "SpawnUser Unloaded");
        
        free(temp_spawnuser);
    }
}
