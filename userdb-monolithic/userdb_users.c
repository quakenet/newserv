/*
 * UserDB
 */

#include "./userdb.h"

/* +E isn't here for a reason */
const flag userdb_statusflags[] = {
    { 'U', UDB_STATUS_UNAUTHED },
    { 'A', UDB_STATUS_AUTHED },
    { 't', UDB_STATUS_TRIAL },
    { 's', UDB_STATUS_STAFF },
    { 'o', UDB_STATUS_OPER },
    { 'a', UDB_STATUS_ADMIN },
    { 'd', UDB_STATUS_DEV },
    { '\0', 0 }
  };

/* +N isn't here for a reason too */
const flag userdb_groupflags[] = {
    { 't', UDB_GROUP_TRUSTS },
    { 'f', UDB_GROUP_FAQS },
    { 'h', UDB_GROUP_HELP },
    { 's', UDB_GROUP_HELPSCRIPT },
    { 'w', UDB_GROUP_WORMSQUAD },
    { 'p', UDB_GROUP_PUBLICRELATIONS },
    { 'F', UDB_GROUP_FEDS },
    { 'S', UDB_GROUP_SECURITY },
    { 'H', UDB_GROUP_HUMANRESOURCES },
    { 'G', UDB_GROUP_GROUPLEADERS },
    { '\0', 0 }
  };

const flag userdb_optionflags[] = {
    { 'n', UDB_OPTION_NOTICES },
    { 'u', UDB_OPTION_UNUSED },
    { 't', UDB_OPTION_TRUST },
    { 'z', UDB_OPTION_SUSPENDED },
    { 'k', UDB_OPTION_INSTANTKILL },
    { 'g', UDB_OPTION_INSTANTGLINE },
    { 'G', UDB_OPTION_DELAYEDGLINE },
    { 'w', UDB_OPTION_WATCH },
    { 'p', UDB_OPTION_PROTECTNICK },
    { 'l', UDB_OPTION_NOAUTHLIMIT },
    { 'c', UDB_OPTION_CLEANUPIGNORE },
    { 'b', UDB_OPTION_BYPASSSECURITY },
    { 'h', UDB_OPTION_HIDEUSERS },
    { '\0', 0 }
  };

const flag userdb_noticeflags[] = {
    { 'v', UDB_NOTICE_VERBOSE },
    { 'c', UDB_NOTICE_CLONING },
    { 'h', UDB_NOTICE_HITS },
    { 'u', UDB_NOTICE_USERACTIONS },
    { 'C', UDB_NOTICE_CLEARCHAN },
    { 'a', UDB_NOTICE_AUTHENTICATION },
    { 'U', UDB_NOTICE_MISC },
    { 'b', UDB_NOTICE_BROADCASTS },
    { 'f', UDB_NOTICE_FAKEUSERS },
    { 't', UDB_NOTICE_TRUSTS },
    { 's', UDB_NOTICE_SUSPENSIONS },
    { 'M', UDB_NOTICE_CHANNELMANAGEMENT },
    { 'm', UDB_NOTICE_ACCOUNTMANAGEMENT },
    { 'o', UDB_NOTICE_OPERATIONS },
    { 'A', UDB_NOTICE_ALLCOMMANDS },
    { '\0', 0 }
  };

int userdb_createuser(char *username, char *password, char *email, userdb_user **user)
{
char emaillocal[USERDB_EMAILLEN + 1], emaildomain[USERDB_EMAILLEN + 1];
unsigned char result[USERDB_PASSWORDHASHLEN];
userdb_user *newuser;
userdb_maildomain *maildomain;
time_t newusertime;

#ifdef USERDB_ENABLEREPLICATION
    if ( !userdb_repl_group || userdb_repl_group->role != REPLICATOR_ROLE_MASTER )
        return USERDB_NOACCESS;
#endif
    
    if ( !userdb_databaseloaded() )
        return USERDB_DBUNAVAILABLE;
    else if ( !username || userdb_zerolengthstring(username) || !password || userdb_zerolengthstring(password) || !email || userdb_zerolengthstring(email) || !user )
        return USERDB_BADPARAMETERS;
    
    switch ( userdb_checkusername(username) )
    {
        case USERDB_OK:
            break;
        case USERDB_BADUSERNAME:
            return USERDB_BADUSERNAME;
        default:
            return USERDB_ERROR;
    }
    
    switch ( userdb_checkpassword(password) )
    {
        case USERDB_OK:
            break;
        case USERDB_BADPASSWORD:
            return USERDB_BADPASSWORD;
        default:
            return USERDB_ERROR;
    }
    
    switch ( userdb_checkemailaddress(email) )
    {
        case USERDB_OK:
            break;
        case USERDB_BADEMAIL:
            return USERDB_BADEMAIL;
        default:
            return USERDB_ERROR;
    }
    
    switch ( userdb_finduserbyusername(username, &newuser) )
    {
        case USERDB_OK:
            return USERDB_ALREADYEXISTS;
        case USERDB_NOTFOUND:
            break;
        default:
            return USERDB_ERROR;
    }
    
    newusertime = time(NULL);
    
    switch ( userdb_hashpasswordbydata((userdb_lastuserid + 1), newusertime, password, result) )
    {
        case USERDB_OK:
            break;
        default:
            return USERDB_ERROR;
    }
    
    switch ( userdb_splitemaillocalpart(email, emaillocal, emaildomain) )
    {
        case USERDB_OK:
            break;
        default:
            return USERDB_ERROR;
    }
    
    switch ( userdb_findmaildomainbyemaildomain(emaildomain, &maildomain, 1) )
    {
        case USERDB_OK:
            break;
        default:
            return USERDB_ERROR;
    }
    
    newuser = userdb_mallocuser();
    newuser->userid = ++userdb_lastuserid;
    newuser->username = getsstring(username, USERDB_MAXUSERNAMELEN);
    memcpy(newuser->password, result, USERDB_PASSWORDHASHLEN);
    
    newuser->languageid = 0;
    newuser->languagedata = NULL;
    
    newuser->emaillocal = getsstring(emaillocal, USERDB_EMAILLEN);
    newuser->maildomain = maildomain;
    
    newuser->statusflags = 0;
    newuser->groupflags = 0;
    newuser->optionflags = (UDB_OPTION_NOTICES | UDB_OPTION_UNUSED);
    newuser->noticeflags = 0;
    
    newuser->created = newusertime;
    newuser->modified = newusertime;
    newuser->revision = 1;
    newuser->transactionid = ++userdb_lastusertransactionid;
    
    userdb_addusertomaildomain(newuser, maildomain);
    userdb_assignusertoauth(newuser);
    userdb_addusertohash(newuser);
    userdb_addusertodb(newuser);
    
    *user = newuser;
    triggerhook(HOOK_USERDB_CREATEUSER, newuser);
    
    return USERDB_OK;
}

int userdb_changeuserusername(userdb_user *user, char *username)
{
userdb_user *tempuser;

#ifdef USERDB_ENABLEREPLICATION
    if ( !userdb_repl_group || userdb_repl_group->role != REPLICATOR_ROLE_MASTER )
        return USERDB_NOACCESS;
#endif
    
    if ( !userdb_databaseloaded() )
        return USERDB_DBUNAVAILABLE;
    else if ( !user || !username || userdb_zerolengthstring(username) || UDBRIsDeleted(user) )
        return USERDB_BADPARAMETERS;
    else if ( user->databaseops > USERDB_DATABASE_MAXPEROBJECT )
        return USERDB_LOCKED;
    
    switch ( userdb_checkusername(username) )
    {
        case USERDB_OK:
            break;
        case USERDB_BADUSERNAME:
            return USERDB_BADUSERNAME;
        default:
            return USERDB_ERROR;
    }
    
    switch ( userdb_finduserbyusername(username, &tempuser) )
    {
        case USERDB_OK:
            return USERDB_ALREADYEXISTS;
        case USERDB_NOTFOUND:
            break;
        default:
            return USERDB_ERROR;
    }
    
    if ( !ircd_strcmp(username, user->username->content) )
        return USERDB_NOOP;
    
    triggerhook(HOOK_USERDB_PREMODIFYUSER, user);
    
    userdb_deleteuserfromhash(user);
    freesstring(user->username);
    user->username = getsstring(username, USERDB_MAXUSERNAMELEN);
    userdb_addusertohash(user);
    
    user->revision++;
    user->modified = time(NULL);
    user->transactionid = ++userdb_lastusertransactionid;
    
    userdb_modifyuserindb(user);
    
    triggerhook(HOOK_USERDB_POSTMODIFYUSER, user);
    
    return USERDB_OK;
}

int userdb_changeuserpassword(userdb_user *user, char *password)
{
unsigned char result[USERDB_PASSWORDHASHLEN];

#ifdef USERDB_ENABLEREPLICATION
    if ( !userdb_repl_group || userdb_repl_group->role != REPLICATOR_ROLE_MASTER )
        return USERDB_NOACCESS;
#endif
    
    if ( !userdb_databaseloaded() )
        return USERDB_DBUNAVAILABLE;
    else if ( !user || !password || userdb_zerolengthstring(password) || UDBRIsDeleted(user) )
        return USERDB_BADPARAMETERS;
    else if ( user->databaseops > USERDB_DATABASE_MAXPEROBJECT )
        return USERDB_LOCKED;
    
    switch ( userdb_checkpassword(password) )
    {
        case USERDB_OK:
            break;
        case USERDB_BADPASSWORD:
            return USERDB_BADPASSWORD;
        default:
            return USERDB_ERROR;
    }
    
    switch ( userdb_hashpasswordbyuser(user, password, result) )
    {
        case USERDB_OK:
            break;
        default:
            return USERDB_ERROR;
    }
    
    if ( !memcmp(result, user->password, USERDB_PASSWORDHASHLEN) )
        return USERDB_NOOP;
    
    triggerhook(HOOK_USERDB_PREMODIFYUSER, user);
    
    memcpy(user->password, result, USERDB_PASSWORDHASHLEN);
    
    user->revision++;
    user->modified = time(NULL);
    user->transactionid = ++userdb_lastusertransactionid;
    
    userdb_modifyuserindb(user);
    
    triggerhook(HOOK_USERDB_POSTMODIFYUSER, user);
    
    return USERDB_OK;
}

int userdb_changeuserlanguage(userdb_user *user, userdb_language *language)
{
#ifdef USERDB_ENABLEREPLICATION
    if ( !userdb_repl_group || userdb_repl_group->role != REPLICATOR_ROLE_MASTER )
        return USERDB_NOACCESS;
#endif
    
    if ( !userdb_databaseloaded() )
        return USERDB_DBUNAVAILABLE;
    else if ( !user || !language || UDBRIsDeleted(user) || UDBRIsDeleted(language) )
        return USERDB_BADPARAMETERS;
    else if ( user->databaseops > USERDB_DATABASE_MAXPEROBJECT )
        return USERDB_LOCKED;
    
    if ( user->languagedata && language->languageid == user->languageid )
        return USERDB_NOOP;
    
    triggerhook(HOOK_USERDB_PREMODIFYUSER, user);
    
    user->languageid = language->languageid;
    user->languagedata = language;
    
    user->revision++;
    user->modified = time(NULL);
    user->transactionid = ++userdb_lastusertransactionid;
    
    userdb_modifyuserindb(user);
    
    triggerhook(HOOK_USERDB_POSTMODIFYUSER, user);
    
    return USERDB_OK;
}

int userdb_changeuseremail(userdb_user *user, char *email)
{
char emaillocal[USERDB_EMAILLEN + 1], emaildomain[USERDB_EMAILLEN + 1];
userdb_maildomain *maildomain;

#ifdef USERDB_ENABLEREPLICATION
    if ( !userdb_repl_group || userdb_repl_group->role != REPLICATOR_ROLE_MASTER )
        return USERDB_NOACCESS;
#endif
    
    if ( !userdb_databaseloaded() )
        return USERDB_DBUNAVAILABLE;
    else if ( !user || !email || userdb_zerolengthstring(email) || UDBRIsDeleted(user) )
        return USERDB_BADPARAMETERS;
    else if ( user->databaseops > USERDB_DATABASE_MAXPEROBJECT )
        return USERDB_LOCKED;
    
    switch ( userdb_checkemailaddress(email) )
    {
        case USERDB_OK:
            break;
        case USERDB_BADEMAIL:
            return USERDB_BADEMAIL;
        default:
            return USERDB_ERROR;
    }
    
    switch ( userdb_splitemaillocalpart(email, emaillocal, emaildomain) )
    {
        case USERDB_OK:
            break;
        default:
            return USERDB_ERROR;
    }
    
    switch ( userdb_findmaildomainbyemaildomain(emaildomain, &maildomain, 1) )
    {
        case USERDB_OK:
            break;
        default:
            return USERDB_ERROR;
    }
    
    if ( user->maildomain == maildomain && !ircd_strcmp(emaillocal, user->emaillocal->content) )
        return USERDB_NOOP;
    
    triggerhook(HOOK_USERDB_PREMODIFYUSER, user);
    
    freesstring(user->emaillocal);
    user->emaillocal = getsstring(emaillocal, USERDB_EMAILLEN);
    
    if ( user->maildomain != maildomain )
    {
        userdb_deleteuserfrommaildomain(user, user->maildomain);
        userdb_cleanmaildomaintree(user->maildomain);
        user->maildomain = maildomain;
        userdb_addusertomaildomain(user, user->maildomain);
    }
    
    user->revision++;
    user->modified = time(NULL);
    user->transactionid = ++userdb_lastusertransactionid;
    
    userdb_modifyuserindb(user);
    
    triggerhook(HOOK_USERDB_POSTMODIFYUSER, user);
    
    return USERDB_OK;
}

int userdb_changeuserstatusflags(userdb_user *user, flag_t flags)
{
flag_t newflags;

#ifdef USERDB_ENABLEREPLICATION
    if ( !userdb_repl_group || userdb_repl_group->role != REPLICATOR_ROLE_MASTER )
        return USERDB_NOACCESS;
#endif
    
    if ( !userdb_databaseloaded() )
        return USERDB_DBUNAVAILABLE;
    else if ( !user || UDBRIsDeleted(user) )
        return USERDB_BADPARAMETERS;
    else if ( user->databaseops > USERDB_DATABASE_MAXPEROBJECT )
        return USERDB_LOCKED;
    
    if ( (flags & UDB_STATUS_DEV) )
        newflags = UDB_STATUS_DEV;
    else if ( (flags & UDB_STATUS_ADMIN) )
        newflags = UDB_STATUS_ADMIN;
    else if ( (flags & UDB_STATUS_OPER) )
        newflags = UDB_STATUS_OPER;
    else if ( (flags & UDB_STATUS_STAFF) )
        newflags = UDB_STATUS_STAFF;
    else if ( (flags & UDB_STATUS_TRIAL) )
        newflags = UDB_STATUS_TRIAL;
    else
        newflags = 0;
    
    if ( user->statusflags == newflags )
        return USERDB_NOOP;
    
    triggerhook(HOOK_USERDB_PREMODIFYUSER, user);
    
    user->statusflags = (newflags & ~(UDB_STATUS_UNAUTHED | UDB_STATUS_AUTHED));
    user->groupflags = (user->groupflags & userdb_getallowedgroupflagsbyuser(user));
    user->optionflags = (user->optionflags & userdb_getallowedoptionflagsbyuser(user));
    user->noticeflags = (user->noticeflags & userdb_getallowednoticeflagsbyuser(user));
    user->revision++;
    user->modified = time(NULL);
    user->transactionid = ++userdb_lastusertransactionid;
    
    userdb_modifyuserindb(user);
    
    triggerhook(HOOK_USERDB_POSTMODIFYUSER, user);
    
    return USERDB_OK;
}

int userdb_changeusergroupflags(userdb_user *user, flag_t flags)
{
#ifdef USERDB_ENABLEREPLICATION
    if ( !userdb_repl_group || userdb_repl_group->role != REPLICATOR_ROLE_MASTER )
        return USERDB_NOACCESS;
#endif
    
    if ( !userdb_databaseloaded() )
        return USERDB_DBUNAVAILABLE;
    else if ( !user || UDBRIsDeleted(user) )
        return USERDB_BADPARAMETERS;
    else if ( user->databaseops > USERDB_DATABASE_MAXPEROBJECT )
        return USERDB_LOCKED;
    
    if ( (flags & userdb_getallowedgroupflagsbyuser(user)) == user->groupflags )
        return USERDB_NOOP;
    
    triggerhook(HOOK_USERDB_PREMODIFYUSER, user);
    
    user->groupflags = (flags & userdb_getallowedgroupflagsbyuser(user));
    user->optionflags = (user->optionflags & userdb_getallowedoptionflagsbyuser(user));
    user->noticeflags = (user->noticeflags & userdb_getallowednoticeflagsbyuser(user));
    user->revision++;
    user->modified = time(NULL);
    user->transactionid = ++userdb_lastusertransactionid;
    
    userdb_modifyuserindb(user);
    
    triggerhook(HOOK_USERDB_POSTMODIFYUSER, user);
    
    return USERDB_OK;
}

int userdb_changeuseroptionflags(userdb_user *user, flag_t flags)
{
#ifdef USERDB_ENABLEREPLICATION
    if ( !userdb_repl_group || userdb_repl_group->role != REPLICATOR_ROLE_MASTER )
        return USERDB_NOACCESS;
#endif
    
    if ( !userdb_databaseloaded() )
        return USERDB_DBUNAVAILABLE;
    else if ( !user || UDBRIsDeleted(user) )
        return USERDB_BADPARAMETERS;
    else if ( user->databaseops > USERDB_DATABASE_MAXPEROBJECT )
        return USERDB_LOCKED;
    
    if ( ((flags & userdb_getallowedoptionflagsbyuser(user)) & ~(UDB_OPTION_SUSPENDED | UDB_OPTION_INSTANTKILL | UDB_OPTION_INSTANTGLINE | UDB_OPTION_DELAYEDGLINE)) == (user->optionflags & ~(UDB_OPTION_SUSPENDED | UDB_OPTION_INSTANTKILL | UDB_OPTION_INSTANTGLINE | UDB_OPTION_DELAYEDGLINE)) )
        return USERDB_NOOP;
    
    triggerhook(HOOK_USERDB_PREMODIFYUSER, user);
    
    user->optionflags = ((flags & userdb_getallowedoptionflagsbyuser(user)) | (user->optionflags & (UDB_OPTION_SUSPENDED | UDB_OPTION_INSTANTKILL | UDB_OPTION_INSTANTGLINE | UDB_OPTION_DELAYEDGLINE)));
    user->revision++;
    user->modified = time(NULL);
    user->transactionid = ++userdb_lastusertransactionid;
    
    userdb_modifyuserindb(user);
    
    triggerhook(HOOK_USERDB_POSTMODIFYUSER, user);
    
    return USERDB_OK;
}

int userdb_changeusernoticeflags(userdb_user *user, flag_t flags)
{
#ifdef USERDB_ENABLEREPLICATION
    if ( !userdb_repl_group || userdb_repl_group->role != REPLICATOR_ROLE_MASTER )
        return USERDB_NOACCESS;
#endif
    
    if ( !userdb_databaseloaded() )
        return USERDB_DBUNAVAILABLE;
    else if ( !user || UDBRIsDeleted(user) )
        return USERDB_BADPARAMETERS;
    else if ( user->databaseops > USERDB_DATABASE_MAXPEROBJECT )
        return USERDB_LOCKED;
    
    if ( (flags & userdb_getallowednoticeflagsbyuser(user)) == user->noticeflags )
        return USERDB_NOOP;
    
    triggerhook(HOOK_USERDB_PREMODIFYUSER, user);
    
    user->noticeflags = (flags & userdb_getallowednoticeflagsbyuser(user));
    user->revision++;
    user->modified = time(NULL);
    user->transactionid = ++userdb_lastusertransactionid;
    
    userdb_modifyuserindb(user);
    
    triggerhook(HOOK_USERDB_POSTMODIFYUSER, user);
    
    return USERDB_OK;
}

int userdb_suspenduser(userdb_user *user, char *reason, flag_t action, time_t suspendend, userdb_user *suspender)
{
time_t suspendusertime;
flag_t newaction;

#ifdef USERDB_ENABLEREPLICATION
    if ( !userdb_repl_group || userdb_repl_group->role != REPLICATOR_ROLE_MASTER )
        return USERDB_NOACCESS;
#endif
    
    if ( !userdb_databaseloaded() )
        return USERDB_DBUNAVAILABLE;
    else if ( !user || !reason || userdb_zerolengthstring(reason) || UDBRIsDeleted(user) )
        return USERDB_BADPARAMETERS;
    else if ( UDBOIsSuspended(user) && !user->suspensiondata )
        return USERDB_ERROR;
    else if ( !UDBOIsSuspended(user) && user->suspensiondata )
        return USERDB_ERROR;
    else if ( UDBOIsSuspended(user) )
        return USERDB_NOOP;
    else if ( user->databaseops > USERDB_DATABASE_MAXPEROBJECT )
        return USERDB_LOCKED;
    
    switch ( userdb_checksuspendreason(reason) )
    {
        case USERDB_OK:
            break;
        case USERDB_BADSUSPENDREASON:
            return USERDB_BADSUSPENDREASON;
        default:
            return USERDB_ERROR;
    }
    
    if ( (action & UDB_OPTION_DELAYEDGLINE) )
        newaction = UDB_OPTION_DELAYEDGLINE;
    else if ( (action & UDB_OPTION_INSTANTGLINE) )
        newaction = UDB_OPTION_INSTANTGLINE;
    else if ( (action & UDB_OPTION_INSTANTKILL) )
        newaction = UDB_OPTION_INSTANTKILL;
    else
        newaction = 0;
    
    suspendusertime = time(NULL);
    
    if ( suspendend && suspendend <= suspendusertime )
        return USERDB_BADSUSPENDENDDATE;
    
    triggerhook(HOOK_USERDB_PREMODIFYUSER, user);
    
    user->suspensiondata = userdb_mallocsuspension();
    user->suspensiondata->suspendstart = suspendusertime;
    user->suspensiondata->suspendend = suspendend;
    user->suspensiondata->reason = getsstring(reason, USERDB_SUSPENDREASONLEN);
    
    if ( suspender )
    {
        user->suspensiondata->suspenderid = suspender->userid;
        user->suspensiondata->suspenderdata = suspender;
    }
    else
    {
        user->suspensiondata->suspenderid = 0;
        user->suspensiondata->suspenderdata = NULL;
    }
    
    user->optionflags |= (UDB_OPTION_SUSPENDED | newaction);
    user->revision++;
    user->modified = suspendusertime;
    user->transactionid = ++userdb_lastusertransactionid;
    
    userdb_addusertosuspendedhash(user);
    
    userdb_modifyuserindb(user);
    
    triggerhook(HOOK_USERDB_POSTMODIFYUSER, user);
    
    return USERDB_OK;
}

int userdb_unsuspenduser(userdb_user *user)
{
#ifdef USERDB_ENABLEREPLICATION
    if ( !userdb_repl_group || userdb_repl_group->role != REPLICATOR_ROLE_MASTER )
        return USERDB_NOACCESS;
#endif
    
    if ( !userdb_databaseloaded() )
        return USERDB_DBUNAVAILABLE;
    else if ( !user || UDBRIsDeleted(user) )
        return USERDB_BADPARAMETERS;
    else if ( UDBOIsSuspended(user) && !user->suspensiondata )
        return USERDB_ERROR;
    else if ( !UDBOIsSuspended(user) && user->suspensiondata )
        return USERDB_ERROR;
    else if ( !UDBOIsSuspended(user) )
        return USERDB_NOOP;
    else if ( user->databaseops > USERDB_DATABASE_MAXPEROBJECT )
        return USERDB_LOCKED;
    
    triggerhook(HOOK_USERDB_PREMODIFYUSER, user);
    
    userdb_deleteuserfromsuspendedhash(user);
    
    userdb_freesuspension(user->suspensiondata);
    user->suspensiondata = NULL;
    
    user->optionflags &= ~(UDB_OPTION_SUSPENDED | UDB_OPTION_INSTANTKILL | UDB_OPTION_INSTANTGLINE | UDB_OPTION_DELAYEDGLINE);
    user->revision++;
    user->modified = time(NULL);
    user->transactionid = ++userdb_lastusertransactionid;
    
    userdb_modifyuserindb(user);
    
    triggerhook(HOOK_USERDB_POSTMODIFYUSER, user);
    
    return USERDB_OK;
}

int userdb_changeuserdetails(userdb_user *user, char *username, unsigned char *password, unsigned long languageid, char *email, flag_t statusflags, flag_t groupflags, flag_t optionflags, flag_t noticeflags, time_t suspendstart, time_t suspendend, char *reason, unsigned long suspenderid)
{
char emaillocal[USERDB_EMAILLEN + 1], emaildomain[USERDB_EMAILLEN + 1];
userdb_language *languagedata;
userdb_user *suspenderdata;
userdb_maildomain *maildomain;

#ifdef USERDB_ENABLEREPLICATION
    if ( !userdb_repl_group || userdb_repl_group->role != REPLICATOR_ROLE_MASTER )
        return USERDB_NOACCESS;
#endif
    
    if ( !userdb_databaseloaded() )
        return USERDB_DBUNAVAILABLE;
    else if ( !user || !username || userdb_zerolengthstring(username) || !password || !email || userdb_zerolengthstring(email) || UDBRIsDeleted(user) )
        return USERDB_BADPARAMETERS;
    else if ( user->databaseops > USERDB_DATABASE_MAXPEROBJECT )
        return USERDB_LOCKED;
    
    switch ( userdb_checkusername(username) )
    {
        case USERDB_OK:
            break;
        case USERDB_BADUSERNAME:
            return USERDB_BADUSERNAME;
        default:
            return USERDB_ERROR;
    }
    
    switch ( userdb_checkemailaddress(email) )
    {
        case USERDB_OK:
            break;
        case USERDB_BADEMAIL:
            return USERDB_BADEMAIL;
        default:
            return USERDB_ERROR;
    }
    
    switch ( userdb_splitemaillocalpart(email, emaillocal, emaildomain) )
    {
        case USERDB_OK:
            break;
        default:
            return USERDB_ERROR;
    }
    
    switch ( userdb_findmaildomainbyemaildomain(emaildomain, &maildomain, 1) )
    {
        case USERDB_OK:
            break;
        default:
            return USERDB_ERROR;
    }
    
    if ( optionflags & UDB_OPTION_SUSPENDED )
    {
        if ( !reason || userdb_zerolengthstring(reason) )
            return USERDB_BADPARAMETERS;
        
        switch ( userdb_checksuspendreason(reason) )
        {
            case USERDB_OK:
                break;
            case USERDB_BADSUSPENDREASON:
                return USERDB_BADSUSPENDREASON;
            default:
                return USERDB_ERROR;
        }
        
        if ( suspenderid )
        {
            switch ( userdb_finduserbyuserid(suspenderid, &suspenderdata) )
            {
                case USERDB_OK:
                    break;
                case USERDB_NOTFOUND:
                    suspenderdata = NULL;
                    break;
                default:
                    return USERDB_ERROR;
            }
        }
        else
        {
            suspenderdata = NULL;
        }
    }
    else
    {
        suspenderdata = NULL;
    }
    
    if ( languageid )
    {
        switch ( userdb_findlanguagebylanguageid(languageid, &languagedata) )
        {
            case USERDB_OK:
                break;
            case USERDB_NOTFOUND:
                languagedata = NULL;
                break;
            default:
                return USERDB_ERROR;
        }
    }
    else
    {
        languagedata = NULL;
    }
    
    triggerhook(HOOK_USERDB_PREMODIFYUSER, user);
    
    if ( ircd_strcmp(user->username->content, username) )
    {
        userdb_deleteuserfromhash(user);
        freesstring(user->username);
        user->username = getsstring(username, USERDB_MAXUSERNAMELEN);
        userdb_addusertohash(user);
    }
    
    memcpy(user->password, password, USERDB_PASSWORDHASHLEN);
    user->languageid = languageid;
    user->languagedata = languagedata;
    
    if ( ircd_strcmp(user->emaillocal->content, emaillocal) )
    {
        freesstring(user->emaillocal);
        user->emaillocal = getsstring(emaillocal, USERDB_EMAILLEN);
    }
    
    if ( user->maildomain != maildomain )
    {
        userdb_deleteuserfrommaildomain(user, user->maildomain);
        userdb_cleanmaildomaintree(user->maildomain);
        user->maildomain = maildomain;
        userdb_addusertomaildomain(user, user->maildomain);
    }
    
    if ( (statusflags & UDB_STATUS_DEV) )
        user->statusflags = UDB_STATUS_DEV;
    else if ( (statusflags & UDB_STATUS_ADMIN) )
        user->statusflags = UDB_STATUS_ADMIN;
    else if ( (statusflags & UDB_STATUS_OPER) )
        user->statusflags = UDB_STATUS_OPER;
    else if ( (statusflags & UDB_STATUS_STAFF) )
        user->statusflags = UDB_STATUS_STAFF;
    else if ( (statusflags & UDB_STATUS_TRIAL) )
        user->statusflags = UDB_STATUS_TRIAL;
    else
        user->statusflags = 0;
    
    user->groupflags = (groupflags & userdb_getallowedgroupflagsbyuser(user));
    user->optionflags = (optionflags & userdb_getallowedoptionflagsbyuser(user));
    user->noticeflags = (noticeflags & userdb_getallowednoticeflagsbyuser(user));
    
    if ( UDBOIsSuspended(user) )
    {
        if ( user->suspensiondata )
        {
            user->suspensiondata->suspendstart = suspendstart;
            user->suspensiondata->suspendend = suspendend;
            
            if ( ircd_strcmp(user->suspensiondata->reason->content, reason) )
            {
                freesstring(user->suspensiondata->reason);
                user->suspensiondata->reason = getsstring(reason, USERDB_SUSPENDREASONLEN);
            }
            
            user->suspensiondata->suspenderid = suspenderid;
            user->suspensiondata->suspenderdata = suspenderdata;
        }
        else
        {
            user->suspensiondata = userdb_mallocsuspension();
            user->suspensiondata->suspendstart = suspendstart;
            user->suspensiondata->suspendend = suspendend;
            user->suspensiondata->reason = getsstring(reason, USERDB_SUSPENDREASONLEN);
            user->suspensiondata->suspenderid = suspenderid;
            user->suspensiondata->suspenderdata = suspenderdata;
            userdb_addusertosuspendedhash(user);
        }
    }
    else if ( user->suspensiondata )
    {
        userdb_deleteuserfromsuspendedhash(user);
        userdb_freesuspension(user->suspensiondata);
        user->suspensiondata = NULL;
    }
    
    user->revision++;
    user->modified = time(NULL);
    user->transactionid = ++userdb_lastusertransactionid;
    
    userdb_modifyuserindb(user);
    
    triggerhook(HOOK_USERDB_POSTMODIFYUSER, user);
    
    return USERDB_OK;
}

int userdb_deleteuser(userdb_user *user)
{
unsigned int i;
userdb_user *tempuser;

#ifdef USERDB_ENABLEREPLICATION
    if ( !userdb_repl_group || userdb_repl_group->role != REPLICATOR_ROLE_MASTER )
        return USERDB_NOACCESS;
#endif
    
    if ( !userdb_databaseloaded() )
        return USERDB_DBUNAVAILABLE;
    else if ( !user || UDBRIsDeleted(user) )
        return USERDB_BADPARAMETERS;
    else if ( user->databaseops > USERDB_DATABASE_MAXPEROBJECT )
        return USERDB_LOCKED;
    
    triggerhook(HOOK_USERDB_DELETEUSER, user);
    
    if ( UDBOIsSuspended(user) )
    {
        userdb_deleteuserfromsuspendedhash(user);
        userdb_freesuspension(user->suspensiondata);
        user->suspensiondata = NULL;
        user->optionflags &= ~(UDB_OPTION_SUSPENDED | UDB_OPTION_INSTANTKILL | UDB_OPTION_INSTANTGLINE | UDB_OPTION_DELAYEDGLINE);
    }
    
    userdb_clearallusertagdatabyuser(user);
    userdb_unassignuserfromauth(user);
    userdb_deleteuserfromhash(user);
    userdb_deleteuserfrommaildomain(user, user->maildomain);
    userdb_cleanmaildomaintree(user->maildomain);
    
    user->languagedata = NULL;
    user->maildomain = NULL;
    user->replicationflags |= UDB_REPLICATION_DELETED;
    user->revision++;
    user->modified = time(NULL);
    user->transactionid = ++userdb_lastusertransactionid;
    
    for ( i = 0; i < USERDB_USERSUSPENDEDHASHSIZE; i++ )
        for ( tempuser = userdb_usersuspendedhashtable[i]; tempuser; tempuser = tempuser->suspensiondata->nextbyid )
            if ( tempuser->suspensiondata->suspenderdata == user )
                tempuser->suspensiondata->suspenderdata = NULL;
    
#ifdef USERDB_ENABLEREPLICATION
    userdb_addusertodeletedhash(user);
    userdb_modifyuserindb(user);
#else
    userdb_deleteuserfromdb(user);
#endif
    
    return USERDB_OK;
}
