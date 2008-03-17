/*
 * UserDB
 */

#include "./userdb.h"

#define USERDB_MAXREPLICATIONBATCH 500

#ifdef USERDB_ENABLEREPLICATION

int userdb_encodestring(char *in, char *out, unsigned long outlen)
{
unsigned long inlen;
unsigned long tempoutlen;

    if ( userdb_strlen(in, 0, &inlen) != USERDB_OK )
        return USERDB_ERROR;
    
    memset(out, 0, (outlen + 1));
    tempoutlen = outlen;
    
    if ( replicator_encodeline(((unsigned char *)in), inlen, ((unsigned char *)out), &tempoutlen) != REPLICATOR_OK )
        return USERDB_ERROR;
    
    return USERDB_OK;
}

int userdb_decodestring(char *in, char *out, unsigned long outlen)
{
unsigned long inlen;
unsigned long tempoutlen;

    if ( userdb_strlen(in, 0, &inlen) != USERDB_OK )
        return USERDB_ERROR;
    
    memset(out, 0, (outlen + 1));
    tempoutlen = outlen;
    
    if ( replicator_decodeline(((unsigned char *)in), inlen, ((unsigned char *)out), &tempoutlen) != REPLICATOR_OK )
        return USERDB_ERROR;
    
    return USERDB_OK;
}

int userdb_serialiselanguage(userdb_language *language, unsigned char *languagestring, unsigned long *languagestringlen)
{
char encodedlanguagecode[(USERDB_LANGUAGELEN * 2) + 1];
char encodedlanguagename[(USERDB_LANGUAGELEN * 2) + 1];

    if ( !language || !languagestring || !languagestringlen )
        return USERDB_BADPARAMETERS;
    
    if ( userdb_encodestring(language->languagecode->content, encodedlanguagecode, (USERDB_LANGUAGELEN * 2)) != USERDB_OK )
    {
        Error("userdb", ERR_ERROR, "Could not encode language code for %s (%lu) in userdb_serialiselanguage", language->languagecode->content, language->languageid);
        return USERDB_ERROR;
    }
    
    encodedlanguagecode[(USERDB_LANGUAGELEN * 2)] = '\0';
    
    if ( userdb_encodestring(language->languagename->content, encodedlanguagename, (USERDB_LANGUAGELEN * 2)) != USERDB_OK )
    {
        Error("userdb", ERR_ERROR, "Could not encode language name for %s (%lu) in userdb_serialiselanguage", language->languagename->content, language->languageid);
        return USERDB_ERROR;
    }
    
    encodedlanguagename[(USERDB_LANGUAGELEN * 2)] = '\0';
    
    *languagestringlen = snprintf((char *)languagestring, REPLICATOR_MAXDATALEN, "%lu %s %s %hu %ld %ld %lu %llu", language->languageid, encodedlanguagecode, encodedlanguagename, language->replicationflags, language->created, language->modified, language->revision, language->transactionid);
    languagestring[REPLICATOR_MAXDATALEN] = '\0';
    return USERDB_OK;
}

int userdb_unserialiselanguage(unsigned char *languagestring, unsigned long *languageid, char **languagecode, char **languagename, flag_t *replicationflags, time_t *created, time_t *modified, unsigned long *revision, unsigned long long *transactionid)
{
int cargc;
char *cargv[20];
static char templanguagecode[USERDB_LANGUAGELEN + 1];
static char templanguagename[USERDB_LANGUAGELEN + 1];

    if ( !languagestring || !languageid || !languagecode || !languagename || !created || !modified || !revision || !transactionid )
        return USERDB_BADPARAMETERS;
    
    cargc = splitline((char *)languagestring, cargv, 20, 0);
    
    if ( cargc != 8 )
    {
        Error("userdb", ERR_ERROR, "Wrong number of values in serialised language: %s", languagestring);
        return USERDB_ERROR;
    }
    
    if ( sscanf(cargv[0], "%lu", languageid) != 1 )
        return USERDB_ERROR;
    
    if ( userdb_decodestring(cargv[1], templanguagecode, USERDB_LANGUAGELEN) != USERDB_OK )
    {
        Error("userdb", ERR_ERROR, "Could not decode language code for language %s in userdb_unserialiselanguage", cargv[0]);
        return USERDB_ERROR;
    }
    
    templanguagecode[USERDB_LANGUAGELEN] = '\0';
    *languagecode = templanguagecode;
    
    if ( userdb_decodestring(cargv[2], templanguagename, USERDB_LANGUAGELEN) != USERDB_OK )
    {
        Error("userdb", ERR_ERROR, "Could not decode language name for language %s in userdb_unserialiselanguage", cargv[0]);
        return USERDB_ERROR;
    }
    
    templanguagename[USERDB_LANGUAGELEN] = '\0';
    *languagename = templanguagename;
    
    if ( sscanf(cargv[3], "%hu", replicationflags) != 1 )
        return USERDB_ERROR;
    
    if ( sscanf(cargv[4], "%ld", created) != 1 )
        return USERDB_ERROR;
    
    if ( sscanf(cargv[5], "%ld", modified) != 1 )
        return USERDB_ERROR;
    
    if ( sscanf(cargv[6], "%lu", revision) != 1 )
        return USERDB_ERROR;
    
    if ( sscanf(cargv[7], "%llu", transactionid) != 1 )
        return USERDB_ERROR;
    
    return USERDB_OK;
}

void userdb_languagedbchange(userdb_language *language, unsigned int operation)
{
    if ( language->databaseops )
        return;
    
    replicator_notifychange(userdb_repl_group, userdb_repl_obj_language, operation);
}

void userdb_languagerequest(replicator_node *node, unsigned long long transactionid)
{
unsigned char buf[REPLICATOR_MAXDATALEN + 1];
unsigned long buflen;
unsigned int i, moredata;
unsigned long long lasttransactionid;
userdb_language *languages[USERDB_MAXREPLICATIONBATCH], *templanguage;

    moredata = 0;
    memset(languages, 0, USERDB_MAXREPLICATIONBATCH * sizeof(userdb_language *));
    lasttransactionid = transactionid;
    
    for ( i = 0; i < USERDB_LANGUAGEHASHSIZE; i++ )
    {
        for ( templanguage = userdb_languagehashtable_id[i]; templanguage; templanguage = templanguage->nextbyid )
        {
            if ( templanguage->transactionid > transactionid && templanguage->transactionid <= ( transactionid + USERDB_MAXREPLICATIONBATCH ) )
                languages[templanguage->transactionid - transactionid - 1] = templanguage;
            else if ( !(templanguage->databaseops) && templanguage->transactionid > ( transactionid + USERDB_MAXREPLICATIONBATCH ) )
                moredata = 1;
        }
    }
    
    for ( i = 0; i < USERDB_LANGUAGEDELETEDHASHSIZE; i++ )
    {
        for ( templanguage = userdb_languagedeletedhashtable[i]; templanguage; templanguage = templanguage->nextbyid )
        {
            if ( templanguage->transactionid > transactionid && templanguage->transactionid <= ( transactionid + USERDB_MAXREPLICATIONBATCH ) )
                languages[templanguage->transactionid - transactionid - 1] = templanguage;
            else if ( !(templanguage->databaseops) && templanguage->transactionid > ( transactionid + USERDB_MAXREPLICATIONBATCH ) )
                moredata = 1;
        }
    }
    
    for ( i = 0; i < USERDB_MAXREPLICATIONBATCH; i++ )
    {
        if ( languages[i] )
        {
            if ( languages[i]->databaseops )
            {
                replicator_sendCtoken(userdb_repl_group, userdb_repl_obj_language, node, lasttransactionid);
                return;
            }
            
            if ( userdb_serialiselanguage(languages[i], buf, &buflen) != USERDB_OK )
            {
                Error("userdb", ERR_ERROR, "Replication error, Code: R-LANGUAGE-E1");
                return;
            }
            
            if ( lasttransactionid < languages[i]->transactionid )
                lasttransactionid = languages[i]->transactionid;
            
            replicator_sendDtoken(userdb_repl_group, userdb_repl_obj_language, node, buf, buflen);
        }
    }
    
    if ( moredata )
        replicator_sendMtoken(userdb_repl_group, userdb_repl_obj_language, node, transactionid + USERDB_MAXREPLICATIONBATCH);
    else
        replicator_sendCtoken(userdb_repl_group, userdb_repl_obj_language, node, lasttransactionid);
    
    return;
}

void userdb_languagereceive(unsigned char *languagestring)
{
unsigned int i;
userdb_language *language;
userdb_user *tempuser;
unsigned long languageid;
char *languagecode;
char *languagename;
flag_t replicationflags;
time_t created, modified;
unsigned long revision;
unsigned long long transactionid;

    if ( userdb_unserialiselanguage(languagestring, &languageid, &languagecode, &languagename, &replicationflags, &created, &modified, &revision, &transactionid) != USERDB_OK )
    {
        Error("userdb", ERR_ERROR, "Replication error, Code: D-LANGUAGE-E1");
        return;
    }
    
    switch ( userdb_findlanguagebylanguageid(languageid, &language) )
    {
        case USERDB_OK:
        
            if ( UDBRIsDeleted(language) )
            {
                Error("userdb", ERR_WARNING, "Replication warning, Code: D-LANGUAGE-W1");
                return;
            }
            
            if ( (replicationflags & UDB_REPLICATION_DELETED) )
                triggerhook(HOOK_USERDB_DELETELANG, language);
            else
                triggerhook(HOOK_USERDB_PREMODIFYLANG, language);
            
            if ( ircd_strcmp(language->languagecode->content, languagecode) )
            {
                userdb_deletelanguagefromhash(language);
                freesstring(language->languagecode);
                language->languagecode = getsstring(languagecode, USERDB_LANGUAGELEN);
                userdb_addlanguagetohash(language);
            }
            
            if ( ircd_strcmp(language->languagename->content, languagename) )
            {
                freesstring(language->languagename);
                language->languagename = getsstring(languagename, USERDB_LANGUAGELEN);
            }
            
            language->replicationflags = replicationflags;
            language->created = created;
            language->modified = modified;
            language->revision = revision;
            language->transactionid = transactionid;
            
            if ( UDBRIsDeleted(language) )
            {
                userdb_deletelanguagefromhash(language);
                
                for ( i = 0; i < USERDB_USERHASHSIZE; i++ )
                    for ( tempuser = userdb_userhashtable_id[i]; tempuser; tempuser = tempuser->nextbyid )
                        if ( tempuser->languagedata == language )
                            tempuser->languagedata = NULL;
                
                userdb_addlanguagetodeletedhash(language);
                
                if ( userdb_repl_group->role == REPLICATOR_ROLE_SLAVE )
                    userdb_deletelanguagefromdb(language);
                else
                    userdb_modifylanguageindb(language);
            }
            else
            {
                userdb_modifylanguageindb(language);
                triggerhook(HOOK_USERDB_POSTMODIFYLANG, language);
            }
            
            if ( userdb_lastlanguagetransactionid < transactionid )
                userdb_lastlanguagetransactionid = transactionid;
            
            break;
            
        case USERDB_NOTFOUND:
        
            if ( (replicationflags & UDB_REPLICATION_DELETED) && userdb_repl_group->role == REPLICATOR_ROLE_SLAVE )
            {
                replicator_notifychange(userdb_repl_group, userdb_repl_obj_language, REPLICATOR_ACTION_NONE);
                return;
            }
            
            language = userdb_malloclanguage();
            language->languageid = languageid;
            language->languagecode = getsstring(languagecode, USERDB_LANGUAGELEN);
            language->languagename = getsstring(languagename, USERDB_LANGUAGELEN);
            language->replicationflags = replicationflags;
            language->created = created;
            language->modified = modified;
            language->revision = revision;
            language->transactionid = transactionid;
            
            if ( UDBRIsDeleted(language) )
            {
                userdb_addlanguagetodeletedhash(language);
                userdb_addlanguagetodb(language);
            }
            else
            {
                for ( i = 0; i < USERDB_USERHASHSIZE; i++ )
                    for ( tempuser = userdb_userhashtable_id[i]; tempuser; tempuser = tempuser->nextbyid )
                        if ( tempuser->languageid == languageid )
                            tempuser->languagedata = language;
                
                userdb_addlanguagetohash(language);
                userdb_addlanguagetodb(language);
                triggerhook(HOOK_USERDB_CREATELANG, language);
            }
            
            if ( userdb_lastlanguagetransactionid < transactionid )
                userdb_lastlanguagetransactionid = transactionid;
            
            break;
            
        default:
            Error("userdb", ERR_ERROR, "Replication error, Code: D-LANGUAGE-E2");
            break;
    }
    
    return;
}

void userdb_languagecleanup(unsigned long long transactionid)
{
unsigned int i;
userdb_language *templanguage;
    
    for ( i = 0; i < USERDB_LANGUAGEDELETEDHASHSIZE; i++ )
        for ( templanguage = userdb_languagedeletedhashtable[i]; templanguage; templanguage = templanguage->nextbyid )
            if ( !(templanguage->databaseops) && templanguage->transactionid <= transactionid )
                userdb_deletelanguagefromdb(templanguage);
}

int userdb_serialiseuser(userdb_user *user, unsigned char *userstring, unsigned long *userstringlen)
{
char encodedusername[(USERDB_MAXUSERNAMELEN * 2) + 1];
char encodedemaillocal[(USERDB_EMAILLEN * 2) + 1];
char encodedemaildomain[(USERDB_EMAILLEN * 2) + 1];
char encodedsuspendreason[(USERDB_SUSPENDREASONLEN * 2) + 1];
unsigned char encodedpassword[(USERDB_PASSWORDHASHLEN * 2) + 1];
unsigned long encodedpasswordlen;
char *emaildomain;

    if ( !user || !userstring || !userstringlen )
        return USERDB_BADPARAMETERS;
    
    if ( user->maildomain )
    {
        if ( userdb_buildemaildomain(user->maildomain, &emaildomain) != USERDB_OK )
        {
            Error("userdb", ERR_ERROR, "Could not build email domain for %s (%lu) in userdb_serialiseuser", user->username->content, user->userid);
            return USERDB_BADEMAIL;
        }
    }
    else
    {
        emaildomain = USERDB_NOMAILDOMAINSYMBOL;
    }
    
    if ( userdb_encodestring(user->username->content, encodedusername, (USERDB_MAXUSERNAMELEN * 2)) != USERDB_OK )
    {
        Error("userdb", ERR_ERROR, "Could not encode username for %s (%lu) in userdb_serialiseuser", user->username->content, user->userid);
        return USERDB_ERROR;
    }
    
    encodedusername[(USERDB_MAXUSERNAMELEN * 2)] = '\0';
    
    memset(encodedpassword, 0, (USERDB_PASSWORDHASHLEN * 2) + 1);
    encodedpasswordlen = (USERDB_PASSWORDHASHLEN * 2);
    
    if ( replicator_encodeline(user->password, USERDB_PASSWORDHASHLEN, encodedpassword, &encodedpasswordlen) != REPLICATOR_OK )
    {
        Error("userdb", ERR_ERROR, "Could not encode user password for %s (%lu) in userdb_serialiseuser", user->username->content, user->userid);
        return USERDB_ERROR;
    }
    
    encodedpassword[(USERDB_PASSWORDHASHLEN * 2)] = '\0';
    
    if ( userdb_encodestring(user->emaillocal->content, encodedemaillocal, (USERDB_EMAILLEN * 2)) != USERDB_OK )
    {
        Error("userdb", ERR_ERROR, "Could not encode email local for %s (%lu) in userdb_serialiseuser", user->username->content, user->userid);
        return USERDB_ERROR;
    }
    
    encodedemaillocal[(USERDB_EMAILLEN * 2)] = '\0';
    
    if ( userdb_encodestring(emaildomain, encodedemaildomain, (USERDB_EMAILLEN * 2)) != USERDB_OK )
    {
        Error("userdb", ERR_ERROR, "Could not encode email domain for %s (%lu) in userdb_serialiseuser", user->username->content, user->userid);
        return USERDB_ERROR;
    }
    
    encodedemaildomain[(USERDB_EMAILLEN * 2)] = '\0';
    
    if ( UDBOIsSuspended(user) )
    {
        if ( userdb_encodestring(user->suspensiondata->reason->content, encodedsuspendreason, (USERDB_SUSPENDREASONLEN * 2)) != USERDB_OK )
        {
            Error("userdb", ERR_ERROR, "Could not encode suspend reason for %s (%lu) in userdb_serialiseuser", user->username->content, user->userid);
            return USERDB_ERROR;
        }
        
        encodedsuspendreason[(USERDB_SUSPENDREASONLEN * 2)] = '\0';
        
        *userstringlen = snprintf((char *)userstring, REPLICATOR_MAXDATALEN, 
        "%lu %s %s %lu %s %s %hu %hu %hu %hu %ld %ld %s %lu %hu %ld %ld %lu %llu", 
        user->userid, encodedusername, encodedpassword, user->languageid, 
        encodedemaillocal, encodedemaildomain, user->statusflags, user->groupflags, user->optionflags, 
        user->noticeflags, user->suspensiondata->suspendstart, user->suspensiondata->suspendend, 
        encodedsuspendreason, user->suspensiondata->suspenderid, user->replicationflags, 
        user->created, user->modified, user->revision, user->transactionid);
    }
    else
    {
        *userstringlen = snprintf((char *)userstring, REPLICATOR_MAXLINELEN, 
        "%lu %s %s %lu %s %s %hu %hu %hu %hu 0 0 0 0 %hu %ld %ld %lu %llu", 
        user->userid, encodedusername, encodedpassword, user->languageid, 
        encodedemaillocal, encodedemaildomain, user->statusflags, user->groupflags, user->optionflags, 
        user->noticeflags, user->replicationflags, user->created, user->modified, user->revision, 
        user->transactionid);
    }
    
    userstring[REPLICATOR_MAXDATALEN] = '\0';
    return USERDB_OK;
}

int userdb_unserialiseuser(unsigned char *userstring, unsigned long *userid, char **username, unsigned char **password, unsigned long *languageid, char **emaillocal, char **emaildomain, flag_t *statusflags, flag_t *groupflags, flag_t *optionflags, flag_t *noticeflags, time_t *suspendstart, time_t *suspendend, char **reason, unsigned long *suspenderid, flag_t *replicationflags, time_t *created, time_t *modified, unsigned long *revision, unsigned long long *transactionid)
{
int cargc;
char *cargv[20];
static char tempusername[USERDB_MAXUSERNAMELEN + 1];
static char tempemaillocal[USERDB_EMAILLEN + 1];
static char tempemaildomain[USERDB_EMAILLEN + 1];
static char tempsuspendreason[USERDB_SUSPENDREASONLEN + 1];
unsigned long tempencodedpasswordlen;
static unsigned char temppassword[USERDB_PASSWORDHASHLEN];
unsigned long temppasswordlen;

    if ( !userstring || !userid || !username || !password || !languageid || !emaillocal || !emaildomain || !statusflags || !groupflags || !optionflags || !noticeflags || !suspendstart || !suspendend || !reason || !suspenderid || !replicationflags || !created || !modified || !revision || !transactionid )
    {
        Error("userdb", ERR_ERROR, "Bad parameters for userdb_unserialiseuser");
        return USERDB_BADPARAMETERS;
    }
    
    cargc = splitline((char *)userstring, cargv, 20, 0);
    
    if ( cargc != 19 )
    {
        Error("userdb", ERR_ERROR, "Wrong number of values in serialised user: %s", userstring);
        return USERDB_ERROR;
    }
    
    if ( sscanf(cargv[0], "%lu", userid) != 1 )
        return USERDB_ERROR;
    
    if ( userdb_decodestring(cargv[1], tempusername, USERDB_MAXUSERNAMELEN) != USERDB_OK )
    {
        Error("userdb", ERR_ERROR, "Could not decode username for user %s", cargv[0]);
        return USERDB_ERROR;
    }
    
    tempusername[USERDB_MAXUSERNAMELEN] = '\0';
    *username = tempusername;
    
    if ( userdb_strlen(cargv[2], 0, &tempencodedpasswordlen) != USERDB_OK )
    {
        Error("userdb", ERR_ERROR, "Could not determine encoded password length");
        return USERDB_ERROR;
    }
    
    memset(temppassword, 0, USERDB_PASSWORDHASHLEN);
    temppasswordlen = USERDB_PASSWORDHASHLEN;
    
    if ( replicator_decodeline(((unsigned char *)cargv[2]), tempencodedpasswordlen, temppassword, &temppasswordlen) != REPLICATOR_OK )
    {
        Error("userdb", ERR_ERROR, "Could not decode password for user %s", cargv[0]);
        return USERDB_ERROR;
    }
    
    if ( temppasswordlen != USERDB_PASSWORDHASHLEN )
    {
        Error("userdb", ERR_ERROR, "Password hash not 32 bytes for user %s", cargv[0]);
        return USERDB_ERROR;
    }
    
    *password = temppassword;
    
    if ( sscanf(cargv[3], "%lu", languageid) != 1 )
        return USERDB_ERROR;
    
    if ( userdb_decodestring(cargv[4], tempemaillocal, USERDB_EMAILLEN) != USERDB_OK )
    {
        Error("userdb", ERR_ERROR, "Could not decode email local for user %s", cargv[0]);
        return USERDB_ERROR;
    }
    
    tempemaillocal[USERDB_EMAILLEN] = '\0';
    *emaillocal = tempemaillocal;
    
    if ( userdb_decodestring(cargv[5], tempemaildomain, USERDB_EMAILLEN) != USERDB_OK )
    {
        Error("userdb", ERR_ERROR, "Could not decode email domain for user %s", cargv[0]);
        return USERDB_ERROR;
    }
    
    tempemaildomain[USERDB_EMAILLEN] = '\0';
    *emaildomain = tempemaildomain;
    
    if ( sscanf(cargv[6], "%hu", statusflags) != 1 )
        return USERDB_ERROR;
    
    if ( sscanf(cargv[7], "%hu", groupflags) != 1 )
        return USERDB_ERROR;
    
    if ( sscanf(cargv[8], "%hu", optionflags) != 1 )
        return USERDB_ERROR;
    
    if ( sscanf(cargv[9], "%hu", noticeflags) != 1 )
        return USERDB_ERROR;
    
    if ( sscanf(cargv[10], "%ld", suspendstart) != 1 )
        return USERDB_ERROR;
    
    if ( sscanf(cargv[11], "%ld", suspendend) != 1 )
        return USERDB_ERROR;
    
    if ( userdb_decodestring(cargv[12], tempsuspendreason, USERDB_SUSPENDREASONLEN) != USERDB_OK )
    {
        Error("userdb", ERR_ERROR, "Could not decode suspend reason for user %s", cargv[0]);
        return USERDB_ERROR;
    }
    
    tempsuspendreason[USERDB_SUSPENDREASONLEN] = '\0';
    *reason = tempsuspendreason;
    
    if ( sscanf(cargv[13], "%lu", suspenderid) != 1 )
        return USERDB_ERROR;
    
    if ( sscanf(cargv[14], "%hu", replicationflags) != 1 )
        return USERDB_ERROR;
    
    if ( sscanf(cargv[15], "%ld", created) != 1 )
        return USERDB_ERROR;
    
    if ( sscanf(cargv[16], "%ld", modified) != 1 )
        return USERDB_ERROR;
    
    if ( sscanf(cargv[17], "%lu", revision) != 1 )
        return USERDB_ERROR;
    
    if ( sscanf(cargv[18], "%llu", transactionid) != 1 )
        return USERDB_ERROR;
    
    return USERDB_OK;
}

void userdb_userdbchange(userdb_user *user, unsigned int operation)
{
    if ( user->databaseops )
        return;
    
    replicator_notifychange(userdb_repl_group, userdb_repl_obj_user, operation);
}

void userdb_userrequest(replicator_node *node, unsigned long long transactionid)
{
unsigned char buf[REPLICATOR_MAXDATALEN + 1];
unsigned long buflen;
unsigned int i, moredata;
unsigned long long lasttransactionid;
userdb_user *users[USERDB_MAXREPLICATIONBATCH], *tempuser;

    moredata = 0;
    memset(users, 0, USERDB_MAXREPLICATIONBATCH * sizeof(userdb_user *));
    lasttransactionid = transactionid;
    
    for ( i = 0; i < USERDB_USERHASHSIZE; i++ )
    {
        for ( tempuser = userdb_userhashtable_id[i]; tempuser; tempuser = tempuser->nextbyid )
        {
            if ( tempuser->transactionid > transactionid && tempuser->transactionid <= ( transactionid + USERDB_MAXREPLICATIONBATCH ) )
                users[tempuser->transactionid - transactionid - 1] = tempuser;
            else if ( !(tempuser->databaseops) && tempuser->transactionid > ( transactionid + USERDB_MAXREPLICATIONBATCH ) )
                moredata = 1;
        }
    }
    
    for ( i = 0; i < USERDB_USERDELETEDHASHSIZE; i++ )
    {
        for ( tempuser = userdb_userdeletedhashtable[i]; tempuser; tempuser = tempuser->nextbyid )
        {
            if ( tempuser->transactionid > transactionid && tempuser->transactionid <= ( transactionid + USERDB_MAXREPLICATIONBATCH ) )
                users[tempuser->transactionid - transactionid - 1] = tempuser;
            else if ( !(tempuser->databaseops) && tempuser->transactionid > ( transactionid + USERDB_MAXREPLICATIONBATCH ) )
                moredata = 1;
        }
    }
    
    for ( i = 0; i < USERDB_MAXREPLICATIONBATCH; i++ )
    {
        if ( users[i] )
        {
            if ( users[i]->databaseops )
            {
                replicator_sendCtoken(userdb_repl_group, userdb_repl_obj_user, node, lasttransactionid);
                return;
            }
            
            if ( userdb_serialiseuser(users[i], buf, &buflen) != USERDB_OK )
            {
                Error("userdb", ERR_ERROR, "Replication error, Code: R-USER-E1");
                return;
            }
            
            if ( lasttransactionid < users[i]->transactionid )
                lasttransactionid = users[i]->transactionid;
            
            replicator_sendDtoken(userdb_repl_group, userdb_repl_obj_user, node, buf, buflen);
        }
    }
    
    if ( moredata )
        replicator_sendMtoken(userdb_repl_group, userdb_repl_obj_user, node, transactionid + USERDB_MAXREPLICATIONBATCH);
    else
        replicator_sendCtoken(userdb_repl_group, userdb_repl_obj_user, node, lasttransactionid);
    
    return;
}

void userdb_userreceive(unsigned char *userstring)
{
unsigned int i;
userdb_user *user, *tempuser;
userdb_maildomain *tempmaildomain;
unsigned long userid;
char *username;
unsigned char *password;
unsigned long languageid;
char *emaillocal;
char *emaildomain;
flag_t statusflags, groupflags, optionflags, noticeflags;
time_t suspendstart, suspendend;
char *reason;
unsigned long suspenderid;
flag_t replicationflags;
time_t created, modified;
unsigned long revision;
unsigned long long transactionid;

    if ( userdb_unserialiseuser(userstring, &userid, &username, &password, &languageid, &emaillocal, &emaildomain, &statusflags, &groupflags, &optionflags, &noticeflags, &suspendstart, &suspendend, &reason, &suspenderid, &replicationflags, &created, &modified, &revision, &transactionid) != USERDB_OK )
    {
        Error("userdb", ERR_ERROR, "Replication error, Code: D-USER-E1");
        return;
    }
    
    switch ( userdb_finduserbyuserid(userid, &user) )
    {
        case USERDB_OK:
            
            if ( UDBRIsDeleted(user) )
            {
                Error("userdb", ERR_WARNING, "Replication warning, Code: D-USER-W1");
                return;
            }
            
            if ( (replicationflags & UDB_REPLICATION_DELETED) )
                triggerhook(HOOK_USERDB_DELETEUSER, user);
            else
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
            
            if ( user->languageid )
            {
                switch ( userdb_findlanguagebylanguageid(languageid, &(user->languagedata)) )
                {
                    case USERDB_OK:
                       break;
                    case USERDB_NOTFOUND:
                        user->languagedata = NULL;
                        break;
                    default:
                        user->languagedata = NULL;
                        Error("userdb", ERR_ERROR, "Replication error, Code: D-USER-E2");
                        break;
                }
            }
            else
            {
                user->languagedata = NULL;
            }
            
            if ( ircd_strcmp(user->emaillocal->content, emaillocal) )
            {
                freesstring(user->emaillocal);
                user->emaillocal = getsstring(emaillocal, USERDB_EMAILLEN);
            }
            
            user->statusflags = statusflags;
            user->groupflags = groupflags;
            user->optionflags = optionflags;
            user->noticeflags = noticeflags;
            
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
                                user->suspensiondata->suspenderdata = NULL;
                                Error("userdb", ERR_ERROR, "Replication error, Code: D-USER-E3");
                                break;
                        }
                    }
                    else
                    {
                        user->suspensiondata->suspenderdata = NULL;
                    }
                }
                else
                {
                    user->suspensiondata = userdb_mallocsuspension();
                    user->suspensiondata->suspendstart = suspendstart;
                    user->suspensiondata->suspendend = suspendend;
                    user->suspensiondata->reason = getsstring(reason, USERDB_SUSPENDREASONLEN);
                    user->suspensiondata->suspenderid = suspenderid;
                    userdb_addusertosuspendedhash(user);
                    
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
                                user->suspensiondata->suspenderdata = NULL;
                                Error("userdb", ERR_ERROR, "Replication error, Code: D-USER-E4");
                                break;
                        }
                    }
                    else
                    {
                        user->suspensiondata->suspenderdata = NULL;
                    }
                }
            }
            else if ( user->suspensiondata )
            {
                userdb_deleteuserfromsuspendedhash(user);
                userdb_freesuspension(user->suspensiondata);
                user->suspensiondata = NULL;
            }
            
            user->replicationflags = replicationflags;
            user->created = created;
            user->modified = modified;
            user->revision = revision;
            user->transactionid = transactionid;
            
            if ( UDBRIsDeleted(user) )
            {
                userdb_clearallusertagdatabyuser(user);
                userdb_unassignuserfromauth(user);
                
                if ( UDBOIsSuspended(user) )
                    userdb_deleteuserfromsuspendedhash(user);
                
                userdb_deleteuserfromhash(user);
                userdb_deleteuserfrommaildomain(user, user->maildomain);
                userdb_cleanmaildomaintree(user->maildomain);
                user->maildomain = NULL;
                
                for ( i = 0; i < USERDB_USERSUSPENDEDHASHSIZE; i++ )
                    for ( tempuser = userdb_usersuspendedhashtable[i]; tempuser; tempuser = tempuser->suspensiondata->nextbyid )
                        if ( tempuser->suspensiondata->suspenderdata == user )
                            tempuser->suspensiondata->suspenderdata = NULL;
                
                userdb_addusertodeletedhash(user);
                
                if ( userdb_repl_group->role == REPLICATOR_ROLE_SLAVE )
                    userdb_deleteuserfromdb(user);
                else
                    userdb_modifyuserindb(user);
            }
            else
            {
                if ( !ircd_strcmp(emaildomain, USERDB_NOMAILDOMAINSYMBOL) )
                {
                    if ( user->maildomain )
                    {
                        userdb_deleteuserfrommaildomain(user, user->maildomain); 
                        userdb_cleanmaildomaintree(user->maildomain);
                        user->maildomain = NULL;
                    }
                }
                else
                {
                    switch ( userdb_findmaildomainbyemaildomain(emaildomain, &tempmaildomain, 1) )
                    {
                        case USERDB_OK:
                            if ( user->maildomain == tempmaildomain )
                                break;
                            
                            userdb_deleteuserfrommaildomain(user, user->maildomain);
                            userdb_cleanmaildomaintree(user->maildomain);
                            user->maildomain = tempmaildomain;
                            userdb_addusertomaildomain(user, user->maildomain);
                            break;
                        default:
                            user->maildomain = NULL;
                            Error("userdb", ERR_ERROR, "Replication error, Code: D-USER-E5");
                            break;
                    }
                }
                
                userdb_modifyuserindb(user);
                triggerhook(HOOK_USERDB_POSTMODIFYUSER, user);
            }
            
            if ( userdb_lastusertransactionid < transactionid )
                userdb_lastusertransactionid = transactionid;
            
            break;
            
        case USERDB_NOTFOUND:
            
            if ( (replicationflags & UDB_REPLICATION_DELETED) && userdb_repl_group->role == REPLICATOR_ROLE_SLAVE )
            {
                replicator_notifychange(userdb_repl_group, userdb_repl_obj_user, REPLICATOR_ACTION_NONE);
                return;
            }
            
            user = userdb_mallocuser();
            user->userid = userid;
            user->username = getsstring(username, USERDB_MAXUSERNAMELEN);
            memcpy(user->password, password, USERDB_PASSWORDHASHLEN);
            user->languageid = languageid;
            
            if ( user->languageid )
            {
                switch ( userdb_findlanguagebylanguageid(languageid, &(user->languagedata)) )
                {
                    case USERDB_OK:
                        break;
                    case USERDB_NOTFOUND:
                        user->languagedata = NULL;
                        break;
                    default:
                        user->languagedata = NULL;
                        Error("userdb", ERR_ERROR, "Replication error, Code: D-USER-E6");
                        break;
                }
            }
            else
            {
                user->languagedata = NULL;
            }
            
            user->emaillocal = getsstring(emaillocal, USERDB_EMAILLEN);
            user->statusflags = statusflags;
            user->groupflags = groupflags;
            user->optionflags = optionflags;
            user->noticeflags = noticeflags;
            
            if ( UDBOIsSuspended(user) )
            {
                user->suspensiondata = userdb_mallocsuspension();
                user->suspensiondata->suspendstart = suspendstart;
                user->suspensiondata->suspendend = suspendend;
                user->suspensiondata->reason = getsstring(reason, USERDB_SUSPENDREASONLEN);
                user->suspensiondata->suspenderid = suspenderid;
                
                if ( user->suspensiondata->suspenderid )
                {
                    if ( user->suspensiondata->suspenderid == user->userid )
                    {
                        user->suspensiondata->suspenderdata = user;
                    }
                    else
                    {
                        switch ( userdb_finduserbyuserid(user->suspensiondata->suspenderid, &(user->suspensiondata->suspenderdata)) )
                        {
                            case USERDB_OK:
                                break;
                            case USERDB_NOTFOUND:
                                user->suspensiondata->suspenderdata = NULL;
                                break;
                            default:
                                user->suspensiondata->suspenderdata = NULL;
                                Error("userdb", ERR_ERROR, "Replication error, Code: D-USER-E7");
                                break;
                        }
                    }
                }
                else
                {
                    user->suspensiondata->suspenderdata = NULL;
                }
            }
            
            user->replicationflags = replicationflags;
            user->created = created;
            user->modified = modified;
            user->revision = revision;
            user->transactionid = transactionid;
            
            if ( UDBRIsDeleted(user) )
            {
                user->maildomain = NULL;
                userdb_addusertodeletedhash(user);
                userdb_addusertodb(user);
            }
            else
            {
                if ( !ircd_strcmp(emaildomain, USERDB_NOMAILDOMAINSYMBOL) )
                {
                    user->maildomain = NULL;
                }
                else
                {
                    switch ( userdb_findmaildomainbyemaildomain(emaildomain, &(user->maildomain), 1) )
                    {
                        case USERDB_OK:
                            userdb_addusertomaildomain(user, user->maildomain);
                            break;
                        default:
                            user->maildomain = NULL;
                            Error("userdb", ERR_ERROR, "Replication error, Code: D-USER-E8");
                            break;
                    }
                }
                
                for ( i = 0; i < USERDB_USERSUSPENDEDHASHSIZE; i++ )
                    for ( tempuser = userdb_usersuspendedhashtable[i]; tempuser; tempuser = tempuser->suspensiondata->nextbyid )
                        if ( tempuser->suspensiondata->suspenderid == userid )
                            tempuser->suspensiondata->suspenderdata = user;
                
                userdb_assignusertoauth(user);
                userdb_addusertohash(user);
                
                if ( UDBOIsSuspended(user) )
                    userdb_addusertosuspendedhash(user);
                
                userdb_addusertodb(user);
                triggerhook(HOOK_USERDB_CREATEUSER, user);
            }
            
            if ( userdb_lastusertransactionid < transactionid )
                userdb_lastusertransactionid = transactionid;
            
            break;
            
        default:
            Error("userdb", ERR_ERROR, "Replication error, Code: D-USER-E9");
            break;
    }
    
    return;
}

void userdb_usercleanup(unsigned long long transactionid)
{
unsigned int i;
userdb_user *tempuser;

    for ( i = 0; i < USERDB_USERDELETEDHASHSIZE; i++ )
        for ( tempuser = userdb_userdeletedhashtable[i]; tempuser; tempuser = tempuser->nextbyid )
            if ( !(tempuser->databaseops) && tempuser->transactionid <= transactionid )
                userdb_deleteuserfromdb(tempuser);
}

int userdb_serialisemaildomainconfig(userdb_maildomainconfig *maildomainconfig, unsigned char *maildomainconfigstring, unsigned long *maildomainconfigstringlen)
{
char encodedconfigname[(USERDB_EMAILLEN * 2) + 1];

    if ( !maildomainconfig || !maildomainconfigstring || !maildomainconfigstringlen )
        return USERDB_BADPARAMETERS;
    
    if ( userdb_encodestring(maildomainconfig->configname->content, encodedconfigname, (USERDB_EMAILLEN * 2)) != USERDB_OK )
    {
        Error("userdb", ERR_ERROR, "Could not encode config name for %s (%lu) in userdb_serialisemaildomainconfig", maildomainconfig->configname->content, maildomainconfig->configid);
        return USERDB_ERROR;
    }
    
    encodedconfigname[(USERDB_EMAILLEN * 2)] = '\0';
    
    *maildomainconfigstringlen = snprintf((char *)maildomainconfigstring, REPLICATOR_MAXDATALEN, "%lu %s %hu %lu %lu %hu %ld %ld %lu %llu", maildomainconfig->configid, encodedconfigname, maildomainconfig->domainflags, maildomainconfig->maxlocal, maildomainconfig->maxdomain, maildomainconfig->replicationflags, maildomainconfig->created, maildomainconfig->modified, maildomainconfig->revision, maildomainconfig->transactionid);
    maildomainconfigstring[REPLICATOR_MAXDATALEN] = '\0';
    return USERDB_OK;
}

int userdb_unserialisemaildomainconfig(unsigned char *maildomainconfigstring, unsigned long *configid, char **configname, flag_t *domainflags, unsigned long *maxlocal, unsigned long *maxdomain, flag_t *replicationflags, time_t *created, time_t *modified, unsigned long *revision, unsigned long long *transactionid)
{
int cargc;
char *cargv[20];
static char tempconfigname[USERDB_EMAILLEN + 1];

    if ( !maildomainconfigstring || !configid || !configname || !domainflags || !maxlocal || !maxdomain || !created || !modified || !revision || !transactionid )
        return USERDB_BADPARAMETERS;
    
    cargc = splitline((char *)maildomainconfigstring, cargv, 20, 0);
    
    if ( cargc != 10 )
    {
        Error("userdb", ERR_ERROR, "Wrong number of values in serialised mail domain config: %s", maildomainconfigstring);
        return USERDB_ERROR;
    }
    
    if ( sscanf(cargv[0], "%lu", configid) != 1 )
        return USERDB_ERROR;
    
    if ( userdb_decodestring(cargv[1], tempconfigname, USERDB_EMAILLEN) != USERDB_OK )
    {
        Error("userdb", ERR_ERROR, "Could not decode config name for mail domain config %s in userdb_unserialisemaildomainconfig", cargv[0]);
        return USERDB_ERROR;
    }
    
    tempconfigname[USERDB_EMAILLEN] = '\0';
    *configname = tempconfigname;
    
    if ( sscanf(cargv[2], "%hu", domainflags) != 1 )
        return USERDB_ERROR;
    
    if ( sscanf(cargv[3], "%lu", maxlocal) != 1 )
        return USERDB_ERROR;
    
    if ( sscanf(cargv[4], "%lu", maxdomain) != 1 )
        return USERDB_ERROR;
    
    if ( sscanf(cargv[5], "%hu", replicationflags) != 1 )
        return USERDB_ERROR;
    
    if ( sscanf(cargv[6], "%ld", created) != 1 )
        return USERDB_ERROR;
    
    if ( sscanf(cargv[7], "%ld", modified) != 1 )
        return USERDB_ERROR;
    
    if ( sscanf(cargv[8], "%lu", revision) != 1 )
        return USERDB_ERROR;
    
    if ( sscanf(cargv[9], "%llu", transactionid) != 1 )
        return USERDB_ERROR;
    
    return USERDB_OK;
}

void userdb_maildomainconfigdbchange(userdb_maildomainconfig *maildomainconfig, unsigned int operation)
{
    if ( maildomainconfig->databaseops )
        return;
    
    replicator_notifychange(userdb_repl_group, userdb_repl_obj_maildomainconfig, operation);
}

void userdb_maildomainconfigrequest(replicator_node *node, unsigned long long transactionid)
{
unsigned char buf[REPLICATOR_MAXDATALEN + 1];
unsigned long buflen;
unsigned int i, moredata;
unsigned long long lasttransactionid;
userdb_maildomainconfig *maildomainconfigs[USERDB_MAXREPLICATIONBATCH], *tempmaildomainconfig;

    moredata = 0;
    memset(maildomainconfigs, 0, USERDB_MAXREPLICATIONBATCH * sizeof(userdb_maildomainconfig *));
    lasttransactionid = transactionid;
    
    for ( i = 0; i < USERDB_MAILDOMAINCONFIGHASHSIZE; i++ )
    {
        for ( tempmaildomainconfig = userdb_maildomainconfighashtable_id[i]; tempmaildomainconfig; tempmaildomainconfig = tempmaildomainconfig->nextbyid )
        {
            if ( tempmaildomainconfig->transactionid > transactionid && tempmaildomainconfig->transactionid <= ( transactionid + USERDB_MAXREPLICATIONBATCH ) )
                maildomainconfigs[tempmaildomainconfig->transactionid - transactionid - 1] = tempmaildomainconfig;
            else if ( !(tempmaildomainconfig->databaseops) && tempmaildomainconfig->transactionid > ( transactionid + USERDB_MAXREPLICATIONBATCH ) )
                moredata = 1;
        }
    }
    
    for ( i = 0; i < USERDB_MAILDOMAINCONFIGDELETEDHASHSIZE; i++ )
    {
        for ( tempmaildomainconfig = userdb_maildomainconfigdeletedhashtable[i]; tempmaildomainconfig; tempmaildomainconfig = tempmaildomainconfig->nextbyid )
        {
            if ( tempmaildomainconfig->transactionid > transactionid && tempmaildomainconfig->transactionid <= ( transactionid + USERDB_MAXREPLICATIONBATCH ) )
                maildomainconfigs[tempmaildomainconfig->transactionid - transactionid - 1] = tempmaildomainconfig;
            else if ( !(tempmaildomainconfig->databaseops) && tempmaildomainconfig->transactionid > ( transactionid + USERDB_MAXREPLICATIONBATCH ) )
                moredata = 1;
        }
    }
    
    for ( i = 0; i < USERDB_MAXREPLICATIONBATCH; i++ )
    {
        if ( maildomainconfigs[i] )
        {
            if ( maildomainconfigs[i]->databaseops )
            {
                replicator_sendCtoken(userdb_repl_group, userdb_repl_obj_maildomainconfig, node, lasttransactionid);
                return;
            }
            
            if ( userdb_serialisemaildomainconfig(maildomainconfigs[i], buf, &buflen) != USERDB_OK )
            {
                Error("userdb", ERR_ERROR, "Replication error, Code: R-MAILDOMAINCONFIG-E1");
                return;
            }
            
            if ( lasttransactionid < maildomainconfigs[i]->transactionid )
                lasttransactionid = maildomainconfigs[i]->transactionid;
            
            replicator_sendDtoken(userdb_repl_group, userdb_repl_obj_maildomainconfig, node, buf, buflen);
        }
    }
    
    if ( moredata )
        replicator_sendMtoken(userdb_repl_group, userdb_repl_obj_maildomainconfig, node, transactionid + USERDB_MAXREPLICATIONBATCH);
    else
        replicator_sendCtoken(userdb_repl_group, userdb_repl_obj_maildomainconfig, node, lasttransactionid);
    
    return;
}

void userdb_maildomainconfigreceive(unsigned char *maildomainconfigstring)
{
userdb_maildomain *maildomain, *tempmaildomain;
userdb_maildomainconfig *maildomainconfig;
unsigned long configid;
char *configname;
flag_t domainflags;
unsigned long maxlocal, maxdomain;
flag_t replicationflags;
time_t created, modified;
unsigned long revision;
unsigned long long transactionid;

    if ( userdb_unserialisemaildomainconfig(maildomainconfigstring, &configid, &configname, &domainflags, &maxlocal, &maxdomain, &replicationflags, &created, &modified, &revision, &transactionid) != USERDB_OK )
    {
        Error("userdb", ERR_ERROR, "Replication error, Code: D-MAILDOMAINCONFIG-E1");
        return;
    }
    
    switch ( userdb_findmaildomainconfigbyid(configid, &maildomainconfig) )
    {
        case USERDB_OK:
        
            if ( UDBRIsDeleted(maildomainconfig) )
            {
                Error("userdb", ERR_WARNING, "Replication warning, Code: D-MAILDOMAINCONFIG-W1");
                return;
            }
            
            if ( (replicationflags & UDB_REPLICATION_DELETED) )
                triggerhook(HOOK_USERDB_DELETEMDC, maildomainconfig);
            else
                triggerhook(HOOK_USERDB_PREMODIFYMDC, maildomainconfig);
            
            if ( ircd_strcmp(maildomainconfig->configname->content, configname) )
            {
                userdb_deletemaildomainconfigfromhash(maildomainconfig);
                freesstring(maildomainconfig->configname);
                maildomainconfig->configname = getsstring(configname, USERDB_EMAILLEN);
                userdb_addmaildomainconfigtohash(maildomainconfig);
            }
            
            maildomainconfig->domainflags = domainflags;
            maildomainconfig->maxlocal = maxlocal;
            maildomainconfig->maxdomain = maxdomain;
            maildomainconfig->replicationflags = replicationflags;
            maildomainconfig->created = created;
            maildomainconfig->modified = modified;
            maildomainconfig->revision = revision;
            maildomainconfig->transactionid = transactionid;
            
            if ( UDBRIsDeleted(maildomainconfig) )
            {
                userdb_deletemaildomainconfigfromhash(maildomainconfig);
                
                for ( tempmaildomain = maildomainconfig->maildomain; tempmaildomain; tempmaildomain = tempmaildomain->parent )
                    tempmaildomain->configcount--;
                
                maildomainconfig->maildomain->config = NULL;
                userdb_cleanmaildomaintree(maildomainconfig->maildomain);
                maildomainconfig->maildomain = NULL;
                
                userdb_addmaildomainconfigtodeletedhash(maildomainconfig);
                
                if ( userdb_repl_group->role == REPLICATOR_ROLE_SLAVE )
                    userdb_deletemaildomainconfigfromdb(maildomainconfig);
                else
                    userdb_modifymaildomainconfigindb(maildomainconfig);
            }
            else
            {
                switch ( userdb_findmaildomainbyemaildomain(configname, &maildomain, 1) )
                {
                    case USERDB_OK:
                        if ( maildomain == maildomainconfig->maildomain )
                            break;
                        
                        for ( tempmaildomain = maildomainconfig->maildomain; tempmaildomain; tempmaildomain = tempmaildomain->parent )
                            tempmaildomain->configcount--;
                        
                        maildomainconfig->maildomain->config = NULL;
                        userdb_cleanmaildomaintree(maildomainconfig->maildomain);
                        maildomainconfig->maildomain = NULL;
                        
                        for ( tempmaildomain = maildomain; tempmaildomain; tempmaildomain = tempmaildomain->parent )
                            tempmaildomain->configcount++;
                        
                        maildomain->config = maildomainconfig;
                        maildomainconfig->maildomain = maildomain;
                        
                        break;
                    default:
                        if ( maildomainconfig->maildomain )
                        {
                            for ( tempmaildomain = maildomainconfig->maildomain; tempmaildomain; tempmaildomain = tempmaildomain->parent )
                                tempmaildomain->configcount--;
                            
                            maildomainconfig->maildomain->config = NULL;
                            userdb_cleanmaildomaintree(maildomainconfig->maildomain);
                            maildomainconfig->maildomain = NULL;
                        }
                        
                        Error("userdb", ERR_ERROR, "Replication error, Code: D-MAILDOMAINCONFIG-E2");
                        break;
                }
                
                userdb_modifymaildomainconfigindb(maildomainconfig);
                triggerhook(HOOK_USERDB_POSTMODIFYMDC, maildomainconfig);
            }
            
            if ( userdb_lastmaildomainconfigtransactionid < transactionid )
                userdb_lastmaildomainconfigtransactionid = transactionid;
            
            break;
            
        case USERDB_NOTFOUND:
            
            if ( (replicationflags & UDB_REPLICATION_DELETED) && userdb_repl_group->role == REPLICATOR_ROLE_SLAVE )
            {
                replicator_notifychange(userdb_repl_group, userdb_repl_obj_maildomainconfig, REPLICATOR_ACTION_NONE);
                return;
            }
            
            maildomainconfig = userdb_mallocmaildomainconfig();
            maildomainconfig->configid = configid;
            maildomainconfig->configname = getsstring(configname, USERDB_EMAILLEN);
            maildomainconfig->domainflags = domainflags;
            maildomainconfig->maxlocal = maxlocal;
            maildomainconfig->maxdomain = maxdomain;
            maildomainconfig->replicationflags = replicationflags;
            maildomainconfig->created = created;
            maildomainconfig->modified = modified;
            maildomainconfig->revision = revision;
            maildomainconfig->transactionid = transactionid;
            
            if ( UDBRIsDeleted(maildomainconfig) )
            {
                userdb_addmaildomainconfigtodeletedhash(maildomainconfig);
                userdb_addmaildomainconfigtodb(maildomainconfig);
            }
            else
            {
                switch ( userdb_findmaildomainbyemaildomain(configname, &maildomain, 1) )
                {
                    case USERDB_OK:
                    
                        for ( tempmaildomain = maildomain; tempmaildomain; tempmaildomain = tempmaildomain->parent )
                            tempmaildomain->configcount++;
                        
                        maildomain->config = maildomainconfig;
                        maildomainconfig->maildomain = maildomain;
                        break;
                    default:
                        maildomainconfig->maildomain = NULL;
                        Error("userdb", ERR_ERROR, "Replication error, Code: D-MAILDOMAINCONFIG-E3");
                        break;
                }
                
                userdb_addmaildomainconfigtohash(maildomainconfig);
                userdb_addmaildomainconfigtodb(maildomainconfig);
                triggerhook(HOOK_USERDB_CREATEMDC, maildomainconfig);
            }
            
            if ( userdb_lastmaildomainconfigtransactionid < transactionid )
                userdb_lastmaildomainconfigtransactionid = transactionid;
            
            break;
            
        default:
            Error("userdb", ERR_ERROR, "Replication error, Code: D-MAILDOMAINCONFIG-E4");
            break;
    }
    
    return;
}

void userdb_maildomainconfigcleanup(unsigned long long transactionid)
{
unsigned int i;
userdb_maildomainconfig *tempmaildomainconfig;

    for ( i = 0; i < USERDB_MAILDOMAINCONFIGDELETEDHASHSIZE; i++ )
        for ( tempmaildomainconfig = userdb_maildomainconfigdeletedhashtable[i]; tempmaildomainconfig; tempmaildomainconfig = tempmaildomainconfig->nextbyid )
            if ( !(tempmaildomainconfig->databaseops) && tempmaildomainconfig->transactionid <= transactionid )
                userdb_deletemaildomainconfigfromdb(tempmaildomainconfig);
}

#endif
