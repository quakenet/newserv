#define RQ_TLZ "#minimoo"

#define RQ_LSERVER "lightweight.quakenet.org"
#define RQ_LNICK "L"

#define RQ_QSERVER "CServe.quakenet.org"
#define RQ_QNICK "Q"

#define RQ_SSERVER "spamscan.quakenet.org"
#define RQ_SNICK "S"

#define RQ_REQUEST_NICK "R"
#define RQ_REQUEST_USER "request"
#define RQ_REQUEST_HOST "request.quakenet.org"
#define RQ_REQUEST_REAL "Service Request v0.23"
#define RQ_REQUEST_AUTH "R"

#define RQU_ANY 0
#define RQU_OPER 1
#define RQU_ACCOUNT 2

/* one week by default */
#define RQU_HELPER_MAXEXPIRE 604800

#define RQ_USERFILE "rqusers"

#define RQ_OK 0
#define RQ_ERROR 1
#define RQ_UNKNOWN 2

extern int rq_failed;
extern int rq_success;

char *rq_longtoduration(unsigned long interval);
