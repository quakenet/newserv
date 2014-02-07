#include "../lib/sstring.h"

#define RQU_ANY 0
#define RQU_OPER 1

#define RQ_OK 0
#define RQ_ERROR 1
#define RQ_UNKNOWN 2

extern int rq_failed;
extern int rq_success;

extern FILE *rq_logfd;

/* config */
extern sstring *rq_qserver, *rq_qnick, *rq_sserver, *rq_snick;
extern sstring *rq_nick, *rq_user, *rq_host, *rq_real, *rq_auth;
extern int rq_authid;

char *rq_longtoduration(unsigned long interval);
