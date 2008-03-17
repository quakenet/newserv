/*
 * UserDB
 */

#include "./userdb.h"

/* Hashing method taken from Q9 */

void userdb_inithashtables()
{
    memset(userdb_languagehashtable_id, 0, USERDB_LANGUAGEHASHSIZE * sizeof(userdb_language *));
    memset(userdb_languagehashtable_code, 0, USERDB_LANGUAGEHASHSIZE * sizeof(userdb_language *));
    memset(userdb_userhashtable_id, 0, USERDB_USERHASHSIZE * sizeof(userdb_user *));
    memset(userdb_userhashtable_username, 0, USERDB_USERHASHSIZE * sizeof(userdb_user *));
    memset(userdb_usersuspendedhashtable, 0, USERDB_USERSUSPENDEDHASHSIZE * sizeof(userdb_user *));
    memset(userdb_usertaginfohashtable, 0, USERDB_USERTAGINFOHASHSIZE * sizeof(userdb_usertaginfo *));
    memset(userdb_maildomainhashtable_name, 0, USERDB_MAILDOMAINHASHSIZE * sizeof(userdb_maildomain *));
    memset(userdb_maildomainconfighashtable_id, 0, USERDB_MAILDOMAINCONFIGHASHSIZE * sizeof(userdb_maildomainconfig *));
    memset(userdb_maildomainconfighashtable_name, 0, USERDB_MAILDOMAINCONFIGHASHSIZE * sizeof(userdb_maildomainconfig *));

#ifdef USERDB_ENABLEREPLICATION
    memset(userdb_languagedeletedhashtable, 0, USERDB_LANGUAGEDELETEDHASHSIZE * sizeof(userdb_language *));
    memset(userdb_userdeletedhashtable, 0, USERDB_USERDELETEDHASHSIZE * sizeof(userdb_user *));
    memset(userdb_maildomainconfigdeletedhashtable, 0, USERDB_MAILDOMAINCONFIGDELETEDHASHSIZE * sizeof(userdb_maildomainconfig *));
#endif
}

/* Language hashing */
void userdb_addlanguagetohash(userdb_language *language)
{
userdb_language **templanguage;
unsigned int hash;
#ifdef USERDB_ADDITIONALHASHCHECKS
int compareresult;
#endif

    if ( !language )
    {
        Error("userdb", ERR_FATAL, "Bad parameters for userdb_addlanguagetohash");
        assert(0);
        return;
    }
    
    /* Add it to the languageid hash */
    hash = userdb_getlanguageidhash(language->languageid);
    
    for ( templanguage = &(userdb_languagehashtable_id[hash]); *templanguage; templanguage = &((*templanguage)->nextbyid) )
    {
        if ( (*templanguage)->languageid == language->languageid )
        {
            Error("userdb", ERR_FATAL, "Hash collision detected for language %s (%lu) in userdb_languagehashtable_id hash table", language->languagecode->content, language->languageid);
            assert(0);
            return;
        }
        else if ( (*templanguage)->languageid > language->languageid )
        {
            language->nextbyid = (*templanguage);
            (*templanguage) = language;
            break;
        }
    }
    
    if ( !(*templanguage) )
    {
        language->nextbyid = NULL;
        (*templanguage) = language;
    }
    
    /* Add it to the languagecode hash */
    hash = userdb_getlanguagecodehash(language->languagecode->content);
#ifdef USERDB_ADDITIONALHASHCHECKS
    
    for ( templanguage = &(userdb_languagehashtable_code[hash]); *templanguage; templanguage = &((*templanguage)->nextbycode) )
    {
        compareresult = ircd_strcmp((*templanguage)->languagecode->content, language->languagecode->content);
        
        if ( !compareresult )
        {
            Error("userdb", ERR_FATAL, "Hash collision detected for language %s (%lu) in userdb_languagehashtable_code hash table", language->languagecode->content, language->languageid);
            assert(0);
            return;
        }
        else if ( compareresult > 0 )
        {
            language->nextbycode = (*templanguage);
            (*templanguage) = language;
            break;
        }
    }
    
    if ( !(*templanguage) )
    {
        language->nextbycode = NULL;
        (*templanguage) = language;
    }
#else
    language->nextbycode = userdb_languagehashtable_code[hash];
    userdb_languagehashtable_code[hash] = language;
#endif
    
    return;
}

int userdb_findlanguagebylanguageid(unsigned long languageid, userdb_language **language)
{
unsigned int hash;
userdb_language *templanguage;

    if ( !languageid || !language )
        return USERDB_BADPARAMETERS;
    
    hash = userdb_getlanguageidhash(languageid);
    
    for ( templanguage = userdb_languagehashtable_id[hash]; templanguage; templanguage = templanguage->nextbyid )
    {
        if ( templanguage->languageid == languageid )
        {
            *language = templanguage;
            return USERDB_OK;
        }
    }
    
    return USERDB_NOTFOUND;
}

int userdb_findlanguagebylanguagecode(char *languagecode, userdb_language **language)
{
unsigned int hash;
userdb_language *templanguage;

    if ( !languagecode || userdb_zerolengthstring(languagecode) || !language )
        return USERDB_BADPARAMETERS;
    
    hash = userdb_getlanguagecodehash(languagecode);
    
    for ( templanguage = userdb_languagehashtable_code[hash]; templanguage; templanguage = templanguage->nextbycode )
    {
        if ( !ircd_strcmp(languagecode, templanguage->languagecode->content) )
        {
            *language = templanguage;
            return USERDB_OK;
        }
    }
    
    return USERDB_NOTFOUND;
}

void userdb_deletelanguagefromhash(userdb_language *language)
{
unsigned int hash;
userdb_language **templanguage;
int found;

    if ( !language )
    {
        Error("userdb", ERR_FATAL, "Bad parameters for userdb_deletelanguagefromhash");
        assert(0);
        return;
    }
    
    found = 0;
    hash = userdb_getlanguageidhash(language->languageid);
    
    for ( templanguage = &(userdb_languagehashtable_id[hash]); *templanguage; templanguage = &((*templanguage)->nextbyid) )
    {
        if ( *templanguage == language )
        {
            *templanguage = language->nextbyid;
            found = 1;
            break;
        }
    }
    
    if ( !found )
    {
        Error("userdb", ERR_FATAL, "Could not delete language %s (%lu) from userdb_languagehashtable_id hash table", language->languagecode->content, language->languageid);
        assert(0);
        return;
    }
    
    found = 0;
    hash = userdb_getlanguagecodehash(language->languagecode->content);
    
    for ( templanguage = &(userdb_languagehashtable_code[hash]); *templanguage; templanguage = &((*templanguage)->nextbycode) )
    {
        if ( *templanguage == language )
        {
            *templanguage = language->nextbycode;
            found = 1;
            break;
        }
    }
    
    if ( !found )
    {
        Error("userdb", ERR_FATAL, "Could not delete language %s (%lu) from userdb_languagehashtable_code hash table", language->languagecode->content, language->languageid);
        assert(0);
        return;
    }
    
    return;
}

/* Username hashing */
void userdb_addusertohash(userdb_user *user)
{
userdb_user **tempuser;
unsigned int hash;
#ifdef USERDB_ADDITIONALHASHCHECKS
int compareresult;
#endif

    if ( !user )
    {
        Error("userdb", ERR_FATAL, "Bad parameters for userdb_addusertohash");
        assert(0);
        return;
    }
    
    /* Add it to the userid hash */
    hash = userdb_getuseridhash(user->userid);
    
    for ( tempuser = &(userdb_userhashtable_id[hash]); *tempuser; tempuser = &((*tempuser)->nextbyid) )
    {
        if ( (*tempuser)->userid == user->userid )
        {
            Error("userdb", ERR_FATAL, "Hash collision detected for user %s (%lu) in userdb_userhashtable_id hash table", user->username->content, user->userid);
            assert(0);
            return;
        }
        else if ( (*tempuser)->userid > user->userid )
        {
            user->nextbyid = (*tempuser);
            (*tempuser) = user;
            break;
        }
    }
    
    if ( !(*tempuser) )
    {
        user->nextbyid = NULL;
        (*tempuser) = user;
    }
    
    /* Add it to the username hash */
    hash = userdb_getusernamehash(user->username->content);
#ifdef USERDB_ADDITIONALHASHCHECKS
    
    for ( tempuser = &(userdb_userhashtable_username[hash]); *tempuser; tempuser = &((*tempuser)->nextbyname) )
    {
        compareresult = ircd_strcmp((*tempuser)->username->content, user->username->content);
        
        if ( !compareresult )
        {
            Error("userdb", ERR_FATAL, "Hash collision detected for user %s (%lu) in userdb_userhashtable_username hash table", user->username->content, user->userid);
            assert(0);
            return;
        }
        else if ( compareresult > 0 )
        {
            user->nextbyname = (*tempuser);
            (*tempuser) = user;
            break;
        }
    }
    
    if ( !(*tempuser) )
    {
        user->nextbyname = NULL;
        (*tempuser) = user;
    }
#else
    user->nextbyname = userdb_userhashtable_username[hash];
    userdb_userhashtable_username[hash] = user;
#endif
    
    return;
}

int userdb_finduserbyuserid(unsigned long userid, userdb_user **user)
{
unsigned int hash;
userdb_user *tempuser;

    if ( !userid || !user )
        return USERDB_BADPARAMETERS;
    
    hash = userdb_getuseridhash(userid);
    
    for ( tempuser = userdb_userhashtable_id[hash]; tempuser; tempuser = tempuser->nextbyid )
    {
        if ( tempuser->userid == userid )
        {
            *user = tempuser;
            return USERDB_OK;
        }
    }
    
    return USERDB_NOTFOUND;
}

int userdb_finduserbyusername(char *username, userdb_user **user)
{
unsigned int hash;
userdb_user *tempuser;

    if ( !username || userdb_zerolengthstring(username) || !user )
        return USERDB_BADPARAMETERS;
    
    hash = userdb_getusernamehash(username);
    
    for ( tempuser = userdb_userhashtable_username[hash]; tempuser; tempuser = tempuser->nextbyname )
    {
        if ( !ircd_strcmp(username, tempuser->username->content) )
        {
            *user = tempuser;
            return USERDB_OK;
        }
    }
    
    return USERDB_NOTFOUND;
}

int userdb_finduserbystring(char *userstring, userdb_user **user)
{
nick *nn;
unsigned long userid;

    if ( !userstring || userdb_zerolengthstring(userstring) || !user )
        return USERDB_BADPARAMETERS;
    else if ( userdb_usernamestring(userstring) )
    {
        if ( userdb_zerolengthstring(userstring + 1) )
            return USERDB_BADUSERNAME;
        
        return userdb_finduserbyusername(userstring + 1, user);
    }
    else if ( userdb_useridstring(userstring) )
    {
        if ( userdb_isnumericstring(userstring + 1) != USERDB_OK )
            return USERDB_BADUSERID;
        
        userid = strtoul(userstring + 1, NULL, 10);
        
        if ( !userid )
            return USERDB_BADUSERID;
        
        return userdb_finduserbyuserid(userid, user);
    }
    else
    {
        nn = getnickbynick(userstring);
        
        if ( !nn )
            return USERDB_NOTONLINE;
        
        return userdb_finduserbynick(nn, user);
    }
}

void userdb_deleteuserfromhash(userdb_user *user)
{
unsigned int hash;
userdb_user **tempuser;
int found;

    if ( !user )
    {
        Error("userdb", ERR_FATAL, "Bad parameters for userdb_deleteuserfromhash");
        assert(0);
        return;
    }
    
    found = 0;
    hash = userdb_getuseridhash(user->userid);
    
    for ( tempuser = &(userdb_userhashtable_id[hash]); *tempuser; tempuser = &((*tempuser)->nextbyid) )
    {
        if ( *tempuser == user )
        {
            *tempuser = user->nextbyid;
            found = 1;
            break;
        }
    }
    
    if ( !found )
    {
        Error("userdb", ERR_FATAL, "Could not delete user %s (%lu) from userdb_userhashtable_id hash table", user->username->content, user->userid);
        assert(0);
        return;
    }
    
    found = 0;
    hash = userdb_getusernamehash(user->username->content);
    
    for ( tempuser = &(userdb_userhashtable_username[hash]); *tempuser; tempuser = &((*tempuser)->nextbyname) )
    {
        if ( *tempuser == user )
        {
            *tempuser = user->nextbyname;
            found = 1;
            break;
        }
    }
    
    if ( !found )
    {
        Error("userdb", ERR_FATAL, "Could not delete user %s (%lu) from userdb_userhashtable_username hash table", user->username->content, user->userid);
        assert(0);
        return;
    }
    
    return;
}

void userdb_addusertaginfotohash(userdb_usertaginfo *usertaginfo)
{
unsigned int hash;
#ifdef USERDB_ADDITIONALHASHCHECKS
userdb_usertaginfo **tempusertaginfo;
#endif

    if ( !usertaginfo )
    {
        Error("userdb", ERR_FATAL, "Bad parameters for userdb_addusertaginfotohash");
        assert(0);
        return;
    }
    
    hash = userdb_getusertaginfohash(usertaginfo->tagid);
#ifdef USERDB_ADDITIONALHASHCHECKS
    
    for ( tempusertaginfo = &(userdb_usertaginfohashtable[hash]); *tempusertaginfo; tempusertaginfo = &((*tempusertaginfo)->nextbyid) )
    {
        if ( (*tempusertaginfo)->tagid == usertaginfo->tagid )
        {
            Error("userdb", ERR_FATAL, "Hash collision detected for usertaginfo %hu in userdb_usertaginfohashtable hash table", usertaginfo->tagid);
            assert(0);
            return;
        }
        else if ( (*tempusertaginfo)->tagid > usertaginfo->tagid )
        {
            usertaginfo->nextbyid = (*tempusertaginfo);
            (*tempusertaginfo) = usertaginfo;
            break;
        }
    }
    
    if ( !(*tempusertaginfo) )
    {
        usertaginfo->nextbyid = NULL;
        (*tempusertaginfo) = usertaginfo;
    }
#else
    usertaginfo->nextbyid = userdb_usertaginfohashtable[hash];
    userdb_usertaginfohashtable[hash] = usertaginfo;
#endif
    
    return;
}

int userdb_findusertaginfo(unsigned short tagid, userdb_usertaginfo **usertaginfo)
{
unsigned int hash;
userdb_usertaginfo *tempusertaginfo;

    if ( !tagid || !usertaginfo )
        return USERDB_BADPARAMETERS;
    
    hash = userdb_getusertaginfohash(tagid);
    
    for ( tempusertaginfo = userdb_usertaginfohashtable[hash]; tempusertaginfo; tempusertaginfo = tempusertaginfo->nextbyid )
    {
        if ( tempusertaginfo->tagid == tagid )
        {
            *usertaginfo = tempusertaginfo;
            return USERDB_OK;
        }
    }
    
    return USERDB_NOTFOUND;
}

void userdb_deleteusertaginfofromhash(userdb_usertaginfo *usertaginfo)
{
unsigned int hash;
userdb_usertaginfo **tempusertaginfo;
int found;

    if ( !usertaginfo )
    {
        Error("userdb", ERR_FATAL, "Bad parameters for userdb_deleteusertaginfofromhash");
        assert(0);
        return;
    }
    
    found = 0;
    hash = userdb_getusertaginfohash(usertaginfo->tagid);
    
    for ( tempusertaginfo = &(userdb_usertaginfohashtable[hash]); *tempusertaginfo; tempusertaginfo = &((*tempusertaginfo)->nextbyid) )
    {
        if ( *tempusertaginfo == usertaginfo )
        {
            *tempusertaginfo = usertaginfo->nextbyid;
            found = 1;
            break;
        }
    }
    
    if ( !found )
    {
        Error("userdb", ERR_FATAL, "Could not delete usertaginfo %hu from userdb_usertaginfohashtable hash table", usertaginfo->tagid);
        assert(0);
        return;
    }
}

void userdb_addusertagdatatohash(userdb_usertagdata *usertagdata)
{
unsigned int hash;
#ifdef USERDB_ADDITIONALHASHCHECKS
userdb_usertagdata **tempusertagdata;
#endif

    if ( !usertagdata || !usertagdata->taginfo || !usertagdata->userinfo )
    {
        Error("userdb", ERR_FATAL, "Bad parameters for userdb_addusertagdatatohash");
        assert(0);
        return;
    }
    
    hash = userdb_getusertagdatahash(usertagdata->userinfo->userid);
#ifdef USERDB_ADDITIONALHASHCHECKS
    
    for ( tempusertagdata = &(usertagdata->taginfo->usertags[hash]); *tempusertagdata; tempusertagdata = &((*tempusertagdata)->nextbytag) )
    {
        if ( (*tempusertagdata)->userinfo->userid == usertagdata->userinfo->userid )
        {
            Error("userdb", ERR_FATAL, "Hash collision detected for usertagdata (%hu,%lu) in usertaginfo->usertags hash table", usertagdata->taginfo->tagid, usertagdata->userinfo->userid);
            assert(0);
            return;
        }
        else if ( (*tempusertagdata)->userinfo->userid > usertagdata->userinfo->userid )
        {
            usertagdata->nextbytag = (*tempusertagdata);
            (*tempusertagdata) = usertagdata;
            break;
        }
    }
    
    if ( !(*tempusertagdata) )
    {
        usertagdata->nextbytag = NULL;
        (*tempusertagdata) = usertagdata;
    }
#else
    usertagdata->nextbytag = usertagdata->taginfo->usertags[hash];
    usertagdata->taginfo->usertags[hash] = usertagdata;
#endif
    
#ifdef USERDB_ADDITIONALHASHCHECKS
    for ( tempusertagdata = &(usertagdata->userinfo->usertags); *tempusertagdata; tempusertagdata = &((*tempusertagdata)->nextbyuser) )
    {
        if ( (*tempusertagdata)->taginfo->tagid == usertagdata->taginfo->tagid )
        {
            Error("userdb", ERR_FATAL, "Link list collision detected for usertagdata (%hu,%lu) in user->usertags link list", usertagdata->taginfo->tagid, usertagdata->userinfo->userid);
            assert(0);
            return;
        }
        else if ( (*tempusertagdata)->taginfo->tagid > usertagdata->taginfo->tagid )
        {
            usertagdata->nextbyuser = (*tempusertagdata);
            (*tempusertagdata) = usertagdata;
            break;
        }
    }
    
    if ( !(*tempusertagdata) )
    {
        usertagdata->nextbyuser = NULL;
        (*tempusertagdata) = usertagdata;
    }
#else
    usertagdata->nextbyuser = usertagdata->userinfo->usertags;
    usertagdata->userinfo->usertags = usertagdata;
#endif
    
    return;
}

int userdb_findusertagdata(userdb_usertaginfo *usertaginfo, userdb_user *user, userdb_usertagdata **usertagdata)
{
#ifdef USERDB_USEUSERINFOHASHFORLOOKUP
unsigned int hash;
#endif
userdb_usertagdata *tempusertagdata;

    if ( !usertaginfo || !user || !usertagdata )
        return USERDB_BADPARAMETERS;
    
#ifdef USERDB_USEUSERINFOHASHFORLOOKUP
    hash = userdb_getusertagdatahash(user->userid);
    
    for ( tempusertagdata = usertaginfo->usertags[hash]; tempusertagdata; tempusertagdata = tempusertagdata->nextbytag )
    {
        if ( tempusertagdata->userinfo == user )
        {
            *usertagdata = tempusertagdata;
            return USERDB_OK;
        }
    }
#else
    for ( tempusertagdata = user->usertags; tempusertagdata; tempusertagdata = tempusertagdata->nextbyuser )
    {
        if ( tempusertagdata->taginfo == usertaginfo )
        {
            *usertagdata = tempusertagdata;
            return USERDB_OK;
        }
    }
#endif
    
    return USERDB_NOTFOUND;
}

void userdb_deleteusertagdatafromhash(userdb_usertagdata *usertagdata)
{
unsigned int hash;
userdb_usertagdata **tempusertagdata;
int found;

    if ( !usertagdata || !usertagdata->taginfo || !usertagdata->userinfo )
    {
        Error("userdb", ERR_FATAL, "Bad parameters for userdb_deleteusertagdatafromhash");
        assert(0);
        return;
    }
    
    found = 0;
    hash = userdb_getusertagdatahash(usertagdata->userinfo->userid);
    
    for ( tempusertagdata = &(usertagdata->taginfo->usertags[hash]); *tempusertagdata; tempusertagdata = &((*tempusertagdata)->nextbytag) )
    {
        if ( *tempusertagdata == usertagdata )
        {
            *tempusertagdata = usertagdata->nextbytag;
            found = 1;
            break;
        }
    }
    
    if ( !found )
    {
        Error("userdb", ERR_FATAL, "Could not delete usertagdata (%hu,%lu) from usertaginfo->usertags hash table", usertagdata->taginfo->tagid, usertagdata->userinfo->userid);
        assert(0);
        return;
    }
    
    found = 0;
    
    for ( tempusertagdata = &(usertagdata->userinfo->usertags); *tempusertagdata; tempusertagdata = &((*tempusertagdata)->nextbyuser) )
    {
        if ( *tempusertagdata == usertagdata )
        {
            *tempusertagdata = usertagdata->nextbyuser;
            found = 1;
            break;
        }
    }
    
    if ( !found )
    {
        Error("userdb", ERR_FATAL, "Could not delete usertagdata (%hu,%lu) from user->usertags linked list", usertagdata->taginfo->tagid, usertagdata->userinfo->userid);
        assert(0);
        return;
    }
    
    return;
}

void userdb_deleteallusertagdatabyinfofromhash(userdb_usertaginfo *usertaginfo)
{
unsigned int i;
userdb_usertagdata **tempusertagdata, *nextusertagdata;
int found;

    if ( !usertaginfo )
    {
        Error("userdb", ERR_FATAL, "Bad parameters for userdb_deleteallusertagdatabyinfofromhash");
        assert(0);
        return;
    }
    
    for ( i = 0; i < USERDB_USERTAGDATAHASHSIZE; i++ )
    {
        while ( usertaginfo->usertags[i] )
        {
            found = 0;
            
            for ( tempusertagdata = &(usertaginfo->usertags[i]->userinfo->usertags); *tempusertagdata; tempusertagdata = &((*tempusertagdata)->nextbyuser) )
            {
                if ( *tempusertagdata == usertaginfo->usertags[i] )
                {
                    *tempusertagdata = usertaginfo->usertags[i]->nextbyuser;
                    found = 1;
                    break;
                }
            }
            
            if ( !found )
            {
                Error("userdb", ERR_FATAL, "Could not delete usertagdata (%hu,%lu) from user->usertags linked list", usertaginfo->tagid, usertaginfo->usertags[i]->userinfo->userid);
                assert(0);
                return;
            }
            
            nextusertagdata = usertaginfo->usertags[i]->nextbytag;
            userdb_freeusertagdata(usertaginfo->usertags[i]);
            usertaginfo->usertags[i] = nextusertagdata;
        }
    }
    
    return;
}

void userdb_deleteallusertagdatabyuserfromhash(userdb_user *user)
{
unsigned int hash;
userdb_usertagdata **tempusertagdata, *nextusertagdata;
int found;

    if ( !user )
    {
        Error("userdb", ERR_FATAL, "Bad parameters for userdb_deleteallusertagdatabyuserfromhash");
        assert(0);
        return;
    }
    
    hash = userdb_getusertagdatahash(user->userid);
    
    while ( user->usertags )
    {
        found = 0;
        
        for ( tempusertagdata = &(user->usertags->taginfo->usertags[hash]); *tempusertagdata; tempusertagdata = &((*tempusertagdata)->nextbytag) )
        {
            if ( *tempusertagdata == user->usertags )
            {
                *tempusertagdata = user->usertags->nextbytag;
                found = 1;
                break;
            }
        }
        
        if ( !found )
        {
            Error("userdb", ERR_FATAL, "Could not delete usertagdata (%hu,%lu) from usertaginfo->usertags hash table", user->usertags->taginfo->tagid, user->userid);
            assert(0);
            return;
        }
        
        nextusertagdata = user->usertags->nextbyuser;
        userdb_freeusertagdata(user->usertags);
        user->usertags = nextusertagdata;
    }
    
    return;
}

/* Maildomain hashing */
void userdb_addmaildomaintohash(userdb_maildomain *maildomain)
{
unsigned int hash;
#ifdef USERDB_ADDITIONALHASHCHECKS
userdb_maildomain **tempmaildomain;
int compareresult;
#endif

    if ( !maildomain )
    {
        Error("userdb", ERR_FATAL, "Bad parameters for userdb_addmaildomaintohash");
        assert(0);
        return;
    }
    
    hash = userdb_getmaildomainnamehash(maildomain->maildomainname->content);
#ifdef USERDB_ADDITIONALHASHCHECKS
    
    for ( tempmaildomain = &(userdb_maildomainhashtable_name[hash]); *tempmaildomain; tempmaildomain = &((*tempmaildomain)->nextbyname) )
    {
        if ( (*tempmaildomain)->parent != maildomain->parent )
            continue;
        
        compareresult = ircd_strcmp((*tempmaildomain)->maildomainname->content, maildomain->maildomainname->content);
        
        if ( !compareresult )
        {
            Error("userdb", ERR_FATAL, "Hash collision detected for maildomain %s in userdb_maildomainhashtable_name hash table", maildomain->maildomainname->content);
            assert(0);
            return;
        }
        else if ( compareresult > 0 )
        {
            maildomain->nextbyname = (*tempmaildomain);
            (*tempmaildomain) = maildomain;
            break;
        }
    }
    
    if ( !(*tempmaildomain) )
    {
        maildomain->nextbyname = NULL;
        (*tempmaildomain) = maildomain;
    }
#else
    maildomain->nextbyname = userdb_maildomainhashtable_name[hash];
    userdb_maildomainhashtable_name[hash] = maildomain;
#endif
    
    return;
}

int userdb_findmaildomainbyname(char *maildomainname, userdb_maildomain *parent, userdb_maildomain **maildomain)
{
unsigned int hash;
userdb_maildomain *tempmaildomain;

    if ( !maildomainname || userdb_zerolengthstring(maildomainname) || !maildomain )
        return USERDB_BADPARAMETERS;
    
    hash = userdb_getmaildomainnamehash(maildomainname);
    
    for ( tempmaildomain = userdb_maildomainhashtable_name[hash]; tempmaildomain; tempmaildomain = tempmaildomain->nextbyname )
    {
        if ( tempmaildomain->parent == parent && !ircd_strcmp(maildomainname, tempmaildomain->maildomainname->content) )
        {
            *maildomain = tempmaildomain;
            return USERDB_OK;
        }
    }
    
    return USERDB_NOTFOUND;
}

int userdb_findmaildomainbyemaildomain(char *emaildomain, userdb_maildomain **maildomain, int allowcreate)
{
char *tempdomainpart;
int searching;
userdb_maildomain *tempmaildomain, *newmaildomain;

    if ( !emaildomain || userdb_zerolengthstring(emaildomain) || !maildomain )
        return USERDB_BADPARAMETERS;
    
    tempdomainpart = NULL;
    tempmaildomain = NULL;
    newmaildomain = NULL;
    searching = 1;
    
    switch ( userdb_splitemaildomainfirst(emaildomain, &tempdomainpart) )
    {
        case USERDB_OK:
            break;
        case USERDB_BADEMAIL:
            return USERDB_BADEMAIL;
        default:
            return USERDB_ERROR;
    }
    
    while ( searching )
    {
        switch ( userdb_findmaildomainbyname(tempdomainpart, tempmaildomain, &tempmaildomain) )
        {
            case USERDB_OK:
                break;
            case USERDB_NOTFOUND:
                
                if ( allowcreate )
                {
                    /* Stop searching and break the loop */
                    searching = 0;
                    break;
                }
                else
                {
                    /* Return the deepest part of the tree */
                    *maildomain = tempmaildomain;
                    return USERDB_NOTFOUND;
                }
                
            case USERDB_ERROR:
            default:
                return USERDB_ERROR;
        }
        
        if ( !searching )
            break;
        
        if ( userdb_splitemaildomainnext(tempdomainpart, &tempdomainpart) != USERDB_OK )
            return USERDB_ERROR;
        
        if ( !tempdomainpart )
        {
            /* Return the exact domain */
            *maildomain = tempmaildomain;
            return USERDB_OK;
        }
    }
    
    /* If we have got here, we want to extend the tree */
    
    while ( tempdomainpart )
    {
        newmaildomain = userdb_mallocmaildomain();
        newmaildomain->maildomainname = getsstring(tempdomainpart, USERDB_EMAILLEN);
        newmaildomain->parent = tempmaildomain;
        userdb_addmaildomaintohash(newmaildomain);
        
        if ( userdb_splitemaildomainnext(tempdomainpart, &tempdomainpart) != USERDB_OK )
            return USERDB_ERROR;
        
        if ( !tempdomainpart )
        {
            /* Return the exact domain */
            *maildomain = newmaildomain;
            return USERDB_OK;
        }
        
        tempmaildomain = newmaildomain;
        newmaildomain = NULL;
    }
    
    return USERDB_ERROR;
}

void userdb_addusertomaildomain(userdb_user *user, userdb_maildomain *maildomain)
{
userdb_maildomain *tempmaildomain;
#ifdef USERDB_ADDITIONALHASHCHECKS
userdb_user **tempuser;
#endif

    if ( !user || !maildomain )
    {
        Error("userdb", ERR_FATAL, "Bad parameters for userdb_addusertomaildomain");
        assert(0);
        return;
    }
    
#ifdef USERDB_ADDITIONALHASHCHECKS
    for ( tempuser = &(maildomain->users); *tempuser; tempuser = &((*tempuser)->nextbymaildomain) )
    {
        if ( (*tempuser)->userid == user->userid )
        {
            Error("userdb", ERR_FATAL, "Link list collision detected for user %s (%lu) in maildomain->users link list", user->username->content, user->userid);
            assert(0);
            return;
        }
        else if ( (*tempuser)->userid > user->userid )
        {
            user->nextbymaildomain = (*tempuser);
            (*tempuser) = user;
            break;
        }
    }
    
    if ( !(*tempuser) )
    {
        user->nextbymaildomain = NULL;
        (*tempuser) = user;
    }
#else
    user->nextbymaildomain = maildomain->users;
    maildomain->users = user;
#endif
    
    maildomain->domaincount++;
    
    for ( tempmaildomain = maildomain; tempmaildomain; tempmaildomain = tempmaildomain->parent )
        tempmaildomain->totaldomaincount++;
    
    return;
}

void userdb_deleteuserfrommaildomain(userdb_user *user, userdb_maildomain *maildomain)
{
userdb_maildomain *tempmaildomain;
userdb_user **tempuser;
int found;

    if ( !user || !maildomain )
    {
        Error("userdb", ERR_FATAL, "Bad parameters for userdb_deleteuserfrommaildomain");
        assert(0);
        return;
    }
    
    found = 0;
    
    for ( tempuser = &(maildomain->users); *tempuser; tempuser = &((*tempuser)->nextbymaildomain) )
    {
        if ( *tempuser == user )
        {
            *tempuser = user->nextbymaildomain;
            found = 1;
            
            maildomain->domaincount--;
            
            for ( tempmaildomain = maildomain; tempmaildomain; tempmaildomain = tempmaildomain->parent )
                tempmaildomain->totaldomaincount--;
            
            break;
        }
    }
    
    if ( !found )
    {
        Error("userdb", ERR_FATAL, "Could not delete user %s (%lu) from maildomain->users link list", user->username->content, user->userid);
        assert(0);
        return;
    }
    
    return;
}

void userdb_cleanmaildomaintree(userdb_maildomain *maildomain)
{
userdb_maildomain *tempmaildomain, *freemaildomain;

    if ( !maildomain )
    {
        Error("userdb", ERR_FATAL, "Bad parameters for userdb_cleanmaildomaintree");
        assert(0);
        return;
    }
    
    tempmaildomain = maildomain;
    
    while ( tempmaildomain )
    {
        if ( (tempmaildomain->totaldomaincount) || (tempmaildomain->configcount) )
            return;
        
        freemaildomain = tempmaildomain;
        tempmaildomain = tempmaildomain->parent;
        userdb_deletemaildomainfromhash(freemaildomain);
        userdb_freemaildomain(freemaildomain);
    }
    
    return;
}

void userdb_deletemaildomainfromhash(userdb_maildomain *maildomain)
{
unsigned int hash;
userdb_maildomain **tempmaildomain;
int found;

    if ( !maildomain )
    {
        Error("userdb", ERR_FATAL, "Bad parameters for userdb_deletemaildomainfromhash");
        assert(0);
        return;
    }
    
    found = 0;
    hash = userdb_getmaildomainnamehash(maildomain->maildomainname->content);
    
    for ( tempmaildomain = &(userdb_maildomainhashtable_name[hash]); *tempmaildomain; tempmaildomain = &((*tempmaildomain)->nextbyname) )
    {
        if ( *tempmaildomain == maildomain )
        {
            *tempmaildomain = maildomain->nextbyname;
            found = 1;
            break;
        }
    }
    
    if ( !found )
    {
        Error("userdb", ERR_FATAL, "Could not delete maildomain %s from userdb_maildomainhashtable_name hash table", maildomain->maildomainname->content);
        assert(0);
        return;
    }
    
    return;
}

/* Mail domain config hashing */
void userdb_addmaildomainconfigtohash(userdb_maildomainconfig *maildomainconfig)
{
userdb_maildomainconfig **tempmaildomainconfig;
unsigned int hash;
#ifdef USERDB_ADDITIONALHASHCHECKS
int compareresult;
#endif

    if ( !maildomainconfig )
    {
        Error("userdb", ERR_FATAL, "Bad parameters for userdb_addmaildomainconfigtohash");
        assert(0);
        return;
    }
    
    /* Add it to the maildomainconfig id hash */
    hash = userdb_getmaildomainconfigidhash(maildomainconfig->configid);
    
    for ( tempmaildomainconfig = &(userdb_maildomainconfighashtable_id[hash]); *tempmaildomainconfig; tempmaildomainconfig = &((*tempmaildomainconfig)->nextbyid) )
    {
        if ( (*tempmaildomainconfig)->configid == maildomainconfig->configid )
        {
            Error("userdb", ERR_FATAL, "Hash collision detected for maildomainconfig %s (%lu) in userdb_maildomainconfighashtable_id hash table", maildomainconfig->configname->content, maildomainconfig->configid);
            assert(0);
            return;
        }
        else if ( (*tempmaildomainconfig)->configid > maildomainconfig->configid )
        {
            maildomainconfig->nextbyid = (*tempmaildomainconfig);
            (*tempmaildomainconfig) = maildomainconfig;
            break;
        }
    }
    
    if ( !(*tempmaildomainconfig) )
    {
        maildomainconfig->nextbyid = NULL;
        (*tempmaildomainconfig) = maildomainconfig;
    }
    
    /* Add it to the maildomain name hash */
    hash = userdb_getmaildomainconfignamehash(maildomainconfig->configname->content);
#ifdef USERDB_ADDITIONALHASHCHECKS
    
    for ( tempmaildomainconfig = &(userdb_maildomainconfighashtable_name[hash]); *tempmaildomainconfig; tempmaildomainconfig = &((*tempmaildomainconfig)->nextbyname) )
    {
        compareresult = ircd_strcmp((*tempmaildomainconfig)->configname->content, maildomainconfig->configname->content);
        
        if ( !compareresult )
        {
            Error("userdb", ERR_FATAL, "Hash collision detected for maildomainconfig %s (%lu) in userdb_maildomainconfighashtable_name hash table", maildomainconfig->configname->content, maildomainconfig->configid);
            assert(0);
            return;
        }
        else if ( compareresult > 0 )
        {
            maildomainconfig->nextbyname = (*tempmaildomainconfig);
            (*tempmaildomainconfig) = maildomainconfig;
            break;
        }
    }
    
    if ( !(*tempmaildomainconfig) )
    {
        maildomainconfig->nextbyname = NULL;
        (*tempmaildomainconfig) = maildomainconfig;
    }
#else
    maildomainconfig->nextbyname = userdb_maildomainconfighashtable_name[hash];
    userdb_maildomainconfighashtable_name[hash] = maildomainconfig;
#endif
    
    return;
}

int userdb_findmaildomainconfigbyid(unsigned long configid, userdb_maildomainconfig **maildomainconfig)
{
unsigned int hash;
userdb_maildomainconfig *tempmaildomainconfig;

    if ( !configid || !maildomainconfig )
        return USERDB_BADPARAMETERS;
    
    hash = userdb_getmaildomainconfigidhash(configid);
    
    for ( tempmaildomainconfig = userdb_maildomainconfighashtable_id[hash]; tempmaildomainconfig; tempmaildomainconfig = tempmaildomainconfig->nextbyid )
    {
        if ( tempmaildomainconfig->configid == configid )
        {
            *maildomainconfig = tempmaildomainconfig;
            return USERDB_OK;
        }
    }
    
    return USERDB_NOTFOUND;
}

int userdb_findmaildomainconfigbyname(char *configname, userdb_maildomainconfig **maildomainconfig)
{
unsigned int hash;
userdb_maildomainconfig *tempmaildomainconfig;

    if ( !configname || userdb_zerolengthstring(configname) || !maildomainconfig )
        return USERDB_BADPARAMETERS;
    
    hash = userdb_getmaildomainconfignamehash(configname);
    
    for ( tempmaildomainconfig = userdb_maildomainconfighashtable_name[hash]; tempmaildomainconfig; tempmaildomainconfig = tempmaildomainconfig->nextbyname )
    {
        if ( !ircd_strcmp(configname, tempmaildomainconfig->configname->content) )
        {
            *maildomainconfig = tempmaildomainconfig;
            return USERDB_OK;
        }
    }
    
    return USERDB_NOTFOUND;
}

void userdb_deletemaildomainconfigfromhash(userdb_maildomainconfig *maildomainconfig)
{
unsigned int hash;
userdb_maildomainconfig **tempmaildomainconfig;
int found;

    if ( !maildomainconfig )
    {
        Error("userdb", ERR_FATAL, "Bad parameters for userdb_deletemaildomainconfigfromhash");
        assert(0);
        return;
    }
    
    found = 0;
    hash = userdb_getmaildomainconfigidhash(maildomainconfig->configid);
    
    for ( tempmaildomainconfig = &(userdb_maildomainconfighashtable_id[hash]); *tempmaildomainconfig; tempmaildomainconfig = &((*tempmaildomainconfig)->nextbyid) )
    {
        if ( *tempmaildomainconfig == maildomainconfig )
        {
            *tempmaildomainconfig = maildomainconfig->nextbyid;
            found = 1;
            break;
        }
    }
    
    if ( !found )
    {
        Error("userdb", ERR_FATAL, "Could not delete maildomainconfig %s (%lu) from userdb_maildomainconfighashtable_id hash table", maildomainconfig->configname->content, maildomainconfig->configid);
        assert(0);
        return;
    }
    
    found = 0;
    hash = userdb_getmaildomainconfignamehash(maildomainconfig->configname->content);
    
    for ( tempmaildomainconfig = &(userdb_maildomainconfighashtable_name[hash]); *tempmaildomainconfig; tempmaildomainconfig = &((*tempmaildomainconfig)->nextbyname) )
    {
        if ( *tempmaildomainconfig == maildomainconfig )
        {
            *tempmaildomainconfig = maildomainconfig->nextbyname;
            found = 1;
            break;
        }
    }
    
    if ( !found )
    {
        Error("userdb", ERR_FATAL, "Could not delete maildomainconfig %s (%lu) from userdb_maildomainconfighashtable_name hash table", maildomainconfig->configname->content, maildomainconfig->configid);
        assert(0);
        return;
    }
}

/* Suspended user hashing */
void userdb_addusertosuspendedhash(userdb_user *user)
{
unsigned int hash;
#ifdef USERDB_ADDITIONALHASHCHECKS
userdb_user **tempuser;
#endif

    if ( !user || !user->suspensiondata )
    {
        Error("userdb", ERR_FATAL, "Bad parameters for userdb_addusertosuspendedhash");
        assert(0);
        return;
    }
    
    hash = userdb_getusersuspendedhash(user->userid);
#ifdef USERDB_ADDITIONALHASHCHECKS
    
    for ( tempuser = &(userdb_usersuspendedhashtable[hash]); *tempuser; tempuser = &((*tempuser)->suspensiondata->nextbyid) )
    {
        if ( (*tempuser)->userid == user->userid )
        {
            Error("userdb", ERR_FATAL, "Hash collision detected for user %s (%lu) in userdb_usersuspendedhashtable hash table", user->username->content, user->userid);
            assert(0);
            return;
        }
        else if ( (*tempuser)->userid > user->userid )
        {
            user->suspensiondata->nextbyid = (*tempuser);
            (*tempuser) = user;
            break;
        }
    }
    
    if ( !(*tempuser) )
    {
        user->suspensiondata->nextbyid = NULL;
        (*tempuser) = user;
    }
#else
    user->suspensiondata->nextbyid = userdb_usersuspendedhashtable[hash];
    userdb_usersuspendedhashtable[hash] = user;
#endif
    
    return;
}

int userdb_findsuspendeduser(unsigned long userid, userdb_user **user)
{
unsigned int hash;
userdb_user *tempuser;

    if ( !userid || !user )
        return USERDB_BADPARAMETERS;
    
    hash = userdb_getusersuspendedhash(userid);
    
    for ( tempuser = userdb_usersuspendedhashtable[hash]; tempuser; tempuser = tempuser->suspensiondata->nextbyid )
    {
        if ( tempuser->userid == userid )
        {
            *user = tempuser;
            return USERDB_OK;
        }
    }
    
    return USERDB_NOTFOUND;
}

void userdb_deleteuserfromsuspendedhash(userdb_user *user)
{
unsigned int hash;
userdb_user **tempuser;
int found;

    if ( !user || !user->suspensiondata )
    {
        Error("userdb", ERR_FATAL, "Bad parameters for userdb_deleteuserfromsuspendedhash");
        assert(0);
        return;
    }
    
    found = 0;
    hash = userdb_getusersuspendedhash(user->userid);
    
    for ( tempuser = &(userdb_usersuspendedhashtable[hash]); *tempuser; tempuser = &((*tempuser)->suspensiondata->nextbyid) )
    {
        if ( *tempuser == user )
        {
            *tempuser = user->suspensiondata->nextbyid;
            found = 1;
            break;
        }
    }
    
    if ( !found )
    {
        Error("userdb", ERR_FATAL, "Could not delete user %s (%lu) from userdb_usersuspendedhashtable hash table", user->username->content, user->userid);
        assert(0);
        return;
    }
    
    return;
}

#ifdef USERDB_ENABLEREPLICATION
/* Deleted language hashing */
void userdb_addlanguagetodeletedhash(userdb_language *language)
{
unsigned int hash;
userdb_language **templanguage;

    if ( !language )
    {
        Error("userdb", ERR_FATAL, "Bad parameters for userdb_addlanguagetodeletedhash");
        assert(0);
        return;
    }
    
    hash = userdb_getlanguagedeletedhash(language->transactionid);

    for ( templanguage = &(userdb_languagedeletedhashtable[hash]); *templanguage; templanguage = &((*templanguage)->nextbyid) )
    {
        if ( (*templanguage)->transactionid == language->transactionid )
        {
            Error("userdb", ERR_FATAL, "Hash collision detected for language %lu (%llu) in userdb_languagedeletedhashtable hash table", language->languageid, language->transactionid);
            assert(0);
            return;
        }
        else if ( (*templanguage)->transactionid > language->transactionid )
        {
            language->nextbyid = (*templanguage);
            (*templanguage) = language;
            break;
        }
    }
    
    if ( !(*templanguage) )
    {
        language->nextbyid = NULL;
        (*templanguage) = language;
    }
    
    language->nextbycode = NULL;
    
    return;
}

int userdb_finddeletedlanguage(unsigned long long transactionid, userdb_language **language)
{
unsigned int hash;
userdb_language *templanguage;

    if ( !transactionid || !language )
        return USERDB_BADPARAMETERS;
    
    hash = userdb_getlanguagedeletedhash(transactionid);
    
    for ( templanguage = userdb_languagedeletedhashtable[hash]; templanguage; templanguage = templanguage->nextbyid )
    {
        if ( templanguage->transactionid == transactionid )
        {
            *language = templanguage;
            return USERDB_OK;
        }
    }
    
    return USERDB_NOTFOUND;
}

void userdb_deletelanguagefromdeletedhash(userdb_language *language)
{
unsigned int hash;
userdb_language **templanguage;
int found;

    if ( !language )
    {
        Error("userdb", ERR_FATAL, "Bad parameters for userdb_deletelanguagefromdeletedhash");
        assert(0);
        return;
    }
    
    found = 0;
    hash = userdb_getlanguagedeletedhash(language->transactionid);
    
    for ( templanguage = &(userdb_languagedeletedhashtable[hash]); *templanguage; templanguage = &((*templanguage)->nextbyid) )
    {
        if ( *templanguage == language )
        {
            *templanguage = language->nextbyid;
            found = 1;
            break;
        }
    }
    
    if ( !found )
    {
        Error("userdb", ERR_FATAL, "Could not delete language %lu (%llu) from userdb_languagedeletedhashtable hash table", language->languageid, language->transactionid);
        assert(0);
        return;
    }
    
    return;
}

/* Deleted user hashing */
void userdb_addusertodeletedhash(userdb_user *user)
{
unsigned int hash;
userdb_user **tempuser;

    if ( !user )
    {
        Error("userdb", ERR_FATAL, "Bad parameters for userdb_addusertodeletedhash");
        assert(0);
        return;
    }
    
    hash = userdb_getuserdeletedhash(user->transactionid);
    
    for ( tempuser = &(userdb_userdeletedhashtable[hash]); *tempuser; tempuser = &((*tempuser)->nextbyid) )
    {
        if ( (*tempuser)->transactionid == user->transactionid )
        {
            Error("userdb", ERR_FATAL, "Hash collision detected for user %lu (%llu) in userdb_userdeletedhashtable hash table", user->userid, user->transactionid);
            assert(0);
            return;
        }
        else if ( (*tempuser)->transactionid > user->transactionid )
        {
            user->nextbyid = (*tempuser);
            (*tempuser) = user;
            break;
        }
    }
    
    if ( !(*tempuser) )
    {
        user->nextbyid = NULL;
        (*tempuser) = user;
    }
    
    user->nextbyname = NULL;
    
    return;
}

int userdb_finddeleteduser(unsigned long long transactionid, userdb_user **user)
{
unsigned int hash;
userdb_user *tempuser;

    if ( !transactionid || !user )
        return USERDB_BADPARAMETERS;
    
    hash = userdb_getuserdeletedhash(transactionid);
    
    for ( tempuser = userdb_userdeletedhashtable[hash]; tempuser; tempuser = tempuser->nextbyid )
    {
        if ( tempuser->transactionid == transactionid )
        {
            *user = tempuser;
            return USERDB_OK;
        }
    }
    
    return USERDB_NOTFOUND;
}

void userdb_deleteuserfromdeletedhash(userdb_user *user)
{
unsigned int hash;
userdb_user **tempuser;
int found;

    if ( !user )
    {
        Error("userdb", ERR_FATAL, "Bad parameters for userdb_deleteuserfromdeletedhash");
        assert(0);
        return;
    }
    
    found = 0;
    hash = userdb_getuserdeletedhash(user->transactionid);
    
    for ( tempuser = &(userdb_userdeletedhashtable[hash]); *tempuser; tempuser = &((*tempuser)->nextbyid) )
    {
        if ( *tempuser == user )
        {
            *tempuser = user->nextbyid;
            found = 1;
            break;
        }
    }
    
    if ( !found )
    {
        Error("userdb", ERR_FATAL, "Could not delete user %lu (%llu) from userdb_userdeletedhashtable hash table", user->userid, user->transactionid);
        assert(0);
        return;
    }
    
    return;
}

/* Deleted maildomainconfig hashing */
void userdb_addmaildomainconfigtodeletedhash(userdb_maildomainconfig *maildomainconfig)
{
unsigned int hash;
userdb_maildomainconfig **tempmaildomainconfig;

    if ( !maildomainconfig )
    {
        Error("userdb", ERR_FATAL, "Bad parameters for userdb_addmaildomainconfigtodeletedhash");
        assert(0);
        return;
    }
    
    hash = userdb_getmaildomainconfigdeletedhash(maildomainconfig->transactionid);
    
    for ( tempmaildomainconfig = &(userdb_maildomainconfigdeletedhashtable[hash]); *tempmaildomainconfig; tempmaildomainconfig = &((*tempmaildomainconfig)->nextbyid) )
    {
        if ( (*tempmaildomainconfig)->transactionid == maildomainconfig->transactionid )
        {
            Error("userdb", ERR_FATAL, "Hash collision detected for maildomainconfig %lu (%llu) in userdb_maildomainconfigdeletedhashtable hash table", maildomainconfig->configid, maildomainconfig->transactionid);
            assert(0);
            return;
        }
        else if ( (*tempmaildomainconfig)->transactionid > maildomainconfig->transactionid )
        {
            maildomainconfig->nextbyid = (*tempmaildomainconfig);
            (*tempmaildomainconfig) = maildomainconfig;
            break;
        }
    }
    
    if ( !(*tempmaildomainconfig) )
    {
        maildomainconfig->nextbyid = NULL;
        (*tempmaildomainconfig) = maildomainconfig;
    }
    
    maildomainconfig->nextbyname = NULL;
    
    return;
}

int userdb_finddeletedmaildomainconfig(unsigned long long transactionid, userdb_maildomainconfig **maildomainconfig)
{
unsigned int hash;
userdb_maildomainconfig *tempmaildomainconfig;

    if ( !transactionid || !maildomainconfig )
        return USERDB_BADPARAMETERS;
    
    hash = userdb_getmaildomainconfigdeletedhash(transactionid);
    
    for ( tempmaildomainconfig = userdb_maildomainconfigdeletedhashtable[hash]; tempmaildomainconfig; tempmaildomainconfig = tempmaildomainconfig->nextbyid )
    {
        if ( tempmaildomainconfig->transactionid == transactionid )
        {
            *maildomainconfig = tempmaildomainconfig;
            return USERDB_OK;
        }
    }
    
    return USERDB_NOTFOUND;
}

void userdb_deletemaildomainconfigfromdeletedhash(userdb_maildomainconfig *maildomainconfig)
{
unsigned int hash;
userdb_maildomainconfig **tempmaildomainconfig;
int found;

    if ( !maildomainconfig )
    {
        Error("userdb", ERR_FATAL, "Bad parameters for userdb_deletemaildomainconfigfromdeletedhash");
        assert(0);
        return;
    }
    
    found = 0;
    hash = userdb_getmaildomainconfigdeletedhash(maildomainconfig->transactionid);
    
    for ( tempmaildomainconfig = &(userdb_maildomainconfigdeletedhashtable[hash]); *tempmaildomainconfig; tempmaildomainconfig = &((*tempmaildomainconfig)->nextbyid) )
    {
        if ( *tempmaildomainconfig == maildomainconfig )
        {
            *tempmaildomainconfig = maildomainconfig->nextbyid;
            found = 1;
            break;
        }
    }
    
    if ( !found )
    {
        Error("userdb", ERR_FATAL, "Could not delete maildomainconfig %lu (%llu) from userdb_maildomainconfigdeletedhashtable hash table", maildomainconfig->configid, maildomainconfig->transactionid);
        assert(0);
        return;
    }
    
    return;
}

#endif
