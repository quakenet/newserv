/*
 * UserDB
 */

#include "./userdb.h"

/* ACL handling */
int userdb_getaclflags(int acl, flag_t *statusflags, flag_t *groupflags)
{
    if ( !statusflags || !groupflags )
        return USERDB_BADPARAMETERS;
    
    *statusflags = ((acl & 0xffff0000) >> 16);
    *groupflags = (acl & 0x0000ffff);
    
    /* Broken ACL detection */
    if ( (*statusflags) == UDB_STATUS_EVERYONE && (*groupflags) != UDB_GROUP_NONE )
    {
        (*statusflags) = (UDB_STATUS_AUTHED | UDB_STATUS_DEV);
        (*groupflags) = (UDB_GROUP_NONE);
    }
    
    return USERDB_OK;
}

int userdb_checkaclbyuser(userdb_user *user, int acl)
{
flag_t statusflags, groupflags;

    if ( userdb_getaclflags(acl, &statusflags, &groupflags) != USERDB_OK )
        return 0;
    else if ( userdb_checkstatusflagsbyuser(user, statusflags) && userdb_checkgroupflagsbyuser(user, groupflags) )
        return 1;
    else
        return 0;
}

int userdb_checkaclbynick(nick *nn, int acl)
{
flag_t statusflags, groupflags;

    if ( userdb_getaclflags(acl, &statusflags, &groupflags) != USERDB_OK )
        return 0;
    else if ( userdb_checkstatusflagsbynick(nn, statusflags) && userdb_checkgroupflagsbynick(nn, groupflags) )
        return 1;
    else
        return 0;
}

int userdb_checkallowusermodify(nick *source, userdb_user *target)
{
userdb_user *user;

    if ( !source || !target )
        return 0;
    else if ( UDBRIsDeleted(target) )
        return 0;
    
    switch ( userdb_finduserbynick(source, &user) )
    {
        case USERDB_OK:
            break;
        default:
            return 0;
    }
    
    if ( user == target )
        return 1;
    else if ( !UDBSIsOper(target) && userdb_checkstatusflagsbynick(source, UDB_STATUS_OPER) )
        return 1;
    else if ( !UDBSIsDev(target) && userdb_checkstatusflagsbynick(source, UDB_STATUS_ADMIN) )
        return 1;
    else if ( userdb_checkstatusflagsbynick(source, UDB_STATUS_DEV) )
        return 1;
    
    return 0;
}

int userdb_checkallowusersuspend(nick *source, userdb_user *target)
{
userdb_user *user;

    if ( !source || !target )
        return 0;
    else if ( UDBRIsDeleted(target) )
        return 0;
    
    switch ( userdb_finduserbynick(source, &user) )
    {
        case USERDB_OK:
            break;
        default:
            return 0;
    }
    
    if ( user == target )
        return 1;
    else if ( !UDBSIsOper(target) && userdb_checkstatusflagsbynick(source, UDB_STATUS_OPER) )
        return 1;
    else if ( !UDBSIsDev(target) && userdb_checkstatusflagsbynick(source, UDB_STATUS_ADMIN) )
        return 1;
    else if ( userdb_checkstatusflagsbynick(source, UDB_STATUS_DEV) )
        return 1;
    else if ( userdb_checkstatusflagsbynick(source, UDB_STATUS_OPER) && userdb_checkgroupflagsbynick(source, UDB_GROUP_SECURITY) )
        return 1;
    
    return 0;
}

int userdb_checkallowuserdelete(nick *source, userdb_user *target)
{
userdb_user *user;

    if ( !source || !target )
        return 0;
    else if ( UDBRIsDeleted(target) )
        return 0;
    
    switch ( userdb_finduserbynick(source, &user) )
    {
        case USERDB_OK:
            break;
        default:
            return 0;
    }
    
    if ( user == target )
        return 1;
    else if ( userdb_checkstatusflagsbynick(source, UDB_STATUS_DEV) )
        return 1;
    
    return 0;
}

/* Status flag policy */
int userdb_checkstatusflagsbyuser(userdb_user *user, flag_t statusflags)
{
flag_t userstatusflags;

    if ( statusflags == UDB_STATUS_EVERYONE )
        return 1;
    
    if ( user )
    {
        if ( UDBOIsSuspended(user) || UDBRIsDeleted(user) )
            return 0;
        
        userstatusflags = (UDB_STATUS_AUTHED | (user->statusflags & (UDB_STATUS_INUSE & ~(UDB_STATUS_UNAUTHED | UDB_STATUS_AUTHED))));
        
        if ( UDBSIsDev(user) )
            userstatusflags |= (UDB_STATUS_INUSE & ~(UDB_STATUS_UNAUTHED | UDB_STATUS_AUTHED));
        else if ( UDBSIsAdmin(user) )
            userstatusflags |= (UDB_STATUS_TRIAL | UDB_STATUS_STAFF | UDB_STATUS_OPER);
        else if ( UDBSIsOper(user) )
            userstatusflags |= (UDB_STATUS_TRIAL | UDB_STATUS_STAFF);
        else if ( UDBSIsStaff(user) )
            userstatusflags |= (UDB_STATUS_TRIAL);
    }
    else
    {
        userstatusflags = (UDB_STATUS_UNAUTHED);
    }
    
    if ( (userstatusflags & statusflags) == statusflags )
        return 1;
    
    return 0;
}

int userdb_checkstatusflagsbynick(nick *nn, flag_t statusflags)
{
userdb_user *user;

    if ( !nn )
        return 0;
    else if ( statusflags == UDB_STATUS_EVERYONE )
        return 1;
    else if ( (statusflags & UDB_STATUS_OPERLOCK) && !IsOper(nn) )
        return 0;
    
    switch ( userdb_finduserbynick(nn, &user) )
    {
        case USERDB_OK:
            break;
        case USERDB_NOTFOUND:
        case USERDB_NOTAUTHED:
            user = NULL;
            break;
        default:
            return 0;
    }
    
    if ( (statusflags & UDB_STATUS_STAFFLOCK) )
    {
        if ( !user )
            return 0;
        else if ( !IsOper(nn) && (userdb_nickonstaffserver(nn) != USERDB_OK) && !userdb_checkoptionflagsbyuser(user, UDB_OPTION_BYPASSSECURITY) )
            return 0;
    }
    
    return userdb_checkstatusflagsbyuser(user, statusflags);
}

flag_t userdb_getallowedstatusflagchanges(nick *source, userdb_user *target)
{
userdb_user *user;
flag_t allowedchanges;

    if ( !source || !target || UDBRIsDeleted(target) )
        return 0;
    
    switch ( userdb_finduserbynick(source, &user) )
    {
        case USERDB_OK:
            break;
        default:
            return 0;
    }
    
    if ( user == target )
        allowedchanges = user->statusflags;
    else if ( userdb_checkstatusflagsbynick(source, UDB_STATUS_DEV) )
        allowedchanges = UDB_STATUS_DEV_CHANGEABLE;
    else if ( userdb_checkstatusflagsbynick(source, UDB_STATUS_ADMIN) )
        allowedchanges = UDB_STATUS_ADMIN_CHANGEABLE;
    else if ( userdb_checkstatusflagsbynick(source, UDB_STATUS_OPER) )
        allowedchanges = UDB_STATUS_OPER_CHANGEABLE;
    else if ( userdb_checkstatusflagsbynick(source, UDB_STATUS_STAFF) )
        allowedchanges = UDB_STATUS_STAFF_CHANGEABLE;
    else if ( userdb_checkstatusflagsbynick(source, UDB_STATUS_TRIAL) )
        allowedchanges = UDB_STATUS_TRIAL_CHANGEABLE;
    else
        allowedchanges = UDB_STATUS_USER_CHANGEABLE;
    
    return allowedchanges;
}

/* Group flag policy */
int userdb_checkgroupflagsbyuser(userdb_user *user, flag_t groupflags)
{
    if ( groupflags == UDB_GROUP_NONE )
        return 1;
    else if ( !user || UDBOIsSuspended(user) || UDBRIsDeleted(user) )
        return 0;
    else if ( userdb_checkstatusflagsbyuser(user, UDB_STATUS_DEV) )
        return 1;
    else if ( (user->groupflags & UDB_GROUP_INUSE) & groupflags )
        return 1;
    else
        return 0;
}

int userdb_checkgroupflagsbynick(nick *nn, flag_t groupflags)
{
userdb_user *user;

    if ( !nn )
        return 0;
    else if ( groupflags == UDB_GROUP_NONE )
        return 1;
    else if ( (groupflags & UDB_GROUP_OPERLOCK) && !IsOper(nn) )
        return 0;
    
    switch ( userdb_finduserbynick(nn, &user) )
    {
        case USERDB_OK:
            break;
        default:
            return 0;
    }
    
    if ( (groupflags & UDB_STATUS_STAFFLOCK) )
    {
        if ( !user )
            return 0;
        else if ( !IsOper(nn) && (userdb_nickonstaffserver(nn) != USERDB_OK) && !userdb_checkoptionflagsbyuser(user, UDB_OPTION_BYPASSSECURITY) )
            return 0;
    }
    
    return userdb_checkgroupflagsbyuser(user, groupflags);
}

flag_t userdb_getallowedgroupflagsbyuser(userdb_user *user)
{
flag_t allowedflags;

    if ( !user || UDBRIsDeleted(user) )
        return 0;
    
    if ( UDBSIsDev(user) )
        allowedflags = UDB_GROUP_DEV_ALLOWED;
    else if ( UDBSIsAdmin(user) )
        allowedflags = UDB_GROUP_ADMIN_ALLOWED;
    else if ( UDBSIsOper(user) )
        allowedflags = UDB_GROUP_OPER_ALLOWED;
    else if ( UDBSIsStaff(user) )
        allowedflags = UDB_GROUP_STAFF_ALLOWED;
    else if ( UDBSIsTrial(user) )
        allowedflags = UDB_GROUP_TRIAL_ALLOWED;
    else
        allowedflags = UDB_GROUP_USER_ALLOWED;
    
    return allowedflags;
}

flag_t userdb_getallowedgroupflagsbynick(nick *nn)
{
userdb_user *user;

    if ( !nn )
        return 0;
    
    switch ( userdb_finduserbynick(nn, &user) )
    {
        case USERDB_OK:
            break;
        default:
            return 0;
    }
    
    return userdb_getallowedgroupflagsbyuser(user);
}

flag_t userdb_getdisallowedgroupflagsbyuser(userdb_user *user, flag_t groupflags)
{
    if ( !user || UDBRIsDeleted(user) )
        return (groupflags & UDB_GROUP_ALL);
    else if ( !groupflags )
        return 0;
    
    return (groupflags & ~(userdb_getallowedgroupflagsbyuser(user)));
}

flag_t userdb_getdisallowedgroupflagsbynick(nick *nn, flag_t groupflags)
{
userdb_user *user;

    if ( !nn )
        return (groupflags & UDB_GROUP_ALL);
    else if ( !groupflags )
        return 0;
    
    switch ( userdb_finduserbynick(nn, &user) )
    {
        case USERDB_OK:
            break;
        default:
            return (groupflags & UDB_GROUP_ALL);
    }
    
    return userdb_getdisallowedgroupflagsbyuser(user, groupflags);
}

flag_t userdb_getallowedgroupflagchanges(nick *source, userdb_user *target)
{
userdb_user *user;
flag_t allowedchanges;

    if ( !source || !target || UDBRIsDeleted(target) )
        return 0;
    
    switch ( userdb_finduserbynick(source, &user) )
    {
        case USERDB_OK:
            break;
        default:
            return 0;
    }
    
    allowedchanges = 0;
    
    if ( user == target )
        allowedchanges = user->groupflags;
    else if ( userdb_checkgroupflagsbynick(source, UDB_STATUS_DEV) )
        allowedchanges = UDB_GROUP_ALL;
    
    allowedchanges &= userdb_getallowedgroupflagsbyuser(target);
    
    return allowedchanges;
}

/* Option flag policy */
int userdb_checkoptionflagsbyuser(userdb_user *user, flag_t optionflags)
{
    if ( !user || !optionflags )
        return 0;
    else if ( UDBRIsDeleted(user) )
        return 0;
    else if ( ((user->optionflags & UDB_OPTION_INUSE) & optionflags) == optionflags )
        return 1;
    else
        return 0;
}

int userdb_checkoptionflagsbynick(nick *nn, flag_t optionflags)
{
userdb_user *user;

    if ( !nn || !optionflags )
        return 0;
    
    switch ( userdb_finduserbynick(nn, &user) )
    {
        case USERDB_OK:
            break;
        default:
            return 0;
    }
    
    return userdb_checkoptionflagsbyuser(user, optionflags);
}

flag_t userdb_getallowedoptionflagsbyuser(userdb_user *user)
{
flag_t allowedflags;

    if ( !user || UDBRIsDeleted(user) )
        return 0;
    
    if ( UDBSIsDev(user) )
        allowedflags = UDB_OPTION_DEV_ALLOWED;
    else if ( UDBSIsAdmin(user) )
        allowedflags = UDB_OPTION_ADMIN_ALLOWED;
    else if ( UDBSIsOper(user) )
        allowedflags = UDB_OPTION_OPER_ALLOWED;
    else if ( UDBSIsStaff(user) )
        allowedflags = UDB_OPTION_STAFF_ALLOWED;
    else if ( UDBSIsTrial(user) )
        allowedflags = UDB_OPTION_TRIAL_ALLOWED;
    else
        allowedflags = UDB_OPTION_USER_ALLOWED;
    
    return allowedflags;
}

flag_t userdb_getallowedoptionflagsbynick(nick *nn)
{
userdb_user *user;

    if ( !nn )
        return 0;
    
    switch ( userdb_finduserbynick(nn, &user) )
    {
        case USERDB_OK:
            break;
        default:
            return 0;
    }
    
    return userdb_getallowedoptionflagsbyuser(user);
}

flag_t userdb_getdisallowedoptionflagsbyuser(userdb_user *user, flag_t optionflags)
{
    if ( !user || UDBRIsDeleted(user) )
        return (optionflags & UDB_OPTION_ALL);
    else if ( !optionflags )
        return 0;
    
    return (optionflags & ~(userdb_getallowedoptionflagsbyuser(user)));
}

flag_t userdb_getdisallowedoptionflagsbynick(nick *nn, flag_t optionflags)
{
userdb_user *user;

    if ( !nn )
        return (optionflags & UDB_OPTION_ALL);
    else if ( !optionflags )
        return 0;
    
    switch ( userdb_finduserbynick(nn, &user) )
    {
        case USERDB_OK:
            break;
        default:
            return (optionflags & UDB_OPTION_ALL);
    }
    
    return userdb_getdisallowedoptionflagsbyuser(user, optionflags);
}

flag_t userdb_getallowedoptionflagchanges(nick *source, userdb_user *target)
{
userdb_user *caller;
flag_t allowedchanges;

    if ( !source || !target || UDBRIsDeleted(target) )
        return 0;
    
    allowedchanges = 0;
    
    switch ( userdb_finduserbynick(source, &caller) )
    {
        case USERDB_OK:
            break;
        default:
            return 0;
    }
    
    if ( caller == target )
    {
        if ( userdb_checkstatusflagsbynick(source, UDB_STATUS_DEV) )
            allowedchanges = UDB_OPTION_DEV_CHANGEABLE;
        else if ( userdb_checkstatusflagsbynick(source, UDB_STATUS_ADMIN) )
            allowedchanges = UDB_OPTION_ADMIN_CHANGEABLE;
        else if ( userdb_checkstatusflagsbynick(source, UDB_STATUS_OPER) )
            allowedchanges = UDB_OPTION_OPER_CHANGEABLE;
        else if ( userdb_checkstatusflagsbynick(source, UDB_STATUS_STAFF) )
            allowedchanges = UDB_OPTION_STAFF_CHANGEABLE;
        else if ( userdb_checkstatusflagsbynick(source, UDB_STATUS_TRIAL) )
            allowedchanges = UDB_OPTION_TRIAL_CHANGEABLE;
        else
            allowedchanges = UDB_OPTION_USER_CHANGEABLE;
        
        return allowedchanges;
    }
    
    if ( userdb_checkstatusflagsbynick(source, UDB_STATUS_DEV) )
        allowedchanges = UDB_OPTION_DEV_CHANGEABLE;
    else if ( userdb_checkstatusflagsbynick(source, UDB_STATUS_ADMIN) )
        allowedchanges = UDB_OPTION_ADMIN_CHANGEABLE;
    else if ( userdb_checkstatusflagsbynick(source, UDB_STATUS_OPER) )
        allowedchanges = UDB_OPTION_OPER_CHANGEABLE;
    
    allowedchanges &= userdb_getallowedoptionflagsbyuser(target);
    
    return allowedchanges;
}

/* Notice flag policy */
int userdb_checknoticeflagsbyuser(userdb_user *user, flag_t noticeflags)
{
    if ( !user || !noticeflags )
        return 0;
    else if ( UDBOIsSuspended(user) || UDBRIsDeleted(user) )
        return 0;
    else if ( ((user->noticeflags & UDB_NOTICE_INUSE) & noticeflags) == noticeflags )
        return 1;
    else
        return 0;
}

int userdb_checknoticeflagsbynick(nick *nn, flag_t noticeflags)
{
userdb_user *user;

    if ( !nn || !noticeflags )
        return 0;
    else if ( (noticeflags & UDB_NOTICE_OPERLOCK) && !IsOper(nn) )
        return 0;
    
    switch ( userdb_finduserbynick(nn, &user) )
    {
        case USERDB_OK:
            break;
        default:
            return 0;
    }
    
    if ( (noticeflags & UDB_NOTICE_STAFFLOCK) )
    {
        if ( !user )
            return 0;
        else if ( !IsOper(nn) && (userdb_nickonstaffserver(nn) != USERDB_OK) && !userdb_checkoptionflagsbyuser(user, UDB_OPTION_BYPASSSECURITY) )
            return 0;
    }
    
    return userdb_checknoticeflagsbyuser(user, noticeflags);
}

flag_t userdb_getallowednoticeflagsbyuser(userdb_user *user)
{
flag_t allowedflags;

    if ( !user || UDBRIsDeleted(user) )
        return 0;
    
    if ( UDBSIsDev(user) )
        allowedflags = UDB_NOTICE_DEV_ALLOWED;
    else if ( UDBSIsAdmin(user) )
        allowedflags = UDB_NOTICE_ADMIN_ALLOWED;
    else if ( UDBSIsOper(user) )
        allowedflags = UDB_NOTICE_OPER_ALLOWED;
    else if ( UDBSIsStaff(user) )
        allowedflags = UDB_NOTICE_STAFF_ALLOWED;
    else if ( UDBSIsTrial(user) )
        allowedflags = UDB_NOTICE_TRIAL_ALLOWED;
    else
        allowedflags = UDB_NOTICE_USER_ALLOWED;
    
    return allowedflags;
}

flag_t userdb_getallowednoticeflagsbynick(nick *nn)
{
userdb_user *user;

    if ( !nn )
        return 0;
    
    switch ( userdb_finduserbynick(nn, &user) )
    {
        case USERDB_OK:
            break;
        default:
            return 0;
    }
    
    return userdb_getallowednoticeflagsbyuser(user);
}

flag_t userdb_getdisallowednoticeflagsbyuser(userdb_user *user, flag_t noticeflags)
{
    if ( !user || UDBRIsDeleted(user) )
        return (noticeflags & UDB_NOTICE_ALL);
    else if ( !noticeflags )
        return 0;
    
    return (noticeflags & ~(userdb_getallowednoticeflagsbyuser(user)));
}

flag_t userdb_getdisallowednoticeflagsbynick(nick *nn, flag_t noticeflags)
{
userdb_user *user;

    if ( !nn )
        return (noticeflags & UDB_NOTICE_ALL);
    else if ( !noticeflags )
        return 0;
    
    switch ( userdb_finduserbynick(nn, &user) )
    {
        case USERDB_OK:
            break;
        default:
            return (noticeflags & UDB_NOTICE_ALL);
    }
    
    return userdb_getdisallowednoticeflagsbyuser(user, noticeflags);
}

flag_t userdb_getallowednoticeflagchanges(nick *source, userdb_user *target)
{
userdb_user *user;
flag_t allowedchanges;

    if ( !source || !target || UDBRIsDeleted(target) )
        return 0;
    
    switch ( userdb_finduserbynick(source, &user) )
    {
        case USERDB_OK:
            break;
        default:
            return 0;
    }
    
    if ( user == target )
    {
        allowedchanges = userdb_getallowednoticeflagsbyuser(target);
    }
    else
    {
        allowedchanges = userdb_getallowednoticeflagsbyuser(target);
        allowedchanges &= userdb_getallowednoticeflagsbyuser(user);
    }
    
    return allowedchanges;
}

/* Wrapped functions */
int userdb_changeuserusernameviapolicy(nick *source, userdb_user *target, char *username)
{
    if ( !source || !target )
        return USERDB_BADPARAMETERS;
    else if ( !userdb_checkallowusermodify(source, target) )
        return USERDB_PERMISSIONDENIED;
    
    return userdb_changeuserusername(target, username);
}

int userdb_changeuserpasswordviapolicy(nick *source, userdb_user *target, char *password)
{
    if ( !source || !target )
        return USERDB_BADPARAMETERS;
    else if ( !userdb_checkallowusermodify(source, target) )
        return USERDB_PERMISSIONDENIED;
    
    return userdb_changeuserpassword(target, password);
}

int userdb_changeuserlanguageviapolicy(nick *source, userdb_user *target, userdb_language *language)
{
    if ( !source || !target )
        return USERDB_BADPARAMETERS;
    else if ( !userdb_checkallowusermodify(source, target) )
        return USERDB_PERMISSIONDENIED;
    
    return userdb_changeuserlanguage(target, language);
}

int userdb_changeuseremailviapolicy(nick *source, userdb_user *target, char *email)
{
    if ( !source || !target )
        return USERDB_BADPARAMETERS;
    else if ( !userdb_checkallowusermodify(source, target) )
        return USERDB_PERMISSIONDENIED;
    
    return userdb_changeuseremail(target, email);
}

int userdb_changeuserstatusflagsviapolicy(nick *source, userdb_user *target, flag_t statusflags, flag_t *rejectflags)
{
flag_t changes, disallowedchanges;

    if ( !source || !target || !rejectflags )
        return USERDB_BADPARAMETERS;
    else if ( !userdb_checkallowusermodify(source, target) )
        return USERDB_PERMISSIONDENIED;
    
    changes = ((target->statusflags) ^ (statusflags));
    disallowedchanges = (changes & ~(userdb_getallowedstatusflagchanges(source, target)));
    
    if ( disallowedchanges )
    {
        *rejectflags = disallowedchanges;
        return USERDB_DISALLOWEDCHANGE;
    }
    
    return userdb_changeuserstatusflags(target, statusflags);
}

int userdb_changeuserstatusflagsbystringviapolicy(nick *source, userdb_user *target, char *statusflagsstring, flag_t *rejectflags, char *rejectflag)
{
flag_t statusflags;

    if ( !source || !target || !statusflagsstring || !rejectflags || !rejectflag )
        return USERDB_BADPARAMETERS;
    else if ( !userdb_checkallowusermodify(source, target) )
        return USERDB_PERMISSIONDENIED;
    
    switch ( userdb_processflagstring(target->statusflags, userdb_statusflags, UDB_STATUS_INTERNALFLAGS, NULL, statusflagsstring, rejectflag, &statusflags) )
    {
        case USERDB_OK:
            break;
        case USERDB_UNKNOWNFLAG:
            return USERDB_UNKNOWNFLAG;
        case USERDB_INTERNALFLAG:
            return USERDB_INTERNALFLAG;
        case USERDB_AUTOMATICFLAG:
            return USERDB_AUTOMATICFLAG;
        default:
            return USERDB_ERROR;
    }
    
    return userdb_changeuserstatusflagsviapolicy(source, target, statusflags, rejectflags);
}

int userdb_changeusergroupflagsviapolicy(nick *source, userdb_user *target, flag_t groupflags, flag_t *rejectflags)
{
flag_t changes, disallowedflags, disallowedchanges;

    if ( !source || !target || !rejectflags )
        return USERDB_BADPARAMETERS;
    else if ( !userdb_checkallowusermodify(source, target) )
        return USERDB_PERMISSIONDENIED;
    
    changes = ((target->groupflags) ^ (groupflags));
    disallowedflags = (changes & ~(userdb_getallowedgroupflagsbyuser(target)));
    disallowedchanges = (changes & ~(userdb_getallowedgroupflagchanges(source, target)));
    
    if ( disallowedflags )
    {
        *rejectflags = disallowedflags;
        return USERDB_DISALLOWEDFLAG;
    }
    else if ( disallowedchanges )
    {
        *rejectflags = disallowedchanges;
        return USERDB_DISALLOWEDCHANGE;
    }
    
    return userdb_changeusergroupflags(target, groupflags);
}

int userdb_changeusergroupflagsbystringviapolicy(nick *source, userdb_user *target, char *groupflagsstring, flag_t *rejectflags, char *rejectflag)
{
flag_t groupflags;

    if ( !source || !target || !groupflagsstring || !rejectflags || !rejectflag )
        return USERDB_BADPARAMETERS;
    else if ( !userdb_checkallowusermodify(source, target) )
        return USERDB_PERMISSIONDENIED;
    
    switch ( userdb_processflagstring(target->groupflags, userdb_groupflags, UDB_GROUP_INTERNALFLAGS, NULL, groupflagsstring, rejectflag, &groupflags) )
    {
        case USERDB_OK:
            break;
        case USERDB_UNKNOWNFLAG:
            return USERDB_UNKNOWNFLAG;
        case USERDB_INTERNALFLAG:
            return USERDB_INTERNALFLAG;
        case USERDB_AUTOMATICFLAG:
            return USERDB_AUTOMATICFLAG;
        default:
            return USERDB_ERROR;
    }
    
    return userdb_changeusergroupflagsviapolicy(source, target, groupflags, rejectflags);
}

int userdb_changeuseroptionflagsviapolicy(nick *source, userdb_user *target, flag_t optionflags, flag_t *rejectflags)
{
flag_t changes, disallowedflags, disallowedchanges;

    if ( !source || !target || !rejectflags )
        return USERDB_BADPARAMETERS;
    else if ( !userdb_checkallowusermodify(source, target) )
        return USERDB_PERMISSIONDENIED;
    
    changes = ((target->optionflags) ^ (optionflags));
    disallowedflags = (changes & ~(userdb_getallowedoptionflagsbyuser(target)));
    disallowedchanges = (changes & ~(userdb_getallowedoptionflagchanges(source, target)));
    
    if ( disallowedflags )
    {
        *rejectflags = disallowedflags;
        return USERDB_DISALLOWEDFLAG;
    }
    else if ( disallowedchanges )
    {
        *rejectflags = disallowedchanges;
        return USERDB_DISALLOWEDCHANGE;
    }
    
    return userdb_changeuseroptionflags(target, optionflags);
}

int userdb_changeuseroptionflagsbystringviapolicy(nick *source, userdb_user *target, char *optionflagsstring, flag_t *rejectflags, char *rejectflag)
{
flag_t optionflags;

    if ( !source || !target || !optionflagsstring || !rejectflags || !rejectflag )
        return USERDB_BADPARAMETERS;
    else if ( !userdb_checkallowusermodify(source, target) )
        return USERDB_PERMISSIONDENIED;
    
    switch ( userdb_processflagstring(target->optionflags, userdb_optionflags, NULL, UDB_OPTION_AUTOMATICFLAGS, optionflagsstring, rejectflag, &optionflags) )
    {
        case USERDB_OK:
            break;
        case USERDB_UNKNOWNFLAG:
            return USERDB_UNKNOWNFLAG;
        case USERDB_INTERNALFLAG:
            return USERDB_INTERNALFLAG;
        case USERDB_AUTOMATICFLAG:
            return USERDB_AUTOMATICFLAG;
        default:
            return USERDB_ERROR;
    }
    
    return userdb_changeuseroptionflagsviapolicy(source, target, optionflags, rejectflags);
}

int userdb_changeusernoticeflagsviapolicy(nick *source, userdb_user *target, flag_t noticeflags, flag_t *rejectflags)
{
flag_t changes, disallowedflags, disallowedchanges;

    if ( !source || !target || !rejectflags )
        return USERDB_BADPARAMETERS;
    else if ( !userdb_checkallowusermodify(source, target) )
        return USERDB_PERMISSIONDENIED;
    
    changes = ((target->noticeflags) ^ (noticeflags));
    disallowedflags = (changes & ~(userdb_getallowednoticeflagsbyuser(target)));
    disallowedchanges = (changes & ~(userdb_getallowednoticeflagchanges(source, target)));
    
    if ( disallowedflags )
    {
        *rejectflags = disallowedflags;
        return USERDB_DISALLOWEDFLAG;
    }
    else if ( disallowedchanges )
    {
        *rejectflags = disallowedchanges;
        return USERDB_DISALLOWEDCHANGE;
    }
    
    return userdb_changeusernoticeflags(target, noticeflags);
}

int userdb_changeusernoticeflagsbystringviapolicy(nick *source, userdb_user *target, char *noticeflagsstring, flag_t *rejectflags, char *rejectflag)
{
flag_t noticeflags;

    if ( !source || !target || !noticeflagsstring || !rejectflags || !rejectflag )
        return USERDB_BADPARAMETERS;
    else if ( !userdb_checkallowusermodify(source, target) )
        return USERDB_PERMISSIONDENIED;
    
    switch ( userdb_processflagstring(target->noticeflags, userdb_noticeflags, NULL, NULL, noticeflagsstring, rejectflag, &noticeflags) )
    {
        case USERDB_OK:
            break;
        case USERDB_UNKNOWNFLAG:
            return USERDB_UNKNOWNFLAG;
        case USERDB_INTERNALFLAG:
            return USERDB_INTERNALFLAG;
        case USERDB_AUTOMATICFLAG:
            return USERDB_AUTOMATICFLAG;
        default:
            return USERDB_ERROR;
    }
    
    return userdb_changeusernoticeflagsviapolicy(source, target, noticeflags, rejectflags);
}

int userdb_suspenduserviapolicy(nick *source, userdb_user *target, char *reason, flag_t action, time_t suspendend, userdb_user *suspender)
{
    if ( !source || !target )
        return USERDB_BADPARAMETERS;
    else if ( UDBOIsSuspended(target) && !target->suspensiondata )
        return USERDB_ERROR;
    else if ( !UDBOIsSuspended(target) && target->suspensiondata )
        return USERDB_ERROR;
    else if ( UDBOIsSuspended(target) )
        return USERDB_NOOP;
    else if ( !userdb_checkallowusersuspend(source, target) )
        return USERDB_PERMISSIONDENIED;
    
    return userdb_suspenduser(target, reason, action, suspendend, suspender);
}

int userdb_unsuspenduserviapolicy(nick *source, userdb_user *target)
{
    if ( !source || !target )
        return USERDB_BADPARAMETERS;
    else if ( UDBOIsSuspended(target) && !target->suspensiondata )
        return USERDB_ERROR;
    else if ( !UDBOIsSuspended(target) && target->suspensiondata )
        return USERDB_ERROR;
    else if ( !UDBOIsSuspended(target) )
        return USERDB_NOOP;
    else if ( !userdb_checkallowusersuspend(source, target) )
        return USERDB_PERMISSIONDENIED;
    
    return userdb_unsuspenduser(target);
}

int userdb_deleteuserviapolicy(nick *source, userdb_user *target)
{
    if ( !source || !target )
        return USERDB_BADPARAMETERS;
    else if ( !userdb_checkallowuserdelete(source, target) )
        return USERDB_PERMISSIONDENIED;
    
    return userdb_deleteuser(target);
}
