/*
 * UserDB
 */

#include "./userdb.h"

const flag userdb_maildomainconfigflags[] = {
    { 'b', UDB_DOMAIN_BANNED },
    { 'w', UDB_DOMAIN_WATCH },
    { 'l', UDB_DOMAIN_LOCALLIMIT },
    { 'L', UDB_DOMAIN_DOMAINLIMIT },
    { 'e', UDB_DOMAIN_EXCLUDESUBDOMAINS },
    { '\0', 0 }
  };

int userdb_checkemaillimits(char *email, char *currentmaillocal, userdb_maildomain *currentmaildomain)
{
unsigned long localcount;
char emaillocal[USERDB_EMAILLEN + 1], emaildomain[USERDB_EMAILLEN + 1];
int exactmatch;
userdb_maildomain *maildomain, *configmaildomain, *tempmaildomain;
userdb_user *user;

    if ( !email || userdb_zerolengthstring(email) || ( currentmaillocal && userdb_zerolengthstring(currentmaillocal) ) )
        return USERDB_BADPARAMETERS;
    
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
        case USERDB_BADEMAIL:
            return USERDB_BADEMAIL;
        default:
            return USERDB_ERROR;
    }
    
    switch ( userdb_findmaildomainbyemaildomain(emaildomain, &maildomain, 0) )
    {
        case USERDB_NOTFOUND:
            
            if ( !maildomain )
                return USERDB_OK;
            
            exactmatch = 0;
            break;
        case USERDB_OK:
            exactmatch = 1;
            break;
        default:
            return USERDB_ERROR;
    }
    
    configmaildomain = NULL;
    
    for ( tempmaildomain = maildomain; tempmaildomain; tempmaildomain = tempmaildomain->parent )
    {
        if ( tempmaildomain->config )
        {
            if ( UDBDIsExcludeSubdomains(tempmaildomain->config) && ( !(tempmaildomain == maildomain) || !(exactmatch) ) )
                continue;
            
            configmaildomain = tempmaildomain;
            break;
        }
    }
    
    if ( configmaildomain && UDBDIsBanned(configmaildomain->config) )
        return USERDB_DOMAINBANNED;
    
    if ( maildomain != currentmaildomain )
    {
        if ( configmaildomain && UDBDIsDomainLimit(configmaildomain->config) )
        {
            if ( (configmaildomain->config->maxdomain) && (maildomain->domaincount >= configmaildomain->config->maxdomain) )
                return USERDB_DOMAINLIMITREACHED;
        }
#if ( USERDB_DEFAULTMAXDOMAIN > 0 )
        else
        {
            if ( maildomain->domaincount >= USERDB_DEFAULTMAXDOMAIN )
                return USERDB_DOMAINLIMITREACHED;
        }
#endif
    }
    
    localcount = 0;
    
    for ( user = maildomain->users; user; user = user->nextbymaildomain )
        if ( user->emaillocal && !ircd_strcmp(user->emaillocal->content, emaillocal) )
            localcount++;
    
    if ( (maildomain != currentmaildomain) || !(currentmaillocal) || (ircd_strcmp(currentmaillocal, emaillocal)) )
    {
        if ( configmaildomain && UDBDIsLocalLimit(configmaildomain->config) )
        {
            if ( (configmaildomain->config->maxlocal) && (localcount >= configmaildomain->config->maxlocal) )
                return USERDB_LOCALLIMITREACHED;
        }
#if ( USERDB_DEFAULTMAXLOCAL > 0 )
        else
        {
            if ( localcount >= USERDB_DEFAULTMAXLOCAL )
                return USERDB_LOCALLIMITREACHED;
        }
#endif
    }
    
    return USERDB_OK;
}

int userdb_createmaildomainconfig(char *configname, flag_t domainflags, unsigned long maxlocal, unsigned long maxdomain, userdb_maildomainconfig **maildomainconfig)
{
userdb_maildomain *maildomain, *tempmaildomain;
userdb_maildomainconfig *newmaildomainconfig;
time_t maildomainconfigtime;

#ifdef USERDB_ENABLEREPLICATION
    if ( !userdb_repl_group || userdb_repl_group->role != REPLICATOR_ROLE_MASTER )
        return USERDB_NOACCESS;
#endif
    
    if ( !userdb_databaseloaded() )
        return USERDB_DBUNAVAILABLE;
    else if ( !configname || userdb_zerolengthstring(configname) || !maildomainconfig )
        return USERDB_BADPARAMETERS;
    else if ( maxlocal > USERDB_MAXMAILDOMAINCONFIGLOCALLIMIT )
        return USERDB_BADLOCALLIMIT;
    else if ( maxdomain > USERDB_MAXMAILDOMAINCONFIGDOMAINLIMIT )
        return USERDB_BADDOMAINLIMIT;
    
    switch ( userdb_checkmaildomainconfigname(configname) )
    {
        case USERDB_OK:
            break;
        case USERDB_BADMAILDOMAINCONFIGNAME:
            return USERDB_BADMAILDOMAINCONFIGNAME;
        default:
            return USERDB_ERROR;
    }
    
    switch ( userdb_findmaildomainconfigbyname(configname, &newmaildomainconfig) )
    {
        case USERDB_OK:
            return USERDB_ALREADYEXISTS;
        case USERDB_NOTFOUND:
            break;
        default:
            return USERDB_ERROR;
    }
    
    switch ( userdb_findmaildomainbyemaildomain(configname, &maildomain, 1) )
    {
        case USERDB_OK:
            break;
        default:
            Error("userdb", ERR_ERROR, "Error creating mail domain config, unable to get mail domain.");
            return USERDB_ERROR;
    }
    
    for ( tempmaildomain = maildomain; tempmaildomain; tempmaildomain = tempmaildomain->parent )
        tempmaildomain->configcount++;
    
    maildomainconfigtime = time(NULL);
    
    newmaildomainconfig = userdb_mallocmaildomainconfig();
    
    newmaildomainconfig->configid = ++userdb_lastmaildomainconfigid;
    newmaildomainconfig->configname = getsstring(configname, USERDB_EMAILLEN);
    newmaildomainconfig->domainflags = domainflags;
    newmaildomainconfig->maxlocal = maxlocal;
    newmaildomainconfig->maxdomain = maxdomain;
    
    newmaildomainconfig->created = maildomainconfigtime;
    newmaildomainconfig->modified = maildomainconfigtime;
    newmaildomainconfig->revision = 1;
    newmaildomainconfig->transactionid = ++userdb_lastmaildomainconfigtransactionid;
    
    maildomain->config = newmaildomainconfig;
    newmaildomainconfig->maildomain = maildomain;
    
    userdb_addmaildomainconfigtohash(newmaildomainconfig);
    userdb_addmaildomainconfigtodb(newmaildomainconfig);
    
    *maildomainconfig = newmaildomainconfig;
    triggerhook(HOOK_USERDB_CREATEMDC, newmaildomainconfig);
    
    return USERDB_OK;
}

int userdb_modifymaildomainconfigflags(userdb_maildomainconfig *maildomainconfig, flag_t domainflags)
{
#ifdef USERDB_ENABLEREPLICATION
    if ( !userdb_repl_group || userdb_repl_group->role != REPLICATOR_ROLE_MASTER )
        return USERDB_NOACCESS;
#endif
    
    if ( !userdb_databaseloaded() )
        return USERDB_DBUNAVAILABLE;
    else if ( !maildomainconfig || UDBRIsDeleted(maildomainconfig) )
        return USERDB_BADPARAMETERS;
    else if ( maildomainconfig->databaseops > USERDB_DATABASE_MAXPEROBJECT )
        return USERDB_LOCKED;
    
    if ( (domainflags & UDB_DOMAIN_INUSE) == (maildomainconfig->domainflags & UDB_DOMAIN_INUSE) )
        return USERDB_NOOP;
    
    triggerhook(HOOK_USERDB_PREMODIFYMDC, maildomainconfig);
    
    maildomainconfig->domainflags = domainflags;
    
    maildomainconfig->revision++;
    maildomainconfig->modified = time(NULL);
    maildomainconfig->transactionid = ++userdb_lastmaildomainconfigtransactionid;
    
    userdb_modifymaildomainconfigindb(maildomainconfig);
    
    triggerhook(HOOK_USERDB_POSTMODIFYMDC, maildomainconfig);
    
    return USERDB_OK;
}

int userdb_modifymaildomainconfigmaxlocal(userdb_maildomainconfig *maildomainconfig, unsigned long maxlocal)
{
#ifdef USERDB_ENABLEREPLICATION
    if ( !userdb_repl_group || userdb_repl_group->role != REPLICATOR_ROLE_MASTER )
        return USERDB_NOACCESS;
#endif
    
    if ( !userdb_databaseloaded() )
        return USERDB_DBUNAVAILABLE;
    else if ( !maildomainconfig || UDBRIsDeleted(maildomainconfig) )
        return USERDB_BADPARAMETERS;
    else if ( maxlocal > USERDB_MAXMAILDOMAINCONFIGLOCALLIMIT )
        return USERDB_BADLOCALLIMIT;
    else if ( maildomainconfig->databaseops > USERDB_DATABASE_MAXPEROBJECT )
        return USERDB_LOCKED;
    
    if ( maxlocal == maildomainconfig->maxlocal )
        return USERDB_NOOP;
    
    triggerhook(HOOK_USERDB_PREMODIFYMDC, maildomainconfig);
    
    maildomainconfig->maxlocal = maxlocal;
    
    maildomainconfig->revision++;
    maildomainconfig->modified = time(NULL);
    maildomainconfig->transactionid = ++userdb_lastmaildomainconfigtransactionid;
    
    userdb_modifymaildomainconfigindb(maildomainconfig);
    
    triggerhook(HOOK_USERDB_POSTMODIFYMDC, maildomainconfig);
    
    return USERDB_OK;
}

int userdb_modifymaildomainconfigmaxdomain(userdb_maildomainconfig *maildomainconfig, unsigned long maxdomain)
{
#ifdef USERDB_ENABLEREPLICATION
    if ( !userdb_repl_group || userdb_repl_group->role != REPLICATOR_ROLE_MASTER )
        return USERDB_NOACCESS;
#endif
    
    if ( !userdb_databaseloaded() )
        return USERDB_DBUNAVAILABLE;
    else if ( !maildomainconfig || UDBRIsDeleted(maildomainconfig) )
        return USERDB_BADPARAMETERS;
    else if ( maxdomain > USERDB_MAXMAILDOMAINCONFIGDOMAINLIMIT )
        return USERDB_BADDOMAINLIMIT;
    else if ( maildomainconfig->databaseops > USERDB_DATABASE_MAXPEROBJECT )
        return USERDB_LOCKED;
    
    if ( maxdomain == maildomainconfig->maxdomain )
        return USERDB_NOOP;
    
    triggerhook(HOOK_USERDB_PREMODIFYMDC, maildomainconfig);
    
    maildomainconfig->maxdomain = maxdomain;
    
    maildomainconfig->revision++;
    maildomainconfig->modified = time(NULL);
    maildomainconfig->transactionid = ++userdb_lastmaildomainconfigtransactionid;
    
    userdb_modifymaildomainconfigindb(maildomainconfig);
    
    triggerhook(HOOK_USERDB_POSTMODIFYMDC, maildomainconfig);
    
    return USERDB_OK;
}

int userdb_deletemaildomainconfig(userdb_maildomainconfig *maildomainconfig)
{
userdb_maildomain *tempmaildomain;

#ifdef USERDB_ENABLEREPLICATION
    if ( !userdb_repl_group || userdb_repl_group->role != REPLICATOR_ROLE_MASTER )
        return USERDB_NOACCESS;
#endif
    
    if ( !userdb_databaseloaded() )
        return USERDB_DBUNAVAILABLE;
    else if ( !maildomainconfig || UDBRIsDeleted(maildomainconfig) )
        return USERDB_BADPARAMETERS;
    else if ( maildomainconfig->databaseops > USERDB_DATABASE_MAXPEROBJECT )
        return USERDB_LOCKED;
    
    triggerhook(HOOK_USERDB_DELETEMDC, maildomainconfig);
    
    for ( tempmaildomain = maildomainconfig->maildomain; tempmaildomain; tempmaildomain = tempmaildomain->parent )
        tempmaildomain->configcount--;
    
    userdb_deletemaildomainconfigfromhash(maildomainconfig);
    maildomainconfig->maildomain->config = NULL;
    userdb_cleanmaildomaintree(maildomainconfig->maildomain);
    
    maildomainconfig->maildomain = NULL;
    maildomainconfig->replicationflags |= UDB_REPLICATION_DELETED;
    maildomainconfig->revision++;
    maildomainconfig->modified = time(NULL);
    maildomainconfig->transactionid = ++userdb_lastmaildomainconfigtransactionid;
    
#ifdef USERDB_ENABLEREPLICATION
    userdb_addmaildomainconfigtodeletedhash(maildomainconfig);
    userdb_modifymaildomainconfigindb(maildomainconfig);
#else
    userdb_deletemaildomainconfigfromdb(maildomainconfig);
#endif
    
    return USERDB_OK;
}
