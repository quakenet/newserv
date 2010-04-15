/* numeric.h */

#ifndef __NUMERIC_H
#define __NUMERIC_H

/* TODO: this should be somewhere central */

/* server reply numerics */

#define RPL_TRACELINK         200
#define RPL_TRACECONNECTING   201
#define RPL_TRACEHANDSHAKE    202
#define RPL_TRACEUNKNOWN      203
#define RPL_TRACEOPERATOR     204
#define RPL_TRACEUSER         205
#define RPL_TRACESERVER       206
#define RPL_TRACENEWTYPE      208
#define RPL_TRACECLASS        209
#define RPL_STATSCOMMANDS     212
#define RPL_STATSPLINE        217
#define RPL_ENDOFSTATS        219
#define RPL_STATSFLINE        238
#define RPL_STATSUPTIME       242
#define RPL_STATSCONN         250
#define RPL_ADMINME           256
#define RPL_ADMINLOC1         257
#define RPL_ADMINLOC2         258
#define RPL_ADMINEMAIL        259
#define RPL_TRACEEND          262
#define RPL_PRIVS             270


#define RPL_AWAY              301
#define RPL_WHOISUSER         311
#define RPL_WHOISSERVER       312
#define RPL_WHOISOPERATOR     313
#define RPL_WHOISIDLE         317
#define RPL_ENDOFWHOIS        318
#define RPL_WHOISCHANNELS     319
#define RPL_WHOISACCOUNT      330
#define RPL_WHOISACTUALLY     338
#define RPL_WHOISOPERNAME     343
#define RPL_VERSION           351
#define RPL_LINKS             364
#define RPL_ENDOFLINKS        365
#define RPL_TIME              391



#define ERR_NOSUCHNICK        401
#define ERR_NOSUCHSERVER      402
#define ERR_NOADMININFO       423
#define ERR_NEEDMOREPARAMS    461

#endif
