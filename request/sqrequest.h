#include "../nick/nick.h"
#include "../channel/channel.h"

#define QR_REQUIREDSIZE_CSERVE   25
#define QR_REQUIREDSIZE_SPAMSCAN 120
#define QR_TOPX                  5
#define QR_AUTHEDPCT_CSERVE      65
#define QR_AUTHEDPCT_SPAMSCAN    50
#define QR_MAXQCHANS             23000
#define QR_MINUSERSPCT           80
#define QR_MAXUSERSPCT           120

/* should we use 'debug' requirements for Q/S? */
#define QR_DEBUG                 0 

void qr_initrequest(void);
void qr_finirequest(void);
int qr_requestq(nick *rqnick, nick *sender, channel *cp, nick *lnick, nick *qnick);
int qr_instantrequestq(nick *sender, channel *cp);
int qr_requests(nick *rqnick, nick *sender, channel *cp, nick *lnick, nick *qnick);
void qr_requeststats(nick *rqnick, nick *np);
void qr_handlenotice(nick *sender, char *message);
