/*
 * UserDB
 */

#include "./userdb.h"
#include "../lib/sha2.h"

int userdb_findauthnamebyusername(char *username, authname **auth)
{
userdb_user *user;

    if ( !username || !auth )
        return USERDB_BADPARAMETERS;
    
    switch ( userdb_finduserbyusername(username, &user) )
    {
        case USERDB_OK:
            break;
        case USERDB_NOTFOUND:
            return USERDB_NOTFOUND;
        default:
            return USERDB_ERROR;
    }
    
    *auth = findauthname(user->userid);
    
    if ( *auth == NULL )
        return USERDB_ERROR;
    
    return USERDB_OK;
}

void userdb_sendmessagetouser(nick *source, nick *target, char *format, ...)
{
char linebuf[513];
int len;
va_list va;

    if ( !source || !target || !format || userdb_zerolengthstring(format) )
        return;
    
    va_start(va, format);
    len = vsnprintf(linebuf, sizeof(linebuf), format, va);
    va_end(va);
    linebuf[sizeof(linebuf) - 1] = '\0';
    
    if ( !IsAccount(target) || !target->auth || !target->auth->exts[userdb_authnameextnum] || userdb_checkoptionflagsbynick(target, UDB_OPTION_NOTICES) )
        sendnoticetouser(source, target, "%s", linebuf);
    else
        sendmessagetouser(source, target, "%s", linebuf);
}

void userdb_sendnoticetouser(nick *source, nick *target, char *format, ...)
{
char linebuf[513];
int len;
va_list va;

    if ( !source || !target || !format || userdb_zerolengthstring(format) )
        return;
    
    va_start(va, format);
    len = vsnprintf(linebuf, sizeof(linebuf), format, va);
    va_end(va);
    linebuf[sizeof(linebuf) - 1] = '\0';
    
    sendnoticetouser(source, target, "%s", linebuf);
}

void userdb_sendwarningtouser(nick *source, nick *target, char *format, ...)
{
char linebuf[513];
int len;
va_list va;

    if ( !source || !target || !format || userdb_zerolengthstring(format) )
        return;
    
    va_start(va, format);
    len = vsnprintf(linebuf, sizeof(linebuf), format, va);
    va_end(va);
    linebuf[sizeof(linebuf) - 1] = '\0';
    
    userdb_sendnoticetouser(source, target, "$!$ %s", linebuf);
}

void userdb_wallmessage(nick *source, flag_t noticeflag, char *format, ...)
{
unsigned int i;
char linebuf[513], *flags;
int len;
nick *nn;
va_list va;

    if ( !source || !noticeflag || !format || userdb_zerolengthstring(format) )
        return;
    
    va_start(va, format);
    len = vsnprintf(linebuf, sizeof(linebuf), format, va);
    va_end(va);
    linebuf[sizeof(linebuf) - 1] = '\0';
    
    flags = printflags_noprefix(noticeflag, userdb_noticeflags);
    
    Error("WallMessage", ERR_INFO, "From %s: $%s$ %s", source->nick, flags, linebuf);
    
    for ( i = 0; i < NICKHASHSIZE; i++ )
        for ( nn = nicktable[i]; nn; nn = nn->next )
            if ( userdb_checknoticeflagsbynick(nn, noticeflag) )
                userdb_sendnoticetouser(source, nn, "$%s$ %s", flags, linebuf);
}

char *userdb_printuserstring(nick *nn, userdb_user *user)
{
static char buf[513];
userdb_user *nnuser;

    if ( nn )
    {
        if ( userdb_finduserbynick(nn, &nnuser) == USERDB_OK )
            snprintf(buf, sizeof(buf), "%s!%s@%s/%s", nn->nick, nn->ident, nn->host->name->content, nnuser->username->content);
        else
            snprintf(buf, sizeof(buf), "%s!%s@%s", nn->nick, nn->ident, nn->host->name->content);
    }
    else if ( user )
    {
        snprintf(buf, sizeof(buf), "<unknown>!<unknown>@<unknown>/%s", user->username->content);
    }
    else
    {
        return "<unknown>!<unknown>@<unknown>/<unknown>";
    }
    
    buf[sizeof(buf) - 1] = '\0';
    return buf;
}

char *userdb_printshortuserstring(nick *nn, userdb_user *user)
{
static char buf[513];
userdb_user *nnuser;

    if ( nn )
    {
        if ( userdb_finduserbynick(nn, &nnuser) == USERDB_OK )
            snprintf(buf, sizeof(buf), "%s/%s", nn->nick, nnuser->username->content);
        else
            snprintf(buf, sizeof(buf), "%s", nn->nick);
    }
    else if ( user )
    {
        snprintf(buf, sizeof(buf), "<unknown>/%s", user->username->content);
    }
    else
    {
        return "<unknown>/<unknown>";
    }
    
    buf[sizeof(buf) - 1] = '\0';
    return buf;
}

char *userdb_printcommandflags(int acl)
{
static char statusflagsbuf[21], groupflagsbuf[21], buf[51];
flag_t statusflags, groupflags;

    if ( userdb_getaclflags(acl, &statusflags, &groupflags) != USERDB_OK )
    {
        buf[0] = '\0';
        return buf;
    }
    
    strncpy(statusflagsbuf, userdb_printcommandstatusflags(statusflags), (sizeof(statusflagsbuf) - 1));
    strncpy(groupflagsbuf, userdb_printcommandgroupflags(groupflags), (sizeof(groupflagsbuf) - 1));
    statusflagsbuf[sizeof(statusflagsbuf) - 1] = '\0';
    groupflagsbuf[sizeof(groupflagsbuf) - 1] = '\0';
    snprintf(buf, sizeof(buf), "%s/%s", statusflagsbuf, groupflagsbuf);
    buf[sizeof(buf) - 1] = '\0';
    return buf;
}

char *userdb_printcommandstatusflags(flag_t statusflags)
{
    if ( statusflags )
        return printflags(statusflags, userdb_statusflags);
    else
        return "+E";
}

char *userdb_printcommandgroupflags(flag_t groupflags)
{
    if ( groupflags )
        return printflags(groupflags, userdb_groupflags);
    else
        return "+N";
}

int userdb_scanuserusernames(char *pattern, unsigned int limit, void *data, userdb_scanuserusernamescallback callback, unsigned int *resultcount)
{
userdb_user *user;
unsigned int i, results;

    if ( !pattern || userdb_zerolengthstring(pattern) || !resultcount )
        return USERDB_BADPARAMETERS;
    
    results = 0;
    
    if ( userdb_ismatchall(pattern) == USERDB_OK )
    {
        for ( i = 0; i < USERDB_USERHASHSIZE; i++ )
        {
            for ( user = userdb_userhashtable_id[i]; user; user = user->nextbyid )
            {
                results++;
                
                if ( callback )
                    (callback)(user, pattern, limit, results, data);
                
                if ( (limit) && (results >= limit) )
                {
                    *resultcount = results;
                    return USERDB_RESULTLIMITREACHED;
                }
            }
        }
    }
    else if ( userdb_containsnowildcards(pattern) == USERDB_OK )
    {
        switch ( userdb_finduserbyusername(pattern, &user) )
        {
            case USERDB_OK:
                results++;
                
                if ( callback )
                    (callback)(user, pattern, limit, results, data);
                
                break;
            case USERDB_NOTFOUND:
                break;
            default:
                return USERDB_ERROR;
        }
    }
    else
    {
        for ( i = 0; i < USERDB_USERHASHSIZE; i++ )
        {
            for ( user = userdb_userhashtable_id[i]; user; user = user->nextbyid )
            {
                if ( match2strings(pattern, user->username->content) )
                {
                    results++;
                    
                    if ( callback )
                        (callback)(user, pattern, limit, results, data);
                    
                    if ( (limit) && (results >= limit) )
                    {
                        *resultcount = results;
                        return USERDB_RESULTLIMITREACHED;
                    }
                }
            }
        }
    }
    
    *resultcount = results;
    return USERDB_OK;
}

int userdb_scanuseremailaddresses(char *pattern, unsigned int limit, void *data, userdb_scanuseremailaddressescallback callback, unsigned int *resultcount)
{
userdb_user *user;
userdb_maildomain *maildomain;
char patternlocal[USERDB_EMAILLEN + 1], patterndomain[USERDB_EMAILLEN + 1];
char *emaildomain;
unsigned int i, results;

    if ( !pattern || userdb_zerolengthstring(pattern) || !resultcount )
        return USERDB_BADPARAMETERS;
    
    user = NULL;
    maildomain = NULL;
    emaildomain = NULL;
    results = 0;
    
    switch ( userdb_splitemaillocalpart(pattern, patternlocal, patterndomain) )
    {
        case USERDB_OK:
            break;
        case USERDB_BADEMAIL:
            return USERDB_BADEMAIL;
        default:
            return USERDB_ERROR;
    }
    
    if ( userdb_ismatchall(patterndomain) == USERDB_OK )
    {
        if ( userdb_ismatchall(patternlocal) == USERDB_OK )
        {
            for ( i = 0; i < USERDB_USERHASHSIZE; i++ )
            {
                for ( user = userdb_userhashtable_id[i]; user; user = user->nextbyid )
                {
                    results++;
                    
                    if ( callback )
                        (callback)(user, pattern, limit, results, data);
                    
                    if ( (limit) && (results >= limit) )
                    {
                        *resultcount = results;
                        return USERDB_RESULTLIMITREACHED;
                    }
                }
            }
        }
        else if ( userdb_containsnowildcards(patternlocal) == USERDB_OK )
        {
            for ( i = 0; i < USERDB_USERHASHSIZE; i++ )
            {
                for ( user = userdb_userhashtable_id[i]; user; user = user->nextbyid )
                {
                    if ( !ircd_strcmp(patternlocal, user->emaillocal->content) )
                    {
                        results++;
                        
                        if ( callback )
                            (callback)(user, pattern, limit, results, data);
                        
                        if ( (limit) && (results >= limit) )
                        {
                            *resultcount = results;
                            return USERDB_RESULTLIMITREACHED;
                        }
                    }
                }
            }
        }
        else
        {
            for ( i = 0; i < USERDB_USERHASHSIZE; i++ )
            {
                for ( user = userdb_userhashtable_id[i]; user; user = user->nextbyid )
                {
                    if ( match2strings(patternlocal, user->emaillocal->content) )
                    {
                        results++;
                        
                        if ( callback )
                            (callback)(user, pattern, limit, results, data);
                        
                        if ( (limit) && (results >= limit) )
                        {
                            *resultcount = results;
                            return USERDB_RESULTLIMITREACHED;
                        }
                    }
                }
            }
        } 
    }
    else if ( userdb_containsnowildcards(patterndomain) == USERDB_OK )
    {
        switch ( userdb_findmaildomainbyemaildomain(patterndomain, &maildomain, 0) )
        {
            case USERDB_OK:
                break;
            default:
                return USERDB_OK;
        }
        
        if ( userdb_ismatchall(patternlocal) == USERDB_OK )
        {
            for ( user = maildomain->users; user; user = user->nextbymaildomain )
            {
                results++;
                
                if ( callback )
                    (callback)(user, pattern, limit, results, data);
                
                if ( (limit) && (results >= limit) )
                {
                    *resultcount = results;
                    return USERDB_RESULTLIMITREACHED;
                }
            }
        }
        else if ( userdb_containsnowildcards(patternlocal) == USERDB_OK )
        {
            for ( user = maildomain->users; user; user = user->nextbymaildomain )
            {
                if ( !ircd_strcmp(patternlocal, user->emaillocal->content) )
                {
                    results++;
                    
                    if ( callback )
                        (callback)(user, pattern, limit, results, data);
                    
                    if ( (limit) && (results >= limit) )
                    {
                        *resultcount = results;
                        return USERDB_RESULTLIMITREACHED;
                    }
                }
            }
        }
        else
        {
            for ( user = maildomain->users; user; user = user->nextbymaildomain )
            {
                if ( match2strings(patternlocal, user->emaillocal->content) )
                {
                    results++;
                    
                    if ( callback )
                        (callback)(user, pattern, limit, results, data);
                    
                    if ( (limit) && (results >= limit) )
                    {
                        *resultcount = results;
                        return USERDB_RESULTLIMITREACHED;
                    }
                }
            }
        }
    }
    else
    {
        if ( userdb_ismatchall(patternlocal) == USERDB_OK )
        {
            for ( i = 0; i < USERDB_MAILDOMAINHASHSIZE; i++ )
            {
                for ( maildomain = userdb_maildomainhashtable_name[i]; maildomain; maildomain = maildomain->nextbyname )
                {
                    if ( !maildomain->domaincount )
                        continue;
                    
                    switch ( userdb_buildemaildomain(maildomain, &emaildomain) )
                    {
                        case USERDB_OK:
                            break;
                        default:
                            return USERDB_ERROR;
                    }
                    
                    if ( match2strings(patterndomain, emaildomain) )
                    {
                        for ( user = maildomain->users; user; user = user->nextbymaildomain )
                        {
                            results++;
                            
                            if ( callback )
                                (callback)(user, pattern, limit, results, data);
                            
                            if ( (limit) && (results >= limit) )
                            {
                                *resultcount = results;
                                return USERDB_RESULTLIMITREACHED;
                            }
                        }
                    }
                }
            }
        }
        else if ( userdb_containsnowildcards(patternlocal) == USERDB_OK )
        {
            for ( i = 0; i < USERDB_MAILDOMAINHASHSIZE; i++ )
            {
                for ( maildomain = userdb_maildomainhashtable_name[i]; maildomain; maildomain = maildomain->nextbyname )
                {
                    if ( !maildomain->domaincount )
                        continue;
                    
                    switch ( userdb_buildemaildomain(maildomain, &emaildomain) )
                    {
                        case USERDB_OK:
                            break;
                        default:
                            return USERDB_ERROR;
                    }
                    
                    if ( match2strings(patterndomain, emaildomain) )
                    {
                        for ( user = maildomain->users; user; user = user->nextbymaildomain )
                        {
                            if ( !ircd_strcmp(patternlocal, user->emaillocal->content) )
                            {
                                results++;
                                
                                if ( callback )
                                    (callback)(user, pattern, limit, results, data);
                                
                                if ( (limit) && (results >= limit) )
                                {
                                    *resultcount = results;
                                    return USERDB_RESULTLIMITREACHED;
                                }
                            }
                        }
                    }
                }
            }
        }
        else
        {
            for ( i = 0; i < USERDB_MAILDOMAINHASHSIZE; i++ )
            {
                for ( maildomain = userdb_maildomainhashtable_name[i]; maildomain; maildomain = maildomain->nextbyname )
                {
                    if ( !maildomain->domaincount )
                        continue;
                    
                    switch ( userdb_buildemaildomain(maildomain, &emaildomain) )
                    {
                        case USERDB_OK:
                            break;
                        default:
                            return USERDB_ERROR;
                    }
                    
                    if ( match2strings(patterndomain, emaildomain) )
                    {
                        for ( user = maildomain->users; user; user = user->nextbymaildomain )
                        {
                            if ( match2strings(patternlocal, user->emaillocal->content) )
                            {
                                results++;
                                
                                if ( callback )
                                    (callback)(user, pattern, limit, results, data);
                                
                                if ( (limit) && (results >= limit) )
                                {
                                    *resultcount = results;
                                    return USERDB_RESULTLIMITREACHED;
                                }
                            }
                        }
                    }
                }
            }
        }

    }
    
    *resultcount = results;
    return USERDB_OK;
}

int userdb_scanuserpasswords(char *password, unsigned int limit, void *data, userdb_scanuserpasswordscallback callback, unsigned int *resultcount)
{
SHA256_CTX hash;
userdb_user *user;
unsigned char buf[USERDB_MAXPASSWORDLEN + 201];
unsigned char result[USERDB_PASSWORDHASHLEN];
unsigned int i, results;
unsigned long buflen;

    if ( !password || userdb_zerolengthstring(password) || !resultcount )
        return USERDB_BADPARAMETERS;
    
    results = 0;
    
    switch ( userdb_checkpassword(password) )
    {
        case USERDB_OK:
            break;
        case USERDB_BADPASSWORD:
            return USERDB_BADPASSWORD;
        default:
            return USERDB_ERROR;
    }
    
    for ( i = 0; i < USERDB_USERHASHSIZE; i++ )
    {
        for ( user = userdb_userhashtable_id[i]; user; user = user->nextbyid )
        {
            buflen = snprintf((char *)buf, sizeof(buf), "QuakeNet User Database System %lu %ld %s", user->userid, user->created, password);
            buf[sizeof(buf) - 1] = '\0';
            
            SHA256_Init(&hash);
            SHA256_Update(&hash, buf, buflen);
            SHA256_Final(result, &hash);
            
            if ( memcmp(user->password, result, USERDB_PASSWORDHASHLEN) )
                continue;
            
            results++;
            
            if ( callback )
                (callback)(user, password, limit, results, data);
            
            if ( (limit) && (results >= limit) )
            {
                *resultcount = results;
                return USERDB_RESULTLIMITREACHED;
            }
        }
    }
    
    *resultcount = results;
    return USERDB_OK;
}

int userdb_scanusersuspensions(char *pattern, unsigned int limit, void *data, userdb_scanusersuspensionscallback callback, unsigned int *resultcount)
{
userdb_user *user;
unsigned int i, results;

    if ( !pattern || userdb_zerolengthstring(pattern) || !resultcount )
        return USERDB_BADPARAMETERS;
    
    results = 0;
    
    if ( userdb_ismatchall(pattern) == USERDB_OK )
    {
        for ( i = 0; i < USERDB_USERSUSPENDEDHASHSIZE; i++ )
        {
            for ( user = userdb_usersuspendedhashtable[i]; user; user = user->suspensiondata->nextbyid )
            {
                results++;
                
                if ( callback )
                    (callback)(user, pattern, limit, results, data);
                
                if ( (limit) && (results >= limit) )
                {
                    *resultcount = results;
                    return USERDB_RESULTLIMITREACHED;
                }
            }
        }
    }
    else if ( userdb_containsnowildcards(pattern) == USERDB_OK )
    {
        switch ( userdb_finduserbyusername(pattern, &user) )
        {
            case USERDB_OK:
                
                if ( UDBOIsSuspended(user) )
                {
                    results++;
                    
                    if ( callback )
                        (callback)(user, pattern, limit, results, data);
                }
                
                break;
            case USERDB_NOTFOUND:
                break;
            default:
                return USERDB_ERROR;
        }
    }
    else
    {
        for ( i = 0; i < USERDB_USERSUSPENDEDHASHSIZE; i++ )
        {
            for ( user = userdb_usersuspendedhashtable[i]; user; user = user->suspensiondata->nextbyid )
            {
                if ( match2strings(pattern, user->username->content) )
                {
                    results++;
                    
                    if ( callback )
                        (callback)(user, pattern, limit, results, data);
                    
                    if ( (limit) && (results >= limit) )
                    {
                        *resultcount = results;
                        return USERDB_RESULTLIMITREACHED;
                    }
                }
            }
        }
    }
    
    *resultcount = results;
    return USERDB_OK;
}

int userdb_scanmaildomainconfignames(char *pattern, unsigned int limit, void *data, userdb_scanmaildomainconfignamescallback callback, unsigned int *resultcount)
{
userdb_maildomainconfig *maildomainconfig;
unsigned int i, results;

    if ( !pattern || userdb_zerolengthstring(pattern) || !resultcount )
        return USERDB_BADPARAMETERS;
    
    maildomainconfig = NULL;
    results = 0;
    
    if ( userdb_ismatchall(pattern) == USERDB_OK )
    {
        for ( i = 0; i < USERDB_MAILDOMAINCONFIGHASHSIZE; i++ )
        {
            for ( maildomainconfig = userdb_maildomainconfighashtable_id[i]; maildomainconfig; maildomainconfig = maildomainconfig->nextbyid )
            {
                results++;
                
                if ( callback )
                    (callback)(maildomainconfig, pattern, limit, results, data);
                
                if ( (limit) && (results >= limit) )
                {
                    *resultcount = results;
                    return USERDB_RESULTLIMITREACHED;
                }
            }
        }
    }
    else if ( userdb_containsnowildcards(pattern) == USERDB_OK )
    {
        switch ( userdb_findmaildomainconfigbyname(pattern, &maildomainconfig) )
        {
            case USERDB_OK:
                results++;
                
                if ( callback )
                    (callback)(maildomainconfig, pattern, limit, results, data);
                
                break;
            case USERDB_NOTFOUND:
                break;
            default:
                return USERDB_ERROR;
        }
    }
    else
    {
        for ( i = 0; i < USERDB_MAILDOMAINCONFIGHASHSIZE; i++ )
        {
            for ( maildomainconfig = userdb_maildomainconfighashtable_id[i]; maildomainconfig; maildomainconfig = maildomainconfig->nextbyid )
            {
                if ( match2strings(pattern, maildomainconfig->configname->content) )
                {
                    results++;
                    
                    if ( callback )
                        (callback)(maildomainconfig, pattern, limit, results, data);
                    
                    if ( (limit) && (results >= limit) )
                    {
                        *resultcount = results;
                        return USERDB_RESULTLIMITREACHED;
                    }
                }
            }
        }
    }
    
    *resultcount = results;
    return USERDB_OK;
}
