/*
 * UserDB
 */

#include "./userdb.h"

int userdb_createlanguage(char *languagecode, char *languagename, userdb_language **language)
{
userdb_language *newlanguage;
time_t newlanguagetime;

#ifdef USERDB_ENABLEREPLICATION
    if ( !userdb_repl_group || userdb_repl_group->role != REPLICATOR_ROLE_MASTER )
        return USERDB_NOACCESS;
#endif
    
    if ( !userdb_databaseloaded() )
        return USERDB_DBUNAVAILABLE;
    else if ( !languagecode || userdb_zerolengthstring(languagecode) || !languagename || userdb_zerolengthstring(languagename) || !language )
        return USERDB_BADPARAMETERS;
    
    switch ( userdb_checklanguagecode(languagecode) )
    {
        case USERDB_OK:
            break;
        case USERDB_BADLANGUAGECODE:
            return USERDB_BADLANGUAGECODE;
        default:
            return USERDB_ERROR;
    }
    
    switch ( userdb_checklanguagename(languagename) )
    {
        case USERDB_OK:
            break;
        case USERDB_BADLANGUAGENAME:
            return USERDB_BADLANGUAGENAME;
        default:
            return USERDB_ERROR;
    }
    
    switch ( userdb_findlanguagebylanguagecode(languagecode, &newlanguage) )
    {
        case USERDB_OK:
            return USERDB_ALREADYEXISTS;
        case USERDB_NOTFOUND:
            break;
        default:
            return USERDB_ERROR;
    }
    
    newlanguagetime = time(NULL);
    
    newlanguage = userdb_malloclanguage();
    newlanguage->languageid = ++userdb_lastlanguageid;
    newlanguage->languagecode = getsstring(languagecode, USERDB_LANGUAGELEN);
    newlanguage->languagename = getsstring(languagename, USERDB_LANGUAGELEN);
    newlanguage->created = newlanguagetime;
    newlanguage->modified = newlanguagetime;
    newlanguage->revision = 1;
    newlanguage->transactionid = ++userdb_lastlanguagetransactionid;
    userdb_addlanguagetohash(newlanguage);
    userdb_addlanguagetodb(newlanguage);
    
    *language = newlanguage;
    triggerhook(HOOK_USERDB_CREATELANG, newlanguage);
    
    return USERDB_OK;
}

int userdb_changelanguagecode(userdb_language *language, char *languagecode)
{
userdb_language *templanguage;

#ifdef USERDB_ENABLEREPLICATION
    if ( !userdb_repl_group || userdb_repl_group->role != REPLICATOR_ROLE_MASTER )
        return USERDB_NOACCESS;
#endif
    
    if ( !userdb_databaseloaded() )
        return USERDB_DBUNAVAILABLE;
    else if ( !language || !languagecode || userdb_zerolengthstring(languagecode) || UDBRIsDeleted(language) )
        return USERDB_BADPARAMETERS;
    else if ( language->databaseops > USERDB_DATABASE_MAXPEROBJECT )
        return USERDB_LOCKED;
    
    if ( !strcmp(languagecode, language->languagecode->content) )
        return USERDB_NOOP;
    
    switch ( userdb_checklanguagecode(languagecode) )
    {
        case USERDB_OK:
            break;
        case USERDB_BADLANGUAGECODE:
            return USERDB_BADLANGUAGECODE;
        default:
            return USERDB_ERROR;
    }
    
    switch ( userdb_findlanguagebylanguagecode(languagecode, &templanguage) )
    {
        case USERDB_OK:
            return USERDB_ALREADYEXISTS;
        case USERDB_NOTFOUND:
            break;
        default:
            return USERDB_ERROR;
    }
    
    triggerhook(HOOK_USERDB_PREMODIFYLANG, language);
    
    userdb_deletelanguagefromhash(language);
    freesstring(language->languagecode);
    language->languagecode = getsstring(languagecode, USERDB_LANGUAGELEN);
    userdb_addlanguagetohash(language);
    
    language->revision++;
    language->modified = time(NULL);
    language->transactionid = ++userdb_lastlanguagetransactionid;
    
    userdb_modifylanguageindb(language);
    
    triggerhook(HOOK_USERDB_POSTMODIFYLANG, language);
    
    return USERDB_OK;
}

int userdb_changelanguagename(userdb_language *language, char *languagename)
{
#ifdef USERDB_ENABLEREPLICATION
    if ( !userdb_repl_group || userdb_repl_group->role != REPLICATOR_ROLE_MASTER )
        return USERDB_NOACCESS;
#endif
    
    if ( !userdb_databaseloaded() )
        return USERDB_DBUNAVAILABLE;
    else if ( !language || !languagename || userdb_zerolengthstring(languagename) || UDBRIsDeleted(language) )
        return USERDB_BADPARAMETERS;
    else if ( language->databaseops > USERDB_DATABASE_MAXPEROBJECT )
        return USERDB_LOCKED;
    
    if ( !strcmp(languagename, language->languagename->content) )
        return USERDB_NOOP;
    
    switch ( userdb_checklanguagename(languagename) )
    {
        case USERDB_OK:
            break;
        case USERDB_BADLANGUAGENAME:
            return USERDB_BADLANGUAGENAME;
        default:
            return USERDB_ERROR;
    }
    
    triggerhook(HOOK_USERDB_PREMODIFYLANG, language);
    
    freesstring(language->languagename);
    language->languagename = getsstring(languagename, USERDB_LANGUAGELEN);
    
    language->revision++;
    language->modified = time(NULL);
    language->transactionid = ++userdb_lastlanguagetransactionid;
    
    userdb_modifylanguageindb(language);
    
    triggerhook(HOOK_USERDB_POSTMODIFYLANG, language);
    
    return USERDB_OK;
}

int userdb_deletelanguage(userdb_language *language)
{
unsigned int i;
userdb_user *user;

#ifdef USERDB_ENABLEREPLICATION
    if ( !userdb_repl_group || userdb_repl_group->role != REPLICATOR_ROLE_MASTER )
        return USERDB_NOACCESS;
#endif
    
    if ( !userdb_databaseloaded() )
        return USERDB_DBUNAVAILABLE;
    else if ( !language || UDBRIsDeleted(language) )
        return USERDB_BADPARAMETERS;
    else if ( language->databaseops > USERDB_DATABASE_MAXPEROBJECT )
        return USERDB_LOCKED;
    
    triggerhook(HOOK_USERDB_DELETELANG, language);
    
    userdb_deletelanguagefromhash(language);
    
    for ( i = 0; i < USERDB_USERHASHSIZE; i++ )
        for ( user = userdb_userhashtable_id[i]; user; user = user->nextbyid )
            if ( user->languagedata == language )
                user->languagedata = NULL;
    
    language->replicationflags |= UDB_REPLICATION_DELETED;
    language->revision++;
    language->modified = time(NULL);
    language->transactionid = ++userdb_lastlanguagetransactionid;
    
#ifdef USERDB_ENABLEREPLICATION
    userdb_addlanguagetodeletedhash(language);
    userdb_modifylanguageindb(language);
#else
    userdb_deletelanguagefromdb(language);
#endif
    
    return USERDB_OK;
}
