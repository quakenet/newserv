/*
 * UserDB
 */

#include "./userdb.h"

int userdb_finduserbyauthname(authname *an, userdb_user **user)
{
    if ( !an || !user )
        return USERDB_BADPARAMETERS;
    else if ( userdb_authnameextnum == -1 )
        return USERDB_ERROR;
    else if ( !an->exts[userdb_authnameextnum] )
        return USERDB_NOTFOUND;
    
    *user = (userdb_user *)an->exts[userdb_authnameextnum];
    return USERDB_OK;
}

int userdb_finduserbynick(nick *nn, userdb_user **user)
{
    if ( !nn || !user )
        return USERDB_BADPARAMETERS;
    else if ( !IsAccount(nn) || !nn->auth )
        return USERDB_NOTAUTHED;
    
    return userdb_finduserbyauthname(nn->auth, user);
}

int userdb_assignusertoauth(userdb_user *user)
{
authname *an;

    if ( !user )
        return USERDB_BADPARAMETERS;
    else if ( userdb_authnameextnum == -1 )
        return USERDB_ERROR;
    
    an = findorcreateauthname(user->userid);
    
    if ( !an )
        return USERDB_ERROR;
    
    an->exts[userdb_authnameextnum] = user;
    
    return USERDB_OK;
}

int userdb_unassignuserfromauth(userdb_user *user)
{
authname *an;

    if ( !user )
        return USERDB_BADPARAMETERS;
    else if ( userdb_authnameextnum == -1 )
        return USERDB_ERROR;
    
    an = findauthname(user->userid);
    
    if ( an )
    {
        an->exts[userdb_authnameextnum] = NULL;
        releaseauthname(an);
    }
    
    return USERDB_OK;
}
