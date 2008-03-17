/*
 * UserDB
 */

#include "./userdb.h"
#include <libpq-fe.h>
#include "../pqsql/pqsql.h"

int userdb_databaseavailable()
{
    return pqconnected();
}

int userdb_databaseloaded()
{
    return (userdb_databaseavailable() && userdb_databasestatus == USERDB_DATABASESTATE_READY);
}

void userdb_initdatabase();
void userdb_loadstatevariables(PGconn *dbconn, void *arg);
void userdb_parselanguagerows(PGconn *dbconn, void *arg);
void userdb_finishedloadinglanguages(PGconn *dbconn, void *arg);
void userdb_parseuserrows(PGconn *dbconn, void *arg);
void userdb_finishedloadingusers(PGconn *dbconn, void *arg);
void userdb_parseusertagsinforows(PGconn *dbconn, void *arg);
void userdb_finishedloadingusertagsinfo(PGconn *dbconn, void *arg);
void userdb_parseusertagsdatarows(PGconn *dbconn, void *arg);
void userdb_finishedloadingusertagsdata(PGconn *dbconn, void *arg);
void userdb_parsemaildomainconfigrows(PGconn *dbconn, void *arg);
void userdb_finishedloadingmaildomainconfigs(PGconn *dbconn, void *arg);

void userdb_initdatabase()
{
    Error("userdb", ERR_INFO, "Starting database load...");
    Error("userdb", ERR_INFO, "Loading state variables table...");
    pqasyncquery(userdb_loadstatevariables, NULL, "SELECT variableid,variablevalue FROM userdb.dbstate");
}

void userdb_loadstatevariables(PGconn *dbconn, void *arg)
{
PGresult *res;
unsigned int numrows, i;

    if ( !dbconn || !(res = PQgetResult(dbconn)) )
    {
        Error("userdb", ERR_ERROR, "No result available for state variables table");
        userdb_databasestatus = USERDB_DATABASESTATE_ERROR;
        return;
    }
    
    if ( PQresultStatus(res) != PGRES_TUPLES_OK && PQresultStatus(res) != PGRES_COMMAND_OK )
    {
        Error("userdb", ERR_ERROR, "Error loading state variables table, query failed: %s : %s", PQresultErrorField(res, PG_DIAG_MESSAGE_PRIMARY), PQresultErrorField(res, PG_DIAG_MESSAGE_DETAIL));
        userdb_databasestatus = USERDB_DATABASESTATE_ERROR;
        PQclear(res);
        return;
    }
    
    numrows = PQntuples(res);
    
    if ( PQnfields(res) != 2 || numrows != 6 )
    {
        Error("userdb", ERR_ERROR, "Error loading state variables table, invalid format");
        userdb_databasestatus = USERDB_DATABASESTATE_ERROR;
        PQclear(res);
        return;
    }
    
    userdb_databasestatus = USERDB_DATABASESTATE_LOADING;
    
    for ( i = 0; i < numrows; i++ )
    {
        switch ( atoi(PQgetvalue(res, i, 0)) )
        {
            case USERDB_VARID_LASTLANGUAGEID:
                userdb_lastlanguageid = strtoull(PQgetvalue(res, i, 1), NULL, 10);
                break;
            case USERDB_VARID_LASTLANGUAGETID:
                userdb_lastlanguagetransactionid = strtoull(PQgetvalue(res, i, 1), NULL, 10);
                break;
            case USERDB_VARID_LASTUSERID:
                userdb_lastuserid = strtoull(PQgetvalue(res, i, 1), NULL, 10);
                break;
            case USERDB_VARID_LASTUSERTID:
                userdb_lastusertransactionid = strtoull(PQgetvalue(res, i, 1), NULL, 10);
                break;
            case USERDB_VARID_LASTMAILDOMAINID:
                userdb_lastmaildomainconfigid = strtoull(PQgetvalue(res, i, 1), NULL, 10);
                break;
            case USERDB_VARID_LASTMAILDOMAINTID:
                userdb_lastmaildomainconfigtransactionid = strtoull(PQgetvalue(res, i, 1), NULL, 10);
                break;
            default:
                Error("userdb", ERR_ERROR, "Error loading state variables table, unknown variable ID loaded from database.");
                userdb_lastlanguageid = 0;
                userdb_lastlanguagetransactionid = 0;
                userdb_lastuserid = 0;
                userdb_lastusertransactionid = 0;
                userdb_lastmaildomainconfigid = 0;
                userdb_lastmaildomainconfigtransactionid = 0;
                PQclear(res);
                userdb_databasestatus = USERDB_DATABASESTATE_ERROR;
                return;
        }
    }
    
    PQclear(res);
    Error("userdb", ERR_INFO, "State variables table loaded.");
    pqloadtable("userdb.languages", NULL, userdb_parselanguagerows, userdb_finishedloadinglanguages);
}

void userdb_parselanguagerows(PGconn *dbconn, void *arg)
{
unsigned int numrows, i;
PGresult *res;
userdb_language *language;

    if ( !dbconn || !(res = PQgetResult(dbconn)) )
    {
        Error("userdb", ERR_ERROR, "No result available for languages table");
        userdb_databasestatus = USERDB_DATABASESTATE_ERROR;
        return;
    }
    
    if ( PQresultStatus(res) != PGRES_TUPLES_OK && PQresultStatus(res) != PGRES_COMMAND_OK )
    {
        Error("userdb", ERR_ERROR, "Error loading languages table, query failed: %s : %s", PQresultErrorField(res, PG_DIAG_MESSAGE_PRIMARY), PQresultErrorField(res, PG_DIAG_MESSAGE_DETAIL));
        userdb_databasestatus = USERDB_DATABASESTATE_ERROR;
        PQclear(res);
        return;
    }
    
    if ( PQnfields(res) != 8 )
    {
        Error("userdb", ERR_ERROR, "Error loading languages table, invalid format");
        userdb_databasestatus = USERDB_DATABASESTATE_ERROR;
        PQclear(res);
        return;
    }
    
    numrows = PQntuples(res);
    
    for ( i = 0; i < numrows; i++ )
    {
        language = userdb_malloclanguage();
        language->languageid = strtoul(PQgetvalue(res, i, 0), NULL, 10);
        language->languagecode = getsstring(PQgetvalue(res, i, 1), USERDB_LANGUAGELEN);
        language->languagename = getsstring(PQgetvalue(res, i, 2), USERDB_LANGUAGELEN);
        language->replicationflags = strtoul(PQgetvalue(res, i, 3), NULL, 10);
        language->created = strtol(PQgetvalue(res, i, 4), NULL, 10);
        language->modified = strtol(PQgetvalue(res, i, 5), NULL, 10);
        language->revision = strtoul(PQgetvalue(res, i, 6), NULL, 10);
        language->transactionid = strtoull(PQgetvalue(res, i, 7), NULL, 10);
        
        if ( UDBRIsDeleted(language) )
#ifdef USERDB_ENABLEREPLICATION
            userdb_addlanguagetodeletedhash(language);
#else
            assert(0);
#endif
        else
            userdb_addlanguagetohash(language);
    }
    
    PQclear(res);
}

void userdb_finishedloadinglanguages(PGconn *dbconn, void *arg)
{
    Error("userdb", ERR_INFO, "Languages table loaded.");
    pqloadtable("userdb.users", NULL, userdb_parseuserrows, userdb_finishedloadingusers);
}

void userdb_parseuserrows(PGconn *dbconn, void *arg)
{
unsigned char *password;
size_t passwordlen;
unsigned int numrows, i;
PGresult *res;
userdb_user *user;

    if ( !dbconn || !(res = PQgetResult(dbconn)) )
    {
        Error("userdb", ERR_ERROR, "No result available for users table");
        userdb_databasestatus = USERDB_DATABASESTATE_ERROR;
        return;
    }
    
    if ( PQresultStatus(res) != PGRES_TUPLES_OK && PQresultStatus(res) != PGRES_COMMAND_OK )
    {
        Error("userdb", ERR_ERROR, "Error loading users table, query failed: %s : %s", PQresultErrorField(res, PG_DIAG_MESSAGE_PRIMARY), PQresultErrorField(res, PG_DIAG_MESSAGE_DETAIL));
        userdb_databasestatus = USERDB_DATABASESTATE_ERROR;
        PQclear(res);
        return;
    }
    
    if ( PQnfields(res) != 19 )
    {
        Error("userdb", ERR_ERROR, "Error loading users table, invalid format");
        userdb_databasestatus = USERDB_DATABASESTATE_ERROR;
        PQclear(res);
        return;
    }
    
    numrows = PQntuples(res);
    
    for ( i = 0; i < numrows; i++ )
    {
        user = userdb_mallocuser();
        user->userid = strtoul(PQgetvalue(res, i, 0), NULL, 10);
        user->username = getsstring(PQgetvalue(res, i, 1), USERDB_MAXUSERNAMELEN);
        
        password = PQunescapeBytea((unsigned char *)PQgetvalue(res, i, 2), &passwordlen);
        
        if ( password && passwordlen == USERDB_PASSWORDHASHLEN )
        {
            memcpy(user->password, password, USERDB_PASSWORDHASHLEN);
            PQfreemem(password);
        }
        else
        {
            Error("userdb", ERR_WARNING, "Unable to load password for %s (%lu)", user->username->content, user->userid);
            
            if ( password )
                PQfreemem(password);
        }
        
        user->languageid = strtoul(PQgetvalue(res, i, 3), NULL, 10);
        user->languagedata = NULL;
        user->emaillocal = getsstring(PQgetvalue(res, i, 4), USERDB_EMAILLEN);
        
        if ( !ircd_strcmp(PQgetvalue(res, i, 5), USERDB_NOMAILDOMAINSYMBOL) )
        {
            user->maildomain = NULL;
        }
        else
        {
            switch ( userdb_findmaildomainbyemaildomain(PQgetvalue(res, i, 5), &(user->maildomain), 1) )
            {
                case USERDB_OK:
                    userdb_addusertomaildomain(user, user->maildomain);
                    break;
                default:
                    Error("userdb", ERR_ERROR, "Error loading users table, unable to get mail domain for user.");
                    userdb_databasestatus = USERDB_DATABASESTATE_ERROR;
                    return;
            }
        }
        
        user->statusflags = strtoul(PQgetvalue(res, i, 6), NULL, 10);
        user->groupflags = strtoul(PQgetvalue(res, i, 7), NULL, 10);
        user->optionflags = strtoul(PQgetvalue(res, i, 8), NULL, 10);
        user->noticeflags = strtoul(PQgetvalue(res, i, 9), NULL, 10);
        
        if ( UDBOIsSuspended(user) )
        {
            user->suspensiondata = userdb_mallocsuspension();
            user->suspensiondata->suspendstart = strtol(PQgetvalue(res, i, 10), NULL, 10);
            user->suspensiondata->suspendend = strtol(PQgetvalue(res, i, 11), NULL, 10);
            user->suspensiondata->reason = getsstring(PQgetvalue(res, i, 12), USERDB_SUSPENDREASONLEN);
            user->suspensiondata->suspenderid = strtoul(PQgetvalue(res, i, 13), NULL, 10);
            user->suspensiondata->suspenderdata = NULL;
        }
        
        user->replicationflags = strtoul(PQgetvalue(res, i, 14), NULL, 10);
        user->created = strtol(PQgetvalue(res, i, 15), NULL, 10);
        user->modified = strtol(PQgetvalue(res, i, 16), NULL, 10);
        user->revision = strtoul(PQgetvalue(res, i, 17), NULL, 10);
        user->transactionid = strtoull(PQgetvalue(res, i, 18), NULL, 10);
        
        if ( UDBRIsDeleted(user) )
        {
#ifdef USERDB_ENABLEREPLICATION
            userdb_addusertodeletedhash(user);
#else
            assert(0);
#endif
        }
        else
        {
            userdb_addusertohash(user);
            
            if ( UDBOIsSuspended(user) )
                userdb_addusertosuspendedhash(user);
            
            userdb_assignusertoauth(user);
        }
    }
    
    PQclear(res);
}

void userdb_finishedloadingusers(PGconn *dbconn, void *arg)
{
unsigned int i;
userdb_user *user;

    Error("userdb", ERR_INFO, "Users table loaded.");
    Error("userdb", ERR_INFO, "Mapping languages and suspenders to users...");
    
    for ( i = 0; i < USERDB_USERHASHSIZE; i++ )
    {
        for ( user = userdb_userhashtable_id[i]; user; user = user->nextbyid )
        {
            if ( user->languageid )
            {
                switch ( userdb_findlanguagebylanguageid(user->languageid, &(user->languagedata)) )
                {
                    case USERDB_OK:
                        break;
                    case USERDB_NOTFOUND:
                        user->languagedata = NULL;
                        break;
                    default:
                        Error("userdb", ERR_ERROR, "Error matching user to language, error searching for language.");
                        userdb_databasestatus = USERDB_DATABASESTATE_ERROR;
                        return;
                }
            }
            
            if ( UDBOIsSuspended(user) )
            {
                if ( user->suspensiondata->suspenderid )
                {
                    switch ( userdb_finduserbyuserid(user->suspensiondata->suspenderid, &(user->suspensiondata->suspenderdata)) )
                    {
                        case USERDB_OK:
                            break;
                        case USERDB_NOTFOUND:
                            user->suspensiondata->suspenderdata = NULL;
                            break;
                        default:
                            Error("userdb", ERR_ERROR, "Error matching suspension to suspender, error searching for suspender.");
                            userdb_databasestatus = USERDB_DATABASESTATE_ERROR;
                            return;
                    }
                }
                else
                {
                    user->suspensiondata->suspenderdata = NULL;
                }
            }
        }
    }
    
    Error("userdb", ERR_INFO, "Mapping complete.");
    pqloadtable("userdb.usertagsinfo", NULL, userdb_parseusertagsinforows, userdb_finishedloadingusertagsinfo);
}

void userdb_parseusertagsinforows(PGconn *dbconn, void *arg)
{
unsigned int numrows, i;
PGresult *res;
userdb_usertaginfo *usertaginfo;

    if ( !dbconn || !(res = PQgetResult(dbconn)) )
    {
        Error("userdb", ERR_ERROR, "No result available for user tags information table");
        userdb_databasestatus = USERDB_DATABASESTATE_ERROR;
        return;
    }
    
    if ( PQresultStatus(res) != PGRES_TUPLES_OK && PQresultStatus(res) != PGRES_COMMAND_OK )
    {
        Error("userdb", ERR_ERROR, "Error loading user tags information table, query failed: %s : %s", PQresultErrorField(res, PG_DIAG_MESSAGE_PRIMARY), PQresultErrorField(res, PG_DIAG_MESSAGE_DETAIL));
        userdb_databasestatus = USERDB_DATABASESTATE_ERROR;
        PQclear(res);
        return;
    }
    
    if ( PQnfields(res) != 3 )
    {
        Error("userdb", ERR_ERROR, "Error loading user tags information table, invalid format");
        userdb_databasestatus = USERDB_DATABASESTATE_ERROR;
        PQclear(res);
        return;
    }
    
    numrows = PQntuples(res);
    
    for ( i = 0; i < numrows; i++ )
    {
        usertaginfo = userdb_mallocusertaginfo();
        usertaginfo->tagid = strtoul(PQgetvalue(res, i, 0), NULL, 10);
        usertaginfo->tagtype = strtoul(PQgetvalue(res, i, 1), NULL, 10);
        usertaginfo->tagname = getsstring(PQgetvalue(res, i, 2), USERDB_USERTAGNAMELEN);
        userdb_addusertaginfotohash(usertaginfo);
    }
    
    PQclear(res);
}

void userdb_finishedloadingusertagsinfo(PGconn *dbconn, void *arg)
{
    Error("userdb", ERR_INFO, "User tags information table loaded.");
    pqloadtable("userdb.usertagsdata", NULL, userdb_parseusertagsdatarows, userdb_finishedloadingusertagsdata);
}

void userdb_parseusertagsdatarows(PGconn *dbconn, void *arg)
{
unsigned int numrows, i;
PGresult *res;
unsigned char *stringdata;
unsigned long userid, stringsize, correctstringsize;
unsigned short usertaginfoid;
userdb_user *user;
userdb_usertaginfo *usertaginfo;
userdb_usertagdata *usertagdata;

    if ( !dbconn || !(res = PQgetResult(dbconn)) )
    {
        Error("userdb", ERR_ERROR, "No result available for user tags data table");
        userdb_databasestatus = USERDB_DATABASESTATE_ERROR;
        return;
    }
    
    if ( PQresultStatus(res) != PGRES_TUPLES_OK && PQresultStatus(res) != PGRES_COMMAND_OK )
    {
        Error("userdb", ERR_ERROR, "Error loading user tag data table, query failed: %s : %s", PQresultErrorField(res, PG_DIAG_MESSAGE_PRIMARY), PQresultErrorField(res, PG_DIAG_MESSAGE_DETAIL));
        userdb_databasestatus = USERDB_DATABASESTATE_ERROR;
        PQclear(res);
        return;
    }
    
    if ( PQnfields(res) != 3 )
    {
        Error("userdb", ERR_ERROR, "Error loading user tag data table, invalid format");
        userdb_databasestatus = USERDB_DATABASESTATE_ERROR;
        PQclear(res);
        return;
    }
    
    numrows = PQntuples(res);
    
    for ( i = 0; i < numrows; i++ )
    {
        usertaginfoid = strtoul(PQgetvalue(res, i, 0), NULL, 10);
        userid = strtoul(PQgetvalue(res, i, 1), NULL, 10);
        
        if ( userdb_findusertaginfo(usertaginfoid, &usertaginfo) != USERDB_OK )
        {
            Error("userdb", ERR_WARNING, "Could not find user tag info for user tag data (%hu,%lu)", usertaginfoid, userid);
            continue;
        }
        
        if ( userdb_finduserbyuserid(userid, &user) != USERDB_OK )
        {
            Error("userdb", ERR_WARNING, "Could not find user for user tag data (%hu,%lu)", usertaginfoid, userid);
            continue;
        }
        
        usertagdata = userdb_mallocusertagdata();
        usertagdata->taginfo = usertaginfo;
        usertagdata->userinfo = user;
        
        stringdata = PQunescapeBytea((unsigned char *)PQgetvalue(res, i, 2), &stringsize);
        
        switch ( usertaginfo->tagtype )
        {
            case USERDB_TAGTYPE_CHAR:
                correctstringsize = sizeof(unsigned char);
                break;
            case USERDB_TAGTYPE_SHORT:
                correctstringsize = sizeof(unsigned short);
                break;
            case USERDB_TAGTYPE_INT:
                correctstringsize = sizeof(unsigned int);
                break;
            case USERDB_TAGTYPE_LONG:
                correctstringsize = sizeof(unsigned long);
                break;
            case USERDB_TAGTYPE_LONGLONG:
                correctstringsize = sizeof(unsigned long long);
                break;
            case USERDB_TAGTYPE_FLOAT:
                correctstringsize = sizeof(float);
                break;
            case USERDB_TAGTYPE_DOUBLE:
                correctstringsize = sizeof(double);
                break;
            case USERDB_TAGTYPE_STRING:
                correctstringsize = stringsize;
                break;
            default:
                Error("userdb", ERR_ERROR, "Invalid tag type for user tag data (%d,%hu,%lu)", usertaginfo->tagtype, usertaginfo->tagid, user->userid);
                userdb_databasestatus = USERDB_DATABASESTATE_ERROR;
                userdb_freeusertagdata(usertagdata);
                PQfreemem(stringdata);
                PQclear(res);
                return;
        }
        
        if ( ( stringsize != correctstringsize ) || ( stringsize > USERDB_MAXUSERTAGSTRINGLEN ) )
        {
            Error("userdb", ERR_ERROR, "Invalid data size for user tag data (%d,%hu,%lu)", usertaginfo->tagtype, usertaginfo->tagid, user->userid);
            userdb_databasestatus = USERDB_DATABASESTATE_ERROR;
            userdb_freeusertagdata(usertagdata);
            PQfreemem(stringdata);
            PQclear(res);
            return;
        }
        
        switch ( usertaginfo->tagtype )
        {
            case USERDB_TAGTYPE_CHAR:
                memcpy(&((usertagdata->tagdata).tagchar), stringdata, sizeof(unsigned char));
                break;
            case USERDB_TAGTYPE_SHORT:
                memcpy(&((usertagdata->tagdata).tagshort), stringdata, sizeof(unsigned short));
                break;
            case USERDB_TAGTYPE_INT:
                memcpy(&((usertagdata->tagdata).tagint), stringdata, sizeof(unsigned int));
                break;
            case USERDB_TAGTYPE_LONG:
                memcpy(&((usertagdata->tagdata).taglong), stringdata, sizeof(unsigned long));
                break;
            case USERDB_TAGTYPE_LONGLONG:
                memcpy(&((usertagdata->tagdata).taglonglong), stringdata, sizeof(unsigned long long));
                break;
            case USERDB_TAGTYPE_FLOAT:
                memcpy(&((usertagdata->tagdata).tagfloat), stringdata, sizeof(float));
                break;
            case USERDB_TAGTYPE_DOUBLE:
                memcpy(&((usertagdata->tagdata).tagdouble), stringdata, sizeof(double));
                break;
            case USERDB_TAGTYPE_STRING:
                (usertagdata->tagdata).tagstring.stringsize = ((unsigned short)stringsize);
                
                if ( userdb_getusertagdatastring((usertagdata->tagdata).tagstring.stringsize, &((usertagdata->tagdata).tagstring.stringdata)) != USERDB_OK )
                {
                    Error("userdb", ERR_WARNING, "String allocation failed for usertagdata (%hu,%lu)", usertaginfo->tagid, user->userid);
                    userdb_databasestatus = USERDB_DATABASESTATE_ERROR;
                    userdb_freeusertagdata(usertagdata);
                    PQfreemem(stringdata);
                    PQclear(res);
                    return;
                }
                
                memcpy((usertagdata->tagdata).tagstring.stringdata, stringdata, (usertagdata->tagdata).tagstring.stringsize);
                break;
            default:
                Error("userdb", ERR_ERROR, "Invalid tag type for usertagdata (%d,%hu,%lu)", usertaginfo->tagtype, usertaginfo->tagid, user->userid);
                userdb_databasestatus = USERDB_DATABASESTATE_ERROR;
                userdb_freeusertagdata(usertagdata);
                PQfreemem(stringdata);
                PQclear(res);
                return;
        }
        
        PQfreemem(stringdata);
        userdb_addusertagdatatohash(usertagdata);
    }
    
    PQclear(res);
}

void userdb_finishedloadingusertagsdata(PGconn *dbconn, void *arg)
{
    Error("userdb", ERR_INFO, "User tags data table loaded.");
    pqloadtable("userdb.maildomainconfigs", NULL, userdb_parsemaildomainconfigrows, userdb_finishedloadingmaildomainconfigs);
}

void userdb_parsemaildomainconfigrows(PGconn *dbconn, void *arg)
{
unsigned int numrows, i;
PGresult *res;
userdb_maildomain *maildomain, *tempmaildomain;
userdb_maildomainconfig *maildomainconfig;

    if ( !dbconn || !(res = PQgetResult(dbconn)) )
    {
        Error("userdb", ERR_ERROR, "No result available for mail domain configurations table");
        userdb_databasestatus = USERDB_DATABASESTATE_ERROR;
        return;
    }
    
    if ( PQresultStatus(res) != PGRES_TUPLES_OK && PQresultStatus(res) != PGRES_COMMAND_OK )
    {
        Error("userdb", ERR_ERROR, "Error loading mail domain configurations table, query failed: %s : %s", PQresultErrorField(res, PG_DIAG_MESSAGE_PRIMARY), PQresultErrorField(res, PG_DIAG_MESSAGE_DETAIL));
        userdb_databasestatus = USERDB_DATABASESTATE_ERROR;
        PQclear(res);
        return;
    }
    
    if ( PQnfields(res) != 10 )
    {
        Error("userdb", ERR_ERROR, "Error loading mail domain configurations table, invalid format");
        userdb_databasestatus = USERDB_DATABASESTATE_ERROR;
        PQclear(res);
        return;
    }
    
    numrows = PQntuples(res);
    
    for ( i = 0; i < numrows; i++ )
    {
        maildomainconfig = userdb_mallocmaildomainconfig();
        maildomainconfig->configid = strtoul(PQgetvalue(res, i, 0), NULL, 10);
        maildomainconfig->configname = getsstring(PQgetvalue(res, i, 1), USERDB_EMAILLEN);
        maildomainconfig->domainflags = strtoul(PQgetvalue(res, i, 2), NULL, 10);
        maildomainconfig->maxlocal = strtoul(PQgetvalue(res, i, 3), NULL, 10);
        maildomainconfig->maxdomain = strtoul(PQgetvalue(res, i, 4), NULL, 10);
        maildomainconfig->replicationflags = strtoul(PQgetvalue(res, i, 5), NULL, 10);
        maildomainconfig->created = strtol(PQgetvalue(res, i, 6), NULL, 10);
        maildomainconfig->modified = strtol(PQgetvalue(res, i, 7), NULL, 10);
        maildomainconfig->revision = strtoul(PQgetvalue(res, i, 8), NULL, 10);
        maildomainconfig->transactionid = strtoull(PQgetvalue(res, i, 9), NULL, 10);
        
        if ( UDBRIsDeleted(maildomainconfig) )
        {
#ifdef USERDB_ENABLEREPLICATION
            userdb_addmaildomainconfigtodeletedhash(maildomainconfig);
#else
            assert(0);
#endif
        }
        else
        {
            switch ( userdb_findmaildomainbyemaildomain(maildomainconfig->configname->content, &maildomain, 1) )
            {
                case USERDB_OK:
                    break;
                default:
                    Error("userdb", ERR_ERROR, "Error loading mail domain configurations table, unable to get mail domain.");
                    userdb_databasestatus = USERDB_DATABASESTATE_ERROR;
                    return;
            }
            
            maildomain->config = maildomainconfig;
            maildomainconfig->maildomain = maildomain;
            
            for ( tempmaildomain = maildomain; tempmaildomain; tempmaildomain = tempmaildomain->parent )
                tempmaildomain->configcount++;
            
            userdb_addmaildomainconfigtohash(maildomainconfig);
        }
    }
    
    PQclear(res);
}

void userdb_finishedloadingmaildomainconfigs(PGconn *dbconn, void *arg)
{
    Error("userdb", ERR_INFO, "Mail domain configurations table loaded.");
    
    userdb_databasestatus = USERDB_DATABASESTATE_READY;
    Error("userdb", ERR_INFO, "Database loaded successfully.");
    triggerhook(HOOK_USERDB_DBLOADED, NULL);
    
#ifdef USERDB_ENABLEREPLICATION
    if ( replicator_enablegroup(userdb_repl_group) == REPLICATOR_OK )
        Error("userdb", ERR_INFO, "Replication group enabled.");
    else
        Error("userdb", ERR_WARNING, "Failed to enable replication group.");
#endif
}

void userdb_handlenullresult(PGconn *dbconn, void *arg)
{
PGresult *res;

    if ( !dbconn || !(res = PQgetResult(dbconn)) )
    {
        Error("userdb", ERR_FATAL, "Invalid call for userdb_handlenullresult");
        assert(0);
        return;
    }
    
    if ( PQresultStatus(res) != PGRES_TUPLES_OK && PQresultStatus(res) != PGRES_COMMAND_OK )
    {
        Error("userdb", ERR_FATAL, "Error running null result query: %s : %s", PQresultErrorField(res, PG_DIAG_MESSAGE_PRIMARY), PQresultErrorField(res, PG_DIAG_MESSAGE_DETAIL));
        userdb_databasestatus = USERDB_DATABASESTATE_ERROR;
        PQclear(res);
        assert(0);
        return;
    }
    
    PQclear(res);
}

void userdb_handleaddlanguagecommit(PGconn *dbconn, void *arg)
{
userdb_language *language;
PGresult *res;

    if ( !dbconn || !(language = (userdb_language *)arg) || !(res = PQgetResult(dbconn)) )
    {
        Error("userdb", ERR_FATAL, "Invalid call for userdb_handleaddlanguagecommit");
        assert(0);
        return;
    }
    
    userdb_languagedatabaseaddcount--;
    language->databaseops--;
    
    if ( PQresultStatus(res) != PGRES_TUPLES_OK && PQresultStatus(res) != PGRES_COMMAND_OK )
    {
        Error("userdb", ERR_FATAL, "Error adding language: %s : %s", PQresultErrorField(res, PG_DIAG_MESSAGE_PRIMARY), PQresultErrorField(res, PG_DIAG_MESSAGE_DETAIL));
        userdb_databasestatus = USERDB_DATABASESTATE_ERROR;
        PQclear(res);
        assert(0);
        return;
    }
    
#ifdef USERDB_ENABLEREPLICATION
    userdb_languagedbchange(language, REPLICATOR_ACTION_ADD);
#endif
    PQclear(res);
}

void userdb_handlemodifylanguagecommit(PGconn *dbconn, void *arg)
{
userdb_language *language;
PGresult *res;

    if ( !dbconn || !(language = (userdb_language *)arg) || !(res = PQgetResult(dbconn)) )
    {
        Error("userdb", ERR_FATAL, "Invalid call for userdb_handlemodifylanguagecommit");
        assert(0);
        return;
    }
    
    userdb_languagedatabasemodifycount--;
    language->databaseops--;
    
    if ( PQresultStatus(res) != PGRES_TUPLES_OK && PQresultStatus(res) != PGRES_COMMAND_OK )
    {
        Error("userdb", ERR_FATAL, "Error modifying language: %s : %s", PQresultErrorField(res, PG_DIAG_MESSAGE_PRIMARY), PQresultErrorField(res, PG_DIAG_MESSAGE_DETAIL));
        userdb_databasestatus = USERDB_DATABASESTATE_ERROR;
        PQclear(res);
        assert(0);
        return;
    }
    
#ifdef USERDB_ENABLEREPLICATION
    userdb_languagedbchange(language, REPLICATOR_ACTION_MODIFY);
#endif
    PQclear(res);
}

void userdb_handledeletelanguagecommit(PGconn *dbconn, void *arg)
{
userdb_language *language;
PGresult *res;

    if ( !dbconn || !(language = (userdb_language *)arg) || !(res = PQgetResult(dbconn)) )
    {
        Error("userdb", ERR_FATAL, "Invalid call for userdb_handledeletelanguagecommit");
        assert(0);
        return;
    }
    
    userdb_languagedatabasedeletecount--;
    language->databaseops--;
    
    if ( PQresultStatus(res) != PGRES_TUPLES_OK && PQresultStatus(res) != PGRES_COMMAND_OK )
    {
        Error("userdb", ERR_FATAL, "Error deleting language: %s : %s", PQresultErrorField(res, PG_DIAG_MESSAGE_PRIMARY), PQresultErrorField(res, PG_DIAG_MESSAGE_DETAIL));
        userdb_databasestatus = USERDB_DATABASESTATE_ERROR;
        PQclear(res);
        assert(0);
        return;
    }
    
#ifdef USERDB_ENABLEREPLICATION
    userdb_languagedbchange(language, REPLICATOR_ACTION_DELETE);
    userdb_deletelanguagefromdeletedhash(language);
#endif
    userdb_freelanguage(language);
    PQclear(res);
}

void userdb_handleaddusercommit(PGconn *dbconn, void *arg)
{
userdb_user *user;
PGresult *res;

    if ( !dbconn || !(user = (userdb_user *)arg) || !(res = PQgetResult(dbconn)) )
    {
        Error("userdb", ERR_FATAL, "Invalid call for userdb_handleaddusercommit");
        assert(0);
        return;
    }
    
    userdb_userdatabaseaddcount--;
    user->databaseops--;
    
    if ( PQresultStatus(res) != PGRES_TUPLES_OK && PQresultStatus(res) != PGRES_COMMAND_OK )
    {
        Error("userdb", ERR_FATAL, "Error adding user: %s : %s", PQresultErrorField(res, PG_DIAG_MESSAGE_PRIMARY), PQresultErrorField(res, PG_DIAG_MESSAGE_DETAIL));
        userdb_databasestatus = USERDB_DATABASESTATE_ERROR;
        PQclear(res);
        assert(0);
        return;
    }
    
#ifdef USERDB_ENABLEREPLICATION
    userdb_userdbchange(user, REPLICATOR_ACTION_ADD);
#endif
    PQclear(res);
}

void userdb_handlemodifyusercommit(PGconn *dbconn, void *arg)
{
userdb_user *user;
PGresult *res;

    if ( !dbconn || !(user = (userdb_user *)arg) || !(res = PQgetResult(dbconn)) )
    {
        Error("userdb", ERR_FATAL, "Invalid call for userdb_handlemodifyusercommit");
        assert(0);
        return;
    }
    
    userdb_userdatabasemodifycount--;
    user->databaseops--;
    
    if ( PQresultStatus(res) != PGRES_TUPLES_OK && PQresultStatus(res) != PGRES_COMMAND_OK )
    {
        Error("userdb", ERR_FATAL, "Error modifying user: %s : %s", PQresultErrorField(res, PG_DIAG_MESSAGE_PRIMARY), PQresultErrorField(res, PG_DIAG_MESSAGE_DETAIL));
        userdb_databasestatus = USERDB_DATABASESTATE_ERROR;
        PQclear(res);
        assert(0);
        return;
    }
    
#ifdef USERDB_ENABLEREPLICATION
    userdb_userdbchange(user, REPLICATOR_ACTION_MODIFY);
#endif
    PQclear(res);
}

void userdb_handledeleteusercommit(PGconn *dbconn, void *arg)
{
userdb_user *user;
PGresult *res;

    if ( !dbconn || !(user = (userdb_user *)arg) || !(res = PQgetResult(dbconn)) )
    {
        Error("userdb", ERR_FATAL, "Invalid call for userdb_handledeleteusercommit");
        assert(0);
        return;
    }
    
    userdb_userdatabasedeletecount--;
    user->databaseops--;
    
    if ( PQresultStatus(res) != PGRES_TUPLES_OK && PQresultStatus(res) != PGRES_COMMAND_OK )
    {
        Error("userdb", ERR_FATAL, "Error deleting user: %s : %s", PQresultErrorField(res, PG_DIAG_MESSAGE_PRIMARY), PQresultErrorField(res, PG_DIAG_MESSAGE_DETAIL));
        userdb_databasestatus = USERDB_DATABASESTATE_ERROR;
        PQclear(res);
        assert(0);
        return;
    }
    
#ifdef USERDB_ENABLEREPLICATION
    userdb_userdbchange(user, REPLICATOR_ACTION_DELETE);
    userdb_deleteuserfromdeletedhash(user);
#endif
    userdb_freeuser(user);
    PQclear(res);
}

void userdb_handleaddmaildomainconfigcommit(PGconn *dbconn, void *arg)
{
userdb_maildomainconfig *maildomainconfig;
PGresult *res;

    if ( !dbconn || !(maildomainconfig = (userdb_maildomainconfig *)arg) || !(res = PQgetResult(dbconn)) )
    {
        Error("userdb", ERR_FATAL, "Invalid call for userdb_handleaddmaildomainconfigcommit");
        assert(0);
        return;
    }
    
    userdb_maildomainconfigdatabaseaddcount--;
    maildomainconfig->databaseops--;
    
    if ( PQresultStatus(res) != PGRES_TUPLES_OK && PQresultStatus(res) != PGRES_COMMAND_OK )
    {
        Error("userdb", ERR_FATAL, "Error adding mail domain config: %s : %s", PQresultErrorField(res, PG_DIAG_MESSAGE_PRIMARY), PQresultErrorField(res, PG_DIAG_MESSAGE_DETAIL));
        userdb_databasestatus = USERDB_DATABASESTATE_ERROR;
        PQclear(res);
        assert(0);
        return;
    }
    
#ifdef USERDB_ENABLEREPLICATION
    userdb_maildomainconfigdbchange(maildomainconfig, REPLICATOR_ACTION_ADD);
#endif
    PQclear(res);
}

void userdb_handlemodifymaildomainconfigcommit(PGconn *dbconn, void *arg)
{
userdb_maildomainconfig *maildomainconfig;
PGresult *res;

    if ( !dbconn || !(maildomainconfig = (userdb_maildomainconfig *)arg) || !(res = PQgetResult(dbconn)) )
    {
        Error("userdb", ERR_FATAL, "Invalid call for userdb_handlemodifymaildomainconfigcommit");
        assert(0);
        return;
    }
    
    userdb_maildomainconfigdatabasemodifycount--;
    maildomainconfig->databaseops--;
    
    if ( PQresultStatus(res) != PGRES_TUPLES_OK && PQresultStatus(res) != PGRES_COMMAND_OK )
    {
        Error("userdb", ERR_FATAL, "Error modifying mail domain config: %s : %s", PQresultErrorField(res, PG_DIAG_MESSAGE_PRIMARY), PQresultErrorField(res, PG_DIAG_MESSAGE_DETAIL));
        userdb_databasestatus = USERDB_DATABASESTATE_ERROR;
        PQclear(res);
        assert(0);
        return;
    }
    
#ifdef USERDB_ENABLEREPLICATION
    userdb_maildomainconfigdbchange(maildomainconfig, REPLICATOR_ACTION_MODIFY);
#endif
    PQclear(res);
}

void userdb_handledeletemaildomainconfigcommit(PGconn *dbconn, void *arg)
{
userdb_maildomainconfig *maildomainconfig;
PGresult *res;

    if ( !dbconn || !(maildomainconfig = (userdb_maildomainconfig *)arg) || !(res = PQgetResult(dbconn)) )
    {
        Error("userdb", ERR_FATAL, "Invalid call for userdb_handledeletemaildomainconfigcommit");
        assert(0);
        return;
    }
    
    userdb_maildomainconfigdatabasedeletecount--;
    maildomainconfig->databaseops--;
    
    if ( PQresultStatus(res) != PGRES_TUPLES_OK && PQresultStatus(res) != PGRES_COMMAND_OK )
    {
        Error("userdb", ERR_FATAL, "Error deleting mail domain config: %s : %s", PQresultErrorField(res, PG_DIAG_MESSAGE_PRIMARY), PQresultErrorField(res, PG_DIAG_MESSAGE_DETAIL));
        userdb_databasestatus = USERDB_DATABASESTATE_ERROR;
        PQclear(res);
        assert(0);
        return;
    }
    
#ifdef USERDB_ENABLEREPLICATION
    userdb_maildomainconfigdbchange(maildomainconfig, REPLICATOR_ACTION_DELETE);
    userdb_deletemaildomainconfigfromdeletedhash(maildomainconfig);
#endif
    userdb_freemaildomainconfig(maildomainconfig);
    PQclear(res);
}

void userdb_addlanguagetodb(userdb_language *language)
{
char escape_languagecode[(USERDB_LANGUAGELEN * 2) + 1], escape_languagename[(USERDB_LANGUAGELEN * 2) + 1];

    if ( !userdb_databaseloaded() || !language )
    {
        Error("userdb", ERR_FATAL, "Invalid call for userdb_addlanguagetodb");
        assert(0);
        return;
    }
    
    PQescapeString(escape_languagecode, language->languagecode->content, USERDB_LANGUAGELEN);
    escape_languagecode[(USERDB_LANGUAGELEN * 2)] = '\0';
    
    PQescapeString(escape_languagename, language->languagename->content, USERDB_LANGUAGELEN);
    escape_languagename[(USERDB_LANGUAGELEN * 2)] = '\0';
    
    userdb_languagedatabaseaddcount++;
    language->databaseops++;
    
    pqasyncquery(userdb_handleaddlanguagecommit, language, "SELECT userdb.addlanguage(%lu,'%s','%s',%hu,%ld,%ld,%lu,%llu)", 
    language->languageid, escape_languagecode, escape_languagename, language->replicationflags, language->created, 
    language->modified, language->revision, language->transactionid);
}

void userdb_modifylanguageindb(userdb_language *language)
{
char escape_languagecode[(USERDB_LANGUAGELEN * 2) + 1], escape_languagename[(USERDB_LANGUAGELEN * 2) + 1];

    if ( !userdb_databaseloaded() || !language )
    {
        Error("userdb", ERR_FATAL, "Invalid call for userdb_modifylanguageindb");
        assert(0);
        return;
    }
    
    PQescapeString(escape_languagecode, language->languagecode->content, USERDB_LANGUAGELEN);
    escape_languagecode[(USERDB_LANGUAGELEN * 2)] = '\0';
    
    PQescapeString(escape_languagename, language->languagename->content, USERDB_LANGUAGELEN);
    escape_languagename[(USERDB_LANGUAGELEN * 2)] = '\0';
    
    userdb_languagedatabasemodifycount++;
    language->databaseops++;
    
    pqasyncquery(userdb_handlemodifylanguagecommit, language, "SELECT userdb.modifylanguage(%lu,'%s','%s',%hu,%ld,%ld,%lu,%llu)", 
    language->languageid, escape_languagecode, escape_languagename, language->replicationflags, language->created, 
    language->modified, language->revision, language->transactionid);
}

void userdb_deletelanguagefromdb(userdb_language *language)
{
    if ( !userdb_databaseloaded() || !language )
    {
        Error("userdb", ERR_FATAL, "Invalid call for userdb_deletelanguagefromdb");
        assert(0);
        return;
    }
    
    userdb_languagedatabasedeletecount++;
    language->databaseops++;
    
    pqasyncquery(userdb_handledeletelanguagecommit, language, "SELECT userdb.deletelanguage(%lu)", language->languageid);
}

void userdb_addusertodb(userdb_user *user)
{
char *emaildomain, escape_username[(USERDB_MAXUSERNAMELEN * 2) + 1], escape_emaillocal[(USERDB_EMAILLEN * 2) + 1], escape_emaildomain[(USERDB_EMAILLEN * 2) + 1], escape_suspendreason[(USERDB_SUSPENDREASONLEN * 2) + 1];
unsigned char *escape_password;
size_t escape_password_len;

    if ( !userdb_databaseloaded() || !user )
    {
        Error("userdb", ERR_FATAL, "Invalid call for userdb_addusertodb");
        assert(0);
        return;
    }
    
    if ( (!UDBOIsSuspended(user) && user->suspensiondata) || (UDBOIsSuspended(user) && (!(user->suspensiondata) || !(user->suspensiondata->reason))) )
    {
        Error("userdb", ERR_FATAL, "Suspension inconsistency in userdb_addusertodb");
        assert(0);
        return;
    }
    
    PQescapeString(escape_username, user->username->content, USERDB_MAXUSERNAMELEN);
    escape_username[(USERDB_MAXUSERNAMELEN * 2)] = '\0';
    
    escape_password = PQescapeBytea(user->password, USERDB_PASSWORDHASHLEN, &escape_password_len);
    
    if ( !escape_password )
    {
        Error("userdb", ERR_FATAL, "Could not get escaped password string for %s (%lu) in userdb_addusertodb", user->username->content, user->userid);
        assert(0);
        return;
    }
    
    PQescapeString(escape_emaillocal, user->emaillocal->content, USERDB_EMAILLEN);
    escape_emaillocal[(USERDB_EMAILLEN * 2)] = '\0';
    
    if ( user->maildomain )
    {
        if ( userdb_buildemaildomain(user->maildomain, &emaildomain) != USERDB_OK )
        {
            Error("userdb", ERR_FATAL, "Could not build email domain for %s (%lu) in userdb_addusertodb", user->username->content, user->userid);
            PQfreemem(escape_password);
            assert(0);
            return;
        }
        
        PQescapeString(escape_emaildomain, emaildomain, USERDB_EMAILLEN);
        escape_emaildomain[(USERDB_EMAILLEN * 2)] = '\0';
    }
    else
    {
        escape_emaildomain[0] = USERDB_NOMAILDOMAINCHAR;
        escape_emaildomain[1] = '\0';
    }
    
    if ( user->suspensiondata && user->suspensiondata->reason )
    {
        PQescapeString(escape_suspendreason, user->suspensiondata->reason->content, USERDB_SUSPENDREASONLEN);
        escape_suspendreason[(USERDB_SUSPENDREASONLEN * 2)] = '\0';
    }
    else
        escape_suspendreason[0] = '\0';
    
    userdb_userdatabaseaddcount++;
    user->databaseops++;
    
    if ( UDBOIsSuspended(user) )
        pqasyncquery(userdb_handleaddusercommit, user, "SELECT userdb.adduser(%lu,'%s','%s',%lu,'%s','%s',%hu,%hu,%hu,%hu,%ld,%ld,'%s',%lu,%hu,%ld,%ld,%lu,%llu)", 
        user->userid, escape_username, escape_password, user->languageid, escape_emaillocal, escape_emaildomain, user->statusflags, user->groupflags, 
        user->optionflags, user->noticeflags, user->suspensiondata->suspendstart, user->suspensiondata->suspendend, escape_suspendreason, 
        user->suspensiondata->suspenderid, user->replicationflags, user->created, user->modified, user->revision, user->transactionid);
    else
        pqasyncquery(userdb_handleaddusercommit, user, "SELECT userdb.adduser(%lu,'%s','%s',%lu,'%s','%s',%hu,%hu,%hu,%hu,NULL,NULL,NULL,NULL,%hu,%ld,%ld,%lu,%llu)", 
        user->userid, escape_username, escape_password, user->languageid, escape_emaillocal, escape_emaildomain, user->statusflags, user->groupflags, 
        user->optionflags, user->noticeflags, user->replicationflags, user->created, user->modified, user->revision, user->transactionid);
    
    PQfreemem(escape_password);
}

void userdb_modifyuserindb(userdb_user *user)
{
char *emaildomain, escape_username[(USERDB_MAXUSERNAMELEN * 2) + 1], escape_emaillocal[(USERDB_EMAILLEN * 2) + 1], escape_emaildomain[(USERDB_EMAILLEN * 2) + 1], escape_suspendreason[(USERDB_SUSPENDREASONLEN * 2) + 1];
unsigned char *escape_password;
size_t escape_password_len;

    if ( !userdb_databaseloaded() || !user )
    {
        Error("userdb", ERR_FATAL, "Invalid call for userdb_modifyuserindb");
        assert(0);
        return;
    }
    
    if ( (!UDBOIsSuspended(user) && user->suspensiondata) || (UDBOIsSuspended(user) && (!(user->suspensiondata) || !(user->suspensiondata->reason))) )
    {
        Error("userdb", ERR_FATAL, "Suspension inconsistency in userdb_modifyuserindb");
        assert(0);
        return;
    }
    
    PQescapeString(escape_username, user->username->content, USERDB_MAXUSERNAMELEN);
    escape_username[(USERDB_MAXUSERNAMELEN * 2)] = '\0';
    
    escape_password = PQescapeBytea(user->password, USERDB_PASSWORDHASHLEN, &escape_password_len);
    
    if ( !escape_password )
    {
        Error("userdb", ERR_FATAL, "Could not get escaped password string for %s (%lu) in userdb_modifyuserindb", user->username->content, user->userid);
        assert(0);
        return;
    }
    
    PQescapeString(escape_emaillocal, user->emaillocal->content, USERDB_EMAILLEN);
    escape_emaillocal[(USERDB_EMAILLEN * 2)] = '\0';
    
    if ( user->maildomain )
    {
        if ( userdb_buildemaildomain(user->maildomain, &emaildomain) != USERDB_OK )
        {
            Error("userdb", ERR_FATAL, "Could not build email domain for %s (%lu) in userdb_modifyuserindb", user->username->content, user->userid);
            PQfreemem(escape_password);
            assert(0);
            return;
        }
        
        PQescapeString(escape_emaildomain, emaildomain, USERDB_EMAILLEN);
        escape_emaildomain[(USERDB_EMAILLEN * 2)] = '\0';
    }
    else
    {
        escape_emaildomain[0] = USERDB_NOMAILDOMAINCHAR;
        escape_emaildomain[1] = '\0';
    }
    
    if ( user->suspensiondata && user->suspensiondata->reason )
    {
        PQescapeString(escape_suspendreason, user->suspensiondata->reason->content, USERDB_SUSPENDREASONLEN);
        escape_suspendreason[(USERDB_SUSPENDREASONLEN * 2)] = '\0';
    }
    else
        escape_suspendreason[0] = '\0';
    
    userdb_userdatabasemodifycount++;
    user->databaseops++;
    
    if ( UDBOIsSuspended(user) )
        pqasyncquery(userdb_handlemodifyusercommit, user, "SELECT userdb.modifyuser(%lu,'%s','%s',%lu,'%s','%s',%hu,%hu,%hu,%hu,%ld,%ld,'%s',%lu,%hu,%ld,%ld,%lu,%llu)", 
        user->userid, escape_username, escape_password, user->languageid, escape_emaillocal, escape_emaildomain, user->statusflags, user->groupflags, 
        user->optionflags, user->noticeflags, user->suspensiondata->suspendstart, user->suspensiondata->suspendend, escape_suspendreason, 
        user->suspensiondata->suspenderid, user->replicationflags, user->created, user->modified, user->revision, user->transactionid);
    else
        pqasyncquery(userdb_handlemodifyusercommit, user, "SELECT userdb.modifyuser(%lu,'%s','%s',%lu,'%s','%s',%hu,%hu,%hu,%hu,NULL,NULL,NULL,NULL,%hu,%ld,%ld,%lu,%llu)", 
        user->userid, escape_username, escape_password, user->languageid, escape_emaillocal, escape_emaildomain, user->statusflags, user->groupflags, 
        user->optionflags, user->noticeflags, user->replicationflags, user->created, user->modified, user->revision, user->transactionid);
    
    PQfreemem(escape_password);
}

void userdb_deleteuserfromdb(userdb_user *user)
{
    if ( !userdb_databaseloaded() || !user )
    {
        Error("userdb", ERR_FATAL, "Invalid call for userdb_deleteuserfromdb");
        assert(0);
        return;
    }
    
    userdb_userdatabasedeletecount++;
    user->databaseops++;
    
    pqasyncquery(userdb_handledeleteusercommit, user, "SELECT userdb.deleteuser(%lu)", user->userid);
}

void userdb_addusertaginfotodb(userdb_usertaginfo *usertaginfo)
{
char escape_tagname[(USERDB_USERTAGNAMELEN * 2) + 1];

    if ( !userdb_databaseloaded() || !usertaginfo )
    {
        Error("userdb", ERR_FATAL, "Invalid call for userdb_addusertaginfotodb");
        assert(0);
        return;
    }
    
    PQescapeString(escape_tagname, usertaginfo->tagname->content, USERDB_USERTAGNAMELEN);
    escape_tagname[(USERDB_USERTAGNAMELEN * 2)] = '\0';
    
    pqasyncquery(userdb_handlenullresult, usertaginfo, "SELECT userdb.addusertaginfo(%hu,%hu,'%s')", usertaginfo->tagid, usertaginfo->tagtype, escape_tagname);
}

void userdb_deleteusertaginfofromdb(userdb_usertaginfo *usertaginfo)
{
    if ( !userdb_databaseloaded() || !usertaginfo )
    {
        Error("userdb", ERR_FATAL, "Invalid call for userdb_deleteusertaginfofromdb");
        assert(0);
        return;
    }
    
    pqasyncquery(userdb_handlenullresult, usertaginfo, "SELECT userdb.deleteusertaginfo(%hu)", usertaginfo->tagid);
}

void userdb_addusertagdatatodb(userdb_usertagdata *usertagdata)
{
void *usertagdatavalue;
unsigned char *escape_usertagdatavalue;
unsigned short tagtype, tagsize;
unsigned long tagsizelong, escapetagsizelong;

    if ( !userdb_databaseloaded() || !usertagdata || !usertagdata->taginfo || !usertagdata->userinfo )
    {
        Error("userdb", ERR_FATAL, "Invalid call for userdb_addusertagdatatodb");
        assert(0);
        return;
    }
    
    if ( userdb_getusertagdatavaluebydata(usertagdata, &tagtype, &tagsize, &usertagdatavalue) != USERDB_OK )
    {
        Error("userdb", ERR_FATAL, "Could not get user tag data value for (%hu,%lu) in userdb_addusertagdatatodb", usertagdata->taginfo->tagid, usertagdata->userinfo->userid);
        assert(0);
        return;
    }
    
    tagsizelong = (unsigned long)tagsize;
    escape_usertagdatavalue = PQescapeBytea(usertagdatavalue, tagsizelong, &escapetagsizelong);
    
    if ( !escape_usertagdatavalue )
    {
        Error("userdb", ERR_FATAL, "Could not get escaped string for (%hu,%lu) in userdb_addusertagdatatodb", usertagdata->taginfo->tagid, usertagdata->userinfo->userid);
        assert(0);
        return;
    }
    
    pqasyncquery(userdb_handlenullresult, usertagdata, "SELECT userdb.addusertagdata(%hu,%lu,'%s')", usertagdata->taginfo->tagid, usertagdata->userinfo->userid, escape_usertagdatavalue);
    PQfreemem(escape_usertagdatavalue);
}

void userdb_modifyusertagdataindb(userdb_usertagdata *usertagdata)
{
void *usertagdatavalue;
unsigned char *escape_usertagdatavalue;
unsigned short tagtype, tagsize;
unsigned long tagsizelong, escapetagsizelong;

    if ( !userdb_databaseloaded() || !usertagdata || !usertagdata->taginfo || !usertagdata->userinfo )
    {
        Error("userdb", ERR_FATAL, "Invalid call for userdb_modifyusertagdatatodb");
        assert(0);
        return;
    }
    
    if ( userdb_getusertagdatavaluebydata(usertagdata, &tagtype, &tagsize, &usertagdatavalue) != USERDB_OK )
    {
        Error("userdb", ERR_FATAL, "Could not get user tag data value for (%hu,%lu) in userdb_modifyusertagdatatodb", usertagdata->taginfo->tagid, usertagdata->userinfo->userid);
        assert(0);
        return;
    }
    
    tagsizelong = (unsigned long)tagsize;
    escape_usertagdatavalue = PQescapeBytea(usertagdatavalue, tagsizelong, &escapetagsizelong);
    
    if ( !escape_usertagdatavalue )
    {
        Error("userdb", ERR_FATAL, "Could not get escaped string for (%hu,%lu) in userdb_modifyusertagdatatodb", usertagdata->taginfo->tagid, usertagdata->userinfo->userid);
        assert(0);
        return;
    }
    
    pqasyncquery(userdb_handlenullresult, usertagdata, "SELECT userdb.modifyusertagdata(%hu,%lu,'%s')", usertagdata->taginfo->tagid, usertagdata->userinfo->userid, escape_usertagdatavalue);
    PQfreemem(escape_usertagdatavalue);
}

void userdb_deleteusertagdatafromdb(userdb_usertagdata *usertagdata)
{
    if ( !userdb_databaseloaded() || !usertagdata || !usertagdata->taginfo || !usertagdata->userinfo )
    {
        Error("userdb", ERR_FATAL, "Invalid call for userdb_deleteusertagdatafromdb");
        assert(0);
        return;
    }
    
    pqasyncquery(userdb_handlenullresult, usertagdata, "SELECT userdb.deleteusertagdata(%hu,%lu)", usertagdata->taginfo->tagid, usertagdata->userinfo->userid);
}

void userdb_deleteallusertagdatabyinfofromdb(userdb_usertaginfo *usertaginfo)
{
    if ( !userdb_databaseloaded() || !usertaginfo )
    {
        Error("userdb", ERR_FATAL, "Invalid call for userdb_deleteallusertagdatabyinfofromdb");
        assert(0);
        return;
    }
    
    pqasyncquery(userdb_handlenullresult, usertaginfo, "SELECT userdb.deleteallusertagdatabytagid(%hu)", usertaginfo->tagid);
}

void userdb_deleteallusertagdatabyuserfromdb(userdb_user *user)
{
    if ( !userdb_databaseloaded() || !user )
    {
        Error("userdb", ERR_FATAL, "Invalid call for userdb_deleteallusertagdatabyuserfromdb");
        assert(0);
        return;
    }
    
    pqasyncquery(userdb_handlenullresult, user, "SELECT userdb.deleteallusertagdatabyuserid(%lu)", user->userid);
}

void userdb_addmaildomainconfigtodb(userdb_maildomainconfig *maildomainconfig)
{
char escape_configname[(USERDB_EMAILLEN * 2) + 1];

    if ( !userdb_databaseloaded() || !maildomainconfig )
    {
        Error("userdb", ERR_FATAL, "Invalid call for userdb_addmaildomainconfigtodb");
        assert(0);
        return;
    }
    
    PQescapeString(escape_configname, maildomainconfig->configname->content, USERDB_EMAILLEN);
    escape_configname[(USERDB_EMAILLEN * 2)] = '\0';
    
    userdb_maildomainconfigdatabaseaddcount++;
    maildomainconfig->databaseops++;
    
    pqasyncquery(userdb_handleaddmaildomainconfigcommit, maildomainconfig, "SELECT userdb.addmaildomainconfig(%lu,'%s',%hu,%lu,%lu,%hu,%ld,%ld,%lu,%llu)", 
    maildomainconfig->configid, escape_configname, maildomainconfig->domainflags, 
    maildomainconfig->maxlocal, maildomainconfig->maxdomain, maildomainconfig->replicationflags, 
    maildomainconfig->created, maildomainconfig->modified, maildomainconfig->revision, maildomainconfig->transactionid);
}

void userdb_modifymaildomainconfigindb(userdb_maildomainconfig *maildomainconfig)
{
char escape_configname[(USERDB_EMAILLEN * 2) + 1];

    if ( !userdb_databaseloaded() || !maildomainconfig )
    {
        Error("userdb", ERR_FATAL, "Invalid call for userdb_modifymaildomainconfigindb");
        assert(0);
        return;
    }
    
    PQescapeString(escape_configname, maildomainconfig->configname->content, USERDB_EMAILLEN);
    escape_configname[(USERDB_EMAILLEN * 2)] = '\0';
    
    userdb_maildomainconfigdatabasemodifycount++;
    maildomainconfig->databaseops++;
    
    pqasyncquery(userdb_handlemodifymaildomainconfigcommit, maildomainconfig, "SELECT userdb.modifymaildomainconfig(%lu,'%s',%hu,%lu,%lu,%hu,%ld,%ld,%lu,%llu)", 
    maildomainconfig->configid, escape_configname, maildomainconfig->domainflags, 
    maildomainconfig->maxlocal, maildomainconfig->maxdomain, maildomainconfig->replicationflags, 
    maildomainconfig->created, maildomainconfig->modified, maildomainconfig->revision, maildomainconfig->transactionid);
}

void userdb_deletemaildomainconfigfromdb(userdb_maildomainconfig *maildomainconfig)
{
    if ( !userdb_databaseloaded() || !maildomainconfig )
    {
        Error("userdb", ERR_FATAL, "Invalid call for userdb_deletemaildomainconfigfromdb");
        assert(0);
        return;
    }
    
    userdb_maildomainconfigdatabasedeletecount++;
    maildomainconfig->databaseops++;
    
    pqasyncquery(userdb_handledeletemaildomainconfigcommit, maildomainconfig, "SELECT userdb.deletemaildomainconfig(%lu)", maildomainconfig->configid);
}
