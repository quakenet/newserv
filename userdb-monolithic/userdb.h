/*
 * UserDB
 */

#ifndef USERDB_H
#define USERDB_H

#undef USERDB_ENABLEREPLICATION

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <pcre.h>
#include <assert.h>
#include "../core/config.h"
#include "../core/error.h"
#include "../core/hooks.h"
#include "../irc/irc_config.h"
#include "../lib/flags.h"
#include "../lib/irc_string.h"
#include "../lib/sstring.h"
#include "../nick/nick.h"
#include "../localuser/localuser.h"
#ifdef USERDB_ENABLEREPLICATION
#include "../replicator/replicator.h"
#endif

/* Usertag registry */
#include "userdb_usertags.h"

/* Compilation control */
#define USERDB_ADDITIONALHASHCHECKS
#define USERDB_USEUSERINFOHASHFORLOOKUP

/* String length control */
#define USERDB_LANGUAGELEN                     25
#define USERDB_DEFAULTPASSWORDLEN              10
#define USERDB_MAXUSERNAMELEN                  15
#define USERDB_MAXPASSWORDLEN                  100
#define USERDB_PASSWORDHASHLEN                 32
#define USERDB_EMAILLEN                        150
#define USERDB_SUSPENDREASONLEN                75
#define USERDB_USERTAGNAMELEN                  25
#define USERDB_MAXUSERTAGSTRINGLEN             4000
#define USERDB_MAXMAILDOMAINCONFIGLOCALLIMIT   500000
#define USERDB_MAXMAILDOMAINCONFIGDOMAINLIMIT  500000

/* Misc */
#define USERDB_NOMAILDOMAINCHAR                '!'
#define USERDB_NOMAILDOMAINSYMBOL              "!"

/* User tags control */
#define USERDB_TAGTYPE_UNKNOWN                 0x0000
#define USERDB_TAGTYPE_CHAR                    0x0001
#define USERDB_TAGTYPE_SHORT                   0x0002
#define USERDB_TAGTYPE_INT                     0x0004
#define USERDB_TAGTYPE_LONG                    0x0008
#define USERDB_TAGTYPE_LONGLONG                0x0010
#define USERDB_TAGTYPE_FLOAT                   0x0020
#define USERDB_TAGTYPE_DOUBLE                  0x0040
#define USERDB_TAGTYPE_STRING                  0x1000

/* Mail domains control */
#define USERDB_DEFAULTMAXLOCAL                 3
#define USERDB_DEFAULTMAXDOMAIN                0

/* Hash table control */
#define USERDB_LANGUAGEHASHSIZE                128
#define USERDB_LANGUAGEDELETEDHASHSIZE         8
#define USERDB_USERHASHSIZE                    131072
#define USERDB_USERSUSPENDEDHASHSIZE           16384
#define USERDB_USERDELETEDHASHSIZE             8192
#define USERDB_USERTAGINFOHASHSIZE             128
#define USERDB_USERTAGDATAHASHSIZE             32768
#define USERDB_MAILDOMAINHASHSIZE              131072
#define USERDB_MAILDOMAINCONFIGHASHSIZE        16384
#define USERDB_MAILDOMAINCONFIGDELETEDHASHSIZE 512

/* Database control */
#define USERDB_DATABASE_CHUNKSIZE              1000
#define USERDB_DATABASE_MAXPEROBJECT           25

/* Status flags */
#define UDB_STATUS_EVERYONE             0x0000 /* +E */ /* Dummy flag, cannot be added to accounts */
#define UDB_STATUS_UNAUTHED             0x0001 /* +U */ /* Dummy flag, cannot be added to accounts */
#define UDB_STATUS_AUTHED               0x0002 /* +A */ /* Dummy flag, cannot be added to accounts */
#define UDB_STATUS_TRIAL                0x0004 /* +t */
/*
#define UDB_STATUS_FREE                 0x0008
#define UDB_STATUS_FREE                 0x0010
*/
#define UDB_STATUS_STAFF                0x0020 /* +s */
/*
#define UDB_STATUS_FREE                 0x0040
#define UDB_STATUS_FREE                 0x0080
*/
#define UDB_STATUS_OPER                 0x0100 /* +o */
/*
#define UDB_STATUS_FREE                 0x0200
#define UDB_STATUS_FREE                 0x0400
*/
#define UDB_STATUS_ADMIN                0x0800 /* +a */
/*
#define UDB_STATUS_FREE                 0x1000
#define UDB_STATUS_FREE                 0x2000
*/
#define UDB_STATUS_DEV                  0x4000 /* +d */
/*
#define UDB_STATUS_FREE                 0x8000
*/

#define UDB_STATUS_INTERNALFLAGS        "EAU"
#define UDB_STATUS_INUSE                (UDB_STATUS_UNAUTHED | UDB_STATUS_AUTHED | UDB_STATUS_TRIAL | UDB_STATUS_STAFF | UDB_STATUS_OPER | UDB_STATUS_ADMIN | UDB_STATUS_DEV)
#define UDB_STATUS_ALL                  0xffff

#define UDB_STATUS_USER_CHANGEABLE      0x0000
#define UDB_STATUS_TRIAL_CHANGEABLE     (UDB_STATUS_USER_CHANGEABLE)
#define UDB_STATUS_STAFF_CHANGEABLE     (UDB_STATUS_TRIAL_CHANGEABLE)
#define UDB_STATUS_OPER_CHANGEABLE      (UDB_STATUS_STAFF_CHANGEABLE)
#define UDB_STATUS_ADMIN_CHANGEABLE     (UDB_STATUS_TRIAL | UDB_STATUS_STAFF | UDB_STATUS_OPER | UDB_STATUS_ADMIN | UDB_STATUS_OPER_CHANGEABLE)
#define UDB_STATUS_DEV_CHANGEABLE       (UDB_STATUS_DEV | UDB_STATUS_ADMIN_CHANGEABLE)

#define UDB_STATUS_STAFFLOCK            (UDB_STATUS_STAFF | UDB_STATUS_OPER | UDB_STATUS_ADMIN | UDB_STATUS_DEV)
#define UDB_STATUS_OPERLOCK             (UDB_STATUS_OPER | UDB_STATUS_ADMIN | UDB_STATUS_DEV)

#define UDBSIsTrial(x)                  ((x)->statusflags & (UDB_STATUS_TRIAL | UDB_STATUS_STAFF | UDB_STATUS_OPER | UDB_STATUS_ADMIN | UDB_STATUS_DEV))
#define UDBSIsStaff(x)                  ((x)->statusflags & (UDB_STATUS_STAFF | UDB_STATUS_OPER | UDB_STATUS_ADMIN | UDB_STATUS_DEV))
#define UDBSIsOper(x)                   ((x)->statusflags & (UDB_STATUS_OPER | UDB_STATUS_ADMIN | UDB_STATUS_DEV))
#define UDBSIsAdmin(x)                  ((x)->statusflags & (UDB_STATUS_ADMIN | UDB_STATUS_DEV))
#define UDBSIsDev(x)                    ((x)->statusflags & (UDB_STATUS_DEV))

/* Group flags */
#define UDB_GROUP_NONE                  0x0000 /* +N */
#define UDB_GROUP_TRUSTS                0x0001 /* +t */
#define UDB_GROUP_FAQS                  0x0002 /* +f */
#define UDB_GROUP_HELP                  0x0004 /* +h */
#define UDB_GROUP_HELPSCRIPT            0x0008 /* +s */
#define UDB_GROUP_WORMSQUAD             0x0010 /* +w */
#define UDB_GROUP_PUBLICRELATIONS       0x0020 /* +p */
#define UDB_GROUP_FEDS                  0x0040 /* +F */
#define UDB_GROUP_SECURITY              0x0080 /* +S */
#define UDB_GROUP_HUMANRESOURCES        0x0100 /* +H */
/*
#define UDB_GROUP_FREE                  0x0200
#define UDB_GROUP_FREE                  0x0400
#define UDB_GROUP_FREE                  0x0800
#define UDB_GROUP_FREE                  0x1000
#define UDB_GROUP_FREE                  0x2000
#define UDB_GROUP_FREE                  0x4000
*/
#define UDB_GROUP_GROUPLEADERS          0x8000 /* +G */

#define UDB_GROUP_INTERNALFLAGS         "N"
#define UDB_GROUP_INUSE                 (UDB_GROUP_TRUSTS | UDB_GROUP_FAQS | UDB_GROUP_HELP | UDB_GROUP_HELPSCRIPT | UDB_GROUP_WORMSQUAD | UDB_GROUP_PUBLICRELATIONS | UDB_GROUP_FEDS | UDB_GROUP_SECURITY | UDB_GROUP_HUMANRESOURCES | UDB_GROUP_GROUPLEADERS)
#define UDB_GROUP_ALL                   0xffff

#define UDB_GROUP_USER_ALLOWED          0x0000
#define UDB_GROUP_TRIAL_ALLOWED         (UDB_GROUP_HELP | UDB_GROUP_HELPSCRIPT)
#define UDB_GROUP_STAFF_ALLOWED         (UDB_GROUP_TRUSTS | UDB_GROUP_FAQS | UDB_GROUP_WORMSQUAD | UDB_GROUP_PUBLICRELATIONS | UDB_GROUP_HUMANRESOURCES | UDB_GROUP_GROUPLEADERS | UDB_GROUP_TRIAL_ALLOWED)
#define UDB_GROUP_OPER_ALLOWED          (UDB_GROUP_FEDS | UDB_GROUP_SECURITY | UDB_GROUP_STAFF_ALLOWED)
#define UDB_GROUP_ADMIN_ALLOWED         (UDB_GROUP_OPER_ALLOWED)
#define UDB_GROUP_DEV_ALLOWED           (UDB_GROUP_ADMIN_ALLOWED)

#define UDB_GROUP_STAFFLOCK             (UDB_GROUP_STAFF_ALLOWED)
#define UDB_GROUP_OPERLOCK              (UDB_GROUP_ALL & ~(UDB_GROUP_STAFF_ALLOWED))

#define UDBGIsTrusts(x)                 ((x)->groupflags & (UDB_GROUP_TRUSTS))
#define UDBGIsFAQs(x)                   ((x)->groupflags & (UDB_GROUP_FAQS))
#define UDBGIsHelp(x)                   ((x)->groupflags & (UDB_GROUP_HELP))
#define UDBGIsHelpScript(x)             ((x)->groupflags & (UDB_GROUP_HELPSCRIPT))
#define UDBGIsWormSquad(x)              ((x)->groupflags & (UDB_GROUP_WORMSQUAD))
#define UDBGIsFeds(x)                   ((x)->groupflags & (UDB_GROUP_FEDS))
#define UDBGIsSecurity(x)               ((x)->groupflags & (UDB_GROUP_SECURITY))
#define UDBGIsGroupLeaders(x)           ((x)->groupflags & (UDB_GROUP_GROUPLEADERS))

/* Option flags */
#define UDB_OPTION_NOTICES              0x0001 /* +n */
#define UDB_OPTION_UNUSED               0x0002 /* +u */
#define UDB_OPTION_TRUST                0x0004 /* +t */
#define UDB_OPTION_SUSPENDED            0x0008 /* +z */
#define UDB_OPTION_INSTANTKILL          0x0010 /* +k */
#define UDB_OPTION_INSTANTGLINE         0x0020 /* +g */
#define UDB_OPTION_DELAYEDGLINE         0x0040 /* +G */
#define UDB_OPTION_WATCH                0x0080 /* +w */
#define UDB_OPTION_PROTECTNICK          0x0100 /* +p */
#define UDB_OPTION_NOAUTHLIMIT          0x0200 /* +l */
#define UDB_OPTION_CLEANUPIGNORE        0x0400 /* +c */
#define UDB_OPTION_BYPASSSECURITY       0x0800 /* +b */
#define UDB_OPTION_HIDEUSERS            0x1000 /* +h */
/*
#define UDB_OPTION_FREE                 0x2000
#define UDB_OPTION_FREE                 0x4000
#define UDB_OPTION_FREE                 0x8000
*/
#define UDB_OPTION_INUSE                (UDB_OPTION_NOTICES | UDB_OPTION_UNUSED | UDB_OPTION_TRUST | UDB_OPTION_SUSPENDED | UDB_OPTION_INSTANTKILL | UDB_OPTION_INSTANTGLINE | UDB_OPTION_DELAYEDGLINE | UDB_OPTION_WATCH | UDB_OPTION_PROTECTNICK | UDB_OPTION_NOAUTHLIMIT | UDB_OPTION_CLEANUPIGNORE | UDB_OPTION_BYPASSSECURITY | UDB_OPTION_HIDEUSERS)
#define UDB_OPTION_ALL                  0xffff
#define UDB_OPTION_AUTOMATIC            (UDB_OPTION_UNUSED | UDB_OPTION_TRUST | UDB_OPTION_SUSPENDED | UDB_OPTION_INSTANTKILL | UDB_OPTION_INSTANTGLINE | UDB_OPTION_DELAYEDGLINE)
#define UDB_OPTION_AUTOMATICFLAGS       "utzkgG"
#define UDB_OPTION_HIDDEN               (UDB_OPTION_SUSPENDED | UDB_OPTION_INSTANTKILL | UDB_OPTION_INSTANTGLINE | UDB_OPTION_DELAYEDGLINE)
#define UDB_OPTION_USER_VISIBLE         (UDB_OPTION_NOTICES)

#define UDB_OPTION_USER_ALLOWED         (UDB_OPTION_NOTICES | UDB_OPTION_UNUSED | UDB_OPTION_TRUST | UDB_OPTION_SUSPENDED | UDB_OPTION_INSTANTKILL | UDB_OPTION_INSTANTGLINE | UDB_OPTION_DELAYEDGLINE | UDB_OPTION_WATCH | UDB_OPTION_CLEANUPIGNORE)
#define UDB_OPTION_TRIAL_ALLOWED        (UDB_OPTION_USER_ALLOWED)
#define UDB_OPTION_STAFF_ALLOWED        (UDB_OPTION_PROTECTNICK | UDB_OPTION_NOAUTHLIMIT | UDB_OPTION_BYPASSSECURITY | UDB_OPTION_HIDEUSERS | UDB_OPTION_TRIAL_ALLOWED)
#define UDB_OPTION_OPER_ALLOWED         (UDB_OPTION_STAFF_ALLOWED)
#define UDB_OPTION_ADMIN_ALLOWED        (UDB_OPTION_OPER_ALLOWED)
#define UDB_OPTION_DEV_ALLOWED          (UDB_OPTION_ADMIN_ALLOWED)

#define UDB_OPTION_USER_CHANGEABLE      (UDB_OPTION_NOTICES)
#define UDB_OPTION_TRIAL_CHANGEABLE     (UDB_OPTION_USER_CHANGEABLE)
#define UDB_OPTION_STAFF_CHANGEABLE     (UDB_OPTION_PROTECTNICK | UDB_OPTION_HIDEUSERS | UDB_OPTION_TRIAL_CHANGEABLE)
#define UDB_OPTION_OPER_CHANGEABLE      (UDB_OPTION_WATCH | UDB_OPTION_NOAUTHLIMIT | UDB_OPTION_CLEANUPIGNORE | UDB_OPTION_STAFF_CHANGEABLE)
#define UDB_OPTION_ADMIN_CHANGEABLE     (UDB_OPTION_OPER_CHANGEABLE)
#define UDB_OPTION_DEV_CHANGEABLE       (UDB_OPTION_BYPASSSECURITY | UDB_OPTION_ADMIN_CHANGEABLE)

#define UDBOIsNotices(x)                ((x)->optionflags & UDB_OPTION_NOTICES)
#define UDBOIsUnused(x)                 ((x)->optionflags & UDB_OPTION_UNUSED)
#define UDBOIsTrust(x)                  ((x)->optionflags & UDB_OPTION_TRUST)
#define UDBOIsSuspended(x)              ((x)->optionflags & UDB_OPTION_SUSPENDED)
#define UDBOIsInstantKill(x)            ((x)->optionflags & UDB_OPTION_INSTANTKILL)
#define UDBOIsInstantGLine(x)           ((x)->optionflags & UDB_OPTION_INSTANTGLINE)
#define UDBOIsDelayedGLine(x)           ((x)->optionflags & UDB_OPTION_DELAYEDGLINE)
#define UDBOIsWatch(x)                  ((x)->optionflags & UDB_OPTION_WATCH)
#define UDBOIsProtectNick(x)            ((x)->optionflags & UDB_OPTION_PROTECTNICK)
#define UDBOIsNoAuthLimit(x)            ((x)->optionflags & UDB_OPTION_NOAUTHLIMIT)
#define UDBOIsCleanupIgnore(x)          ((x)->optionflags & UDB_OPTION_CLEANUPIGNORE)
#define UDBOIsBypassSecurity(x)         ((x)->optionflags & UDB_OPTION_BYPASSSECURITY)
#define UDBOIsHideUsers(x)              ((x)->optionflags & UDB_OPTION_HIDEUSERS)

/* Notice flags */
#define UDB_NOTICE_VERBOSE              0x0001 /* +v */ /* Verbose, for the bored type */
#define UDB_NOTICE_CLONING              0x0002 /* +c */ /* Clone detection */
#define UDB_NOTICE_HITS                 0x0004 /* +h */ /* Where a GLINE or KILL is set automatically by the bot */
#define UDB_NOTICE_USERACTIONS          0x0008 /* +u */ /* KICK/KILL/GLINE commands */
#define UDB_NOTICE_SUSPENSIONS          0x0010 /* +s */ /* Suspensions */
#define UDB_NOTICE_CLEARCHAN            0x0020 /* +C */ /* When someone CLEARCHANs */
#define UDB_NOTICE_AUTHENTICATION       0x0040 /* +a */ /* When someone AUTHs/OPERs */
#define UDB_NOTICE_MISC                 0x0080 /* +U */ /* Misc commands */
#define UDB_NOTICE_BROADCASTS           0x0100 /* +b */ /* Broadcast/MBroadcast/SBroadcast/CBroadcast */
#define UDB_NOTICE_FAKEUSERS            0x0200 /* +f */ /* Fakeuser operations */
#define UDB_NOTICE_TRUSTS               0x0400 /* +t */ /* Trust operations */
/*
#define UDB_NOTICE_FREE                 0x0800
*/
#define UDB_NOTICE_CHANNELMANAGEMENT    0x1000 /* +M */ /* ADDCHAN, DELCHAN, SUSPENDCHAN */
#define UDB_NOTICE_ACCOUNTMANAGEMENT    0x2000 /* +m */ /* HELLO, CHANGEPASSWORD, STATUSFLAGS, NOTICEFLAGS */
#define UDB_NOTICE_OPERATIONS           0x4000 /* +o */ /* INSMOD/RMMOD/RELOAD */
#define UDB_NOTICE_ALLCOMMANDS          0x8000 /* +A */ /* All commands sent to a bot */

#define UDB_NOTICE_INUSE                (UDB_NOTICE_VERBOSE | UDB_NOTICE_CLONING | UDB_NOTICE_HITS | UDB_NOTICE_USERACTIONS | UDB_NOTICE_SUSPENSIONS | UDB_NOTICE_CLEARCHAN | UDB_NOTICE_AUTHENTICATION | UDB_NOTICE_MISC | UDB_NOTICE_BROADCASTS | UDB_NOTICE_FAKEUSERS | UDB_NOTICE_TRUSTS | UDB_NOTICE_CHANNELMANAGEMENT | UDB_NOTICE_ACCOUNTMANAGEMENT | UDB_NOTICE_OPERATIONS | UDB_NOTICE_ALLCOMMANDS)
#define UDB_NOTICE_ALL                  0xffff

#define UDB_NOTICE_USER_ALLOWED         0x0000
#define UDB_NOTICE_TRIAL_ALLOWED        (UDB_NOTICE_USER_ALLOWED)
#define UDB_NOTICE_STAFF_ALLOWED        (UDB_NOTICE_VERBOSE | UDB_NOTICE_TRUSTS | UDB_NOTICE_TRIAL_ALLOWED)
#define UDB_NOTICE_OPER_ALLOWED         (UDB_NOTICE_CLONING | UDB_NOTICE_HITS | UDB_NOTICE_USERACTIONS | UDB_NOTICE_SUSPENSIONS | UDB_NOTICE_CLEARCHAN | UDB_NOTICE_AUTHENTICATION | UDB_NOTICE_MISC | UDB_NOTICE_BROADCASTS | UDB_NOTICE_FAKEUSERS | UDB_NOTICE_CHANNELMANAGEMENT | UDB_NOTICE_ACCOUNTMANAGEMENT | UDB_NOTICE_STAFF_ALLOWED)
#define UDB_NOTICE_ADMIN_ALLOWED        (UDB_NOTICE_OPERATIONS | UDB_NOTICE_OPER_ALLOWED)
#define UDB_NOTICE_DEV_ALLOWED          (UDB_NOTICE_ALLCOMMANDS | UDB_NOTICE_ADMIN_ALLOWED)

#define UDB_NOTICE_STAFFLOCK            (UDB_NOTICE_STAFF_ALLOWED)
#define UDB_NOTICE_OPERLOCK             (UDB_NOTICE_ALL & ~(UDB_NOTICE_STAFF_ALLOWED))

/* Mail domain flags */
#define UDB_DOMAIN_BANNED               0x0001 /* +b */
#define UDB_DOMAIN_WATCH                0x0002 /* +w */
#define UDB_DOMAIN_LOCALLIMIT           0x0004 /* +l */
#define UDB_DOMAIN_DOMAINLIMIT          0x0008 /* +L */
#define UDB_DOMAIN_EXCLUDESUBDOMAINS    0x0010 /* +e */
/*
#define UDB_DOMAIN_UNUSED               0x0020
#define UDB_DOMAIN_UNUSED               0x0040
#define UDB_DOMAIN_UNUSED               0x0080
#define UDB_DOMAIN_UNUSED               0x0100
#define UDB_DOMAIN_UNUSED               0x0200
#define UDB_DOMAIN_UNUSED               0x0400
#define UDB_DOMAIN_UNUSED               0x0800
#define UDB_DOMAIN_UNUSED               0x1000
#define UDB_DOMAIN_UNUSED               0x2000
#define UDB_DOMAIN_UNUSED               0x4000
#define UDB_DOMAIN_UNUSED               0x8000
*/
#define UDB_DOMAIN_INUSE                (UDB_DOMAIN_BANNED | UDB_DOMAIN_WATCH | UDB_DOMAIN_LOCALLIMIT | UDB_DOMAIN_DOMAINLIMIT | UDB_DOMAIN_EXCLUDESUBDOMAINS)
#define UDB_DOMAIN_ALL                  0xffff

#define UDBDIsBanned(x)                 ((x)->domainflags & UDB_DOMAIN_BANNED)
#define UDBDIsWatch(x)                  ((x)->domainflags & UDB_DOMAIN_WATCH)
#define UDBDIsLocalLimit(x)             ((x)->domainflags & UDB_DOMAIN_LOCALLIMIT)
#define UDBDIsDomainLimit(x)            ((x)->domainflags & UDB_DOMAIN_DOMAINLIMIT)
#define UDBDIsExcludeSubdomains(x)      ((x)->domainflags & UDB_DOMAIN_EXCLUDESUBDOMAINS)

/* Replication flags */
#define UDB_REPLICATION_DELETED         0x0001 /* +d */
/*
#define UDB_REPLICATION_UNUSED          0x0002
#define UDB_REPLICATION_UNUSED          0x0004
#define UDB_REPLICATION_UNUSED          0x0008
#define UDB_REPLICATION_UNUSED          0x0010
#define UDB_REPLICATION_UNUSED          0x0020
#define UDB_REPLICATION_UNUSED          0x0040
#define UDB_REPLICATION_UNUSED          0x0080
#define UDB_REPLICATION_UNUSED          0x0100
#define UDB_REPLICATION_UNUSED          0x0200
#define UDB_REPLICATION_UNUSED          0x0400
#define UDB_REPLICATION_UNUSED          0x0800
#define UDB_REPLICATION_UNUSED          0x1000
#define UDB_REPLICATION_UNUSED          0x2000
#define UDB_REPLICATION_UNUSED          0x4000
#define UDB_REPLICATION_UNUSED          0x8000
*/
#define UDB_REPLICATION_ALL             0xffff

#define UDBRIsDeleted(x)                ((x)->replicationflags & UDB_REPLICATION_DELETED)

/* Variable ID numbers */
#define USERDB_VARID_LASTLANGUAGEID     1
#define USERDB_VARID_LASTLANGUAGETID    2
#define USERDB_VARID_LASTUSERID         3
#define USERDB_VARID_LASTUSERTID        4
#define USERDB_VARID_LASTMAILDOMAINID   5
#define USERDB_VARID_LASTMAILDOMAINTID  6

/* Database status codes */
#define USERDB_DATABASESTATE_INIT       1
#define USERDB_DATABASESTATE_LOADING    2
#define USERDB_DATABASESTATE_READY      3
#define USERDB_DATABASESTATE_ERROR      4

/* Generic return codes */
#define USERDB_OK                       0
#define USERDB_NOOP                     1
#define USERDB_ERROR                    2
#define USERDB_BADPARAMETERS            3

/* Library return codes */
#define USERDB_LENGTHEXCEEDED           10
#define USERDB_WRONGPASSWORD            11

/* Database return codes */
#define USERDB_NOACCESS                 20
#define USERDB_LOCKED                   21
#define USERDB_DBUNAVAILABLE            22

/* Language validation failure return codes */
#define USERDB_BADLANGUAGECODE          20
#define USERDB_BADLANGUAGENAME          21

/* User validation failure return codes */
#define USERDB_BADUSERID                30
#define USERDB_BADUSERNAME              31
#define USERDB_BADPASSWORD              32
#define USERDB_BADEMAIL                 33
#define USERDB_BADSUSPENDENDDATE        34
#define USERDB_BADSUSPENDREASON         35

/* Mail domain configuration validation failure return codes */
#define USERDB_BADMAILDOMAINCONFIGNAME  40
#define USERDB_BADLOCALLIMIT            41
#define USERDB_BADDOMAINLIMIT           42

/* Hash result codes */
#define USERDB_ALREADYEXISTS            50
#define USERDB_NOTFOUND                 51
#define USERDB_NOTAUTHED                52
#define USERDB_NOTONLINE                53

/* Email check return codes */
#define USERDB_DOMAINBANNED             60
#define USERDB_DOMAINLIMITREACHED       61
#define USERDB_LOCALLIMITREACHED        62
#define USERDB_RESULTLIMITREACHED       63

/* User tag return codes */
#define USERDB_MISMATCH                 70

/* ACL return codes */
#define USERDB_PERMISSIONDENIED         80
#define USERDB_UNKNOWNFLAG              81
#define USERDB_INTERNALFLAG             82
#define USERDB_AUTOMATICFLAG            83
#define USERDB_DISALLOWEDFLAG           84
#define USERDB_DISALLOWEDCHANGE         85

typedef struct userdb_language
{
    /* Language Details */
    unsigned long languageid;
    sstring *languagecode;
    sstring *languagename;
    
    /* Replication */
    flag_t replicationflags;
    time_t created, modified;
    unsigned long revision;
    unsigned long long transactionid;
    
    /* Database control */
    unsigned short databaseops;
    
    /* Hash table control */
    struct userdb_language *nextbyid;
    struct userdb_language *nextbycode;
} userdb_language;

typedef struct userdb_user
{
    /* Account Details */
    unsigned long userid;
    sstring *username;
    unsigned char password[USERDB_PASSWORDHASHLEN];
    
    /* Language */
    unsigned long languageid;
    struct userdb_language *languagedata;
    
    /* Email */
    sstring *emaillocal;
    struct userdb_maildomain *maildomain;
    
    /* Flags */
    flag_t statusflags, groupflags, optionflags, noticeflags;
    
    /* Suspension */
    struct userdb_suspension *suspensiondata;
    
    /* User tags */
    struct userdb_usertagdata *usertags;
    
    /* Replication */
    flag_t replicationflags;
    time_t created, modified;
    unsigned long revision;
    unsigned long long transactionid;
    
    /* Database control */
    unsigned short databaseops;
    
    /* Hash table control */
    struct userdb_user *nextbyid;
    struct userdb_user *nextbyname;
    struct userdb_user *nextbymaildomain;
} userdb_user;

typedef struct userdb_usertagdatastringholder
{
    unsigned char *stringdata;
    unsigned short stringsize;
} userdb_usertagdatastringholder;

typedef union userdb_usertagdataholder
{
    unsigned char tagchar;
    unsigned short tagshort;
    unsigned int tagint;
    unsigned long taglong;
    unsigned long long taglonglong;
    float tagfloat;
    double tagdouble;
    struct userdb_usertagdatastringholder tagstring;
} userdb_usertagdataholder;

typedef struct userdb_usertaginfo
{
    /* Tag information */
    unsigned short tagid, tagtype;
    sstring *tagname;
    struct userdb_usertagdata *usertags[USERDB_USERTAGDATAHASHSIZE];
    
    /* Hash table control */
    struct userdb_usertaginfo *nextbyid;
} userdb_usertaginfo;

typedef struct userdb_usertagdata
{
    /* Tag details */
    struct userdb_usertaginfo *taginfo;
    struct userdb_user *userinfo;
    
    /* Data in the tag */
    union userdb_usertagdataholder tagdata;
    
    /* Hash table control */
    struct userdb_usertagdata *nextbytag;
    struct userdb_usertagdata *nextbyuser;
} userdb_usertagdata;

typedef struct userdb_suspension
{
    /* Suspension details */
    time_t suspendstart, suspendend;
    sstring *reason;
    
    /* Suspender details */
    unsigned long suspenderid;
    struct userdb_user *suspenderdata;
    
    /* Hash table control */
    struct userdb_user *nextbyid;
} userdb_suspension;

typedef struct userdb_maildomain
{
    /* Domain details */
    sstring *maildomainname;
    
    /* Stats */
    unsigned long domaincount, totaldomaincount, configcount;
    
    /* Users */
    struct userdb_user *users;
    
    /* Tree control */
    struct userdb_maildomain *parent;
    
    /* Mail domain configuration */
    struct userdb_maildomainconfig *config;
    
    /* Hash table control */
    struct userdb_maildomain *nextbyname;
} userdb_maildomain;

typedef struct userdb_maildomainconfig
{
    /* Domain details */
    unsigned long configid;
    sstring *configname;
    
    /* Flags */
    flag_t domainflags;
    
    /* Limits */
    unsigned long maxlocal, maxdomain;
    
    /* Mail domain */
    userdb_maildomain *maildomain;
    
    /* Database control */
    unsigned short databaseops;
    
    /* Replication */
    flag_t replicationflags;
    time_t created, modified;
    unsigned long revision;
    unsigned long long transactionid;
    
    /* Hash table control */
    struct userdb_maildomainconfig *nextbyid;
    struct userdb_maildomainconfig *nextbyname;
} userdb_maildomainconfig;

/* Function typedefs */
typedef void (*userdb_scanuserusernamescallback)(userdb_user *, char *, unsigned int, unsigned int, void *);
typedef void (*userdb_scanuseremailaddressescallback)(userdb_user *, char *, unsigned int, unsigned int, void *);
typedef void (*userdb_scanuserpasswordscallback)(userdb_user *, char *, unsigned int, unsigned int, void *);
typedef void (*userdb_scanusersuspensionscallback)(userdb_user *, char *, unsigned int, unsigned int, void *);
typedef void (*userdb_scanmaildomainconfignamescallback)(userdb_maildomainconfig *, char *, unsigned int, unsigned int, void *);

/* Module status */
extern unsigned long userdb_moduleloaded;

/* Database status */
extern unsigned long userdb_databasestatus;
extern unsigned long userdb_languagedatabaseaddcount, userdb_userdatabaseaddcount, userdb_maildomainconfigdatabaseaddcount;
extern unsigned long userdb_languagedatabasemodifycount, userdb_userdatabasemodifycount, userdb_maildomainconfigdatabasemodifycount;
extern unsigned long userdb_languagedatabasedeletecount, userdb_userdatabasedeletecount, userdb_maildomainconfigdatabasedeletecount;

/* Unique number control */
extern unsigned long userdb_lastlanguageid, userdb_lastuserid, userdb_lastmaildomainconfigid;

/* Replication control */
#ifdef USERDB_ENABLEREPLICATION
extern replicator_group *userdb_repl_group;
extern replicator_object *userdb_repl_obj_language;
extern replicator_object *userdb_repl_obj_user;
extern replicator_object *userdb_repl_obj_maildomainconfig;
#endif
extern unsigned long long userdb_lastlanguagetransactionid, userdb_lastusertransactionid, userdb_lastmaildomainconfigtransactionid;

/* Auth extension */
extern int userdb_authnameextnum;

/* Email address regex */
#define USERDB_EMAILREGEX               "^([a-zA-Z0-9_\\-\\.]+)@([a-zA-Z0-9_\\-\\.]+)\\.([a-zA-Z]{2,5})$"
extern pcre *userdb_emailregex;
extern pcre_extra *userdb_emailregexextra;

/* Hash tables */
extern userdb_language *userdb_languagehashtable_id[USERDB_LANGUAGEHASHSIZE];
extern userdb_language *userdb_languagehashtable_code[USERDB_LANGUAGEHASHSIZE];
extern userdb_user *userdb_userhashtable_id[USERDB_USERHASHSIZE];
extern userdb_user *userdb_userhashtable_username[USERDB_USERHASHSIZE];
extern userdb_user *userdb_usersuspendedhashtable[USERDB_USERSUSPENDEDHASHSIZE];
extern userdb_usertaginfo *userdb_usertaginfohashtable[USERDB_USERTAGINFOHASHSIZE];
extern userdb_maildomain *userdb_maildomainhashtable_name[USERDB_MAILDOMAINHASHSIZE];
extern userdb_maildomainconfig *userdb_maildomainconfighashtable_id[USERDB_MAILDOMAINCONFIGHASHSIZE];
extern userdb_maildomainconfig *userdb_maildomainconfighashtable_name[USERDB_MAILDOMAINCONFIGHASHSIZE];

#ifdef USERDB_ENABLEREPLICATION
extern userdb_language *userdb_languagedeletedhashtable[USERDB_LANGUAGEDELETEDHASHSIZE];
extern userdb_user *userdb_userdeletedhashtable[USERDB_USERDELETEDHASHSIZE];
extern userdb_maildomainconfig *userdb_maildomainconfigdeletedhashtable[USERDB_MAILDOMAINCONFIGDELETEDHASHSIZE];
#endif

#define userdb_getlanguageidhash(x)                        ((x) % USERDB_LANGUAGEHASHSIZE)
#define userdb_getlanguagecodehash(x)                      ((crc32i(x)) % USERDB_LANGUAGEHASHSIZE)
#define userdb_getlanguagedeletedhash(x)                   ((x) % USERDB_LANGUAGEDELETEDHASHSIZE)
#define userdb_getuseridhash(x)                            ((x) % USERDB_USERHASHSIZE)
#define userdb_getusernamehash(x)                          ((crc32i(x)) % USERDB_USERHASHSIZE)
#define userdb_getusersuspendedhash(x)                     ((x) % USERDB_USERSUSPENDEDHASHSIZE)
#define userdb_getuserdeletedhash(x)                       ((x) % USERDB_USERDELETEDHASHSIZE)
#define userdb_getusertaginfohash(x)                       ((x) % USERDB_USERTAGINFOHASHSIZE)
#define userdb_getusertagdatahash(x)                       ((x) % USERDB_USERTAGDATAHASHSIZE)
#define userdb_getmaildomainnamehash(x)                    ((crc32i(x)) % USERDB_MAILDOMAINHASHSIZE)
#define userdb_getmaildomainconfigidhash(x)                ((x) % USERDB_MAILDOMAINCONFIGHASHSIZE)
#define userdb_getmaildomainconfignamehash(x)              ((crc32i(x)) % USERDB_MAILDOMAINCONFIGHASHSIZE)
#define userdb_getmaildomainconfigdeletedhash(x)           ((x) % USERDB_MAILDOMAINCONFIGDELETEDHASHSIZE)

/* Flags */
extern const flag userdb_statusflags[];
extern const flag userdb_groupflags[];
extern const flag userdb_optionflags[];
extern const flag userdb_noticeflags[];
extern const flag userdb_maildomainconfigflags[];

/* Access control */
extern long userdb_staffservernumeric;

/* userdb.c */
int userdb_moduleready();

/* userdb_authext.c */
int userdb_finduserbyauthname(authname *an, userdb_user **user);
int userdb_finduserbynick(nick *nn, userdb_user **user);
int userdb_assignusertoauth(userdb_user *user); /* INTERNAL USE ONLY */
int userdb_unassignuserfromauth(userdb_user *user); /* INTERNAL USE ONLY */

/* userdb_db.c */
int userdb_databaseavailable(); /* INTERNAL USE ONLY */
int userdb_databaseloaded();
void userdb_initdatabase(); /* INTERNAL USE ONLY */
void userdb_addlanguagetodb(userdb_language *language); /* INTERNAL USE ONLY */
void userdb_modifylanguageindb(userdb_language *language); /* INTERNAL USE ONLY */
void userdb_deletelanguagefromdb(userdb_language *language); /* INTERNAL USE ONLY */
void userdb_addusertodb(userdb_user *user); /* INTERNAL USE ONLY */
void userdb_modifyuserindb(userdb_user *user); /* INTERNAL USE ONLY */
void userdb_deleteuserfromdb(userdb_user *user); /* INTERNAL USE ONLY */
void userdb_addusertaginfotodb(userdb_usertaginfo *usertaginfo); /* INTERNAL USE ONLY */
void userdb_deleteusertaginfofromdb(userdb_usertaginfo *usertaginfo); /* INTERNAL USE ONLY */
void userdb_addusertagdatatodb(userdb_usertagdata *usertagdata); /* INTERNAL USE ONLY */
void userdb_modifyusertagdataindb(userdb_usertagdata *usertagdata); /* INTERNAL USE ONLY */
void userdb_deleteusertagdatafromdb(userdb_usertagdata *usertagdata); /* INTERNAL USE ONLY */
void userdb_deleteallusertagdatabyinfofromdb(userdb_usertaginfo *usertaginfo); /* INTERNAL USE ONLY */
void userdb_deleteallusertagdatabyuserfromdb(userdb_user *user); /* INTERNAL USE ONLY */
void userdb_addmaildomainconfigtodb(userdb_maildomainconfig *maildomainconfig); /* INTERNAL USE ONLY */
void userdb_modifymaildomainconfigindb(userdb_maildomainconfig *maildomainconfig); /* INTERNAL USE ONLY */
void userdb_deletemaildomainconfigfromdb(userdb_maildomainconfig *maildomainconfig); /* INTERNAL USE ONLY */

/* userdb_hash.c */
void userdb_inithashtables(); /* INTERNAL USE ONLY */
void userdb_addlanguagetohash(userdb_language *language); /* INTERNAL USE ONLY */
int userdb_findlanguagebylanguageid(unsigned long languageid, userdb_language **language);
int userdb_findlanguagebylanguagecode(char *languagecode, userdb_language **language);
void userdb_deletelanguagefromhash(userdb_language *language); /* INTERNAL USE ONLY */
void userdb_addusertohash(userdb_user *user); /* INTERNAL USE ONLY */
int userdb_finduserbyuserid(unsigned long userid, userdb_user **user);
int userdb_finduserbyusername(char *username, userdb_user **user);
int userdb_finduserbystring(char *userstring, userdb_user **user);
void userdb_deleteuserfromhash(userdb_user *user); /* INTERNAL USE ONLY */
void userdb_addusertaginfotohash(userdb_usertaginfo *usertaginfo); /* INTERNAL USE ONLY */
int userdb_findusertaginfo(unsigned short tagid, userdb_usertaginfo **usertaginfo);
void userdb_deleteusertaginfofromhash(userdb_usertaginfo *usertaginfo); /* INTERNAL USE ONLY */
void userdb_addusertagdatatohash(userdb_usertagdata *usertagdata); /* INTERNAL USE ONLY */
int userdb_findusertagdata(userdb_usertaginfo *usertaginfo, userdb_user *user, userdb_usertagdata **usertagdata);
void userdb_deleteusertagdatafromhash(userdb_usertagdata *usertagdata); /* INTERNAL USE ONLY */
void userdb_deleteallusertagdatabyinfofromhash(userdb_usertaginfo *usertaginfo); /* INTERNAL USE ONLY */
void userdb_deleteallusertagdatabyuserfromhash(userdb_user *user); /* INTERNAL USE ONLY */
void userdb_addmaildomaintohash(userdb_maildomain *maildomain); /* INTERNAL USE ONLY */
int userdb_findmaildomainbyname(char *maildomainname, userdb_maildomain *parent, userdb_maildomain **maildomain);
int userdb_findmaildomainbyemaildomain(char *emaildomain, userdb_maildomain **maildomain, int allowcreate);
void userdb_addusertomaildomain(userdb_user *user, userdb_maildomain *maildomain); /* INTERNAL USE ONLY */
void userdb_deleteuserfrommaildomain(userdb_user *user, userdb_maildomain *maildomain); /* INTERNAL USE ONLY */
void userdb_cleanmaildomaintree(userdb_maildomain *maildomain); /* INTERNAL USE ONLY */
void userdb_deletemaildomainfromhash(userdb_maildomain *maildomain); /* INTERNAL USE ONLY */
void userdb_addmaildomainconfigtohash(userdb_maildomainconfig *maildomainconfig); /* INTERNAL USE ONLY */
int userdb_findmaildomainconfigbyid(unsigned long configid, userdb_maildomainconfig **maildomainconfig);
int userdb_findmaildomainconfigbyname(char *configname, userdb_maildomainconfig **maildomainconfig);
void userdb_deletemaildomainconfigfromhash(userdb_maildomainconfig *maildomainconfig); /* INTERNAL USE ONLY */
void userdb_addusertosuspendedhash(userdb_user *user); /* INTERNAL USE ONLY */
int userdb_findsuspendeduser(unsigned long userid, userdb_user **user);
void userdb_deleteuserfromsuspendedhash(userdb_user *user); /* INTERNAL USE ONLY */
#ifdef USERDB_ENABLEREPLICATION
void userdb_addlanguagetodeletedhash(userdb_language *language); /* INTERNAL USE ONLY */
int userdb_finddeletedlanguage(unsigned long long transactionid, userdb_language **language); /* INTERNAL USE ONLY */
void userdb_deletelanguagefromdeletedhash(userdb_language *language); /* INTERNAL USE ONLY */
void userdb_addusertodeletedhash(userdb_user *user); /* INTERNAL USE ONLY */
int userdb_finddeleteduser(unsigned long long transactionid, userdb_user **user); /* INTERNAL USE ONLY */
void userdb_deleteuserfromdeletedhash(userdb_user *user); /* INTERNAL USE ONLY */
void userdb_addmaildomainconfigtodeletedhash(userdb_maildomainconfig *maildomainconfig); /* INTERNAL USE ONLY */
int userdb_finddeletedmaildomainconfig(unsigned long long transactionid, userdb_maildomainconfig **maildomainconfig); /* INTERNAL USE ONLY */
void userdb_deletemaildomainconfigfromdeletedhash(userdb_maildomainconfig *maildomainconfig); /* INTERNAL USE ONLY */
#endif

/* userdb_interaction.c */
int userdb_findauthnamebyusername(char *username, authname **auth);
void userdb_sendmessagetouser(nick *source, nick *target, char *format, ...);
void userdb_sendnoticetouser(nick *source, nick *target, char *format, ...);
void userdb_sendwarningtouser(nick *source, nick *target, char *format, ...);
void userdb_wallmessage(nick *source, flag_t noticeflag, char *format, ...);
char *userdb_printuserstring(nick *nn, userdb_user *user);
char *userdb_printshortuserstring(nick *nn, userdb_user *user);
char *userdb_printcommandflags(int acl);
#define userdb_printstatusflags(flags)                  (printflags((flags), userdb_statusflags))
#define userdb_printstatusflagsnoprefix(flags)          (printflags_noprefix((flags), userdb_statusflags))
char *userdb_printcommandstatusflags(flag_t statusflags);
#define userdb_printgroupflags(flags)                   (printflags((flags), userdb_groupflags))
#define userdb_printgroupflagsnoprefix(flags)           (printflags_noprefix((flags), userdb_groupflags))
char *userdb_printcommandgroupflags(flag_t groupflags);
#define userdb_printoptionflags(flags)                  (printflags((flags), userdb_optionflags))
#define userdb_printoptionflagsnoprefix(flags)          (printflags_noprefix((flags), userdb_optionflags))
#define userdb_printnoticeflags(flags)                  (printflags((flags), userdb_noticeflags))
#define userdb_printnoticeflagsnoprefix(flags)          (printflags_noprefix((flags), userdb_noticeflags))
#define userdb_printdomainflags(flags)                  (printflags((flags), userdb_domainflags))
#define userdb_printdomainflagsnoprefix(flags)          (printflags_noprefix((flags), userdb_domainflags))
#define userdb_printstatusflagsdiff(oldflags, newflags) (printflagdiff((oldflags), (newflags), userdb_statusflags))
#define userdb_printgroupflagsdiff(oldflags, newflags)  (printflagdiff((oldflags), (newflags), userdb_groupflags))
#define userdb_printoptionflagsdiff(oldflags, newflags) (printflagdiff((oldflags), (newflags), userdb_optionflags))
#define userdb_printnoticeflagsdiff(oldflags, newflags) (printflagdiff((oldflags), (newflags), userdb_noticeflags))
#define userdb_printdomainflagsdiff(oldflags, newflags) (printflagdiff((oldflags), (newflags), userdb_domainflags))
int userdb_scanuserusernames(char *pattern, unsigned int limit, void *data, userdb_scanuserusernamescallback callback, unsigned int *resultcount);
int userdb_scanuseremailaddresses(char *pattern, unsigned int limit, void *data, userdb_scanuseremailaddressescallback callback, unsigned int *resultcount);
int userdb_scanuserpasswords(char *password, unsigned int limit, void *data, userdb_scanuserpasswordscallback callback, unsigned int *resultcount);
int userdb_scanusersuspensions(char *pattern, unsigned int limit, void *data, userdb_scanusersuspensionscallback callback, unsigned int *resultcount);
int userdb_scanmaildomainconfignames(char *pattern, unsigned int limit, void *data, userdb_scanmaildomainconfignamescallback callback, unsigned int *resultcount);

/* userdb_languages.c */
int userdb_createlanguage(char *languagecode, char *languagename, userdb_language **language);
int userdb_changelanguagecode(userdb_language *language, char *languagecode);
int userdb_changelanguagename(userdb_language *language, char *languagename);
int userdb_deletelanguage(userdb_language *language);

/* userdb_lib.c */
char *userdb_generatepassword();
int userdb_hashpasswordbydata(unsigned long userid, time_t created, char *password, unsigned char *result);
int userdb_hashpasswordbyuser(userdb_user *user, char *password, unsigned char *result);
int userdb_verifypasswordbydata(unsigned long userid, time_t created, unsigned char *userpassword, char *testpassword);
int userdb_verifypasswordbyuser(userdb_user *user, char *testpassword);
int userdb_checklanguagecode(char *languagecode);
int userdb_checklanguagename(char *languagename);
int userdb_checkusername(char *username);
int userdb_checkpassword(char *password);
int userdb_checkemailaddress(char *email);
int userdb_checksuspendreason(char *reason);
int userdb_checkmaildomainconfigname(char *configname);
int userdb_splitemaillocalpart(char *email, char *emaillocal, char *emaildomain);
int userdb_splitemaildomainfirst(char *domain, char **start);
int userdb_splitemaildomainnext(char *prev, char **next);
int userdb_buildemaildomain(userdb_maildomain *maildomain, char **emaildomain);
int userdb_isnumericstring(char *in);
int userdb_containsnowildcards(char *in);
int userdb_ismatchall(char *in);
int userdb_nickonstaffserver(nick *nn);
int userdb_getusertagdatastring(unsigned short stringsize, unsigned char **stringdata);
int userdb_strlen(char *in, unsigned long inlen, unsigned long *outlen);
int userdb_processflagstring(flag_t inputflags, const flag *flagslist, char *internalflagslist, char *automaticflagslist, char *flagstring, char *rejectflag, flag_t *outputflags);
userdb_language *userdb_malloclanguage();
userdb_user *userdb_mallocuser();
userdb_usertaginfo *userdb_mallocusertaginfo();
userdb_usertagdata *userdb_mallocusertagdata();
userdb_maildomain *userdb_mallocmaildomain();
userdb_maildomainconfig *userdb_mallocmaildomainconfig();
userdb_suspension *userdb_mallocsuspension();
void userdb_freelanguage(userdb_language *language);
void userdb_freeuser(userdb_user *user);
void userdb_freeusertaginfo(userdb_usertaginfo *usertaginfo);
void userdb_freeusertagdata(userdb_usertagdata *usertagdata);
void userdb_freemaildomain(userdb_maildomain *maildomain);
void userdb_freemaildomainconfig(userdb_maildomainconfig *config);
void userdb_freesuspension(userdb_suspension *suspension);

/* userdb_maildomains.c */
int userdb_checkemaillimits(char *email, char *currentmaillocal, userdb_maildomain *currentmaildomain);
int userdb_createmaildomainconfig(char *configname, flag_t domainflags, unsigned long maxlocal, unsigned long maxdomain, userdb_maildomainconfig **maildomainconfig);
int userdb_modifymaildomainconfigflags(userdb_maildomainconfig *maildomainconfig, flag_t domainflags);
int userdb_modifymaildomainconfigmaxlocal(userdb_maildomainconfig *maildomainconfig, unsigned long maxlocal);
int userdb_modifymaildomainconfigmaxdomain(userdb_maildomainconfig *maildomainconfig, unsigned long maxdomain);
int userdb_deletemaildomainconfig(userdb_maildomainconfig *maildomainconfig);

/* userdb_policy.c */
#define userdb_generateacl(statusflags, groupflags) ((((int)(statusflags)) << 16) | ((int)(groupflags)))
int userdb_getaclflags(int acl, flag_t *statusflags, flag_t *groupflags);
int userdb_checkaclbyuser(userdb_user *user, int acl);  /* NOT recommended, does not check for staff/oper status */
int userdb_checkaclbynick(nick *nn, int acl); /* Recommended, does check for staff/oper status */
int userdb_checkallowusermodify(nick *source, userdb_user *target);
int userdb_checkallowusersuspend(nick *source, userdb_user *target);
int userdb_checkallowuserdelete(nick *source, userdb_user *target);
int userdb_checkstatusflagsbyuser(userdb_user *user, flag_t statusflags); /* NOT recommended, does not check for staff/oper status */
int userdb_checkstatusflagsbynick(nick *nn, flag_t statusflags); /* Recommended, does check for staff/oper status */
flag_t userdb_getallowedstatusflagchanges(nick *source, userdb_user *target);
int userdb_checkgroupflagsbyuser(userdb_user *user, flag_t groupflags); /* NOT recommended, does not check for staff/oper status */
int userdb_checkgroupflagsbynick(nick *nn, flag_t groupflags); /* Recommended, does check for staff/oper status */
flag_t userdb_getallowedgroupflagsbyuser(userdb_user *user);
flag_t userdb_getallowedgroupflagsbynick(nick *nn);
flag_t userdb_getdisallowedgroupflagsbyuser(userdb_user *user, flag_t groupflags);
flag_t userdb_getdisallowedgroupflagsbynick(nick *nn, flag_t groupflags);
flag_t userdb_getallowedgroupflagchanges(nick *source, userdb_user *target);
int userdb_checkoptionflagsbyuser(userdb_user *user, flag_t optionflags);
int userdb_checkoptionflagsbynick(nick *nn, flag_t optionflags);
flag_t userdb_getallowedoptionflagsbyuser(userdb_user *user);
flag_t userdb_getallowedoptionflagsbynick(nick *nn);
flag_t userdb_getdisallowedoptionflagsbyuser(userdb_user *user, flag_t optionflags);
flag_t userdb_getdisallowedoptionflagsbynick(nick *nn, flag_t optionflags);
flag_t userdb_getallowedoptionflagchanges(nick *source, userdb_user *target);
int userdb_checknoticeflagsbyuser(userdb_user *user, flag_t noticeflags); /* NOT recommended, does not check for staff/oper status */
int userdb_checknoticeflagsbynick(nick *nn, flag_t noticeflags); /* Recommended, does check for staff/oper status */
flag_t userdb_getallowednoticeflagsbyuser(userdb_user *user);
flag_t userdb_getallowednoticeflagsbynick(nick *nn);
flag_t userdb_getdisallowednoticeflagsbyuser(userdb_user *user, flag_t noticeflags);
flag_t userdb_getdisallowednoticeflagsbynick(nick *nn, flag_t noticeflags);
flag_t userdb_getallowednoticeflagchanges(nick *source, userdb_user *target);
int userdb_changeuserusernameviapolicy(nick *source, userdb_user *target, char *username);
int userdb_changeuserpasswordviapolicy(nick *source, userdb_user *target, char *password);
int userdb_changeuserlanguageviapolicy(nick *source, userdb_user *target, userdb_language *language);
int userdb_changeuseremailviapolicy(nick *source, userdb_user *target, char *email);
int userdb_changeuserstatusflagsviapolicy(nick *source, userdb_user *target, flag_t statusflags, flag_t *rejectflags);
int userdb_changeuserstatusflagsbystringviapolicy(nick *source, userdb_user *target, char *statusflagsstring, flag_t *rejectflags, char *rejectflag);
int userdb_changeusergroupflagsviapolicy(nick *source, userdb_user *target, flag_t groupflags, flag_t *rejectflags);
int userdb_changeusergroupflagsbystringviapolicy(nick *source, userdb_user *target, char *groupflagsstring, flag_t *rejectflags, char *rejectflag);
int userdb_changeuseroptionflagsviapolicy(nick *source, userdb_user *target, flag_t optionflags, flag_t *rejectflags);
int userdb_changeuseroptionflagsbystringviapolicy(nick *source, userdb_user *target, char *optionflagsstring, flag_t *rejectflags, char *rejectflag);
int userdb_changeusernoticeflagsviapolicy(nick *source, userdb_user *target, flag_t noticeflags, flag_t *rejectflags);
int userdb_changeusernoticeflagsbystringviapolicy(nick *source, userdb_user *target, char *noticeflagsstring, flag_t *rejectflags, char *rejectflag);
int userdb_suspenduserviapolicy(nick *source, userdb_user *target, char *reason, flag_t action, time_t suspendend, userdb_user *suspender);
int userdb_unsuspenduserviapolicy(nick *source, userdb_user *target);
int userdb_deleteuserviapolicy(nick *source, userdb_user *target);

#ifdef USERDB_ENABLEREPLICATION
/* userdb_replication.c */
int userdb_serialiselanguage(userdb_language *language, unsigned char *languagestring, unsigned long *languagestringlen);
int userdb_unserialiselanguage(unsigned char *languagestring, unsigned long *languageid, char **languagecode, char **languagename, flag_t *replicationflags, time_t *created, time_t *modified, unsigned long *revision, unsigned long long *transactionid);
void userdb_languagedbchange(userdb_language *language, unsigned int operation);
void userdb_languagerequest(replicator_node *node, unsigned long long transactionid);
void userdb_languagereceive(unsigned char *languagestring);
void userdb_languagecleanup(unsigned long long transactionid);
int userdb_serialiseuser(userdb_user *user, unsigned char *userstring, unsigned long *userstringlen);
int userdb_unserialiseuser(unsigned char *userstring, unsigned long *userid, char **username, unsigned char **password, unsigned long *languageid, char **emaillocal, char **emaildomain, flag_t *statusflags, flag_t *groupflags, flag_t *optionflags, flag_t *noticeflags, time_t *suspendstart, time_t *suspendend, char **reason, unsigned long *suspenderid, flag_t *replicationflags, time_t *created, time_t *modified, unsigned long *revision, unsigned long long *transactionid);
void userdb_userdbchange(userdb_user *user, unsigned int operation);
void userdb_userrequest(replicator_node *node, unsigned long long transactionid);
void userdb_userreceive(unsigned char *userstring);
void userdb_usercleanup(unsigned long long transactionid);
int userdb_serialisemaildomainconfig(userdb_maildomainconfig *maildomainconfig, unsigned char *maildomainconfigstring, unsigned long *maildomainconfigstringlen);
int userdb_unserialisemaildomainconfig(unsigned char *maildomainconfigstring, unsigned long *maildomainid, char **maildomainname, flag_t *domainflags, unsigned long *maxlocal, unsigned long *maxdomain, flag_t *replicationflags, time_t *created, time_t *modified, unsigned long *revision, unsigned long long *transactionid);
void userdb_maildomainconfigdbchange(userdb_maildomainconfig *maildomainconfig, unsigned int operation);
void userdb_maildomainconfigrequest(replicator_node *node, unsigned long long transactionid);
void userdb_maildomainconfigreceive(unsigned char *maildomainconfigstring);
void userdb_maildomainconfigcleanup(unsigned long long transactionid);
#endif

/* userdb_users.c */
int userdb_createuser(char *username, char *password, char *email, userdb_user **user);
int userdb_changeuserusername(userdb_user *user, char *username);
int userdb_changeuserpassword(userdb_user *user, char *password);
int userdb_changeuserlanguage(userdb_user *user, userdb_language *language);
int userdb_changeuseremail(userdb_user *user, char *email);
int userdb_changeuserstatusflags(userdb_user *user, flag_t flags);
int userdb_changeusergroupflags(userdb_user *user, flag_t flags);
int userdb_changeuseroptionflags(userdb_user *user, flag_t flags);
int userdb_changeusernoticeflags(userdb_user *user, flag_t flags);
int userdb_suspenduser(userdb_user *user, char *reason, flag_t action, time_t suspendend, userdb_user *suspender);
int userdb_unsuspenduser(userdb_user *user);
int userdb_changeuserdetails(userdb_user *user, char *username, unsigned char *password, unsigned long languageid, char *email, flag_t statusflags, flag_t groupflags, flag_t optionflags, flag_t noticeflags, time_t suspendstart, time_t suspendend, char *reason, unsigned long suspenderid);
int userdb_deleteuser(userdb_user *user);

/* userdb_usertags.c */
int userdb_registerusertag(unsigned short tagid, unsigned short tagtype, char *tagname, userdb_usertaginfo **usertaginfo);
int userdb_unregisterusertag(userdb_usertaginfo *usertaginfo);
int userdb_getusertagdatavaluebydata(userdb_usertagdata *usertagdata, unsigned short *tagtype, unsigned short *tagsize, void **data);
int userdb_getusertagdatavaluebyinfo(userdb_usertaginfo *usertaginfo, userdb_user *user, unsigned short *tagtype, unsigned short *tagsize, void **data, userdb_usertagdata **usertagdata);
int userdb_setusertagdatavaluebydata(userdb_usertagdata *usertagdata, void *data, unsigned short datalen);
int userdb_setusertagdatavaluebyinfo(userdb_usertaginfo *usertaginfo, userdb_user *user, void *data, unsigned short datalen, userdb_usertagdata **usertagdata);
int userdb_clearusertagdatavaluebydata(userdb_usertagdata *usertagdata);
int userdb_clearusertagdatavaluebyinfo(userdb_usertaginfo *usertaginfo, userdb_user *user);
int userdb_clearallusertagdatabyinfo(userdb_usertaginfo *usertaginfo);
int userdb_clearallusertagdatabyuser(userdb_user *user);

/* Defined functions */
#define userdb_zerolengthstring(in)                 ((in)[0] == '\0')
#define userdb_usernamestring(in)                   ((in)[0] == '#')
#define userdb_useridstring(in)                     ((in)[0] == '&')
#endif
