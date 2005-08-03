/* new (and old) H user commands */
#ifndef HCOMMANDS_H
#define HCOMMANDS_H

#include "../channel/channel.h"

#include "huser.h"

#define H_CMD_MAX_ARGS 6

#define HCMD_OUT_DEFAULT (10 * HDEF_m)

void hcommands_add(void);

void helpmod_command(huser *, channel*, char*);

#endif
