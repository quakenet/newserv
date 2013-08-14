/* irc.h */

#ifndef __IRC_H
#define __IRC_H

#include "../parser/parser.h"
#include <stdlib.h>
#include "../lib/sstring.h"
#include <time.h>

/* This defines the maximum possible local masked numeric */
#define MAXLOCALUSER     65535

/* Are we connected to IRC or not? */
extern int connected;
extern sstring *mynumeric;
extern sstring *myserver;
extern long mylongnum;
extern time_t starttime;

/* Functions from irc.c */
void irc_connect(void *arg);
void irc_disconnected();
void irc_send(char *format, ... ) __attribute__ ((format (printf, 1, 2)));
void handledata(int fd, short events);
int parseline();
int registerserverhandler(const char *command, CommandHandler handler, int maxparams);
int deregisterserverhandler(const char *command, CommandHandler handler);
int registernumerichandler(const int numeric, CommandHandler handler, int maxparams);
int deregisternumerichandler(const int numeric, CommandHandler handler);
char *getmynumeric();
time_t getnettime();
void setnettime(time_t newtime);
void stats_commands(char *sourcenum);

/* Functions from irchandlers.c */
int handleping(void *sender, int cargc, char **cargv);
int handlesettime(void *sender, int cargc, char **cargv);
int handlepingreply(void *sender, int cargc, char **cargv);
int irc_handleserver(void *source, int cargc, char **cargv);
void sendping(void *arg);

#endif
