
#ifndef __CONTROL_H
#define __CONTROL_H

#include "../parser/parser.h"
#include "../nick/nick.h"
#include "../channel/channel.h"

void registercontrolcmd(const char *name, int level, int maxparams, CommandHandler handler);
int deregistercontrolcmd(const char *name, CommandHandler handler);
void controlreply(nick *target, char *message, ... );
void controlchanmsg(channel *cp, char *message, ...);
void controlnotice(nick *target, char *message, ...);

#endif  
