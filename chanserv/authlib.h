/* authlib.h */

#include "../nick/nick.h"

/* Functions in authlib.c */
int cs_checkeboy(nick *np, char *arg);
void cs_createrandompw(char *arg, int n);
int csa_initregex(void);
void csa_freeregex(void);
int csa_checkaccountname(nick *sender, char *accountname);
