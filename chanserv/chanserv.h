/* 
 * chanserv.h:
 *  Top level data structures and function prototypes for the
 *  channel service module
 */

#ifndef __CHANSERV_H
#define __CHANSERV_H

#define _GNU_SOURCE
#include <string.h>
#include <time.h>
#include "../lib/sstring.h"
#include "../core/schedule.h"
#include "../lib/flags.h"
#include "../nick/nick.h"
#include "../channel/channel.h"
#include "../parser/parser.h"
#include "../localuser/localuserchannel.h"

#define CS_PARANOID

#ifdef CS_PARANOID
#include <assert.h>
#endif

/* Q9 Version */
#define QVERSION "0.80"

/* Mini-hash of known users on channels to make lookups faster;
 * how big do we make it?  */
#define   REGCHANUSERHASHSIZE 5
#define   PASSLEN             10
#define   WELCOMELEN          250
#define   INFOLEN             100
#define   AUTOLIMIT_INTERVAL  60
#define   COUNTERSYNCINTERVAL 600
#define   LINGERTIME          300
#define   DUMPINTERVAL        300
#define   EMAILLEN            60
#define   CHANTYPES           7

/* Suspension and g-line hit count limits */
#define   MAXGLINEUSERS       50
#define   MAXSUSPENDHIT       500

/* Maximum number of times a user may attempt to auth */
#define   MAXAUTHATTEMPT      3

/* Maximum number of hellos in a session */
#define   MAXHELLOS           3

/* Maximum number of times a user may actually be authed */
#define   MAXAUTHCOUNT        2

/* Maximum number of accounts that may share an email address */
#define   MD_DEFAULTACTLIMIT  2

/* Sizes of the main hashes */
#define   REGUSERHASHSIZE     60000
#define   MAILDOMAINHASHSIZE  60000

/* Number of languages and messages */
#define MAXLANG      50
#define MAXMESSAGES  200

/* Maximum number of user chanlevs and bans */
#define MAXCHANLEVS  250
#define MAXBANS      50

/* Sources of entropy and standard length defines */
#define ENTROPYSOURCE "/dev/urandom"
#define ENTROPYLEN    8

/* The list of messages */
#define QM_PROTECTED                 0
#define QM_UNKNOWNCMD                1
#define QM_SECUREONLY                2
#define QM_AUTHEDONLY                3
#define QM_UNAUTHEDONLY              4
#define QM_COMMANDLIST               5
#define QM_ENDOFLIST                 6
#define QM_DONE                      7
#define QM_NOTENOUGHPARAMS           8
#define QM_UNKNOWNCHAN               9
#define QM_NOACCESSONCHAN           10
#define QM_CURCHANFLAGS             11
#define QM_CURFORCEMODES            12
#define QM_NOACCESS                 13
#define QM_UNKNOWNUSER              14
#define QM_USERNOTAUTHED            15
#define QM_CHANLEVHEADER            16
#define QM_CHANLEVCOLSHORT          17
#define QM_CHANLEVCOLFULL           18
#define QM_NOUSERSONCHANLEV         19
#define QM_CHANUSERFLAGS            20
#define QM_CHANUSERUNKNOWN          21
#define QM_INVALIDCHANLEVCHANGE     22
#define QM_USERISAUTHEDAS           23
#define QM_USERISHELPER             24
#define QM_USERISOPER               25
#define QM_USERISADMIN              26
#define QM_USERISDEV                27
#define QM_WHOISHEADER              28
#define QM_WHOISCOLS                29
#define QM_USERONNOCHANS            30
#define QM_CHANAUTOLIMIT            31
#define QM_CHANBANAUTOREMOVE        32
#define QM_NOCHANBANAUTOREMOVE      33
#define QM_INVALIDCHANNAME          34
#define QM_ALREADYREGISTERED        35
#define QM_CURUSERFLAGS             36
#define QM_WELCOMEMESSAGEIS         37
#define QM_GLOBALINFO               38
#define QM_CHANNELINFO              39
#define QM_PROTECTEDNICK            40
#define QM_NICKWASFAKED             41
#define QM_NOTPROTECTED             42
#define QM_SAMEAUTH                 43
#define QM_AUTHFAIL                 44
#define QM_AUTHOK                   45
#define QM_NEWACCOUNT               46
#define QM_AUTHNAMEINUSE            47
#define QM_ALREADYONCHAN            48
#define QM_NOTONCHAN                49
#define QM_ALREADYOPPED             50
#define QM_USERNOTONCHAN            51
#define QM_USEROPPEDONCHAN          52
#define QM_CANTOP                   53
#define QM_NOTOPPED                 54
#define QM_ALREADYVOICED            55
#define QM_NOTVOICED                56
#define QM_USERVOICEDONCHAN         57
#define QM_REGBANHEADER             58
#define QM_NOBANS                   59
#define QM_REMOVEDCHANBAN           60
#define QM_NOTREMOVEDPERMBAN        61
#define QM_REMOVEDPERMBAN           62
#define QM_CANTVOICE                63
#define QM_NOHELP                   64
#define QM_WHOISHEADER_NICK         65
#define QM_WHOISHEADER_AUTH         66
#define QM_WHOIS_USERS              67
#define QM_WHOIS_LASTAUTH           68
#define QM_WHOIS_LASTUSERHOST       69
#define QM_WHOIS_USERID             70
#define QM_WHOIS_INFO               71
#define QM_WHOIS_COMMENT            72
#define QM_WHOIS_FLAGS              73
#define QM_WHOIS_CREATED            74
#define QM_WHOIS_CHANHEADER         75
#define QM_WHOIS_EMAIL              76
#define QM_WHOIS_EMAILSET           77
#define QM_COMMENT                  78
#define QM_NOCOMMENT                79
#define QM_BADAUTH                  80
#define QM_USERHASBADAUTH           81
#define QM_BADEMAIL                 82
#define QM_MAILTHROTTLED            83
#define QM_MAILQUEUED               84
#define QM_PWDONTMATCH              85
#define QM_PWTOSHORT                86
#define QM_PWCHANGED                87
#define QM_DURATIONTOOLONG          88
#define QM_EMAILCHANGED             89
#define QM_EMAILDONTMATCH           90
#define QM_INVALIDEMAIL             91
#define QM_EMAILTOOSHORT            92
#define QM_NOTYOUREMAIL             93
#define QM_EMAILNOAT                94
#define QM_EMAILATEND               95
#define QM_EMAILINVCHR              96
#define QM_CHANNELNOTSUSPENDED      97
#define QM_CHANNELALREADYSUSPENDED  98
#define QM_CHANTYPE                 99
#define QM_UNKNOWNCHANTYPE         100
#define QM_FOUNDER                 101
#define QM_ADDEDBY                 102
#define QM_ALREADYKNOWNONCHAN      103
#define QM_CANNOTREMOVEOWNER       104
#define QM_CANNOTREMOVEMASTER      105
#define QM_UNKNOWNBAN              106
#define QM_YOURLANGUAGE            107
#define QM_LANGUAGELIST            108
#define QM_UNKNOWNLANGUAGE         109
#define QM_WHOIS_USERLANG          110
#define QM_CHANTYPEIS              111
#define QM_STATSHEADER             112
#define QM_STATSADDED              113
#define QM_STATSJOINS              114
#define QM_STATSLASTACTIVE         115
#define QM_AUTHSUSPENDED           116
#define QM_REASON                  117
#define QM_EXPIRES                 118
#define QM_TOOMANYAUTHS            119
#define QM_INVALIDDURATION         120
#define QM_DISCONNECTINGUSER       121
#define QM_USERALREADYSUSPENDED    122
#define QM_USERNOTSUSPENDED        123
#define QM_SPEWHEADER              124
#define QM_TOOMANYRESULTS          125
#define QM_RESULTCOUNT             126
#define QM_SUSPENDKILL             127
#define QM_LISTFLAGSHEADER         128
#define QM_SUSPENDUSERLISTHEADER   129
#define QM_SUSPENDCHANLISTHEADER   130
#define QM_NOREQUESTOWNER          131
#define QM_GRANTEDOWNER            132
#define QM_AUTHHISTORYHEADER       133
#define QM_CURDOMAINMODES          134
#define QM_SPEWDOMAINHEADER        135
#define QM_DOMAINLIMIT             136
#define QM_PWTOWEAK                137
#define QM_GIVEOWNERNOTMASTER      138
#define QM_GIVEOWNERALREADYOWNER   139
#define QM_GIVEOWNERNEEDHASH       140
#define QM_GIVEOWNERWRONGHASH      141
#define QM_SHOWINGDURATION         142
#define QM_INVALIDACCOUNTNAME      143
#define QM_CHALLENGEBADALGORITHM   144
#define QM_NOCHALLENGE             145
#define QM_USEGIVEOWNER            146
#define QM_NOFLAGSPECIFIED         147
#define QM_NEWBANALREADYBANNED     148
#define QM_NEWBANOVERLAPS          149
#define QM_REPLACINGTEMPBAN        150
#define QM_PERMBANALREADYSET       151
#define QM_NOTREPLACINGBANLDURATION 152
#define QM_REPLACINGBANSDURATION   153
#define QM_TOOMANYCHANLEVS         154
#define QM_TOOMANYBANS             155
#define QM_WARNNOTREMOVEDPERMBAN   156
#define QM_MAXHELLOLIMIT           157
#define QM_ADDRESSLIMIT            158
#define QM_DOMAINBANNED            159

/* List of privileged operations */

#define   QPRIV_SUSPENDBYPASS       1
#define   QPRIV_VIEWCHANFLAGS       100
#define   QPRIV_VIEWFULLCHANLEV     101
#define   QPRIV_VIEWFULLWHOIS       102
#define   QPRIV_VIEWCHANMODES       103
#define   QPRIV_VIEWAUTOLIMIT       104
#define   QPRIV_VIEWBANTIMER        105
#define   QPRIV_VIEWUSERFLAGS       106
#define   QPRIV_VIEWWELCOME         107
#define   QPRIV_VIEWCOMMENTS        108
#define   QPRIV_VIEWEMAIL           109

#define   QPRIV_CHANGECHANFLAGS     200
#define   QPRIV_CHANGECHANLEV       201
#define   QPRIV_CHANGECHANMODES     202
#define   QPRIV_CHANGEAUTOLIMIT     203
#define   QPRIV_CHANGEBANTIMER      204
#define   QPRIV_CHANGEUSERFLAGS     205
#define   QPRIV_CHANGEWELCOME       206

/* List of access checks */

#define CA_AUTHED      0x0001
#define CA_TOPICPRIV   0x0008
#define CA_KNOWN       0x0010
#define CA_VOICEPRIV   0x0020
#define CA_OPPRIV      0x0040
#define CA_MASTERPRIV  0x0080
#define CA_OWNERPRIV   0x0100
/* SPARE               0x0200 */
#define CA_ONCHAN      0x0400
#define CA_OFFCHAN     0x0800
#define CA_OPPED       0x1000
#define CA_DEOPPED     0x2000
#define CA_VOICED      0x4000
#define CA_DEVOICED    0x8000

#define CA_NEEDKNOWN  (CA_KNOWN|CA_OPPRIV|CA_MASTERPRIV|CA_TOPICPRIV|CA_OWNERPRIV)
#define CA_ONCHANREQ  (CA_ONCHAN|CA_OPPED|CA_DEOPPED|CA_VOICED|CA_DEVOICED)


/* User flags */
#define   QUFLAG_NEEDAUTH      0x0001  /* +l */
#define   QUFLAG_GLINE         0x0002  /* +g */
#define   QUFLAG_NOTICE        0x0004  /* +n */
#define   QUFLAG_RESTRICTED    0x0008  /* +r */
#define   QUFLAG_SUSPENDED     0x0010  /* +z */
#define   QUFLAG_OPER          0x0020  /* +o */
#define   QUFLAG_DEV           0x0040  /* +d */
#define   QUFLAG_PROTECT       0x0080  /* +p */
#define   QUFLAG_HELPER        0x0100  /* +h */
#define   QUFLAG_ADMIN         0x0200  /* +a */
#define   QUFLAG_INFO          0x0400  /* +i */
#define   QUFLAG_DELAYEDGLINE  0x0800  /* +G */
#define   QUFLAG_NOAUTHLIMIT   0x1000  /* +L */
#define   QUFLAG_CLEANUPEXEMPT 0x4000  /* +D */
#define   QUFLAG_TRUST         0x8000  /* +T */
#define   QUFLAG_ALL           0xdfff

#define UIsNeedAuth(x)      ((x)->flags & QUFLAG_NEEDAUTH)
#define UIsGline(x)         ((x)->flags & QUFLAG_GLINE)
#define UIsNotice(x)        ((x)->flags & QUFLAG_NOTICE)
#define UIsRestricted(x)    ((x)->flags & QUFLAG_RESTRICTED)
#define UIsSuspended(x)     ((x)->flags & QUFLAG_SUSPENDED)
#define UIsOper(x)          ((x)->flags & QUFLAG_OPER)
#define UIsDev(x)           ((x)->flags & QUFLAG_DEV)
#define UIsProtect(x)       ((x)->flags & QUFLAG_PROTECT)
#define UIsHelper(x)        ((x)->flags & QUFLAG_HELPER)
#define UIsAdmin(x)         ((x)->flags & QUFLAG_ADMIN)
#define UIsInfo(x)          ((x)->flags & QUFLAG_INFO)
#define UIsDelayedGline(x)  ((x)->flags & QUFLAG_DELAYEDGLINE)
#define UIsNoAuthLimit(x)   ((x)->flags & QUFLAG_NOAUTHLIMIT)
#define UIsCleanupExempt(x) ((x)->flags & QUFLAG_CLEANUPEXEMPT)

#define UHasSuspension(x)   ((x)->flags & (QUFLAG_GLINE|QUFLAG_DELAYEDGLINE|QUFLAG_SUSPENDED|QUFLAG_NEEDAUTH))

#define UHasHelperPriv(x)   ((x)->flags & (QUFLAG_HELPER | QUFLAG_OPER | QUFLAG_ADMIN | QUFLAG_DEV))
#define UHasOperPriv(x)     ((x)->flags & (QUFLAG_OPER | QUFLAG_ADMIN | QUFLAG_DEV))
#define UHasAdminPriv(x)    ((x)->flags & (QUFLAG_ADMIN | QUFLAG_DEV))

#define USetNeedAuth(x)      ((x)->flags |= QUFLAG_NEEDAUTH)
#define USetGline(x)         ((x)->flags |= QUFLAG_GLINE)
#define USetNotice(x)        ((x)->flags |= QUFLAG_NOTICE)
#define USetRestricted(x)    ((x)->flags |= QUFLAG_RESTRICTED)
#define USetSuspended(x)     ((x)->flags |= QUFLAG_SUSPENDED)
#define USetOper(x)          ((x)->flags |= QUFLAG_OPER)
#define USetDev(x)           ((x)->flags |= QUFLAG_DEV)
#define USetProtect(x)       ((x)->flags |= QUFLAG_PROTECT)
#define USetHelper(x)        ((x)->flags |= QUFLAG_HELPER)
#define USetAdmin(x)         ((x)->flags |= QUFLAG_ADMIN)
#define USetInfo(x)          ((x)->flags |= QUFLAG_INFO)
#define USetDelayedGline(x)  ((x)->flags |= QUFLAG_DELAYEDGLINE)
#define USetNoAuthLimit(x)   ((x)->flags |= QUFLAG_NOAUTHLIMIT)
#define USetCleanupExempt(x) ((x)->flags |= QUFLAG_CLEANUPEXEMPT)
#define USetTrust(x)         ((x)->flags |= QUFLAG_TRUST)

#define UClearNeedAuth(x)      ((x)->flags &= ~QUFLAG_NEEDAUTH)
#define UClearGline(x)         ((x)->flags &= ~QUFLAG_GLINE)
#define UClearNotice(x)        ((x)->flags &= ~QUFLAG_NOTICE)
#define UClearRestricted(x)    ((x)->flags &= ~QUFLAG_RESTRICTED)
#define UClearSuspended(x)     ((x)->flags &= ~QUFLAG_SUSPENDED)
#define UClearOper(x)          ((x)->flags &= ~QUFLAG_OPER)
#define UClearDev(x)           ((x)->flags &= ~QUFLAG_DEV)
#define UClearProtect(x)       ((x)->flags &= ~QUFLAG_PROTECT)
#define UClearHelper(x)        ((x)->flags &= ~QUFLAG_HELPER)
#define UClearAdmin(x)         ((x)->flags &= ~QUFLAG_ADMIN)
#define UClearInfo(x)          ((x)->flags &= ~QUFLAG_INFO)
#define UClearDelayedGline(x)  ((x)->flags &= ~QUFLAG_DELAYEDGLINE)
#define UClearNoAuthLimit(x)   ((x)->flags &= ~QUFLAG_NOAUTHLIMIT)
#define UClearCleanupExempt(x) ((x)->flags &= ~QUFLAG_CLEANUPEXEMPT)
#define UClearTrust(x)         ((x)->flags &= ~QUFLAG_TRUST)

/* email */
#define MAX_RESEND_TIME      2*3600L  /* cooling off period */
#define VALID_EMAIL         "^[-_.+[:alpha:][:digit:]]+(\\.[-_[:digit:][:alpha:]]+)*@([[:digit:][:alpha:]](-?[[:digit:][:alpha:]])*\\.)+[[:alpha:]]{2}([zmuvtgo]|fo|me|seum|op|ro)?$"

#define VALID_ACCOUNT_NAME  "^[-a-z0-9]*$"

#define QMAIL_NEWACCOUNT           1  /* new account */
#define QMAIL_REQPW                2  /* requestpassword */
#define QMAIL_NEWPW                3  /* new password */
#define QMAIL_NEWEMAIL             5  /* new email address */


/* Channel flags */
#define   QCFLAG_AUTOOP       0x0001  /* +a */
#define   QCFLAG_BITCH        0x0002  /* +b */
#define   QCFLAG_AUTOLIMIT    0x0004  /* +c */
#define   QCFLAG_ENFORCE      0x0008  /* +e */
#define   QCFLAG_FORCETOPIC   0x0010  /* +f */
#define   QCFLAG_AUTOVOICE    0x0020  /* +g */
#define   QCFLAG_INFO         0x0040  /* +i */
#define   QCFLAG_JOINED       0x0080  /* +j */
#define   QCFLAG_KNOWNONLY    0x0100  /* +k */
#define   QCFLAG_PROTECT      0x0200  /* +p */
#define   QCFLAG_TOPICSAVE    0x0800  /* +t */
#define   QCFLAG_VOICEALL     0x1000  /* +v */
#define   QCFLAG_WELCOME      0x2000  /* +w */
#define   QCFLAG_SUSPENDED    0x4000  /* +z */

#define CIsAutoOp(x)        ((x)->flags & QCFLAG_AUTOOP)
#define CIsBitch(x)         ((x)->flags & QCFLAG_BITCH)
#define CIsAutoLimit(x)     ((x)->flags & QCFLAG_AUTOLIMIT)
#define CIsEnforce(x)       ((x)->flags & QCFLAG_ENFORCE)
#define CIsForceTopic(x)    ((x)->flags & QCFLAG_FORCETOPIC)
#define CIsAutoVoice(x)     ((x)->flags & QCFLAG_AUTOVOICE)
#define CIsJoined(x)        ((x)->flags & QCFLAG_JOINED)
#define CIsKnownOnly(x)     ((x)->flags & QCFLAG_KNOWNONLY)
#define CIsProtect(x)       ((x)->flags & QCFLAG_PROTECT)
#define CIsTopicSave(x)     ((x)->flags & QCFLAG_TOPICSAVE)
#define CIsVoiceAll(x)      ((x)->flags & QCFLAG_VOICEALL)
#define CIsWelcome(x)       ((x)->flags & QCFLAG_WELCOME)
#define CIsSuspended(x)     ((x)->flags & QCFLAG_SUSPENDED)
#define CIsInfo(x)          ((x)->flags & QCFLAG_INFO)

#define CSetAutoOp(x)        ((x)->flags |= QCFLAG_AUTOOP)
#define CSetBitch(x)         ((x)->flags |= QCFLAG_BITCH)
#define CSetAutoLimit(x)     ((x)->flags |= QCFLAG_AUTOLIMIT)
#define CSetEnforce(x)       ((x)->flags |= QCFLAG_ENFORCE)
#define CSetForceTopic(x)    ((x)->flags |= QCFLAG_FORCETOPIC)
#define CSetAutoVoice(x)     ((x)->flags |= QCFLAG_AUTOVOICE)
#define CSetJoined(x)        ((x)->flags |= QCFLAG_JOINED)
#define CSetKnownOnly(x)     ((x)->flags |= QCFLAG_KNOWNONLY)
#define CSetProtect(x)       ((x)->flags |= QCFLAG_PROTECT)
#define CSetTopicSave(x)     ((x)->flags |= QCFLAG_TOPICSAVE)
#define CSetVoiceAll(x)      ((x)->flags |= QCFLAG_VOICEALL)
#define CSetWelcome(x)       ((x)->flags |= QCFLAG_WELCOME)
#define CSetSuspended(x)     ((x)->flags |= QCFLAG_SUSPENDED)

#define CClearAutoOp(x)        ((x)->flags &= ~QCFLAG_AUTOOP)
#define CClearBitch(x)         ((x)->flags &= ~QCFLAG_BITCH)
#define CClearAutoLimit(x)     ((x)->flags &= ~QCFLAG_AUTOLIMIT)
#define CClearEnforce(x)       ((x)->flags &= ~QCFLAG_ENFORCE)
#define CClearForceTopic(x)    ((x)->flags &= ~QCFLAG_FORCETOPIC)
#define CClearAutoVoice(x)     ((x)->flags &= ~QCFLAG_AUTOVOICE)
#define CClearJoined(x)        ((x)->flags &= ~QCFLAG_JOINED)
#define CClearKnownOnly(x)     ((x)->flags &= ~QCFLAG_KNOWNONLY)
#define CClearProtect(x)       ((x)->flags &= ~QCFLAG_PROTECT)
#define CClearTopicSave(x)     ((x)->flags &= ~QCFLAG_TOPICSAVE)
#define CClearVoiceAll(x)      ((x)->flags &= ~QCFLAG_VOICEALL)
#define CClearWelcome(x)       ((x)->flags &= ~QCFLAG_WELCOME)
#define CClearSuspended(x)     ((x)->flags &= ~QCFLAG_SUSPENDED)

#define   QCFLAG_USERCONTROL (QCFLAG_AUTOOP|QCFLAG_BITCH|QCFLAG_AUTOLIMIT| \
			       QCFLAG_ENFORCE|QCFLAG_FORCETOPIC|QCFLAG_AUTOVOICE| \
			       QCFLAG_PROTECT|QCFLAG_TOPICSAVE|QCFLAG_VOICEALL| \
			       QCFLAG_WELCOME|QCFLAG_KNOWNONLY)

#define   QCFLAG_ALL          0x7bff


/* Channel user ("chanlev") flags */
/* Slightly funny order here: list the "biggest" flags at the
 * top so we can do sorted "chanlev" output more easily */
#define   QCUFLAG_OWNER       0x8000  /* +n */
#define   QCUFLAG_MASTER      0x4000  /* +m */
#define   QCUFLAG_OP          0x2000  /* +o */
#define   QCUFLAG_VOICE       0x1000  /* +v */
#define   QCUFLAG_AUTOOP      0x0001  /* +a */
#define   QCUFLAG_BANNED      0x0002  /* +b */
#define   QCUFLAG_DENY        0x0004  /* +d */
#define   QCUFLAG_AUTOVOICE   0x0008  /* +g */
#define   QCUFLAG_QUIET       0x0010  /* +q */
#define   QCUFLAG_TOPIC       0x0040  /* +t */
#define   QCUFLAG_HIDEWELCOME 0x0080  /* +w */
#define   QCUFLAG_PROTECT     0x0100  /* +p */
#define   QCUFLAG_HIDEINFO    0x0200  /* +i */
#define   QCUFLAG_KNOWN       0x0400  /* +k */
#define   QCUFLAG_AUTOINVITE  0x0800  /* +j */

#define   QCUFLAG_SIGNIFICANT  (QCUFLAG_MASTER|QCUFLAG_OWNER|QCUFLAG_OP)

#define   QCUFLAG_MASTERCON (QCUFLAG_AUTOOP|QCUFLAG_BANNED|QCUFLAG_DENY| \
			     QCUFLAG_GIVE|QCUFLAG_OP|QCUFLAG_QUIET|QCUFLAG_TOPIC| \
			     QCUFLAG_VOICE|QCUFLAG_PROTECT)

#define   QCUFLAG_SELFCON   (QCUFLAG_OP | QCUFLAG_VOICE | QCUFLAG_AUTOOP | QCUFLAG_AUTOVOICE | \
                             QCUFLAG_TOPIC | QCUFLAG_HIDEINFO)

#define   QCUFLAGS_PERSONAL (QCUFLAG_HIDEINFO | QCUFLAG_HIDEWELCOME | QCUFLAG_AUTOINVITE)

#define   QCUFLAG_ALL         0xffdf

#define   CUIsOwner(x)        ((x)->flags & QCUFLAG_OWNER)
#define   CUIsMaster(x)       ((x)->flags & QCUFLAG_MASTER)
#define   CUIsOp(x)           ((x)->flags & QCUFLAG_OP)
#define   CUIsVoice(x)        ((x)->flags & QCUFLAG_VOICE)
#define   CUIsAutoOp(x)       ((x)->flags & QCUFLAG_AUTOOP)
#define   CUIsBanned(x)       ((x)->flags & QCUFLAG_BANNED)
#define   CUIsDeny(x)         ((x)->flags & QCUFLAG_DENY)
#define   CUIsAutoVoice(x)    ((x)->flags & QCUFLAG_AUTOVOICE)
#define   CUIsQuiet(x)        ((x)->flags & QCUFLAG_QUIET)
#define   CUIsTopic(x)        ((x)->flags & QCUFLAG_TOPIC)
#define   CUIsHideWelcome(x)  ((x)->flags & QCUFLAG_HIDEWELCOME)
#define   CUIsProtect(x)      ((x)->flags & QCUFLAG_PROTECT)
#define   CUIsHideInfo(x)     ((x)->flags & QCUFLAG_HIDEINFO)
#define   CUIsKnown(x)        ((x)->flags & QCUFLAG_KNOWN)
#define   CUIsAutoInvite(x)   ((x)->flags & QCUFLAG_AUTOINVITE)

#define   CUKnown(x)          (CUIsKnown(x) || CUHasVoicePriv(x))
#define   CUHasVoicePriv(x)   ((x)->flags & (QCUFLAG_VOICE | QCUFLAG_OP | QCUFLAG_MASTER | QCUFLAG_OWNER))
#define   CUHasOpPriv(x)      ((x)->flags & (QCUFLAG_OP | QCUFLAG_MASTER | QCUFLAG_OWNER))
#define   CUHasMasterPriv(x)  ((x)->flags & (QCUFLAG_MASTER | QCUFLAG_OWNER))
#define   CUHasTopicPriv(x)   ((x)->flags & (QCUFLAG_MASTER | QCUFLAG_OWNER | QCUFLAG_TOPIC))

#define   CUSetOwner(x)        ((x)->flags |= QCUFLAG_OWNER)
#define   CUSetMaster(x)       ((x)->flags |= QCUFLAG_MASTER)
#define   CUSetOp(x)           ((x)->flags |= QCUFLAG_OP)
#define   CUSetVoice(x)        ((x)->flags |= QCUFLAG_VOICE)
#define   CUSetAutoOp(x)       ((x)->flags |= QCUFLAG_AUTOOP)
#define   CUSetBanned(x)       ((x)->flags |= QCUFLAG_BANNED)
#define   CUSetDeny(x)         ((x)->flags |= QCUFLAG_DENY)
#define   CUSetAutoVoice(x)    ((x)->flags |= QCUFLAG_AUTOVOICE)
#define   CUSetQuiet(x)        ((x)->flags |= QCUFLAG_QUIET)
#define   CUSetTopic(x)        ((x)->flags |= QCUFLAG_TOPIC)
#define   CUSetHideWelcome(x)  ((x)->flags |= QCUFLAG_HIDEWELCOME)
#define   CUSetKnown(x)        ((x)->flags |= QCUFLAG_KNOWN)
#define   CUSetAutoInvite(x)   ((x)->flags |= QCUFLAG_AUTOINVITE)

#define   CUClearOwner(x)        ((x)->flags &= ~QCUFLAG_OWNER)
#define   CUClearMaster(x)       ((x)->flags &= ~QCUFLAG_MASTER)
#define   CUClearOp(x)           ((x)->flags &= ~QCUFLAG_OP)
#define   CUClearVoice(x)        ((x)->flags &= ~QCUFLAG_VOICE)
#define   CUClearAutoOp(x)       ((x)->flags &= ~QCUFLAG_AUTOOP)
#define   CUClearBanned(x)       ((x)->flags &= ~QCUFLAG_BANNED)
#define   CUClearDeny(x)         ((x)->flags &= ~QCUFLAG_DENY)
#define   CUClearAutoVoice(x)    ((x)->flags &= ~QCUFLAG_AUTOVOICE)
#define   CUClearQuiet(x)        ((x)->flags &= ~QCUFLAG_QUIET)
#define   CUClearTopic(x)        ((x)->flags &= ~QCUFLAG_TOPIC)
#define   CUClearHideWelcome(x)  ((x)->flags &= ~QCUFLAG_HIDEWELCOME)
#define   CUClearKnown(x)        ((x)->flags &= ~QCUFLAG_KNOWN)
#define   CUClearAutoInvite(x)   ((x)->flags &= ~QCUFLAG_AUTOINVITE)

#define   QCSTAT_OPCHECK      0x0001 /* Do op check */
#define   QCSTAT_MODECHECK    0x0002 /* Do mode check */
#define   QCSTAT_BANCHECK     0x0004 /* Do ban check */

#define   QUSTAT_DEAD         0x8000 /* User has been deleted.. */

#define   QCMD_SECURE         0x0001 /* Must use "user@server" to use this command */
#define   QCMD_AUTHED         0x0002 /* Only available to authed users */
#define   QCMD_NOTAUTHED      0x0004 /* Only available to NON-authed users */

#define   QCMD_HELPER         0x0010 /* Only available to helpers */
#define   QCMD_OPER           0x0020 /* Only available to opers */
#define   QCMD_ADMIN          0x0040 /* Only available to admins */
#define   QCMD_DEV            0x0080 /* Only available to developers */

#define   QCMD_ALIAS          0x0100 /* Don't list on SHOWCOMMANDS */

#define   CS_INIT_DB          0x1    /* Loading database.. */
#define   CS_INIT_NOUSER      0x2    /* Loaded DB, waiting for user to be created */
#define   CS_INIT_READY       0x3    /* Ready for action! */

struct regchanuser;
struct reguser;

typedef struct maildomain {
#ifdef CS_PARANOID
  unsigned int       maildomain_magic;
#endif
  unsigned int       ID;
  sstring           *name;
  unsigned int       count;
  unsigned int       limit;
  unsigned int       actlimit;
  flag_t             flags;
  struct reguser    *users;
  struct maildomain *parent;
  struct maildomain *nextbyname;
  struct maildomain *nextbyID;
} maildomain;

#define   MDFLAG_LIMIT       0x0001 /* +l */
#define   MDFLAG_BANNED      0x0002 /* +b */
#define   MDFLAG_ACTLIMIT    0x0004 /* +u */
#define   MDFLAG_ALL         0x0007

#define   MDFLAG_DEFAULT     MDFLAG_ACTLIMIT

#define MDHasLimit(x)        ((x)->flags & MDFLAG_LIMIT)
#define MDSetLimit(x)        ((x)->flags |= MDFLAG_LIMIT)
#define MDClearLimit(x)      ((x)->flags &= ~MDFLAG_LIMIT)
#define MDIsBanned(x)        ((x)->flags & MDFLAG_BANNED)
#define MDSetBanned(x)       ((x)->flags |= MDFLAG_BANNED)
#define MDClearBanned(x)     ((x)->flags &= ~MDFLAG_BANNED)
#define MDHasActLimit(x)     ((x)->flags & MDFLAG_ACTLIMIT)
#define MDSetActLimit(x)     ((x)->flags |= MDFLAG_ACTLIMIT)
#define MDClearActLimit(x)   ((x)->flags &= ~MDFLAG_ACTLIMIT)

/* "Q" ban */
typedef struct regban {
#ifdef CS_PARANOID
  unsigned int        regchanban_magic;
#endif
  unsigned int        ID;              /* ID in the database */
  struct chanban     *cbp;             /* Standard chanban struct */
  unsigned int        setby;           /* Who set the ban */
  time_t              expiry;          /* When the ban expires */
  sstring            *reason;          /* Reason to attach to related kicks */
  struct regban      *next;            /* Next ban on this channel */
} regban;

/* Registered channel */
typedef struct regchan {
#ifdef CS_PARANOID
  unsigned int        regchan_magic;
#endif
  unsigned int        ID;              /* Unique number from database */
  chanindex          *index;           /* Pointer to the channel index record */
  time_t              ltimestamp;      /* The last timestamp we saw on this channel */

  flag_t              flags;           /* Chanflags */
  flag_t              status;          /* Runtime status codes */
  flag_t              forcemodes;      /* Forced modes */
  flag_t              denymodes;       /* Denied modes */

  short               limit;           /* Limit to enforce if +l is set */
  short               autolimit;       /* How many slots to leave when autolimiting */
  short               banstyle;        /* Ban style for +b type bans */

  time_t              created;         /* When the service was added */
  time_t              lastactive;      /* When the channel was last "active" */
  time_t              statsreset;      /* When the users stats were last reset */
  time_t              ostatsreset;     /* When the oper stats were last reset */
  time_t              banduration;     /* How long to remove bans after (0=don't) */
  time_t              autoupdate;      /* When the autolimit next needs checking */
  time_t              lastbancheck;    /* Timestamp of last ban check */
  time_t              lastcountersync; /* When the counters were last synced.. */
  time_t              lastpart;        /* When the last user left the channel */
  
  unsigned int        founder;         /* founder */
  unsigned int        addedby;         /* oper adding chan */
  unsigned int        suspendby;       /* who suspended chan */

  short               chantype;        /* What "type" the channel is */

  unsigned int        totaljoins;      /* Total joins since created */
  unsigned int        tripjoins;       /* Total joins since last stats reset */
  unsigned int        otripjoins;      /* Total joins since last oper stats reset */
  unsigned int        maxusers;        /* Max users since created */
  unsigned int        tripusers;       /* Max users since last stats reset */
  unsigned int        otripusers;      /* Max users since last oper stats reset */

  sstring            *welcome;         /* Welcome message */
  sstring            *topic;           /* "default" topic set by settopic */
  sstring            *key;             /* Key as enforced by +k */
  sstring            *suspendreason;   /* Suspend reason */
  sstring            *comment;         /* Oper-settable channel comment */

  void               *checksched;      /* Handle for channel check schedule */

  struct regchanuser *regusers[REGCHANUSERHASHSIZE];
  struct regban      *bans;            /* List of bans on the channel */
} regchan;

/* Registered user */
typedef struct reguser {
#ifdef CS_PARANOID
  unsigned int        reguser_magic;
#endif
  unsigned int        ID;            /* From the database */
  
  char                username[NICKLEN+1];

  time_t              created;       
  time_t              lastauth;      
  time_t              lastemailchange;

  flag_t              flags;         /* user flags */
  flag_t              status;        /* runtime status */
  short               languageid;    /* what language to speak to the user */
 
  unsigned int        suspendby;     /* Userid of oper who suspended this user */
  time_t              suspendexp;    /* Expiry date of suspension */

  char                password[PASSLEN+1];

  sstring            *localpart;
  maildomain         *domain;

  sstring            *email;         /* Registered e-mail */
  sstring            *lastuserhost;  /* Last user@host */
  sstring            *suspendreason; /* Why the account is suspended */
  sstring            *comment;       /* Oper-settable user comment */
  sstring            *info;          /* User-settable info line */

  struct regchanuser *knownon;       /* Which channels this user is known on */

  /* These fields are for the nick protection system */
  void               *checkshd;      /* When we're going to check for an imposter */
  int                 stealcount;    /* How many times we've had to free the nick up */
  nick               *fakeuser;      /* If we had to "take" the nick, here's the pointer */

  struct reguser     *nextbydomain;
  struct reguser     *nextbyname;
  struct reguser     *nextbyID;
} reguser;

/* Registered channel user */
typedef struct regchanuser {
#ifdef CS_PARANOID
  unsigned int         regchanuser_magic;
#endif
  struct reguser      *user;
  struct regchan      *chan;
  flag_t               flags; 
  time_t               changetime; /* Timestamp of last "significant" change */
  time_t               usetime;    /* Timestamp of last use */
  sstring             *info;       /* User-settable info line */
  struct regchanuser  *nextbyuser;
  struct regchanuser  *nextbychan;
} regchanuser;

typedef struct cslang {
  char code[3];
  sstring *name;
} cslang;

typedef struct cmdsummary {
  sstring *def;
  sstring *bylang[MAXLANG];
  char *defhelp;
} cmdsummary;

typedef struct activeuser {
#ifdef CS_PARANOID
  unsigned int       activeuser_magic;
#endif
  unsigned short                authattempts; /* number of times user has attempted to auth */
  unsigned short                helloattempts; /* number of times user has attempted to hello... */
  unsigned char      entropy[ENTROPYLEN]; /* entropy used for challengeauth */
  time_t             entropyttl;
  struct activeuser *next;         /* purely for keeping track of free, but not free'd structures */
} activeuser;

#ifdef CS_PARANOID

#define REGUSERMAGIC      0x4d42de03
#define REGCHANMAGIC      0x5bf2aa30
#define REGCHANUSERMAGIC  0x19628b63
#define REGCHANBANMAGIC   0x5a6f555a
#define ACTIVEUSERMAGIC   0x897f98a0
#define MAILDOMAINMAGIC   0x27cde46f

#define verifyreguser(x)      assert((x)->reguser_magic      == REGUSERMAGIC)
#define verifyregchan(x)      assert((x)->regchan_magic      == REGCHANMAGIC)
#define verifyregchanuser(x)  assert((x)->regchanuser_magic  == REGCHANUSERMAGIC)
#define verifyregchanban(x)   assert((x)->regchanban_magic   == REGCHANBANMAGIC)
#define verifyactiveuser(x)   assert((x)->activeuser_magic   == ACTIVEUSERMAGIC)
#define verifymaildomain(x)   assert((x)->maildomain_magic   == MAILDOMAINMAGIC)

#define tagreguser(x)         ((x)->reguser_magic = REGUSERMAGIC)
#define tagregchan(x)         ((x)->regchan_magic = REGCHANMAGIC)
#define tagregchanuser(x)     ((x)->regchanuser_magic = REGCHANUSERMAGIC)
#define tagregchanban(x)      ((x)->regchanban_magic = REGCHANBANMAGIC)
#define tagactiveuser(x)      ((x)->activeuser_magic = ACTIVEUSERMAGIC)
#define tagmaildomain(x)      ((x)->maildomain_magic = MAILDOMAINMAGIC)

#else

#define verifyreguser(x)
#define verifyregchan(x)
#define verifyregchanuser(x)
#define verifyregchanban(x)
#define verifyactiveuser(x)
#define verifymaildomain(x)

#define tagreguser(x)
#define tagregchan(x)
#define tagregchanuser(x)
#define tagregchanban(x)
#define tagactiveuser(x)
#define tagmaildomain(x)

#endif

#define getactiveuserfromnick(x)  ((activeuser*)(x)->exts[chanservnext])
#define getreguserfromnick(x)     ((x)->auth?(reguser *)(x)->auth->exts[chanservaext]:NULL)
   
/* Global variables for chanserv module */
extern unsigned int lastuserID;
extern unsigned int lastchannelID;
extern unsigned int lastbanID;
extern unsigned int lastdomainID;

extern int chanserv_init_status;
extern int chanservdb_ready;

extern maildomain *maildomainnametable[MAILDOMAINHASHSIZE];
extern maildomain *maildomainIDtable[MAILDOMAINHASHSIZE];

extern reguser *regusernicktable[REGUSERHASHSIZE];
extern reguser *deadusers;

extern nick *chanservnick;
extern int chanservext;
extern int chanservnext;
extern int chanservaext;

extern cslang *cslanguages[MAXLANG];
extern unsigned int cslangcount;

extern sstring *csmessages[MAXLANG][MAXMESSAGES];
extern char *defaultmessages[MAXMESSAGES];

extern const flag rcflags[];
extern const flag rcuflags[];
extern const flag ruflags[];
extern const flag mdflags[];

extern CommandTree *cscommands;

extern sstring **chantypes;

/* Function prototypes */

/* chanserv.c */
void chanserv_finalinit();

/* chanservalloc.c */
void chanservallocinit();
void csfreeall();
regchan *getregchan();
void freeregchan(regchan *rcp);
reguser *getreguser();
void freereguser(reguser *rup);
regchanuser *getregchanuser();
void freeregchanuser(regchanuser *rcup);
regban *getregban();
void freeregban(regban *rbp);
activeuser *getactiveuser();
void freeactiveuser(activeuser *aup);
maildomain *getmaildomain();
void freemaildomain(maildomain *mdp);

/* chanservhash.c */
void chanservhashinit();
void addregusertohash(reguser *rup);
reguser *findreguserbyID(unsigned int ID);
reguser *findreguserbynick(const char *nick);
reguser *findreguser(nick *sender, const char *str);
void removereguserfromhash(reguser *rup);
void addregchantohash(regchan *rcp);
regchan *findregchanbyID(unsigned int ID);
regchan *findregchanbyname(const char *name);
void removeregchanfromhash(regchan *rcp);
void addregusertochannel(regchanuser *rcup);
regchanuser *findreguseronchannel(regchan *rcp, reguser *rup);
void delreguserfromchannel(regchan *rcp, reguser *rup);
void addmaildomaintohash(maildomain *mdp);
maildomain *findmaildomainbyID(unsigned int ID);
maildomain *findmaildomainbydomain(char *domain);
maildomain *findmaildomainbyemail(char *email);
maildomain *findorcreatemaildomain(char *email);
maildomain *findnearestmaildomain(char *domain);
void removemaildomainfromhash(maildomain *mdp);
void addregusertomaildomain(reguser *rup, maildomain *mdp);
void delreguserfrommaildomain(reguser *rup, maildomain *mdp);

/* chanservdb.c */
int chanservdbinit();
void loadmessages();
void loadcommandsummary(Command *cmd);
void chanservdbclose();
void csdb_updatetopic(regchan *rcp);
void csdb_updatelastjoin(regchanuser *rcup);
void csdb_updateauthinfo(reguser *rup);
void csdb_updatechannel(regchan *rcp);
void csdb_createchannel(regchan *rcp);
void csdb_deletechannel(regchan *rcp);
void csdb_deleteuser(reguser *rup);
void csdb_updatechannelcounters(regchan *rcp);
void csdb_updatechanneltimestamp(regchan *rcp);
void csdb_updatechanuser(regchanuser *rcup);
void csdb_createchanuser(regchanuser *rcup);
void csdb_deletechanuser(regchanuser *rcup);
void csdb_createuser(reguser *rup);
void csdb_updateuser(reguser *rup);
void csdb_createban(regchan *rcp, regban *rbp);
void csdb_deleteban(regban *rbp);
void csdb_updateban(regchan *rcp, regban *rbp);
char *csdb_gethelpstr(char *command, int language);
void csdb_createmail(reguser *rup, int type);
void csdb_dohelp(nick *np, Command *cmd);

/* chanservcrypto.c */
typedef int (*CRAlgorithm)(char *, const char *, const char *, const char *);
void chanservcryptoinit(void);
void cs_getrandbytes(unsigned char *buf, size_t bytes);
char *cs_calcchallenge(const unsigned char *entropy);
CRAlgorithm cs_cralgorithm(const char *algorithm);
const char *cs_cralgorithmlist(void);
int cs_checkhashpass(const char *username, const char *password, const char *junk, const char *hash);

/* chanservuser.c */
void chanservreguser(void *arg);
void chanservjoinchan(channel *cp);
void chanservsendmessage(nick *np, char *message, ... );
void chanservwallmessage(char *message, ... );
void chanservcommandinit();
void chanservcommandclose();
void chanservstdmessage(nick *np, int messageid, ... );
void chanservaddcommand(char *command, int flags, int maxparams, CommandHandler handler, char *description, const char *help);
void chanservremovecommand(char *command, CommandHandler handler);
void chanservaddctcpcommand(char *command, CommandHandler hander);
void chanservremovectcpcommand(char *command, CommandHandler handler);
void chanservkillstdmessage(nick *target, int messageid, ... );
int checkpassword(reguser *rup, const char *pass);
int setpassword(reguser *rup, const char *pass);
/*reguser *getreguserfromnick(nick *np);
activeuser *getactiveuserfromnick(nick *np);*/
void cs_checknick(nick *np);
void cs_checkchanmodes(channel *cp);
void cs_docheckchanmodes(channel *cp, modechanges *changes);
void cs_docheckopvoice(channel *cp, modechanges *changes);
void cs_checkbans(channel *cp);
void cs_schedupdate(chanindex *cip, int mintime, int maxtime);
void cs_timerfunc(void *arg);
void cs_removechannel(regchan *rcp);
void cs_doallautomodes(nick *np);
void cs_checknickbans(nick *np);
void cs_setregban(chanindex *cip, regban *rbp);
int cs_bancheck(nick *np, channel *cp);
void cs_banuser(modechanges *changes, chanindex *cip, nick *np, const char *reason);
void cs_removeuser(reguser *rup);
int checkresponse(reguser *rup, const unsigned char *entropy, const char *response, CRAlgorithm algorithm);
int checkhashpass(reguser *rup, const char *junk, const char *hash);
flag_t cs_sanitisechanlev(flag_t flags);

typedef int (*UnbanFN)(void *arg, struct chanban *ban);
void cs_unbanfn(nick *sender, chanindex *cip, UnbanFN fn, void *arg, int removepermbans);

/* chanservstdcmds.c */
int cs_doshowcommands(void *source, int cargc, char **cargv);
int cs_dohelp(void *source, int cargc, char **cargv);
int cs_doquit(void *source, int cargc, char **cargv);
int cs_dorename(void *source, int cargc, char **cargv);
int cs_dorehash(void *source, int cargc, char **cargv);
int cs_doversion(void *source, int cargc, char **cargv);
int cs_doctcpping(void *source, int cargc, char **cargv);
int cs_doctcpversion(void *source, int cargc, char **cargv);
int cs_doctcpgender(void *source, int cargc, char **cargv);

/* chanservnetevents.c */
void cs_handlenick(int hooknum, void *arg);
void cs_handlesethost(int hooknum, void *arg);
void cs_handlelostnick(int hooknum, void *arg);
void cs_handlenewchannel(int hooknum, void *arg);
void cs_handlelostchannel(int hooknum, void *arg);
void cs_handlejoin(int hooknum, void *arg);
void cs_handlemodechange(int hooknum, void *arg);
void cs_handleburst(int hooknum, void *arg);
void cs_handletopicchange(int hooknum, void *arg);
void cs_handleopchange(int hooknum, void *arg);
void cs_handlenewban(int hooknum, void *arg);
void cs_handlechanlostuser(int hooknum, void *arg);

/* chanservmessages.c */
void initmessages();

/* chanservprivs.c */
int cs_privcheck(int privnum, nick *np);
chanindex *cs_checkaccess(nick *np, const char *chan, unsigned int flags, chanindex *cip, const char *cmdname, int priv, int quiet);

/* chanservlog.c */
void cs_initlog();
void cs_closelog();
void cs_log(nick *np, char *event, ...);

/* chanservdump.c */
int dumplastjoindata(const char *filename);
int readlastjoindata(const char *filename);

/* chanservschedule.c */
void chanservdgline(void *arg);

/* authlib.c */
int csa_checkeboy(nick *sender, char *eboy);
void csa_createrandompw(char *pw, int n);
int csa_checkthrottled(nick *sender, reguser *rup, char *s);

/* chanservdb_updates.c */
void csdb_updateauthinfo(reguser *rup);
void csdb_updatelastjoin(regchanuser *rcup);
void csdb_updatetopic(regchan *rcp);
void csdb_updatechannel(regchan *rcp);
void csdb_updatechannelcounters(regchan *rcp);
void csdb_updatechanneltimestamp(regchan *rcp);
void csdb_createchannel(regchan *rcp);
void csdb_deletechannel(regchan *rcp);
void csdb_deleteuser(reguser *rup);
void csdb_updateuser(reguser *rup);
void csdb_createuser(reguser *rup);
void csdb_updatechanuser(regchanuser *rcup);
void csdb_createchanuser(regchanuser *rcup);
void csdb_deletechanuser(regchanuser *rcup);
void csdb_createban(regchan *rcp, regban *rbp);
void csdb_deleteban(regban *rbp);
void csdb_createmail(reguser *rup, int type);
void csdb_deletemaildomain(maildomain *mdp);
void csdb_createmaildomain(maildomain *mdp);
void csdb_updatemaildomain(maildomain *mdp);
void csdb_chanlevhistory_insert(regchan *rcp, nick *np, reguser *trup, flag_t oldflags, flag_t newflags);
void csdb_accounthistory_insert(nick *np, char *oldpass, char *newpass, sstring *oldemail, sstring *newemail);
void csdb_cleanuphistories();

#endif
