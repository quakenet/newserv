/* msg.h */

#ifndef __MSG_H
#define __MSG_H

/* TODO: this should be somewhere central */

/* IRC commands and P10 tokens */

#define MSG_ACCOUNT          "ACCOUNT"         /* ACCOUNT */
#define TOK_ACCOUNT          "AC"

#define MSG_ADMIN            "ADMIN"           /* ADMIN */
#define TOK_ADMIN            "AD"

#define MSG_ASLL             "ASLL"            /* ASLL */
#define TOK_ASLL             "LL"

#define MSG_AWAY             "AWAY"            /* AWAY */
#define TOK_AWAY             "A"

#define MSG_BURST            "BURST"           /* BURST */
#define TOK_BURST            "B"

#define MSG_CHECK            "CHECK"           /* CHECK */
#define TOK_CHECK            "CC"

#define MSG_CLEARMODE        "CLEARMODE"       /* CLEARMODE */
#define TOK_CLEARMODE        "CM"

#define MSG_CONNECT          "CONNECT"         /* CONNECT */
#define TOK_CONNECT          "CO"

#define MSG_CREATE           "CREATE"          /* CREATE */
#define TOK_CREATE           "C"

#define MSG_DESTRUCT         "DESTRUCT"        /* DESTRUCT */
#define TOK_DESTRUCT         "DE"

#define MSG_DESYNCH          "DESYNCH"         /* DESYNCH */
#define TOK_DESYNCH          "DS"

#define MSG_END_OF_BURST     "END_OF_BURST"    /* END_OF_BURST */
#define TOK_END_OF_BURST     "EB"

#define MSG_END_OF_BURST_ACK "EOB_ACK"         /* END_OF_BURST_ACK */
#define TOK_END_OF_BURST_ACK "EA"

#define MSG_ERROR            "ERROR"           /* ERROR */
#define TOK_ERROR            "Y"

#define MSG_GLINE            "GLINE"           /* GLINES */
#define TOK_GLINE            "GL"

#define MSG_INFO             "INFO"            /* INFO */
#define TOK_INFO             "F"

#define MSG_INVITE           "INVITE"          /* INVITE */
#define TOK_INVITE           "I"

#define MSG_JOIN             "JOIN"            /* JOIN */
#define TOK_JOIN             "J"

#define MSG_JUPE             "JUPE"            /* JUPE */
#define TOK_JUPE             "JU"

#define MSG_KICK             "KICK"            /* KICK */
#define TOK_KICK             "K"

#define MSG_KILL             "KILL"            /* KILL */
#define TOK_KILL             "D"

#define MSG_LINKS            "LINKS"           /* LINKS */
#define TOK_LINKS            "LI" 

#define MSG_LUSERS           "LUSERS"          /* LUSERS */
#define TOK_LUSERS           "LU"

#define MSG_MODE             "MODE"            /* MODE */
#define TOK_MODE             "M"

#define MSG_MOTD             "MOTD"            /* MOTD */
#define TOK_MOTD             "MO"

#define MSG_NAMES            "NAMES"           /* NAMES */
#define TOK_NAMES            "E"

#define MSG_NICK             "NICK"            /* NICK */
#define TOK_NICK             "N"

#define MSG_NOTICE           "NOTICE"          /* NOTICE */
#define TOK_NOTICE           "O"

#define MSG_OPMODE           "OPMODE"          /* OPMODE */
#define TOK_OPMODE           "OM"

#define MSG_PART             "PART"            /* PART */
#define TOK_PART             "L"

#define MSG_PASS             "PASS"            /* PASS */
#define TOK_PASS             "PA"

#define MSG_PING             "PING"            /* PING */
#define TOK_PING             "G"

#define MSG_PONG             "PONG"            /* PONG */
#define TOK_PONG             "Z"

#define MSG_PRIVATE          "PRIVMSG"         /* PRIVMSG */
#define TOK_PRIVATE          "P"

#define MSG_PRIVS            "PRIVS"           /* PRIVS */
#define TOK_PRIVS            "PR"

#define MSG_QUIT             "QUIT"            /* QUIT */
#define TOK_QUIT             "Q"

#define MSG_REBURST          "REBURST"         /* REBURST */
#define TOK_REBURST          "RB"

#define MSG_RPING            "RPING"           /* RPING */
#define TOK_RPING            "RI"

#define MSG_RPONG            "RPONG"           /* RPONG */
#define TOK_RPONG            "RO"

#define MSG_SERVER           "SERVER"          /* SERVER */
#define TOK_SERVER           "S"

#define MSG_SETHOST          "SETHOST"         /* SETHOST */
#define TOK_SETHOST          "SH"

#define MSG_SETTIME          "SETTIME"         /* SETTIME */
#define TOK_SETTIME          "SE"

#define MSG_SILENCE          "SILENCE"         /* SILENCE */
#define TOK_SILENCE          "U"

#define MSG_SQUIT            "SQUIT"           /* SQUIT */
#define TOK_SQUIT            "SQ"

#define MSG_STATS            "STATS"           /* STATS */
#define TOK_STATS            "R"

#define MSG_TIME             "TIME"            /* TIME */
#define TOK_TIME             "TI"

#define MSG_TOPIC            "TOPIC"           /* TOPIC */
#define TOK_TOPIC            "T"

#define MSG_TRACE            "TRACE"           /* TRACE */
#define TOK_TRACE            "TR"

#define MSG_UPING            "UPING"           /* UPING */
#define TOK_UPING            "UP"

#define MSG_VERSION          "VERSION"         /* VERSION */
#define TOK_VERSION          "V"

#define MSG_WALLCHOPS        "WALLCHOPS"       /* WALLCHOPS */
#define TOK_WALLCHOPS        "WC"

#define MSG_WALLOPS          "WALLOPS"         /* WALLOPS */
#define TOK_WALLOPS          "WA"

#define MSG_WALLUSERS        "WALLUSERS"       /* WALLUSERS */
#define TOK_WALLUSERS        "WU"

#define MSG_WALLVOICES       "WALLVOICES"      /* WALLVOICES */
#define TOK_WALLVOICES       "WV"

#define MSG_WHOIS            "WHOIS"           /* WHOIS */
#define TOK_WHOIS            "W"

#define MSG_WHOWAS           "WHOWAS"          /* WHOWAS */
#define TOK_WHOWAS           "X"

#define MSG_XQUERY           "XQUERY"          /* XQUERY */
#define TOK_XQUERY           "XQ"

#define MSG_XREPLY           "XREPLY"          /* XREPLY */
#define TOK_XREPLY           "XR"


#endif
