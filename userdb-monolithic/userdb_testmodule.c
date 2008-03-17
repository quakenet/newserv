/*
 * UserDB
 */

#include "./userdb.h"
#include "../core/schedule.h"

#define USERDB_TEST_MINTAGSTRINGSIZE  0
#define USERDB_TEST_MAXTAGSTRINGSIZE  25
#define USERDB_TEST_REPAIRDATABASE

#define USERDB_TEST_PREDICTABLETAGS

schedule *userdb_testschedule;
unsigned int userdb_testingstarted, userdb_dumpedmaildomaintree, userdb_lastlanguagetestaction, userdb_lastusertestaction, userdb_lastmaildomainconfigtestaction;
FILE *userdb_randomsource;
userdb_usertaginfo *userdb_charusertaginfo, *userdb_shortusertaginfo, *userdb_intusertaginfo, *userdb_longusertaginfo, *userdb_longlongusertaginfo, *userdb_floatusertaginfo, *userdb_doubleusertaginfo, *userdb_stringusertaginfo;

int userdb_openrandomsource()
{
    userdb_randomsource = fopen("/dev/urandom", "r");
    return (userdb_randomsource != NULL);
}

unsigned char userdb_getrandomuchar()
{
unsigned char buf;

    if ( !userdb_randomsource )
        return 0;
    
    fread(&buf, sizeof(unsigned char), 1, userdb_randomsource);
    return buf;
}

unsigned short userdb_getrandomushort()
{
unsigned short buf;

    if ( !userdb_randomsource )
        return 0;
    
    fread(&buf, sizeof(unsigned short), 1, userdb_randomsource);
    return buf;
}

unsigned int userdb_getrandomuint()
{
unsigned int buf;

    if ( !userdb_randomsource )
        return 0;
    
    fread(&buf, sizeof(unsigned int), 1, userdb_randomsource);
    return buf;
}

unsigned long userdb_getrandomulong()
{
unsigned long buf;

    if ( !userdb_randomsource )
        return 0;
    
    fread(&buf, sizeof(unsigned long), 1, userdb_randomsource);
    return buf;
}

unsigned long userdb_getrandomulonglong()
{
unsigned long long buf;

    if ( !userdb_randomsource )
        return 0;
    
    fread(&buf, sizeof(unsigned long long), 1, userdb_randomsource);
    return buf;
}

float userdb_getrandomfloat()
{
float buf;

    if ( !userdb_randomsource )
        return 0;
    
    fread(&buf, sizeof(float), 1, userdb_randomsource);
    return buf;
}

double userdb_getrandomdouble()
{
double buf;

    if ( !userdb_randomsource )
        return 0;
    
    fread(&buf, sizeof(double), 1, userdb_randomsource);
    return buf;
}

char *userdb_randomstring()
{
unsigned int i;
char allowedchars[] = "ABCDEFGHIJKLMNPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
static char buf[USERDB_DEFAULTPASSWORDLEN + 1];

    memset(buf, 0, (USERDB_DEFAULTPASSWORDLEN + 1));
    
    for ( i = 0; i < USERDB_DEFAULTPASSWORDLEN; i++ )
        buf[i] = allowedchars[userdb_getrandomulong() % (sizeof(allowedchars) - 1)];
    
    return buf;
}

char *userdb_randomemail()
{
unsigned int i;
char allowedchars[] = "ABCDEFGHIJKLMNPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
static char buf[50];

    i = 0;
    memset(buf, 0, 50);
    
    while ( i < 10 )
    {
        buf[i] = allowedchars[userdb_getrandomulong() % (sizeof(allowedchars) - 1)];
        i++;
    }
    
    buf[i++] = '@';
    
    while ( i < 15 )
    {
        buf[i] = allowedchars[userdb_getrandomulong() % (sizeof(allowedchars) - 1)];
        i++;
    }
    
    buf[i++] = '.';
    
    while ( i < 20 )
    {
        buf[i] = allowedchars[userdb_getrandomulong() % (sizeof(allowedchars) - 1)];
        i++;
    }
    
    buf[i++] = '.';
    
    while ( i < 24 )
    {
        buf[i] = allowedchars[userdb_getrandomulong() % (sizeof(allowedchars) - 1)];
        i++;
    }
    
    return buf;
}

char *userdb_randomemaildomain()
{
unsigned int i;
char allowedchars[] = "ABCDEFGHIJKLMNPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
static char buf[50];

    i = 0;
    memset(buf, 0, 50);
    
    while ( i < 4 )
    {
        buf[i] = allowedchars[userdb_getrandomulong() % (sizeof(allowedchars) - 1)];
        i++;
    }
    
    buf[i++] = '.';
    
    while ( i < 9 )
    {
        buf[i] = allowedchars[userdb_getrandomulong() % (sizeof(allowedchars) - 1)];
        i++;
    }
    
    buf[i++] = '.';
    
    while ( i < 13 )
    {
        buf[i] = allowedchars[userdb_getrandomulong() % (sizeof(allowedchars) - 1)];
        i++;
    }
    
    return buf;
}

int userdb_closerandomsource()
{
    if ( !userdb_randomsource )
        return 0;
    
    fclose(userdb_randomsource);
    userdb_randomsource = NULL;
    return 1;
}

int userdb_languagesfsck()
{
userdb_language *languageA, *languageB;
unsigned int i;
unsigned int errorcount, totalcount;
unsigned long languageopcount;

    errorcount = 0;
    totalcount = 0;
    languageopcount = 0;
    
    /* Test 1: Check you can find a language from the ID table in the ID table */
    for ( i = 0; i < USERDB_LANGUAGEHASHSIZE; i++ )
    {
        for ( languageA = userdb_languagehashtable_id[i]; languageA; languageA = languageA->nextbyid )
        {
            switch ( userdb_findlanguagebylanguageid(languageA->languageid, &languageB) )
            {
                case USERDB_OK:
                    if ( languageA != languageB )
                    {
                        errorcount++;
                        totalcount++;
                    }
                    
                    break;
                default:
                    errorcount++;
                    totalcount++;
                    break;
            }
        }
    }
    
    if ( errorcount )
    {
        Error("userdbtest", ERR_ERROR, "Language test 1: %u errors", errorcount);
        errorcount = 0;
    }
    
    /* Test 2: Check you can find a language from the code table in the code table */
    for ( i = 0; i < USERDB_LANGUAGEHASHSIZE; i++ )
    {
        for ( languageA = userdb_languagehashtable_code[i]; languageA; languageA = languageA->nextbycode )
        {
            switch ( userdb_findlanguagebylanguagecode(languageA->languagecode->content, &languageB) )
            {
                case USERDB_OK:
                    if ( languageA != languageB )
                    {
                        errorcount++;
                        totalcount++;
                    }
                    
                    break;
                default:
                    errorcount++;
                    totalcount++;
                    break;
            }
        }
    }
    
    if ( errorcount )
    {
        Error("userdbtest", ERR_ERROR, "Language test 2: %u errors", errorcount);
        errorcount = 0;
    }
    
    /* Test 3: Check you can find a language from the code table in the ID table */
    for ( i = 0; i < USERDB_LANGUAGEHASHSIZE; i++ )
    {
        for ( languageA = userdb_languagehashtable_code[i]; languageA; languageA = languageA->nextbycode )
        {
            switch ( userdb_findlanguagebylanguageid(languageA->languageid, &languageB) )
            {
                case USERDB_OK:
                    if ( languageA != languageB )
                    {
                        errorcount++;
                        totalcount++;
                    }
                    
                    break;
                default:
                    errorcount++;
                    totalcount++;
                    break;
            }
        }
    }
    
    if ( errorcount )
    {
        Error("userdbtest", ERR_ERROR, "Language test 3: %u errors", errorcount);
        errorcount = 0;
    }
    
    /* Test 4: Check you can find a language from the ID table in the code table */
    for ( i = 0; i < USERDB_LANGUAGEHASHSIZE; i++ )
    {
        for ( languageA = userdb_languagehashtable_id[i]; languageA; languageA = languageA->nextbyid )
        {
            switch ( userdb_findlanguagebylanguagecode(languageA->languagecode->content, &languageB) )
            {
                case USERDB_OK:
                    if ( languageA != languageB )
                    {
                        errorcount++;
                        totalcount++;
                    }
                    
                    break;
                default:
                    errorcount++;
                    totalcount++;
                    break;
            }
        }
    }
    
    if ( errorcount )
    {
        Error("userdbtest", ERR_ERROR, "Language test 4: %u errors", errorcount);
        errorcount = 0;
    }
    
    /* Test 5: Check struct details */
    for ( i = 0; i < USERDB_LANGUAGEHASHSIZE; i++ )
    {
        for ( languageA = userdb_languagehashtable_id[i]; languageA; languageA = languageA->nextbyid )
        {
#ifdef USERDB_ENABLEREPLICATION
            if ( userdb_repl_group && userdb_repl_group->role == REPLICATOR_ROLE_MASTER && languageA->languageid > userdb_lastlanguageid )
#else
            if ( languageA->languageid > userdb_lastlanguageid )
#endif
            {
                errorcount++;
                totalcount++;
            }
            
/*
            if ( languageA->created > languageA->modified )
            {
                errorcount++;
                totalcount++;
            }
*/
            
            languageopcount += languageA->databaseops;
            
            if ( languageA->databaseops > USERDB_DATABASE_MAXPEROBJECT )
            {
                errorcount++;
                totalcount++;
            }
            
            if ( !languageA->revision )
            {
                errorcount++;
                totalcount++;
            }
            
            if ( !languageA->transactionid )
            {
                errorcount++;
                totalcount++;
            }
            
            if ( languageA->transactionid > userdb_lastlanguagetransactionid )
            {
                errorcount++;
                totalcount++;
            }
        }
    }
    
    if ( errorcount )
    {
        Error("userdbtest", ERR_ERROR, "Language test 5: %u errors", errorcount);
        errorcount = 0;
    }
    
#ifdef USERDB_ENABLEREPLICATION
    /* Test 6: Check you can find a language from the deleted table in the deleted table */
    for ( i = 0; i < USERDB_LANGUAGEDELETEDHASHSIZE; i++ )
    {
        for ( languageA = userdb_languagedeletedhashtable[i]; languageA; languageA = languageA->nextbyid )
        {
            languageopcount += languageA->databaseops;
            
            switch ( userdb_finddeletedlanguage(languageA->transactionid, &languageB) )
            {
                case USERDB_OK:
                    if ( languageA != languageB )
                    {
                        errorcount++;
                        totalcount++;
                    }
                    
                    break;
                default:
                    errorcount++;
                    totalcount++;
                    break;
            }
        }
    }
    
    if ( errorcount )
    {
        Error("userdbtest", ERR_ERROR, "Language test 6: %u errors", errorcount);
        errorcount = 0;
    }
#endif
    
    /* Test 7: Check no deleted users exist in the live hash */
    for ( i = 0; i < USERDB_LANGUAGEHASHSIZE; i++ )
    {
        for ( languageA = userdb_languagehashtable_id[i]; languageA; languageA = languageA->nextbyid )
        {
            if ( UDBRIsDeleted(languageA) )
            {
                errorcount++;
                totalcount++;
            }
        }
    }
    
    if ( errorcount )
    {
        Error("userdbtest", ERR_ERROR, "Language test 7: %u errors", errorcount);
        errorcount = 0;
    }
    
#ifdef USERDB_ENABLEREPLICATION
    /* Test 8: Check no live users exist in the deleted hash */
    for ( i = 0; i < USERDB_LANGUAGEDELETEDHASHSIZE; i++ )
    {
        for ( languageA = userdb_languagedeletedhashtable[i]; languageA; languageA = languageA->nextbyid )
        {
            if ( !UDBRIsDeleted(languageA) )
            {
                errorcount++;
                totalcount++;
            }
        }
    }
    
    if ( errorcount )
    {
        Error("userdbtest", ERR_ERROR, "Language test 8: %u errors", errorcount);
        errorcount = 0;
    }
    
    /* Test 9: Check live languages cannot be found in the deleted hash */
    for ( i = 0; i < USERDB_LANGUAGEHASHSIZE; i++ )
    {
        for ( languageA = userdb_languagehashtable_id[i]; languageA; languageA = languageA->nextbyid )
        {
            switch ( userdb_finddeletedlanguage(languageA->transactionid, &languageB) )
            {
                case USERDB_NOTFOUND:
                    break;
                default:
                    errorcount++;
                    totalcount++;
                    break;
            }
        }
        
        for ( languageA = userdb_languagehashtable_code[i]; languageA; languageA = languageA->nextbycode )
        {
            switch ( userdb_finddeletedlanguage(languageA->transactionid, &languageB) )
            {
                case USERDB_NOTFOUND:
                    break;
                default:
                    errorcount++;
                    totalcount++;
                    break;
            }
        }
    }
    
    if ( errorcount )
    {
        Error("userdbtest", ERR_ERROR, "Language test 9: %u errors", errorcount);
        errorcount = 0;
    }
    
    /* Test 10: Check no deleted languages exist in the live hash */
    for ( i = 0; i < USERDB_LANGUAGEDELETEDHASHSIZE; i++ )
    {
        for ( languageA = userdb_languagedeletedhashtable[i]; languageA; languageA = languageA->nextbyid )
        {
            switch ( userdb_findlanguagebylanguageid(languageA->languageid, &languageB) )
            {
                case USERDB_NOTFOUND:
                    break;
                default:
                    errorcount++;
                    totalcount++;
                    break;
            }
            
            switch ( userdb_findlanguagebylanguagecode(languageA->languagecode->content, &languageB) )
            {
                case USERDB_NOTFOUND:
                    break;
                default:
                    errorcount++;
                    totalcount++;
                    break;
            }
        }
    }
    
    if ( errorcount )
    {
        Error("userdbtest", ERR_ERROR, "Language test 10: %u errors", errorcount);
        errorcount = 0;
    }
#endif
    
    if ( languageopcount != (userdb_languagedatabaseaddcount + userdb_languagedatabasemodifycount + userdb_languagedatabasedeletecount) )
    {
        totalcount++;
        Error("userdbtest", ERR_ERROR, "Language database operations count mismatch: %lu VS %lu", languageopcount, (userdb_languagedatabaseaddcount + userdb_languagedatabasemodifycount + userdb_languagedatabasedeletecount));
    }
    
    if ( totalcount )
    {
        Error("userdbtest", ERR_ERROR, "Language total errors: %u", totalcount);
        return USERDB_ERROR;
    }
    
    Error("userdbtest", ERR_INFO, "All language tests passed");
    return USERDB_OK;
}

int userdb_usersfsck()
{
userdb_language *languageB;
userdb_user *userA, *userB;
userdb_maildomain *maildomainB;
char *emaildomain;
unsigned int i, j;
unsigned int errorcount, totalcount;
unsigned long useropcount;

    errorcount = 0;
    totalcount = 0;
    useropcount = 0;
    
    /* Test 1: Check you can find a user from the ID table in the ID table */
    for ( i = 0; i < USERDB_USERHASHSIZE; i++ )
    {
        for ( userA = userdb_userhashtable_id[i]; userA; userA = userA->nextbyid )
        {
            switch ( userdb_finduserbyuserid(userA->userid, &userB) )
            {
                case USERDB_OK:
                    if ( userA != userB )
                    {
                        errorcount++;
                        totalcount++;
                    }
                    
                    break;
                default:
                    errorcount++;
                    totalcount++;
                    break;
            }
        }
    }
    
    if ( errorcount )
    {
        Error("userdbtest", ERR_ERROR, "User test 1: %u errors", errorcount);
        errorcount = 0;
    }
    
    /* Test 2: Check you can find a user from the username table in the username table */
    for ( i = 0; i < USERDB_USERHASHSIZE; i++ )
    {
        for ( userA = userdb_userhashtable_username[i]; userA; userA = userA->nextbyname )
        {
            switch ( userdb_finduserbyusername(userA->username->content, &userB) )
            {
                case USERDB_OK:
                    if ( userA != userB )
                    {
                        errorcount++;
                        totalcount++;
                    }
                    
                    break;
                default:
                    errorcount++;
                    totalcount++;
                    break;
            }
        }
    }
    
    if ( errorcount )
    {
        Error("userdbtest", ERR_ERROR, "User test 2: %u errors", errorcount);
        errorcount = 0;
    }
    
    /* Test 3: Check you can find a user from the username table in the ID table */
    for ( i = 0; i < USERDB_USERHASHSIZE; i++ )
    {
        for ( userA = userdb_userhashtable_username[i]; userA; userA = userA->nextbyname )
        {
            switch ( userdb_finduserbyuserid(userA->userid, &userB) )
            {
                case USERDB_OK:
                    if ( userA != userB )
                    {
                        errorcount++;
                        totalcount++;
                    }
                    
                    break;
                default:
                    errorcount++;
                    totalcount++;
                    break;
            }
        }
    }
    
    if ( errorcount )
    {
        Error("userdbtest", ERR_ERROR, "User test 3: %u errors", errorcount);
        errorcount = 0;
    }
    
    /* Test 4: Check you can find a user from the ID table in the username table */
    for ( i = 0; i < USERDB_USERHASHSIZE; i++ )
    {
        for ( userA = userdb_userhashtable_id[i]; userA; userA = userA->nextbyid )
        {
            switch ( userdb_finduserbyusername(userA->username->content, &userB) )
            {
                case USERDB_OK:
                    if ( userA != userB )
                    {
                        errorcount++;
                        totalcount++;
                    }
                    
                    break;
                default:
                    errorcount++;
                    totalcount++;
                    break;
            }
        }
    }
    
    if ( errorcount )
    {
        Error("userdbtest", ERR_ERROR, "User test 4: %u errors", errorcount);
        errorcount = 0;
    }
    
    /* Test 5: Verify suspension struct details */
    for ( i = 0; i < USERDB_USERHASHSIZE; i++ )
    {
        for ( userA = userdb_userhashtable_id[i]; userA; userA = userA->nextbyid )
        {
            if ( !UDBOIsSuspended(userA) )
            {
                if ( userA->suspensiondata )
                {
                    errorcount++;
                    totalcount++;
                    continue;
                }
            }
            else 
            {
                if ( !userA->suspensiondata )
                {
                    errorcount++;
                    totalcount++;
                    continue;
                }
                
                if ( userA->suspensiondata->suspendend && userA->suspensiondata->suspendstart > userA->suspensiondata->suspendend )
                {
                    errorcount++;
                    totalcount++;
                }
                
                if ( userA->suspensiondata->suspenderid )
                {
                    switch ( userdb_finduserbyuserid(userA->suspensiondata->suspenderid, &userB) )
                    {
                        case USERDB_OK:
                            break;
                        case USERDB_NOTFOUND:
                            userB = NULL;
                            break;
                        default:
                            userB = NULL;
                            errorcount++;
                            totalcount++;
                            break;
                    }
                    
                    if ( userA->suspensiondata->suspenderdata != userB )
                    {
                        errorcount++;
                        totalcount++;
                    }
                }
                else
                {
                    if ( userA->suspensiondata->suspenderdata )
                    {
                        errorcount++;
                        totalcount++;
                    }
                }
            }
        }
    }
    
    if ( errorcount )
    {
        Error("userdbtest", ERR_ERROR, "User test 5: %u errors", errorcount);
        errorcount = 0;
    }
    
    /* Test 6: Check to see if you can find suspended users in the suspension hash */
    for ( i = 0; i < USERDB_USERHASHSIZE; i++ )
    {
        for ( userA = userdb_userhashtable_id[i]; userA; userA = userA->nextbyid )
        {
            switch ( userdb_findsuspendeduser(userA->userid, &userB) )
            {
                case USERDB_OK:
                    break;
                case USERDB_NOTFOUND:
                    userB = NULL;
                    break;
                default:
                    userB = NULL;
                    errorcount++;
                    totalcount++;
                    break;
            }
            
            if ( UDBOIsSuspended(userA) )
            {
                if ( userB != userA )
                {
                    errorcount++;
                    totalcount++;
                }
            }
            else
            {
                if ( userB != NULL )
                {
                    errorcount++;
                    totalcount++;
                }
            }
        }
    }
    
    if ( errorcount )
    {
        Error("userdbtest", ERR_ERROR, "User test 6: %u errors", errorcount);
        errorcount = 0;
    }
    
    /* Test 7: Check to see if you can find suspended users in the ID hash */
    for ( i = 0; i < USERDB_USERSUSPENDEDHASHSIZE; i++ )
    {
        for ( userA = userdb_usersuspendedhashtable[i]; userA; userA = userA->suspensiondata->nextbyid )
        {
            if ( !UDBOIsSuspended(userA) )
            {
                errorcount++;
                totalcount++;
            }
            
            switch ( userdb_finduserbyuserid(userA->userid, &userB) )
            {
                case USERDB_OK:
                    break;
                default:
                    userB = NULL;
                    errorcount++;
                    totalcount++;
                    break;
            }
            
            if ( userA != userB )
            {
                errorcount++;
                totalcount++;
            }
        }
    }
    
    if ( errorcount )
    {
        Error("userdbtest", ERR_ERROR, "User test 7: %u errors", errorcount);
        errorcount = 0;
    }
    
    /* Test 8: Verify language details */
    for ( i = 0; i < USERDB_USERHASHSIZE; i++ )
    {
        for ( userA = userdb_userhashtable_id[i]; userA; userA = userA->nextbyid )
        {
            if ( userA->languageid )
            {
                switch ( userdb_findlanguagebylanguageid(userA->languageid, &languageB) )
                {
                    case USERDB_OK:
                        break;
                    case USERDB_NOTFOUND:
                        languageB = NULL;
                        break;
                    default:
                        languageB = NULL;
                        errorcount++;
                        totalcount++;
                        break;
                }
                
                if ( userA->languagedata != languageB )
                {
                    errorcount++;
                    totalcount++;
                }
            }
            else
            {
                if ( userA->languagedata )
                {
                    errorcount++;
                    totalcount++;
                }
            }
        }
    }
    
    if ( errorcount )
    {
        Error("userdbtest", ERR_ERROR, "User test 8: %u errors", errorcount);
        errorcount = 0;
    }
    
    /* Test 9: Check the user exists on the mail domain it has been assigned to and the mail domain actually exists */
    for ( i = 0; i < USERDB_USERHASHSIZE; i++ )
    {
        for ( userA = userdb_userhashtable_id[i]; userA; userA = userA->nextbyid )
        {
            if ( !userA->maildomain )
            {
                errorcount++;
                totalcount++;
                continue;
            }
            
            if ( !userA->maildomain->domaincount )
            {
                errorcount++;
                totalcount++;
                continue;
            }
            
            j = 0;
            
            for ( userB = userA->maildomain->users; userB; userB = userB->nextbymaildomain )
            {
                if ( userB == userA )
                {
                    j = 1;
                    break;
                }
            }
            
            if ( !j )
            {
                errorcount++;
                totalcount++;
                continue;
            }
            
            switch ( userdb_buildemaildomain(userA->maildomain, &emaildomain) )
            {
                case USERDB_OK:
                    break;
                default:
                    emaildomain = NULL;
                    break;
            }
            
            if ( !emaildomain )
            {
                errorcount++;
                totalcount++;
                continue;
            }
            
            switch ( userdb_findmaildomainbyemaildomain(emaildomain, &maildomainB, 0) )
            {
                case USERDB_OK:
                    break;
                default:
                    maildomainB = NULL;
                    break;
            }
            
            if ( userA->maildomain != maildomainB )
            {
                errorcount++;
                totalcount++;
            }
        }
    }
    
    if ( errorcount )
    {
        Error("userdbtest", ERR_ERROR, "User test 9: %u errors", errorcount);
        errorcount = 0;
    }
    
    /* Test 10: Check struct details */
    for ( i = 0; i < USERDB_USERHASHSIZE; i++ )
    {
        for ( userA = userdb_userhashtable_id[i]; userA; userA = userA->nextbyid )
        {
            if ( userdb_getdisallowedgroupflagsbyuser(userA, userA->groupflags) )
            {
                errorcount++;
                totalcount++;
#ifdef USERDB_TEST_REPAIRDATABASE
                
#ifdef USERDB_ENABLEREPLICATION
                if ( userdb_repl_group && userdb_repl_group->role == REPLICATOR_ROLE_MASTER )
#endif
                    userdb_changeusergroupflags(userA, userA->groupflags);
#endif
            }
            
            if ( userdb_getdisallowedoptionflagsbyuser(userA, userA->optionflags) )
            {
                errorcount++;
                totalcount++;
#ifdef USERDB_TEST_REPAIRDATABASE
                
#ifdef USERDB_ENABLEREPLICATION
                if ( userdb_repl_group && userdb_repl_group->role == REPLICATOR_ROLE_MASTER )
#endif
                    userdb_changeuseroptionflags(userA, userA->optionflags);
#endif
            }
            
            if ( userdb_getdisallowednoticeflagsbyuser(userA, userA->noticeflags) )
            {
                errorcount++;
                totalcount++;
#ifdef USERDB_TEST_REPAIRDATABASE
                
#ifdef USERDB_ENABLEREPLICATION
                if ( userdb_repl_group && userdb_repl_group->role == REPLICATOR_ROLE_MASTER )
#endif
                    userdb_changeusernoticeflags(userA, userA->noticeflags);
#endif
            }
            
#ifdef USERDB_ENABLEREPLICATION
            if ( userdb_repl_group && userdb_repl_group->role == REPLICATOR_ROLE_MASTER && userA->userid > userdb_lastuserid )
#else
            if ( userA->userid > userdb_lastuserid )
#endif
            {
                errorcount++;
                totalcount++;
            }
            
            if ( !memcmp(userA->password, "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", USERDB_PASSWORDHASHLEN) )
            {
                errorcount++;
                totalcount++;
            }
            
/*
            if ( userA->created > userA->modified )
            {
                errorcount++;
                totalcount++;
            }
*/
            
            useropcount += userA->databaseops;
            
            if ( userA->databaseops > USERDB_DATABASE_MAXPEROBJECT )
            {
                errorcount++;
                totalcount++;
            }
            
            if ( !userA->revision )
            {
                errorcount++;
                totalcount++;
            }
            
            if ( !userA->transactionid )
            {
                errorcount++;
                totalcount++;
            }
            
            if ( userA->transactionid > userdb_lastusertransactionid )
            {
                errorcount++;
                totalcount++;
            }
        }
    }
    
    if ( errorcount )
    {
        Error("userdbtest", ERR_ERROR, "User test 10: %u errors", errorcount);
        errorcount = 0;
    }
    
#ifdef USERDB_ENABLEREPLICATION
    /* Test 11: Check you can find a user from the deleted table in the deleted table */
    for ( i = 0; i < USERDB_USERDELETEDHASHSIZE; i++ )
    {
        for ( userA = userdb_userdeletedhashtable[i]; userA; userA = userA->nextbyid )
        {
            useropcount += userA->databaseops;
            
            switch ( userdb_finddeleteduser(userA->transactionid, &userB) )
            {
                case USERDB_OK:
                    if ( userA != userB )
                    {
                        errorcount++;
                        totalcount++;
                    }
                    
                    break;
                default:
                    errorcount++;
                    totalcount++;
                    break;
            }
        }
    }
    
    if ( errorcount )
    {
        Error("userdbtest", ERR_ERROR, "User test 11: %u errors", errorcount);
        errorcount = 0;
    }
#endif
    
    /* Test 12: Check no deleted users exist in the live hash */
    for ( i = 0; i < USERDB_USERHASHSIZE; i++ )
    {
        for ( userA = userdb_userhashtable_id[i]; userA; userA = userA->nextbyid )
        {
            if ( UDBRIsDeleted(userA) )
            {
                errorcount++;
                totalcount++;
            }
        }
    }
    
    if ( errorcount )
    {
        Error("userdbtest", ERR_ERROR, "User test 12: %u errors", errorcount);
        errorcount = 0;
    }
    
#ifdef USERDB_ENABLEREPLICATION
    /* Test 13: Check no live users exist in the deleted hash */
    for ( i = 0; i < USERDB_USERDELETEDHASHSIZE; i++ )
    {
        for ( userA = userdb_userdeletedhashtable[i]; userA; userA = userA->nextbyid )
        {
            if ( !UDBRIsDeleted(userA) )
            {
                errorcount++;
                totalcount++;
            }
        }
    }
    
    if ( errorcount )
    {
        Error("userdbtest", ERR_ERROR, "User test 13: %u errors", errorcount);
        errorcount = 0;
    }
    
    /* Test 14: Check live users cannot be found in the deleted hash */
    for ( i = 0; i < USERDB_USERHASHSIZE; i++ )
    {
        for ( userA = userdb_userhashtable_id[i]; userA; userA = userA->nextbyid )
        {
            switch ( userdb_finddeleteduser(userA->transactionid, &userB) )
            {
                case USERDB_NOTFOUND:
                    break;
                default:
                    errorcount++;
                    totalcount++;
                    break;
            }
        }
        
        for ( userA = userdb_userhashtable_username[i]; userA; userA = userA->nextbyname )
        {
            switch ( userdb_finddeleteduser(userA->transactionid, &userB) )
            {
                case USERDB_NOTFOUND:
                    break;
                default:
                    errorcount++;
                    totalcount++;
                    break;
            }
        }
    }
    
    if ( errorcount )
    {
        Error("userdbtest", ERR_ERROR, "User test 14: %u errors", errorcount);
        errorcount = 0;
    }
    
    /* Test 15: Check no deleted users exist in the live hash */
    for ( i = 0; i < USERDB_USERDELETEDHASHSIZE; i++ )
    {
        for ( userA = userdb_userdeletedhashtable[i]; userA; userA = userA->nextbyid )
        {
            switch ( userdb_finduserbyuserid(userA->userid, &userB) )
            {
                case USERDB_NOTFOUND:
                    break;
                default:
                    errorcount++;
                    totalcount++;
                    break;
            }
            
            switch ( userdb_finduserbyusername(userA->username->content, &userB) )
            {
                case USERDB_NOTFOUND:
                    break;
                default:
                    errorcount++;
                    totalcount++;
                    break;
            }
        }
    }
    
    if ( errorcount )
    {
        Error("userdbtest", ERR_ERROR, "User test 15: %u errors", errorcount);
        errorcount = 0;
    }
#endif
    
    if ( useropcount != (userdb_userdatabaseaddcount + userdb_userdatabasemodifycount + userdb_userdatabasedeletecount) )
    {
        totalcount++;
        Error("userdbtest", ERR_ERROR, "User database operations count mismatch: %lu VS %lu", useropcount, (userdb_userdatabaseaddcount + userdb_userdatabasemodifycount + userdb_userdatabasedeletecount));
    }
    
    if ( totalcount )
    {
        Error("userdbtest", ERR_ERROR, "User total errors: %u", totalcount);
        return USERDB_ERROR;
    }
    
    Error("userdbtest", ERR_INFO, "All user tests passed");
    return USERDB_OK;
    
    /* Test 16: Check there are no duplicate IDs, Username and Transaction IDs in the live hash */
    for ( i = 0; i < USERDB_USERHASHSIZE; i++ )
    {
        for ( userA = userdb_userhashtable_id[i]; userA; userA = userA->nextbyid )
        {
            for ( userB = userdb_userhashtable_id[userdb_getuseridhash(userA->userid)]; userB; userB = userB->nextbyid )
            {
                if ( userA->userid == userB->userid && userA != userB )
                {
                    errorcount++;
                    totalcount++;
                }
            }
            
            for ( userB = userdb_userhashtable_username[userdb_getusernamehash(userA->username->content)]; userB; userB = userB->nextbyname )
            {
                if ( !ircd_strcmp(userA->username->content, userB->username->content) && userA != userB )
                {
                    errorcount++;
                    totalcount++;
                }
            }
            
            for ( j = 0; j < USERDB_USERHASHSIZE; j++ )
            {
                for ( userB = userdb_userhashtable_id[j]; userB; userB = userB->nextbyid )
                {
                    if ( userA->transactionid == userB->transactionid && userA != userB )
                    {
                        errorcount++;
                        totalcount++;
                    }
                }
            }
        }
    }
    
    if ( errorcount )
    {
        Error("userdbtest", ERR_ERROR, "User test 16: %u errors", errorcount);
        errorcount = 0;
    }
    
#ifdef USERDB_ENABLEREPLICATION
    /* Test 17: Check there are no duplicate IDs and Transaction IDs in the deleted hash */
    for ( i = 0; i < USERDB_USERDELETEDHASHSIZE; i++ )
    {
        for ( userA = userdb_userdeletedhashtable[i]; userA; userA = userA->nextbyid )
        {
            for ( userB = userdb_userdeletedhashtable[userdb_getuserdeletedhash(userA->transactionid)]; userB; userB = userB->nextbyid )
            {
                if ( userA->transactionid == userB->transactionid && userA != userB )
                {
                    errorcount++;
                    totalcount++;
                }
            }
            
            for ( j = 0; j < USERDB_USERDELETEDHASHSIZE; j++ )
            {
                for ( userB = userdb_userdeletedhashtable[j]; userB; userB = userB->nextbyid )
                {
                    if ( userA->userid == userB->userid && userA != userB )
                    {
                        errorcount++;
                        totalcount++;
                    }
                }
            }
        }
    }
    
    if ( errorcount )
    {
        Error("userdbtest", ERR_ERROR, "User test 17: %u errors", errorcount);
        errorcount = 0;
    }
#endif
    
    if ( totalcount )
    {
        Error("userdbtest", ERR_ERROR, "User total errors: %u", totalcount);
        return USERDB_ERROR;
    }
    
    Error("userdbtest", ERR_INFO, "All user tests passed");
    return USERDB_OK;
}

int userdb_usertagsfsck()
{
userdb_usertaginfo *usertaginfoA, *usertaginfoB;
userdb_usertagdata *usertagdataA, *usertagdataB;
userdb_user *userA, *userB;
unsigned int i, j, hash, found;
unsigned int errorcount, totalcount;
#ifdef USERDB_TEST_PREDICTABLETAGS
unsigned int verified;
unsigned short testtype, testsize;
unsigned char testchar;
unsigned short testshort;
unsigned int testint;
unsigned long testlong;
unsigned long long testlonglong;
float testfloat;
double testdouble;
unsigned char *teststring;
unsigned char buf[101];
#endif

    errorcount = 0;
    totalcount = 0;
    
    /* Test 1: Check all usertagdata's in the usertaginfo's hash table point to the correct usertaginfo */
    for ( i = 0; i < USERDB_USERTAGINFOHASHSIZE; i++ )
    {
        for ( usertaginfoA = userdb_usertaginfohashtable[i]; usertaginfoA; usertaginfoA = usertaginfoA->nextbyid )
        {
            for ( j = 0; j < USERDB_USERTAGDATAHASHSIZE; j++ )
            {
                for ( usertagdataA = usertaginfoA->usertags[j]; usertagdataA; usertagdataA = usertagdataA->nextbytag )
                {
                    if ( usertagdataA->taginfo != usertaginfoA )
                    {
                        errorcount++;
                        totalcount++;
                    }
                }
            }
        }
    }
    
    if ( errorcount )
    {
        Error("userdbtest", ERR_ERROR, "User tags test 1: %u errors", errorcount);
        errorcount = 0;
    }
    
    /* Test 2: Check all usertagdata's in the user's link list point to the correct user */
    for ( i = 0; i < USERDB_USERHASHSIZE; i++ )
    {
        for ( userA = userdb_userhashtable_id[i]; userA; userA = userA->nextbyid )
        {
            for ( usertagdataA = userA->usertags; usertagdataA; usertagdataA = usertagdataA->nextbyuser )
            {
                if ( usertagdataA->userinfo != userA )
                {
                    errorcount++;
                    totalcount++;
                }
            }
        }
    }
    
    if ( errorcount )
    {
        Error("userdbtest", ERR_ERROR, "User tags test 2: %u errors", errorcount);
        errorcount = 0;
    }
    
    /* Test 3: Check all usertagdata->userinfo's are valid in the usertaginfo hash table */
    for ( i = 0; i < USERDB_USERTAGINFOHASHSIZE; i++ )
    {
        for ( usertaginfoA = userdb_usertaginfohashtable[i]; usertaginfoA; usertaginfoA = usertaginfoA->nextbyid )
        {
            for ( j = 0; j < USERDB_USERTAGDATAHASHSIZE; j++ )
            {
                for ( usertagdataA = usertaginfoA->usertags[j]; usertagdataA; usertagdataA = usertagdataA->nextbytag )
                {
                    userA = usertagdataA->userinfo;
                    
                    switch ( userdb_finduserbyuserid(userA->userid, &userB) )
                    {
                        case USERDB_OK:
                            
                            if ( userA != userB )
                            {
                                errorcount++;
                                totalcount++;
                            }
                            
                            break;
                        default:
                            errorcount++;
                            totalcount++;
                            break;
                    }
                }
            }
        }
    }
    
    if ( errorcount )
    {
        Error("userdbtest", ERR_ERROR, "User tags test 3: %u errors", errorcount);
        errorcount = 0;
    }
    
    /* Test 4: Check all usertagdata->taginfo's are valid in the users link list */
    for ( i = 0; i < USERDB_USERHASHSIZE; i++ )
    {
        for ( userA = userdb_userhashtable_id[i]; userA; userA = userA->nextbyid )
        {
            for ( usertagdataA = userA->usertags; usertagdataA; usertagdataA = usertagdataA->nextbyuser )
            {
                usertaginfoA = usertagdataA->taginfo;
                
                switch ( userdb_findusertaginfo(usertaginfoA->tagid, &usertaginfoB) )
                {
                    case USERDB_OK:
                        
                        if ( usertaginfoA != usertaginfoB )
                        {
                            errorcount++;
                            totalcount++;
                        }
                        
                        break;
                    default:
                        errorcount++;
                        totalcount++;
                        break;
                }
            }
        }
    }
    
    if ( errorcount )
    {
        Error("userdbtest", ERR_ERROR, "User tags test 4: %u errors", errorcount);
        errorcount = 0;
    }
    
    /* Test 5: Check all usertagdata's in the usertaginfo's hash table can be found in the users link list */
    for ( i = 0; i < USERDB_USERTAGINFOHASHSIZE; i++ )
    {
        for ( usertaginfoA = userdb_usertaginfohashtable[i]; usertaginfoA; usertaginfoA = usertaginfoA->nextbyid )
        {
            for ( j = 0; j < USERDB_USERTAGDATAHASHSIZE; j++ )
            {
                for ( usertagdataA = usertaginfoA->usertags[j]; usertagdataA; usertagdataA = usertagdataA->nextbytag )
                {
                    found = 0;
                    userA = usertagdataA->userinfo;
                    
                    for ( usertagdataB = userA->usertags; usertagdataB; usertagdataB = usertagdataB->nextbyuser )
                    {
                        if ( usertagdataA == usertagdataB )
                        {
                            found = 1;
                            break;
                        }
                    }
                    
                    if ( !found )
                    {
                        errorcount++;
                        totalcount++;
                    }
                }
            }
        }
    }
    
    if ( errorcount )
    {
        Error("userdbtest", ERR_ERROR, "User tags test 5: %u errors", errorcount);
        errorcount = 0;
    }
    
    /* Test 6: Check all usertagdata's in the user's link list can be found in the usertaginfo's hash table */
    for ( i = 0; i < USERDB_USERHASHSIZE; i++ )
    {
        for ( userA = userdb_userhashtable_id[i]; userA; userA = userA->nextbyid )
        {
            for ( usertagdataA = userA->usertags; usertagdataA; usertagdataA = usertagdataA->nextbyuser )
            {
                usertaginfoA = usertagdataA->taginfo;
                hash = userdb_getusertagdatahash(userA->userid);
                found = 0;
                
                for ( usertagdataB = usertaginfoA->usertags[hash]; usertagdataB; usertagdataB = usertagdataB->nextbytag )
                {
                    if ( usertagdataA == usertagdataB )
                    {
                        found = 1;
                        break;
                    }
                }
                
                if ( !found )
                {
                    errorcount++;
                    totalcount++;
                }
            }
        }
    }
    
    if ( errorcount )
    {
        Error("userdbtest", ERR_ERROR, "User tags test 6: %u errors", errorcount);
        errorcount = 0;
    }
    
    /* Test 7: Check all tagtypes are valid */
    for ( i = 0; i < USERDB_USERTAGINFOHASHSIZE; i++ )
    {
        for ( usertaginfoA = userdb_usertaginfohashtable[i]; usertaginfoA; usertaginfoA = usertaginfoA->nextbyid )
        {
            switch ( usertaginfoA->tagtype )
            {
                case USERDB_TAGTYPE_CHAR:
                case USERDB_TAGTYPE_SHORT:
                case USERDB_TAGTYPE_INT:
                case USERDB_TAGTYPE_LONG:
                case USERDB_TAGTYPE_LONGLONG:
                case USERDB_TAGTYPE_FLOAT:
                case USERDB_TAGTYPE_DOUBLE:
                case USERDB_TAGTYPE_STRING:
                    break;
                default:
                    errorcount++;
                    totalcount++;
                    break;
            }
        }
    }
    
    if ( errorcount )
    {
        Error("userdbtest", ERR_ERROR, "User tags test 7: %u errors", errorcount);
        errorcount = 0;
    }
    
    /* Test 8: Check all usertaginfo's of type STRING contain strings */
    for ( i = 0; i < USERDB_USERTAGINFOHASHSIZE; i++ )
    {
        for ( usertaginfoA = userdb_usertaginfohashtable[i]; usertaginfoA; usertaginfoA = usertaginfoA->nextbyid )
        {
            switch ( usertaginfoA->tagtype )
            {
                case USERDB_TAGTYPE_CHAR:
                case USERDB_TAGTYPE_SHORT:
                case USERDB_TAGTYPE_INT:
                case USERDB_TAGTYPE_LONG:
                case USERDB_TAGTYPE_LONGLONG:
                case USERDB_TAGTYPE_FLOAT:
                case USERDB_TAGTYPE_DOUBLE:
                    break;
                case USERDB_TAGTYPE_STRING:
                    
                    for ( j = 0; j < USERDB_USERTAGDATAHASHSIZE; j++ )
                    {
                        for ( usertagdataA = usertaginfoA->usertags[j]; usertagdataA; usertagdataA = usertagdataA->nextbytag )
                        {
                            if ( !((usertagdataA->tagdata).tagstring.stringdata) )
                            {
                                errorcount++;
                                totalcount++;
                            }
                        }
                    }
                    
                    break;
                default:
                    errorcount++;
                    totalcount++;
                    break;
            }
        }
    }
    
    if ( errorcount )
    {
        Error("userdbtest", ERR_ERROR, "User tags test 8: %u errors", errorcount);
        errorcount = 0;
#ifdef USERDB_TEST_PREDICTABLETAGS
        Error("userdbtest", ERR_ERROR, "Aborting tests as further tests may cause instability");
        return USERDB_ERROR;
#endif
    }
    
#ifdef USERDB_TEST_PREDICTABLETAGS
    /* Test P: Check all usertagdata's in the user's link list can be found in the usertaginfo's hash table */
    for ( i = 0; i < USERDB_USERHASHSIZE; i++ )
    {
        for ( userA = userdb_userhashtable_id[i]; userA; userA = userA->nextbyid )
        {
            for ( usertagdataA = userA->usertags; usertagdataA; usertagdataA = usertagdataA->nextbyuser )
            {
                verified = 0;
                
                switch ( usertagdataA->taginfo->tagtype )
                {
                    case USERDB_TAGTYPE_CHAR:
                        
                        switch ( userdb_getusertagdatavaluebydata(usertagdataA, &testtype, &testsize, ((void *)&testchar)) )
                        {
                            case USERDB_OK:
                                
                                if ( testchar == (userA->userid % 256) )
                                    verified = 1;
                                
                                break;
                            default:
                                break;
                        }
                        
                        break;
                    case USERDB_TAGTYPE_SHORT:
                        
                        switch ( userdb_getusertagdatavaluebydata(usertagdataA, &testtype, &testsize, ((void *)&testshort)) )
                        {
                            case USERDB_OK:
                                
                                if ( testshort == (userA->userid % 65536) )
                                    verified = 1;
                                
                                break;
                            default:
                                break;
                        }
                        
                        break;
                    case USERDB_TAGTYPE_INT:
                        
                        switch ( userdb_getusertagdatavaluebydata(usertagdataA, &testtype, &testsize, ((void *)&testint)) )
                        {
                            case USERDB_OK:
                                
                                if ( testint == (userA->userid % 56264) )
                                    verified = 1;
                                
                                break;
                            default:
                                break;
                        }
                        
                        break;
                    case USERDB_TAGTYPE_LONG:
                        
                        switch ( userdb_getusertagdatavaluebydata(usertagdataA, &testtype, &testsize, ((void *)&testlong)) )
                        {
                            case USERDB_OK:
                                
                                if ( testlong == (userA->userid % 91647) )
                                    verified = 1;
                                
                                break;
                            default:
                                break;
                        }
                        
                        break;
                    case USERDB_TAGTYPE_LONGLONG:
                        
                        switch ( userdb_getusertagdatavaluebydata(usertagdataA, &testtype, &testsize, ((void *)&testlonglong)) )
                        {
                            case USERDB_OK:
                                
                                if ( testlonglong == (userA->userid % 78573) )
                                    verified = 1;
                                
                                break;
                            default:
                                break;
                        }
                        
                        break;
                    case USERDB_TAGTYPE_FLOAT:
                        
                        switch ( userdb_getusertagdatavaluebydata(usertagdataA, &testtype, &testsize, ((void *)&testfloat)) )
                        {
                            case USERDB_OK:
                                
                                if ( testfloat == (userA->userid % 25593) )
                                    verified = 1;
                                
                                break;
                            default:
                                break;
                        }
                        
                        break;
                    case USERDB_TAGTYPE_DOUBLE:
                        
                        switch ( userdb_getusertagdatavaluebydata(usertagdataA, &testtype, &testsize, ((void *)&testdouble)) )
                        {
                            case USERDB_OK:
                                
                                if ( testdouble == (userA->userid % 10573) )
                                    verified = 1;
                                
                                break;
                            default:
                                break;
                        }
                        
                        break;
                    case USERDB_TAGTYPE_STRING:
                        
                        snprintf((char *)buf, sizeof(buf), "%lu", userA->userid);
                        buf[sizeof(buf) - 1] = '\0';
                        
                        switch ( userdb_getusertagdatavaluebydata(usertagdataA, &testtype, &testsize, ((void *)&teststring)) )
                        {
                            case USERDB_OK:
                                
                                if ( !memcmp(teststring, buf, testsize) )
                                    verified = 1;
                                
                                break;
                            default:
                                break;
                        }
                        
                        break;
                    default:
                        errorcount++;
                        totalcount++;
                        break;
                }
            }
        }
    }
    
    if ( errorcount )
    {
        Error("userdbtest", ERR_ERROR, "User tags test P: %u errors", errorcount);
        errorcount = 0;
    }
#endif
    
    if ( totalcount )
    {
        Error("userdbtest", ERR_ERROR, "User tags total errors: %u", totalcount);
        return USERDB_ERROR;
    }
    
    Error("userdbtest", ERR_INFO, "All user tags tests passed");
    return USERDB_OK;
}

int userdb_maildomainsfsck()
{
userdb_maildomain *maildomainA, *maildomainB;
userdb_maildomainconfig *maildomainconfigB;
userdb_user *userB;
char *emaildomain;
unsigned int i, j;
unsigned int errorcount, totalcount;

    errorcount = 0;
    totalcount = 0;
    
    /* Test 1: Check to see if you can find a maildomain in the maildomain tree */
    for ( i = 0; i < USERDB_MAILDOMAINHASHSIZE; i++ )
    {
        for ( maildomainA = userdb_maildomainhashtable_name[i]; maildomainA; maildomainA = maildomainA->nextbyname )
        {
            switch ( userdb_buildemaildomain(maildomainA, &emaildomain) )
            {
                case USERDB_OK:
                    break;
                default:
                    emaildomain = NULL;
                    break;
            }
            
            if ( !emaildomain )
            {
                errorcount++;
                totalcount++;
                continue;
            }
            
            switch ( userdb_findmaildomainbyemaildomain(emaildomain, &maildomainB, 0) )
            {
                case USERDB_OK:
                    break;
                default:
                    maildomainB = NULL;
                    break;
            }
            
            if ( maildomainA != maildomainB )
            {
                errorcount++;
                totalcount++;
            }
        }
    }
    
    if ( errorcount )
    {
        Error("userdbtest", ERR_ERROR, "Mail domain test 1: %u errors", errorcount);
        errorcount = 0;
    }
    
    /* Test 2: Check for unfree'ed nodes */
    for ( i = 0; i < USERDB_MAILDOMAINHASHSIZE; i++ )
    {
        for ( maildomainA = userdb_maildomainhashtable_name[i]; maildomainA; maildomainA = maildomainA->nextbyname )
        {
            if ( !maildomainA->totaldomaincount && !maildomainA->configcount )
            {
                errorcount++;
                totalcount++;
            }
        }
    }
    
    if ( errorcount )
    {
        Error("userdbtest", ERR_ERROR, "Mail domain test 2: %u errors", errorcount);
        errorcount = 0;
    }
    
    /* Test 3: Verify per domain user count */
    for ( i = 0; i < USERDB_MAILDOMAINHASHSIZE; i++ )
    {
        for ( maildomainA = userdb_maildomainhashtable_name[i]; maildomainA; maildomainA = maildomainA->nextbyname )
        {
            j = 0;
            
            for ( userB = maildomainA->users; userB; userB = userB->nextbymaildomain )
                j++;
            
            if ( j != maildomainA->domaincount )
            {
                errorcount++;
                totalcount++;
            }
        }
    }
    
    if ( errorcount )
    {
        Error("userdbtest", ERR_ERROR, "Mail domain test 3: %u errors", errorcount);
        errorcount = 0;
    }
    
    /* Test 4: Verify parent user and config count */
    for ( i = 0; i < USERDB_MAILDOMAINHASHSIZE; i++ )
    {
        for ( maildomainA = userdb_maildomainhashtable_name[i]; maildomainA; maildomainA = maildomainA->nextbyname )
        {
            for ( maildomainB = maildomainA; maildomainB; maildomainB = maildomainB->parent )
            {
                if ( !maildomainB->parent )
                    continue;
                
                if ( maildomainB->parent->totaldomaincount < maildomainB->totaldomaincount )
                {
                    errorcount++;
                    totalcount++;
                }
                
                if ( maildomainB->parent->configcount < maildomainB->configcount )
                {
                    errorcount++;
                    totalcount++;
                }
            }
        }
    }
    
    if ( errorcount )
    {
        Error("userdbtest", ERR_ERROR, "Mail domain test 4: %u errors", errorcount);
        errorcount = 0;
    }
    
    /* Test 5: Verify users on the domain have the correct maildomain */
    for ( i = 0; i < USERDB_MAILDOMAINHASHSIZE; i++ )
    {
        for ( maildomainA = userdb_maildomainhashtable_name[i]; maildomainA; maildomainA = maildomainA->nextbyname )
        {
            for ( userB = maildomainA->users; userB; userB = userB->nextbymaildomain )
            {
                if ( userB->maildomain != maildomainA )
                {
                    errorcount++;
                    totalcount++;
                }
            }
        }
    }
    
    if ( errorcount )
    {
        Error("userdbtest", ERR_ERROR, "Mail domain test 5: %u errors", errorcount);
        errorcount = 0;
    }
    
    /* Test 6: Check the right mail domain config is assigned to the domain */
    for ( i = 0; i < USERDB_MAILDOMAINHASHSIZE; i++ )
    {
        for ( maildomainA = userdb_maildomainhashtable_name[i]; maildomainA; maildomainA = maildomainA->nextbyname )
        {
            switch ( userdb_buildemaildomain(maildomainA, &emaildomain) )
            {
                case USERDB_OK:
                    break;
                default:
                    emaildomain = NULL;
                    break;
            }
            
            if ( !emaildomain )
            {
                errorcount++;
                totalcount++;
                continue;
            }
            
            switch ( userdb_findmaildomainconfigbyname(emaildomain, &maildomainconfigB) )
            {
                case USERDB_OK:
                    break;
                default:
                    maildomainconfigB = NULL;
                    break;
            }
            
            if ( maildomainA->config )
            {
                if ( maildomainconfigB != maildomainA->config )
                {
                    errorcount++;
                    totalcount++;
                }
                
                if ( !maildomainconfigB )
                    continue;
                
                if ( maildomainconfigB->maildomain != maildomainA )
                {
                    errorcount++;
                    totalcount++;
                }
                
                switch ( userdb_findmaildomainbyemaildomain(maildomainconfigB->configname->content, &maildomainB, 0) )
                {
                    case USERDB_OK:
                        break;
                    default:
                        maildomainB = NULL;
                        break;
                }
                
                if ( maildomainA != maildomainB )
                {
                    errorcount++;
                    totalcount++;
                }
            }
            else
            {
                if ( maildomainconfigB != NULL )
                {
                    errorcount++;
                    totalcount++;
                }
            }
        }
    }
    
    if ( errorcount )
    {
        Error("userdbtest", ERR_ERROR, "Mail domain test 6: %u errors", errorcount);
        errorcount = 0;
    }
    
    if ( totalcount )
    {
        Error("userdbtest", ERR_ERROR, "Mail domain total errors: %u", totalcount);
        return USERDB_ERROR;
    }
    
    Error("userdbtest", ERR_INFO, "All mail domain tests passed");
    return USERDB_OK;
}

int userdb_maildomainconfigfsck()
{
userdb_maildomainconfig *maildomainconfigA, *maildomainconfigB;
userdb_maildomain *maildomainB;
char *emaildomain;
unsigned int i;
unsigned int errorcount, totalcount;
unsigned long maildomainconfigopcount;

    errorcount = 0;
    totalcount = 0;
    maildomainconfigopcount = 0;
    
    /* Test 1: Check you can find a mail domain config from the ID table in the ID table */
    for ( i = 0; i < USERDB_MAILDOMAINCONFIGHASHSIZE; i++ )
    {
        for ( maildomainconfigA = userdb_maildomainconfighashtable_id[i]; maildomainconfigA; maildomainconfigA = maildomainconfigA->nextbyid )
        {
            switch ( userdb_findmaildomainconfigbyid(maildomainconfigA->configid, &maildomainconfigB) )
            {
                case USERDB_OK:
                    if ( maildomainconfigA != maildomainconfigB )
                    {
                        errorcount++;
                        totalcount++;
                    }
                    
                    break;
                default:
                    errorcount++;
                    totalcount++;
                    break;
            }
        }
    }
    
    if ( errorcount )
    {
        Error("userdbtest", ERR_ERROR, "Mail domain config test 1: %u errors", errorcount);
        errorcount = 0;
    }
    
    /* Test 2: Check you can find a mail domain config from the name table in the name table */
    for ( i = 0; i < USERDB_MAILDOMAINCONFIGHASHSIZE; i++ )
    {
        for ( maildomainconfigA = userdb_maildomainconfighashtable_name[i]; maildomainconfigA; maildomainconfigA = maildomainconfigA->nextbyname )
        {
            switch ( userdb_findmaildomainconfigbyname(maildomainconfigA->configname->content, &maildomainconfigB) )
            {
                case USERDB_OK:
                    if ( maildomainconfigA != maildomainconfigB )
                    {
                        errorcount++;
                        totalcount++;
                    }
                    
                    break;
                default:
                    errorcount++;
                    totalcount++;
                    break;
            }
        }
    }
    
    if ( errorcount )
    {
        Error("userdbtest", ERR_ERROR, "Mail domain config test 2: %u errors", errorcount);
        errorcount = 0;
    }
    
    /* Test 3: Check you can find a mail domain config from the ID table in the name table */
    for ( i = 0; i < USERDB_MAILDOMAINCONFIGHASHSIZE; i++ )
    {
        for ( maildomainconfigA = userdb_maildomainconfighashtable_id[i]; maildomainconfigA; maildomainconfigA = maildomainconfigA->nextbyid )
        {
            switch ( userdb_findmaildomainconfigbyname(maildomainconfigA->configname->content, &maildomainconfigB) )
            {
                case USERDB_OK:
                    if ( maildomainconfigA != maildomainconfigB )
                    {
                        errorcount++;
                        totalcount++;
                    }
                    
                    break;
                default:
                    errorcount++;
                    totalcount++;
                    break;
            }
        }
    }
    
    if ( errorcount )
    {
        Error("userdbtest", ERR_ERROR, "Mail domain config test 3: %u errors", errorcount);
        errorcount = 0;
    }
    
    /* Test 4: Check you can find a mail domain config from the name table in the ID table */
    for ( i = 0; i < USERDB_MAILDOMAINCONFIGHASHSIZE; i++ )
    {
        for ( maildomainconfigA = userdb_maildomainconfighashtable_name[i]; maildomainconfigA; maildomainconfigA = maildomainconfigA->nextbyname )
        {
            switch ( userdb_findmaildomainconfigbyid(maildomainconfigA->configid, &maildomainconfigB) )
            {
                case USERDB_OK:
                    if ( maildomainconfigA != maildomainconfigB )
                    {
                        errorcount++;
                        totalcount++;
                    }
                    
                    break;
                default:
                    errorcount++;
                    totalcount++;
                    break;
            }
        }
    }
    
    if ( errorcount )
    {
        Error("userdbtest", ERR_ERROR, "Mail domain config test 4: %u errors", errorcount);
        errorcount = 0;
    }
    
    /* Test 5: Check struct details */
    for ( i = 0; i < USERDB_MAILDOMAINCONFIGHASHSIZE; i++ )
    {
        for ( maildomainconfigA = userdb_maildomainconfighashtable_id[i]; maildomainconfigA; maildomainconfigA = maildomainconfigA->nextbyid )
        {
#ifdef USERDB_ENABLEREPLICATION
            if ( userdb_repl_group && userdb_repl_group->role == REPLICATOR_ROLE_MASTER && maildomainconfigA->configid > userdb_lastmaildomainconfigid )
#else
            if ( maildomainconfigA->configid > userdb_lastmaildomainconfigid )
#endif
            {
                errorcount++;
                totalcount++;
            }
            
/*
            if ( maildomainconfigA->created > maildomainconfigA->modified )
            {
                errorcount++;
                totalcount++;
            }
*/
            
            maildomainconfigopcount += maildomainconfigA->databaseops;
            
            if ( maildomainconfigA->databaseops > USERDB_DATABASE_MAXPEROBJECT )
            {
                errorcount++;
                totalcount++;
            }
            
            if ( !maildomainconfigA->revision )
            {
                errorcount++;
                totalcount++;
            }
            
            if ( !maildomainconfigA->transactionid )
            {
                errorcount++;
                totalcount++;
            }
            
            if ( maildomainconfigA->transactionid > userdb_lastmaildomainconfigtransactionid )
            {
                errorcount++;
                totalcount++;
            }
        }
    }
    
    if ( errorcount )
    {
        Error("userdbtest", ERR_ERROR, "Mail domain config test 5: %u errors", errorcount);
        errorcount = 0;
    }
    
    /* Test 6: Check mail domains have the correct mail domain config assigned */
    for ( i = 0; i < USERDB_MAILDOMAINCONFIGHASHSIZE; i++ )
    {
        for ( maildomainconfigA = userdb_maildomainconfighashtable_id[i]; maildomainconfigA; maildomainconfigA = maildomainconfigA->nextbyid )
        {
            if ( !maildomainconfigA->maildomain )
            {
                errorcount++;
                totalcount++;
                continue;
            }
            
            switch ( userdb_findmaildomainbyemaildomain(maildomainconfigA->configname->content, &maildomainB, 0) )
            {
                case USERDB_OK:
                    break;
                default:
                    maildomainB = NULL;
                    break;
            }
            
            if ( maildomainconfigA->maildomain != maildomainB )
            {
                errorcount++;
                totalcount++;
            }
            
            if ( !maildomainB )
                continue;
            
            if ( maildomainB->config != maildomainconfigA )
            {
                errorcount++;
                totalcount++;
            }
            
            if ( !maildomainB->configcount )
            {
                errorcount++;
                totalcount++;
            }
            
            switch ( userdb_buildemaildomain(maildomainB, &emaildomain) )
            {
                case USERDB_OK:
                    break;
                default:
                    emaildomain = NULL;
                    break;
            }
            
            if ( !emaildomain )
            {
                errorcount++;
                totalcount++;
                continue;
            }
            
            switch ( userdb_findmaildomainconfigbyname(emaildomain, &maildomainconfigB) )
            {
                case USERDB_OK:
                    break;
                default:
                    maildomainconfigB = NULL;
                    break;
            }
            
            if ( maildomainconfigA != maildomainconfigB )
            {
                errorcount++;
                totalcount++;
            }
        }
    }
    
    if ( errorcount )
    {
        Error("userdbtest", ERR_ERROR, "Mail domain config test 6: %u errors", errorcount);
        errorcount = 0;
    }
    
#ifdef USERDB_ENABLEREPLICATION
    /* Test 7: Check to see if you can find a deleted mail domain config in the deleted hash */
    for ( i = 0; i < USERDB_MAILDOMAINCONFIGDELETEDHASHSIZE; i++ )
    {
        for ( maildomainconfigA = userdb_maildomainconfigdeletedhashtable[i]; maildomainconfigA; maildomainconfigA = maildomainconfigA->nextbyid )
        {
            maildomainconfigopcount += maildomainconfigA->databaseops;
            
            switch ( userdb_finddeletedmaildomainconfig(maildomainconfigA->transactionid, &maildomainconfigB) )
            {
                case USERDB_OK:
                    break;
                default:
                    maildomainconfigB = NULL;
                    break;
            }
            
            if ( maildomainconfigA != maildomainconfigB )
            {
                errorcount++;
                totalcount++;
            }
        }
    }
    
    if ( errorcount )
    {
        Error("userdbtest", ERR_ERROR, "Mail domain config test 7: %u errors", errorcount);
        errorcount = 0;
    }
#endif
    
    /* Test 8: Check no deleted mail domain configs exist in the live hash */
    for ( i = 0; i < USERDB_MAILDOMAINCONFIGHASHSIZE; i++ )
    {
        for ( maildomainconfigA = userdb_maildomainconfighashtable_id[i]; maildomainconfigA; maildomainconfigA = maildomainconfigA->nextbyid )
        {
            if ( UDBRIsDeleted(maildomainconfigA) )
            {
                errorcount++;
                totalcount++;
            }
        }
    }
    
    if ( errorcount )
    {
        Error("userdbtest", ERR_ERROR, "Mail domain config test 8: %u errors", errorcount);
        errorcount = 0;
    }
    
#ifdef USERDB_ENABLEREPLICATION
    /* Test 9: Check to see no live mail domain configs are in the deleted hash */
    for ( i = 0; i < USERDB_MAILDOMAINCONFIGDELETEDHASHSIZE; i++ )
    {
        for ( maildomainconfigA = userdb_maildomainconfigdeletedhashtable[i]; maildomainconfigA; maildomainconfigA = maildomainconfigA->nextbyid )
        {
            if ( !UDBRIsDeleted(maildomainconfigA) )
            {
                errorcount++;
                totalcount++;
            }
        }
    }
    
    if ( errorcount )
    {
        Error("userdbtest", ERR_ERROR, "Mail domain config test 9: %u errors", errorcount);
        errorcount = 0;
    }
    
    /* Test 10: Check no deleted mail domain configs exist in the live hash */
    for ( i = 0; i < USERDB_MAILDOMAINCONFIGHASHSIZE; i++ )
    {
        for ( maildomainconfigA = userdb_maildomainconfighashtable_id[i]; maildomainconfigA; maildomainconfigA = maildomainconfigA->nextbyid )
        {
            switch ( userdb_finddeletedmaildomainconfig(maildomainconfigA->transactionid, &maildomainconfigB) )
            {
                case USERDB_NOTFOUND:
                    break;
                default:
                    errorcount++;
                    totalcount++;
                    break;
            }
        }
        
        for ( maildomainconfigA = userdb_maildomainconfighashtable_name[i]; maildomainconfigA; maildomainconfigA = maildomainconfigA->nextbyname )
        {
            switch ( userdb_finddeletedmaildomainconfig(maildomainconfigA->transactionid, &maildomainconfigB) )
            {
                case USERDB_NOTFOUND:
                    break;
                default:
                    errorcount++;
                    totalcount++;
                    break;
            }
        }
    }
    
    if ( errorcount )
    {
        Error("userdbtest", ERR_ERROR, "Mail domain config test 10: %u errors", errorcount);
        errorcount = 0;
    }
    
    /* Test 11: Check to see no deleted mail domains are in the live hash */
    for ( i = 0; i < USERDB_MAILDOMAINCONFIGDELETEDHASHSIZE; i++ )
    {
        for ( maildomainconfigA = userdb_maildomainconfigdeletedhashtable[i]; maildomainconfigA; maildomainconfigA = maildomainconfigA->nextbyid )
        {
            switch ( userdb_findmaildomainconfigbyid(maildomainconfigA->configid, &maildomainconfigB) )
            {
                case USERDB_NOTFOUND:
                    break;
                default:
                    errorcount++;
                    totalcount++;
                    break;
            }
            
            switch ( userdb_findmaildomainconfigbyname(maildomainconfigA->configname->content, &maildomainconfigB) )
            {
                case USERDB_NOTFOUND:
                    break;
                default:
                    errorcount++;
                    totalcount++;
                    break;
            }
        }
    }
    
    if ( errorcount )
    {
        Error("userdbtest", ERR_ERROR, "Mail domain config test 11: %u errors", errorcount);
        errorcount = 0;
    }
#endif
    
    if ( maildomainconfigopcount != (userdb_maildomainconfigdatabaseaddcount + userdb_maildomainconfigdatabasemodifycount + userdb_maildomainconfigdatabasedeletecount) )
    {
        totalcount++;
        Error("userdbtest", ERR_ERROR, "Language database operations count mismatch: %lu VS %lu", maildomainconfigopcount, (userdb_maildomainconfigdatabaseaddcount + userdb_maildomainconfigdatabasemodifycount + userdb_maildomainconfigdatabasedeletecount));
    }
    
    if ( totalcount )
    {
        Error("userdbtest", ERR_ERROR, "Mail domain config total errors: %u", totalcount);
        return USERDB_ERROR;
    }
    
    Error("userdbtest", ERR_INFO, "All mail domain config tests passed");
    return USERDB_OK;
}

int userdb_fsck()
{
int err, nochangeresult;

    nochangeresult = 0;
    
    if ( !nochangeresult )
        err = userdb_languagesfsck();
    else
        userdb_languagesfsck();
    
    if ( err != USERDB_OK )
        nochangeresult = 1;
    
    if ( !nochangeresult )
        err = userdb_usersfsck();
    else
        userdb_usersfsck();
    
    if ( err != USERDB_OK )
        nochangeresult = 1;
    
    if ( !nochangeresult )
        err = userdb_usertagsfsck();
    else
        userdb_usertagsfsck();
    
    if ( err != USERDB_OK )
        nochangeresult = 1;
    
    if ( !nochangeresult )
        err = userdb_maildomainsfsck();
    else
        userdb_maildomainsfsck();
    
    if ( err != USERDB_OK )
        nochangeresult = 1;
    
    if ( !nochangeresult )
        err = userdb_maildomainconfigfsck();
    else
        userdb_maildomainconfigfsck();
    
    if ( err != USERDB_OK )
        nochangeresult = 1;
    
    return err;
}

char *userdb_spacer(unsigned int count)
{
unsigned int i;
static char buf[200];

    for ( i = 0; i < count && i < 200; i++ )
        buf[i] = ' ';
    
    buf[count] = '\0';
    return buf;
}

int userdb_dumpmaildomaintree(userdb_maildomain *parent, unsigned int depth, FILE *output)
{
unsigned int i;
userdb_maildomain *tempmaildomain;

    if ( depth > 1 )
        return USERDB_OK;

    for ( i = 0; i < USERDB_MAILDOMAINHASHSIZE; i++ )
    {
        for ( tempmaildomain = userdb_maildomainhashtable_name[i]; tempmaildomain; tempmaildomain = tempmaildomain->nextbyname )
        {
            if ( tempmaildomain->parent == parent )
            {
                fprintf(output, "%s%s [%lu/%lu/%lu]\n", userdb_spacer(depth), tempmaildomain->maildomainname->content, tempmaildomain->domaincount, tempmaildomain->totaldomaincount, tempmaildomain->configcount);
                userdb_dumpmaildomaintree(tempmaildomain, depth + 1, output);
            }
        }
    }
    
    return USERDB_OK;
}

void userdb_createrandomlanguage()
{
int err;
userdb_language *language;

    switch ( err = userdb_createlanguage(userdb_randomstring(), userdb_randomemail(), &language) )
    {
        case USERDB_OK:
            break;
        default:
            Error("userdbtest", ERR_ERROR, "Could not make language: %d", err);
            return;
    }
}

void userdb_changerandomlanguage(userdb_language *language)
{
int code, err;

    err = USERDB_ERROR;
    
    switch ( code = ++userdb_lastlanguagetestaction )
    {
        case 0:
            err = userdb_changelanguagecode(language, userdb_randomstring());
            break;
        case 1:
            err = userdb_changelanguagename(language, userdb_randomemail());
            break;
        case 2:
            err = userdb_deletelanguage(language);
            userdb_lastlanguagetestaction = 0;
            break;
        default:
            userdb_lastlanguagetestaction = 0;
            break;
    }
    
    if ( err != USERDB_OK && err != USERDB_NOOP )
        Error("userdbtest", ERR_ERROR, "Something is erroring... language %d %d", code, err);
}

userdb_language *userdb_getrandomlanguage()
{
unsigned int hash, start;
userdb_language *language;

    hash = (userdb_getrandomulong() % USERDB_LANGUAGEHASHSIZE);
    start = hash;
    language = userdb_languagehashtable_id[hash];
    
    while ( !language )
    {
        hash++;
        
        if ( hash == start )
            return NULL;
        else if ( hash >= USERDB_LANGUAGEHASHSIZE )
            hash = 0;
        
        language = userdb_languagehashtable_id[hash];
    }
    
    return language;
}

void userdb_createrandomuser()
{
int err;
userdb_user *user;

    switch ( err = userdb_createuser(userdb_randomstring(), userdb_generatepassword(), userdb_randomemail(), &user) )
    {
        case USERDB_OK:
            break;
        default:
            Error("userdbtest", ERR_ERROR, "Could not make user: %d", err);
            return;
    }
}

void userdb_changerandomuser(userdb_user *user)
{
int code, err;
userdb_language *language;
flag_t suspendaction;
time_t suspendend;

    err = USERDB_ERROR;
    
    if ( user->userid <= 10 )
        return;
    
    switch ( code = ++userdb_lastusertestaction )
    {
        case 0:
            err = userdb_changeuserusername(user, userdb_randomstring());
            break;
        case 1:
            err = userdb_changeuserpassword(user, userdb_randomstring());
            break;
        case 2:
            language = userdb_getrandomlanguage();
            
            if ( !language )
            {
                Error("userdbtest", ERR_INFO, "No language found for test");
                return;
            }
            
            err = userdb_changeuserlanguage(user, language);
            break;
        case 3:
            err = userdb_changeuseremail(user, userdb_randomemail());
            break;
        case 4:
            err = userdb_changeuserstatusflags(user, (userdb_getrandomushort() & ~(UDB_STATUS_UNAUTHED | UDB_STATUS_AUTHED)));
            break;
        case 5:
            err = userdb_changeusergroupflags(user, userdb_getrandomushort());
            break;
        case 6:
            err = userdb_changeuseroptionflags(user, (userdb_getrandomushort() & ~(UDB_OPTION_SUSPENDED | UDB_OPTION_INSTANTKILL | UDB_OPTION_INSTANTGLINE | UDB_OPTION_DELAYEDGLINE)));
            break;
        case 7:
            err = userdb_changeusernoticeflags(user, userdb_getrandomushort());
            break;
        case 8:
            if ( UDBOIsSuspended(user) )
            {
                err = userdb_unsuspenduser(user);
            }
            else
            {
                switch ( userdb_getrandomulong() % 2 )
                {
                    case 0:
                        suspendend = (time(NULL) + (userdb_getrandomulong() % 1048576));
                        break;
                    case 1:
                        suspendend = 0;
                        break;
                    default:
                        suspendend = 0;
                        break;
                }
                
                switch ( userdb_getrandomulong() % 4 )
                {
                    case 0:
                        suspendaction = 0;
                        break;
                    case 1:
                        suspendaction = UDB_OPTION_INSTANTKILL;
                        break;
                    case 2:
                        suspendaction = UDB_OPTION_INSTANTGLINE;
                        break;
                    case 3:
                        suspendaction = UDB_OPTION_DELAYEDGLINE;
                        break;
                    default:
                        suspendaction = 0;
                        break;
                }
                
                err = userdb_suspenduser(user, userdb_randomstring(), suspendaction, suspendend, userdb_userhashtable_id[(userdb_getrandomulong() % USERDB_USERHASHSIZE)]);
            }
            break;
        case 9:
            err = userdb_deleteuser(user);
            userdb_lastusertestaction = 0;
            break;
        default:
            userdb_lastusertestaction = 0;
            break;
    }
    
    if ( err != USERDB_OK && err != USERDB_NOOP )
        Error("userdbtest", ERR_ERROR, "Something is erroring... user %d %d", code, err);
}

void userdb_updaterandomusertagdata()
{
int code, err;
userdb_user *user;
userdb_usertaginfo *usertaginfo;
userdb_usertagdata *usertagdataA, *usertagdataB;
unsigned int hash, complete;
unsigned short testtagtype, testtagsize;
void *testreturnvalue;
unsigned char testchar, *returnchar;
unsigned short testshort, *returnshort;
unsigned int testint, *returnint;
unsigned long testlong, *returnlong;
unsigned long long testlonglong, *returnlonglong;
float testfloat, *returnfloat;
double testdouble, *returndouble;
unsigned char teststringdata[USERDB_TEST_MAXTAGSTRINGSIZE + 1], *returnstringdata;
unsigned short teststringsize;
#ifndef USERDB_TEST_PREDICTABLETAGS
float testfloatnandetect;
double testdoublenandetect;
unsigned short i;
#endif

    complete = 0;
    err = USERDB_ERROR;
    hash = (userdb_getrandomulong() % USERDB_USERHASHSIZE);
    user = userdb_userhashtable_id[hash];
    
    if ( !user )
        return;
    
    switch ( userdb_getrandomulong() % 8 )
    {
        case 0:
            usertaginfo = userdb_charusertaginfo;
            break;
        case 1:
            usertaginfo = userdb_shortusertaginfo;
            break;
        case 2:
            usertaginfo = userdb_intusertaginfo;
            break;
        case 3:
            usertaginfo = userdb_longusertaginfo;
            break;
        case 4:
            usertaginfo = userdb_longlongusertaginfo;
            break;
        case 5:
            usertaginfo = userdb_floatusertaginfo;
            break;
        case 6:
            usertaginfo = userdb_doubleusertaginfo;
            break;
        case 7:
            usertaginfo = userdb_stringusertaginfo;
            break;
        default:
            usertaginfo = NULL;
    }
    
    if ( !usertaginfo )
        return;
    
    switch ( (code = userdb_getrandomulong() % 2) )
    {
        case 0:
            
            switch ( usertaginfo->tagtype )
            {
                case USERDB_TAGTYPE_CHAR:
#ifdef USERDB_TEST_PREDICTABLETAGS
                    testchar = (user->userid % 256);
#else
                    testchar = userdb_getrandomuchar();
#endif
                    err = userdb_setusertagdatavaluebyinfo(usertaginfo, user, &testchar, 0, &usertagdataA);
                    
                    if ( err != USERDB_OK && err != USERDB_NOOP )
                    {
                        err = 300;
                        break;
                    }
                    
                    err = userdb_getusertagdatavaluebyinfo(usertaginfo, user, &testtagtype, &testtagsize, &testreturnvalue, &usertagdataB);
                    returnchar = (unsigned char *)testreturnvalue;
/*
                    Error("userdbtest", ERR_INFO, "testchar: %hu, returnchar: %hu", testchar, *returnchar);
*/
                    
                    if ( err != USERDB_OK )
                    {
                        err = 400;
                        break;
                    }
                    else if ( testtagsize != sizeof(unsigned char) )
                    {
                        err = 502;
                        break;
                    }
                    else if ( testchar != *returnchar )
                    {
                        err = 503;
                        break;
                    }
                    
                    complete = 1;
                    break;
                case USERDB_TAGTYPE_SHORT:
#ifdef USERDB_TEST_PREDICTABLETAGS
                    testshort = (user->userid % 65536);
#else
                    testshort = userdb_getrandomushort();
#endif
                    err = userdb_setusertagdatavaluebyinfo(usertaginfo, user, &testshort, 0, &usertagdataA);
                    
                    if ( err != USERDB_OK && err != USERDB_NOOP )
                    {
                        err = 300;
                        break;
                    }
                    
                    err = userdb_getusertagdatavaluebyinfo(usertaginfo, user, &testtagtype, &testtagsize, &testreturnvalue, &usertagdataB);
                    returnshort = (unsigned short *)testreturnvalue;
/*
                    Error("userdbtest", ERR_INFO, "testshort: %hu, returnshort: %hu", testshort, *returnshort);
*/
                    
                    if ( err != USERDB_OK )
                    {
                        err = 400;
                        break;
                    }
                    else if ( testtagsize != sizeof(unsigned short) )
                    {
                        err = 502;
                        break;
                    }
                    else if ( testshort != *returnshort )
                    {
                        err = 503;
                        break;
                    }
                    
                    complete = 1;
                    break;
                case USERDB_TAGTYPE_INT:
#ifdef USERDB_TEST_PREDICTABLETAGS
                    testint = (user->userid % 56264);
#else
                    testint = userdb_getrandomuint();
#endif
                    err = userdb_setusertagdatavaluebyinfo(usertaginfo, user, &testint, 0, &usertagdataA);
                    
                    if ( err != USERDB_OK && err != USERDB_NOOP )
                    {
                        err = 300;
                        break;
                    }
                    
                    err = userdb_getusertagdatavaluebyinfo(usertaginfo, user, &testtagtype, &testtagsize, &testreturnvalue, &usertagdataB);
                    returnint = (unsigned int *)testreturnvalue;
/*
                    Error("userdbtest", ERR_INFO, "testint: %u, returnint: %u", testint, *returnint);
*/
                    
                    if ( err != USERDB_OK )
                    {
                        err = 400;
                        break;
                    }
                    else if ( testtagsize != sizeof(unsigned int) )
                    {
                        err = 502;
                        break;
                    }
                    else if ( testint != *returnint )
                    {
                        err = 503;
                        break;
                    }
                    
                    complete = 1;
                    break;
                case USERDB_TAGTYPE_LONG:
#ifdef USERDB_TEST_PREDICTABLETAGS
                    testlong = (user->userid % 91647);
#else
                    testlong = userdb_getrandomulong();
#endif
                    err = userdb_setusertagdatavaluebyinfo(usertaginfo, user, &testlong, 0, &usertagdataA);
                    
                    if ( err != USERDB_OK && err != USERDB_NOOP )
                    {
                        err = 300;
                        break;
                    }
                    
                    err = userdb_getusertagdatavaluebyinfo(usertaginfo, user, &testtagtype, &testtagsize, &testreturnvalue, &usertagdataB);
                    returnlong = (unsigned long *)testreturnvalue;
/*
                    Error("userdbtest", ERR_INFO, "testlong: %lu, returnlong: %lu", testlong, *returnlong);
*/
                    
                    if ( err != USERDB_OK )
                    {
                        err = 400;
                        break;
                    }
                    else if ( testtagsize != sizeof(unsigned long) )
                    {
                        err = 502;
                        break;
                    }
                    else if ( testlong != *returnlong )
                    {
                        err = 503;
                        break;
                    }
                    
                    complete = 1;
                    break;
                case USERDB_TAGTYPE_LONGLONG:
#ifdef USERDB_TEST_PREDICTABLETAGS
                    testlonglong = (user->userid % 78573);
#else
                    testlonglong = userdb_getrandomulonglong();
#endif
                    err = userdb_setusertagdatavaluebyinfo(usertaginfo, user, &testlonglong, 0, &usertagdataA);
                    
                    if ( err != USERDB_OK && err != USERDB_NOOP )
                    {
                        err = 300;
                        break;
                    }
                    
                    err = userdb_getusertagdatavaluebyinfo(usertaginfo, user, &testtagtype, &testtagsize, &testreturnvalue, &usertagdataB);
                    returnlonglong = (unsigned long long *)testreturnvalue;
/*
                    Error("userdbtest", ERR_INFO, "testlonglong: %llu, returnlonglong: %llu", testlonglong, *returnlonglong);
*/
                    
                    if ( err != USERDB_OK )
                    {
                        err = 400;
                        break;
                    }
                    else if ( testtagsize != sizeof(unsigned long long) )
                    {
                        err = 502;
                        break;
                    }
                    else if ( testlonglong != *returnlonglong )
                    {
                        err = 503;
                        break;
                    }
                    
                    complete = 1;
                    break;
                case USERDB_TAGTYPE_FLOAT:
                    
#ifdef USERDB_TEST_PREDICTABLETAGS
                    testfloat = (user->userid % 25593);
#else
                    /* Detect NaN's */
                    do
                    {
                        testfloat = userdb_getrandomfloat();
                        testfloatnandetect = testfloat;
                    }
                    while ( testfloat != testfloatnandetect );
#endif
                    
                    err = userdb_setusertagdatavaluebyinfo(usertaginfo, user, &testfloat, 0, &usertagdataA);
                    
                    if ( err != USERDB_OK && err != USERDB_NOOP )
                    {
                        err = 300;
                        break;
                    }
                    
                    err = userdb_getusertagdatavaluebyinfo(usertaginfo, user, &testtagtype, &testtagsize, &testreturnvalue, &usertagdataB);
                    returnfloat = (float *)testreturnvalue;
/*
                    Error("userdbtest", ERR_INFO, "testfloat: %.100f, returnfloat: %.100f", testfloat, *returnfloat);
*/
                    
                    if ( err != USERDB_OK )
                    {
                        err = 400;
                        break;
                    }
                    else if ( testtagsize != sizeof(float) )
                    {
                        err = 502;
                        break;
                    }
                    else if ( testfloat != *returnfloat )
                    {
                        Error("userdbtest", ERR_INFO, "testfloat: %.100f, returnfloat: %.100f", testfloat, *returnfloat);
                        err = 503;
                        break;
                    }
                    
                    complete = 1;
                    break;
                case USERDB_TAGTYPE_DOUBLE:
                    
#ifdef USERDB_TEST_PREDICTABLETAGS
                    testdouble = (user->userid % 10573);
#else
                    /* Detect NaN's */
                    do
                    {
                        testdouble = userdb_getrandomdouble();
                        testdoublenandetect = testdouble;
                    }
                    while ( testdouble != testdoublenandetect );
#endif
                    
                    err = userdb_setusertagdatavaluebyinfo(usertaginfo, user, &testdouble, 0, &usertagdataA);
                    
                    if ( err != USERDB_OK && err != USERDB_NOOP )
                    {
                        err = 300;
                        break;
                    }
                    
                    err = userdb_getusertagdatavaluebyinfo(usertaginfo, user, &testtagtype, &testtagsize, &testreturnvalue, &usertagdataB);
                    returndouble = (double *)testreturnvalue;
/*
                    Error("userdbtest", ERR_INFO, "testdouble: %.100f, returndouble: %.100f", testdouble, *returndouble);
*/
                    
                    if ( err != USERDB_OK )
                    {
                        err = 400;
                        break;
                    }
                    else if ( testtagsize != sizeof(double) )
                    {
                        err = 502;
                        break;
                    }
                    else if ( testdouble != *returndouble )
                    {
                        Error("userdbtest", ERR_INFO, "testdouble: %.100f, returndouble: %.100f", testdouble, *returndouble);
                        err = 503;
                        break;
                    }
                    
                    complete = 1;
                    break;
                case USERDB_TAGTYPE_STRING:
                    memset(teststringdata, 0, (USERDB_TEST_MAXTAGSTRINGSIZE + 1));
#ifdef USERDB_TEST_PREDICTABLETAGS
                    teststringsize = (unsigned short)snprintf((char *)teststringdata, sizeof(teststringdata), "%lu", user->userid);
                    teststringdata[sizeof(teststringdata) - 1] = '\0';
#else
                    teststringsize = (userdb_getrandomushort() % (USERDB_TEST_MAXTAGSTRINGSIZE - USERDB_TEST_MINTAGSTRINGSIZE)) + USERDB_TEST_MINTAGSTRINGSIZE;
                    
                    for ( i = 0; i < teststringsize; i++ )
                        teststringdata[i] = userdb_getrandomuchar();
#endif
                    err = userdb_setusertagdatavaluebyinfo(usertaginfo, user, &teststringdata, teststringsize, &usertagdataA);
                    
                    if ( err != USERDB_OK && err != USERDB_NOOP )
                    {
                        err = 300;
                        break;
                    }
                    
                    err = userdb_getusertagdatavaluebyinfo(usertaginfo, user, &testtagtype, &testtagsize, &testreturnvalue, &usertagdataB);
                    returnstringdata = (unsigned char *)testreturnvalue;
                    
                    if ( err != USERDB_OK )
                    {
                        err = 400;
                        break;
                    }
                    else if ( testtagsize != teststringsize )
                    {
                        err = 502;
                        break;
                    }
                    else if ( memcmp(returnstringdata, teststringdata, teststringsize) )
                    {
                        err = 503;
                        break;
                    }
                    
                    complete = 1;
                    break;
                default:
                    break;
            }
            
            if ( complete && usertagdataA != usertagdataB )
            {
                err = 500;
                break;
            }
            else if ( complete && testtagtype != usertaginfo->tagtype )
            {
                err = 501;
                break;
            }
            
            break;
        case 1:
            err = userdb_clearusertagdatavaluebyinfo(usertaginfo, user);
            break;
        default:
            break;
    }
    
    if ( err != USERDB_OK && err != USERDB_NOOP && err != USERDB_NOTFOUND )
        Error("userdbtest", ERR_ERROR, "Something is erroring... user tag %d %d %hu %lu", code, err, usertaginfo->tagtype, user->userid);
}

void userdb_createrandommaildomainconfig()
{
int err;
userdb_maildomainconfig *maildomainconfig;

    switch ( err = userdb_createmaildomainconfig(userdb_randomemaildomain(), userdb_getrandomushort(), (userdb_getrandomulong() % 1000), (userdb_getrandomulong() % 1000), &maildomainconfig) )
    {
        case USERDB_OK:
            break;
        default:
            Error("userdbtest", ERR_ERROR, "Could not make mail domain config: %d", err);
            return;
    }
}

void userdb_changerandommaildomainconfig(userdb_maildomainconfig *maildomainconfig)
{
int code, err;

    err = USERDB_ERROR;
    
    switch ( code = ++userdb_lastmaildomainconfigtestaction )
    {
        case 0:
            err = userdb_modifymaildomainconfigflags(maildomainconfig, userdb_getrandomushort());
            break;
        case 1:
            err = userdb_modifymaildomainconfigmaxlocal(maildomainconfig, (userdb_getrandomulong() % 1000));
            break;
        case 2:
            err = userdb_modifymaildomainconfigmaxdomain(maildomainconfig, (userdb_getrandomulong() % 1000));
            break;
        case 3:
            err = userdb_deletemaildomainconfig(maildomainconfig);
            userdb_lastmaildomainconfigtestaction = 0;
            break;
        default:
            userdb_lastmaildomainconfigtestaction = 0;
            break;
    }
    
    if ( err != USERDB_OK && err != USERDB_NOOP )
        Error("userdbtest", ERR_ERROR, "Something is erroring... mail domain config %d %d", code, err);
}

void userdb_changedata()
{
unsigned int hash, i;

#ifdef USERDB_ENABLEREPLICATION
    if ( userdb_repl_group && userdb_repl_group->role == REPLICATOR_ROLE_MASTER )
    {
#endif
        hash = (userdb_getrandomulong() % USERDB_LANGUAGEHASHSIZE);
        
        for ( i = 0; i < 10; i++ )
        {
            if ( !userdb_languagehashtable_id[hash] )
                userdb_createrandomlanguage();
            else
                userdb_changerandomlanguage(userdb_languagehashtable_id[hash]);
            
            hash++;
            
            if ( hash >= USERDB_LANGUAGEHASHSIZE )
                hash = 0;
        }
        
        hash = (userdb_getrandomulong() % USERDB_USERHASHSIZE);
        
        for ( i = 0; i < 2500; i++ )
        {
            if ( !userdb_userhashtable_id[hash] )
                userdb_createrandomuser();
            else
                userdb_changerandomuser(userdb_userhashtable_id[hash]);
            
            hash++;
            
            if ( hash >= USERDB_USERHASHSIZE )
                hash = 0;
        }
        
        hash = (userdb_getrandomulong() % USERDB_MAILDOMAINCONFIGHASHSIZE);
        
        for ( i = 0; i < 1000; i++ )
        {
            if ( !userdb_maildomainconfighashtable_id[hash] )
                userdb_createrandommaildomainconfig();
            else
                userdb_changerandommaildomainconfig(userdb_maildomainconfighashtable_id[hash]);
            
            hash++;
            
            if ( hash >= USERDB_MAILDOMAINCONFIGHASHSIZE )
                hash = 0;
        }
#ifdef USERDB_ENABLEREPLICATION
    }
#endif
    
    for ( i = 0; i < 2500; i++ )
        userdb_updaterandomusertagdata();
}

void userdb_changedatalarge()
{
userdb_user *user;
unsigned int i, j;

#ifdef USERDB_ENABLEREPLICATION
    if ( !userdb_repl_group || userdb_repl_group->role != REPLICATOR_ROLE_MASTER )
        return;
#endif
    
    for ( i = 0; i < 3; i++ )
        for ( j = 0; j < (USERDB_USERHASHSIZE / 10); j++ )
            for ( user = userdb_userhashtable_id[j]; user; user = user->nextbyid )
                userdb_changeuserusername(user, userdb_randomstring());
}

void userdb_test()
{
    userdb_changedata();
    
    if ( userdb_fsck() == USERDB_OK )
    {
        if ( !userdb_dumpedmaildomaintree )
        {
/*
            FILE *mdtree;
            
            mdtree = fopen("./treedump", "w");
            
            if ( !mdtree )
            {
                Error("userdbtest", ERR_INFO, "Could not create output file for mail domain tree dump");
                return;
            }
            
            userdb_dumpmaildomaintree(NULL, 0, mdtree);
            fclose(mdtree);
*/
            userdb_dumpedmaildomaintree = 1;
        }
    }
}

void userdb_starttesting()
{
    userdb_testingstarted = 1;
    
    switch ( userdb_registerusertag(USERTAG_TEST_CHAR, USERDB_TAGTYPE_CHAR, "userdbtest", &userdb_charusertaginfo) )
    {
        case USERDB_OK:
        case USERDB_ALREADYEXISTS:
            break;
        default:
            userdb_charusertaginfo = NULL;
            Error("userdbtest", ERR_FATAL, "Failed to register USERTAG_TEST_CHAR");
            return;
    }
    
    switch ( userdb_registerusertag(USERTAG_TEST_SHORT, USERDB_TAGTYPE_SHORT, "userdbtest", &userdb_shortusertaginfo) )
    {
        case USERDB_OK:
        case USERDB_ALREADYEXISTS:
            break;
        default:
            userdb_shortusertaginfo = NULL;
            Error("userdbtest", ERR_FATAL, "Failed to register USERTAG_TEST_SHORT");
            return;
    }
    
    switch ( userdb_registerusertag(USERTAG_TEST_INT, USERDB_TAGTYPE_INT, "userdbtest", &userdb_intusertaginfo) )
    {
        case USERDB_OK:
        case USERDB_ALREADYEXISTS:
            break;
        default:
            userdb_intusertaginfo = NULL;
            Error("userdbtest", ERR_FATAL, "Failed to register USERTAG_TEST_INT");
            return;
    }
    
    switch ( userdb_registerusertag(USERTAG_TEST_LONG, USERDB_TAGTYPE_LONG, "userdbtest", &userdb_longusertaginfo) )
    {
        case USERDB_OK:
        case USERDB_ALREADYEXISTS:
            break;
        default:
            userdb_longusertaginfo = NULL;
            Error("userdbtest", ERR_FATAL, "Failed to register USERTAG_TEST_LONG");
            return;
    }
    
    switch ( userdb_registerusertag(USERTAG_TEST_LONGLONG, USERDB_TAGTYPE_LONGLONG, "userdbtest", &userdb_longlongusertaginfo) )
    {
        case USERDB_OK:
        case USERDB_ALREADYEXISTS:
            break;
        default:
            userdb_longlongusertaginfo = NULL;
            Error("userdbtest", ERR_FATAL, "Failed to register USERTAG_TEST_LONGLONG");
            return;
    }
    
    switch ( userdb_registerusertag(USERTAG_TEST_FLOAT, USERDB_TAGTYPE_FLOAT, "userdbtest", &userdb_floatusertaginfo) )
    {
        case USERDB_OK:
        case USERDB_ALREADYEXISTS:
            break;
        default:
            userdb_floatusertaginfo = NULL;
            Error("userdbtest", ERR_FATAL, "Failed to register USERTAG_TEST_FLOAT");
            return;
    }
    
    switch ( userdb_registerusertag(USERTAG_TEST_DOUBLE, USERDB_TAGTYPE_DOUBLE, "userdbtest", &userdb_doubleusertaginfo) )
    {
        case USERDB_OK:
        case USERDB_ALREADYEXISTS:
            break;
        default:
            userdb_doubleusertaginfo = NULL;
            Error("userdbtest", ERR_FATAL, "Failed to register USERTAG_TEST_DOUBLE");
            return;
    }
    
    switch ( userdb_registerusertag(USERTAG_TEST_STRING, USERDB_TAGTYPE_STRING, "userdbtest", &userdb_stringusertaginfo) )
    {
        case USERDB_OK:
        case USERDB_ALREADYEXISTS:
            break;
        default:
            userdb_stringusertaginfo = NULL;
            Error("userdbtest", ERR_FATAL, "Failed to register USERTAG_TEST_STRING");
            return;
    }
    
    userdb_fsck();
/*
    userdb_changedatalarge();
*/
    
#ifdef USERDB_ENABLEREPLICATION
    if ( userdb_repl_group && userdb_repl_group->role == REPLICATOR_ROLE_MASTER )
        userdb_testschedule = schedulerecurring((time(NULL) + 5), 0, 300, userdb_test, NULL);
    else
        userdb_testschedule = schedulerecurring((time(NULL) + 5), 0, 300, userdb_test, NULL);
#else
    userdb_testschedule = schedulerecurring((time(NULL) + 5), 0, 300, userdb_test, NULL);
#endif
}

void userdb_handleuserdbloaded(int hooknum, void *arg)
{
    Error("userdbtest", ERR_INFO, "UserDB database is now loaded, starting tests");
    userdb_starttesting();
}

void _init()
{
    userdb_testschedule = NULL;
    userdb_testingstarted = 0;
    userdb_dumpedmaildomaintree = 0;
    userdb_lastlanguagetestaction = 0;
    userdb_lastusertestaction = 0;
    userdb_lastmaildomainconfigtestaction = 0;
    userdb_charusertaginfo = NULL;
    userdb_shortusertaginfo = NULL;
    userdb_intusertaginfo = NULL;
    userdb_longusertaginfo = NULL;
    userdb_longlongusertaginfo = NULL;
    userdb_floatusertaginfo = NULL;
    userdb_doubleusertaginfo = NULL;
    userdb_stringusertaginfo = NULL;
    userdb_randomsource = NULL;
    
    if ( !userdb_moduleready() )
    {
        Error("userdbtest", ERR_FATAL, "UserDB module is not ready");
        return;
    }
    
    registerhook(HOOK_USERDB_DBLOADED, &userdb_handleuserdbloaded);
    
    if ( !userdb_openrandomsource() )
    {
        Error("userdbtest", ERR_FATAL, "Failed to open random data source");
        return;
    }
    
    if ( userdb_databaseloaded() )
        userdb_starttesting();
    else
        Error("userdbtest", ERR_INFO, "UserDB database is not loaded, deferring testing");
}

void _fini()
{
    deregisterhook(HOOK_USERDB_DBLOADED, &userdb_handleuserdbloaded);
    
    userdb_closerandomsource();
    
    if ( userdb_testschedule )
        deleteschedule(userdb_testschedule, userdb_test, NULL);
    
    if ( !userdb_testingstarted )
        return;
    
/*
    switch ( userdb_clearallusertagdatabyinfo(userdb_charusertaginfo) )
    {
        case USERDB_OK:
            break;
        default:
            Error("userdbtest", ERR_FATAL, "Failed to clear userdb_charusertaginfo");
            return;
    }
    
    userdb_charusertaginfo = NULL;
    
    switch ( userdb_unregisterusertagbydata(userdb_shortusertaginfo) )
    {
        case USERDB_OK:
            break;
        default:
            Error("userdbtest", ERR_FATAL, "Failed to unregister userdb_shortusertaginfo");
            return;
    }
    
    userdb_shortusertaginfo = NULL;
    
    switch ( userdb_clearallusertagdatabyinfo(userdb_intusertaginfo) )
    {
        case USERDB_OK:
            break;
        default:
            Error("userdbtest", ERR_FATAL, "Failed to clear userdb_intusertaginfo");
            return;
    }
    
    userdb_intusertaginfo = NULL;
    
    switch ( userdb_unregisterusertagbydata(userdb_longusertaginfo) )
    {
        case USERDB_OK:
            break;
        default:
            Error("userdbtest", ERR_FATAL, "Failed to unregister userdb_longusertaginfo");
            return;
    }
    
    userdb_longusertaginfo = NULL;
    
    switch ( userdb_clearallusertagdatabyinfo(userdb_longlongusertaginfo) )
    {
        case USERDB_OK:
            break;
        default:
            Error("userdbtest", ERR_FATAL, "Failed to clear userdb_longlongusertaginfo");
            return;
    }
    
    userdb_longlongusertaginfo = NULL;
    
    switch ( userdb_unregisterusertagbydata(userdb_floatusertaginfo) )
    {
        case USERDB_OK:
            break;
        default:
            Error("userdbtest", ERR_FATAL, "Failed to unregister userdb_floatusertaginfo");
            return;
    }
    
    userdb_floatusertaginfo = NULL;
    
    switch ( userdb_clearallusertagdatabyinfo(userdb_doubleusertaginfo) )
    {
        case USERDB_OK:
            break;
        default:
            Error("userdbtest", ERR_FATAL, "Failed to clear userdb_doubleusertaginfo");
            return;
    }
    
    userdb_doubleusertaginfo = NULL;
    
    switch ( userdb_unregisterusertagbydata(userdb_stringusertaginfo) )
    {
        case USERDB_OK:
            break;
        default:
            Error("userdbtest", ERR_FATAL, "Failed to unregister userdb_stringusertaginfo");
            return;
    }
    
    userdb_stringusertaginfo = NULL;
*/
}
