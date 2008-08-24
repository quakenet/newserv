#ifndef __XSB_H
#define __XSB_H

#include "../parser/parser.h"
#include "../nick/nick.h"
#include "../server/server.h"

void xsb_addcommand(const char *name, const int maxparams, CommandHandler handler);
void xsb_delcommand(const char *name, CommandHandler handler);
void xsb_broadcast(const char *command, server *service, const char *format, ...) __attribute__ ((format (printf, 3, 4)));
void xsb_unicast(const char *command, nick *np, const char *format, ...) __attribute__ ((format (printf, 3, 4)));
int xsb_isservice(server *service);

#endif
