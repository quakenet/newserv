#include "../nick/nick.h"
#include "../channel/channel.h"

#define QR_REQUIREDSIZE_CSERVE   15 
#define QR_REQUIREDSIZE_SPAMSCAN 120
#define QR_TOPX                  5
#define QR_AUTHEDPCT_SPAMSCANMIN 25
#define QR_AUTHEDPCT_SCALE       35 
#define QR_AUTHEDPCT_SCALEMAX    300
#define QR_AUTHEDPCT_CSERVE      60 
#define QR_AUTHEDPCT_SPAMSCAN    50
#define QR_AUTHEDPCT_CSERVEMIN   25
#define QR_MAXQCHANS             29500
#define QR_MINUSERSPCT           60
#define QR_MAXUSERSPCT           500 

/* should we use 'debug' requirements for Q/S? */
#define QR_DEBUG                 0 

void qr_initrequest(void);
void qr_finirequest(void);
int qr_requestq(nick *rqnick, nick *sender, channel *cp, nick *lnick, nick *qnick);
int qr_instantrequestq(nick *sender, channel *cp);
int qr_requests(nick *rqnick, nick *sender, channel *cp, nick *lnick, nick *qnick);
void qr_requeststats(nick *rqnick, nick *np);
void qr_handle_notice(nick *sender, char *message);
