/* authlib.h */

#include "../nick/nick.h"

/* Functions in authlib.c */
int csa_checkeboy(nick *sender, char *eboy);
void csa_createrandompw(char *pw, int n);
int csa_checkthrottled(nick *sender, reguser *rup, char *s);
void csa_initregex(void);
void csa_freeregex(void);
int csa_checkaccountname(nick *sender, char *accountname);
int csa_checkaccountname_r(char *accountname);
int csa_checkeboy_r(char *eboy);
int csa_checkpasswordquality(char *password);
reguser *csa_createaccount(char *username, char *password, char *email);
