/* miscreply.h */

#ifndef __MISCREPLY_H
#define __MISCREPLY_H

#include "../nick/nick.h"


#define MISCREPLY_VERSION "1.0.0"


/* admin info */
extern sstring *admin1;
extern sstring *admin2;
extern sstring *admin3;


/* remote request handle functions */
int handleadminmsg(void *source, int cargc, char **cargv);    /* ADMIN   */
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
long miscreply_findserver(char *sourcenum, char *command);
long miscreply_findservermatch(char *sourcenum, char *servermask);
nick *miscreply_finduser(char *sourcenum, char *command);


#endif
