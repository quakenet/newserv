#define RQ_TLZ "#twilightzone"

#define RQ_QSERVER "nslave.netsplit.net"
#define RQ_QNICK "Q"

#define RQ_SSERVER "services.de.quakenet.org"
#define RQ_SNICK "S"

#define RQ_REQUEST_NICK "R"
#define RQ_REQUEST_USER "request"
#define RQ_REQUEST_HOST "request.quakenet.org"
#define RQ_REQUEST_REAL "Service Request v0.23"
#define RQ_REQUEST_AUTH "R"
#define RQ_REQUEST_AUTHID 1780711

#define RQU_ANY 0
#define RQU_OPER 1

#define RQ_LOGFILE "logs/request.log"

#define RQ_OK 0
#define RQ_ERROR 1
#define RQ_UNKNOWN 2

extern int rq_failed;
extern int rq_success;

extern FILE *rq_logfd;

char *rq_longtoduration(unsigned long interval);
