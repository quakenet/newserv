/* miscreply.h */

#ifndef __MISCREPLY_H
#define __MISCREPLY_H

#include "../nick/nick.h"

/* TODO: this should be moved to nick.h ? if approved */
/* char to use in case no opername is known */
#define NOOPERNAMECHARACTER "-"

/* remote request handle functions */
int handleadminmsg(void *source, int cargc, char **cargv);    /* ADMIN   */
int handlelinksmsg(void *source, int cargc, char **cargv);    /* LINKS   */
int handleprivsmsg(void *source, int cargc, char **cargv);    /* PRIVS   */
int handlerpingmsg(void *source, int cargc, char **cargv);    /* RPING   */
int handlerpongmsg(void *source, int cargc, char **cargv);    /* RPONG   */
int handlestatsmsg(void *source, int cargc, char **cargv);    /* STATS   */
int handletimemsg(void *source, int cargc, char **cargv);     /* TIME    */
int handletracemsg(void *source, int cargc, char **cargv);    /* TRACE   */
int handleversionmsg(void *source, int cargc, char **cargv);  /* VERSION */
int handlewhoismsg(void *source, int cargc, char **cargv);    /* WHOIS   */

/* helper functions */
void miscreply_needmoreparams(char *sourcenum, char *command);
int miscreply_findserver(char *sourcenum, char *command);
int miscreply_findservernum(char *sourcenum, char *servernum, char *command);
int miscreply_findservermatch(char *sourcenum, char *servermask);
int miscreply_myserver(int numeric);
nick *miscreply_finduser(char *sourcenum, char *command);

/* TODO: these should be moved to irc.c irc.h ? if approved */
void send_reply(char *sourcenum, int numeric, char *targetnum, char *pattern, ...) __attribute__ ((format (printf, 4, 5)));
void send_snotice(char *targetnum, char *pattern, ...) __attribute__ ((format (printf, 2, 3)));

/* TODO: these should be moved to irc.h and nick.h ? if approved */
/* test if servername is me */
#define IsMeStr(x)       (!strcmp((x), myserver->content)
/* test if numeric is me */
#define IsMeNum(x)       (!strcmp((x), getmynumeric()))
/* test if long numeric is me */
#define IsMeLong(x)      ((x) == mylongnum)
/* test if a numeric is a user */
#define IsUser(x)        (strlen((x)) == 5)
/* test if a numeric is a server */
#define IsServer(x)      (strlen((x)) == 2)
/* test if a nick is my user */
#define MyUser(x)        (IsMeLong(homeserver((x)->numeric)))
/* test if a user has an opername */
#define HasOperName(x)   ((x)->opername && strcmp((x)->opername->content, NOOPERNAMECHARACTER))
/* test if a user has a hidden host (mode +rx) */
#define HasHiddenHost(x) (IsAccount((x)) && IsHideHost((x)))
/* test if a user is away */
#define IsAway(x)        ((x)->away) 

/* TODO: this should be moved to irc.h ? if approved */
/* maximum length of a protocol message, including \r\n */
#define BUFSIZE 512

#endif
