#ifndef __XSB_H
#define __XSB_H

#include "../parser/parser.h"

void xsb_addcommand(const char *name, const int maxparams, CommandHandler handler);
void xsb_delcommand(const char *name, CommandHandler handler);
void xsb_command(const char *command, const char *format, ...);

#endif
