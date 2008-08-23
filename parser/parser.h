/* 
 * parser.h
 * Definitions etc. for the common parser engine.
 *
 * The aim is to use this code for incoming server messages,
 * and also for any service modules which accept user commands
 *
 * This is heavily inspired by the ircu code.
 */

#ifndef __PARSER_H
#define __PARSER_H

#include "../lib/sstring.h"

/* Maximum allowed command length */
/* Not actually used for any static buffers hence fairly large */

#define MAX_COMMAND_LEN     100

#define CMD_OK              0
#define CMD_ERROR           1
#define CMD_LAST            2
#define CMD_USAGE           3

/* Generic CommandHandler type
 * void * = pointer to some relevant object to identify where the message came from
 * int = number of parameters
 * char ** = parameter vector
 */

typedef int (*CommandHandler)(void *, int, char**);
 
typedef struct Command {
  sstring        *command;       /* Name of the command/token/thing */
  sstring        *help;          /* Help information, sorry splidge! */
  int             level;         /* "level" required to use the command/token/thing */
  int             maxparams;     /* Maximum number of parameters for the command/token/thing */
  CommandHandler  handler;       /* Function to deal with the message */
  void           *ext;           /* Pointer to some arbitrary other data */
  struct Command *next;          /* Next handler chained onto this command */
} Command;
  
typedef struct CommandTree {
  Command            *cmd;
  char               *final;
  struct CommandTree *next[26]; 
} CommandTree;

CommandTree *newcommandtree();
void destroycommandtree(CommandTree *ct);
Command *addcommandhelptotree(CommandTree *ct, const char *cmdname, int level, int maxparams, CommandHandler handler, const char *help);
int deletecommandfromtree(CommandTree *ct, const char *cmdname, CommandHandler handler);
Command *findcommandintree(CommandTree *ct, const char *cmdname, int strictcheck);
int getcommandlist(CommandTree *ct, Command **commandlist, int maxcommands);
sstring *getcommandname(CommandTree *ct, CommandHandler handler);

#define addcommandtotree(a, b, c, d, e) addcommandhelptotree(a, b, c, d, e, NULL)

#endif
