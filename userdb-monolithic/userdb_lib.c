/*
 * UserDB
 */

#include "./userdb.h"
#include "../lib/sha2.h"

/* Taken from Q9, which was taken from O */
char *userdb_generatepassword()
{
unsigned int i;
char allowedchars[] = "ABCDEFGHIJKLMNPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
static char buf[USERDB_DEFAULTPASSWORDLEN + 1];
  
    for ( i = 0; i < USERDB_DEFAULTPASSWORDLEN; i++ )
        buf[i] = allowedchars[rand() % (sizeof(allowedchars) - 1)];
    
    buf[USERDB_DEFAULTPASSWORDLEN] = '\0';
    return buf;
}

int userdb_hashpasswordbydata(unsigned long userid, time_t created, char *password, unsigned char *result)
{
SHA256_CTX hash;
unsigned char buf[USERDB_MAXPASSWORDLEN + 201];
unsigned long buflen;

    if ( !userid || !created || !password || userdb_zerolengthstring(password) || !result )
        return USERDB_BADPARAMETERS;
    
    switch ( userdb_checkpassword(password) )
    {
        case USERDB_OK:
            break;
        case USERDB_BADPASSWORD:
            return USERDB_BADPASSWORD;
        default:
            return USERDB_ERROR;
    }
    
    buflen = snprintf((char *)buf, sizeof(buf), "QuakeNet User Database System %lu %ld %s", userid, created, password);
    buf[sizeof(buf) - 1] = '\0';
    
    SHA256_Init(&hash);
    SHA256_Update(&hash, buf, buflen);
    SHA256_Final(result, &hash);
    
    return USERDB_OK;
}

int userdb_hashpasswordbyuser(userdb_user *user, char *password, unsigned char *result)
{
    if ( !user )
        return USERDB_BADPARAMETERS;
    
    return userdb_hashpasswordbydata(user->userid, user->created, password, result);
}

int userdb_verifypasswordbydata(unsigned long userid, time_t created, unsigned char *userpassword, char *testpassword)
{
unsigned char result[USERDB_PASSWORDHASHLEN];

    if ( !userid || !created || !userpassword || !testpassword )
        return USERDB_BADPARAMETERS;
    
    switch ( userdb_hashpasswordbydata(userid, created, testpassword, result) )
    {
        case USERDB_OK:
            break;
        case USERDB_BADPARAMETERS:
            return USERDB_BADPARAMETERS;
        case USERDB_BADPASSWORD:
            return USERDB_BADPASSWORD;
        default:
            return USERDB_ERROR;
    }
    
    if ( !memcmp(userpassword, result, USERDB_PASSWORDHASHLEN) )
        return USERDB_OK;
    else
        return USERDB_WRONGPASSWORD;
}

int userdb_verifypasswordbyuser(userdb_user *user, char *testpassword)
{
    if ( !user )
        return USERDB_BADPARAMETERS;
    
    return userdb_verifypasswordbydata(user->userid, user->created, user->password, testpassword);
}

int userdb_checklanguagecode(char *languagecode)
{
unsigned int i;

    if ( !languagecode || userdb_zerolengthstring(languagecode) )
        return USERDB_BADPARAMETERS;
    
    for ( i = 0; languagecode[i]; i++ )
    {
        if ( i >= USERDB_LANGUAGELEN )
            return USERDB_BADLANGUAGECODE;
        else if ( languagecode[i] != ' ' )
            continue;
        else
            return USERDB_BADLANGUAGECODE;
    }
    
    return USERDB_OK;
}

int userdb_checklanguagename(char *languagename)
{
unsigned int i;

    if ( !languagename || userdb_zerolengthstring(languagename) )
        return USERDB_BADPARAMETERS;
    
    for ( i = 0; languagename[i]; i++ )
    {
        if ( i >= USERDB_LANGUAGELEN )
            return USERDB_BADLANGUAGENAME;
        else if ( languagename[i] != ' ' )
            continue;
        else
            return USERDB_BADLANGUAGENAME;
    }
    
    return USERDB_OK;
}

int userdb_checkusername(char *username)
{
unsigned int i;

    if ( !username || userdb_zerolengthstring(username) )
        return USERDB_BADPARAMETERS;
    else if ( (username[0] == '-') || (username[0] == '+') )
        return USERDB_BADUSERNAME;
    
    /* Is this REALLY needed? */
    for ( i = 0; username[i]; i++ )
    {
        if ( i >= USERDB_MAXUSERNAMELEN )
            return USERDB_BADUSERNAME;
        else if ( (username[i] == '-') || (username[i] >= '0' && username[i] <= '9') || (username[i] >= 'A' && username[i] <= 'Z') || (username[i] >= 'a' && username[i] <= 'z') )
            continue;
        else
            return USERDB_BADUSERNAME;
    }
    
    return USERDB_OK;
}

int userdb_checkpassword(char *password)
{
unsigned int i;

    if ( !password || userdb_zerolengthstring(password) )
        return USERDB_BADPARAMETERS;
    
    for ( i = 0; password[i]; i++ )
    {
        if ( i >= USERDB_MAXPASSWORDLEN )
            return USERDB_BADPASSWORD;
        else if ( password[i] >= '!' && password[i] <= '~' )
            continue;
        else
            return USERDB_BADPASSWORD;
    }
    
    return USERDB_OK;
}

int userdb_checkemailaddress(char *email)
{
unsigned long emaillen, i;

    if ( !email || userdb_zerolengthstring(email) )
        return USERDB_BADPARAMETERS;
    
    switch ( userdb_strlen(email, USERDB_EMAILLEN + 1, &emaillen) )
    {
        case USERDB_OK:
            break;
        case USERDB_LENGTHEXCEEDED:
            return USERDB_BADEMAIL;
        default:
            return USERDB_ERROR;
    }
    
    if ( !(strchr(email, '@')) || (email[0] == '@') || (email[emaillen - 1] == '@') )
        return USERDB_BADEMAIL;
    
    for ( i = 0; i < emaillen; i++ )
    {
        if ( (email[i] == '-') || (email[i] == '_') || (email[i] == '.') || (email[i] == '@') || (email[i] >= '0' && email[i] <= '9') || (email[i] >= 'A' && email[i] <= 'Z') || (email[i] >= 'a' && email[i] <= 'z') )
            continue;
        else
            return USERDB_BADEMAIL;
    }
    
    if ( !ircd_strcmp("user@mymailhost.xx", email)
      || !ircd_strcmp("info@quakenet.org", email)
      || !ircd_strcmp("staff@quakenet.org", email)
      || !ircd_strcmp("opers@quakenet.org", email)
      || !ircd_strcmp("pr@quakenet.org", email)
      || !ircd_strcmp("user@mymail.xx", email)
      || !ircd_strcmp("user@mail.cc", email)
      || !ircd_strcmp("user@host.com", email)
      || !ircd_strcmp("Jackie@your.isp.com", email)
      || !ircd_strcmp("qbot@quakenet.org", email))
        return USERDB_BADEMAIL;
    
    if ( pcre_exec(userdb_emailregex, userdb_emailregexextra, email, emaillen, 0, 0, NULL, 0) < 0 )
        return USERDB_BADEMAIL;
    
    return USERDB_OK;
}

int userdb_checksuspendreason(char *reason)
{
unsigned int i;

    if ( !reason || userdb_zerolengthstring(reason) )
        return USERDB_BADPARAMETERS;
    
    for ( i = 0; reason[i]; i++ )
    {
        if ( i >= USERDB_SUSPENDREASONLEN )
            return USERDB_BADSUSPENDREASON;
        else if ( reason[i] >= ' ' && reason[i] <= '~' )
            continue;
        else
            return USERDB_BADSUSPENDREASON;
    }
    
    return USERDB_OK;
}

int userdb_checkmaildomainconfigname(char *configname)
{
unsigned int i;

    if ( !configname || userdb_zerolengthstring(configname) )
        return USERDB_BADPARAMETERS;
    
    for ( i = 0; configname[i]; i++ )
    {
        if ( i >= USERDB_EMAILLEN )
            return USERDB_BADMAILDOMAINCONFIGNAME;
        else if ( configname[i] == '.' )
        {
            if ( (configname[i + 1] == '.') || (configname[i + 1] == '\0') )
                return USERDB_BADMAILDOMAINCONFIGNAME;
            
            continue;
        }
        else if ( (configname[i] == '-') || (configname[i] == '_') || (configname[i] >= '0' && configname[i] <= '9') || (configname[i] >= 'A' && configname[i] <= 'Z') || (configname[i] >= 'a' && configname[i] <= 'z') )
            continue;
        else
            return USERDB_BADMAILDOMAINCONFIGNAME;
    }
    
    return USERDB_OK;
}

int userdb_splitemaillocalpart(char *email, char *emaillocal, char *emaildomain)
{
char emailsplit_copy[USERDB_EMAILLEN + 1], *emailsplit_divide;
unsigned int i;

    if ( !email || userdb_zerolengthstring(email) || !emaillocal || !emaildomain )
        return USERDB_BADPARAMETERS;
    
    if ( email[0] == '@' )
        return USERDB_BADEMAIL;
    
    memset(emailsplit_copy, 0, USERDB_EMAILLEN + 1);
    
    for ( i = 0; email[i]; i++ )
    {
        if ( i >= USERDB_EMAILLEN )
            return USERDB_BADEMAIL;
        else if ( email[i] >= 'A' && email[i] <= 'Z' )
            emailsplit_copy[i] = email[i] + ('a' - 'A');
        else
            emailsplit_copy[i] = email[i];
    }
    
    emailsplit_divide = strchr(emailsplit_copy, '@');
    
    if ( !emailsplit_divide )
        return USERDB_BADEMAIL;
    
    *emailsplit_divide = '\0';
    emailsplit_divide++;
    
    if ( *emailsplit_divide == '\0' )
        return USERDB_BADEMAIL;
    
    for ( i = 0; emailsplit_divide[i]; i++ )
        if ( emailsplit_divide[i] == '@' )
            return USERDB_BADEMAIL;
    
    strncpy(emaillocal, emailsplit_copy, USERDB_EMAILLEN);
    strncpy(emaildomain, emailsplit_divide, USERDB_EMAILLEN);
    emaillocal[USERDB_EMAILLEN] = '\0';
    emaildomain[USERDB_EMAILLEN] = '\0';
    return USERDB_OK;
}

int userdb_splitemaildomainfirst(char *domain, char **start)
{
unsigned long domainlen, i;
char *emaildomain_divide;
static char emaildomain_copy[USERDB_EMAILLEN + 3];

    if ( !domain || userdb_zerolengthstring(domain) || !start )
        return USERDB_BADPARAMETERS;
    
    switch ( userdb_strlen(domain, USERDB_EMAILLEN + 1, &domainlen) )
    {
        case USERDB_OK:
            break;
        case USERDB_LENGTHEXCEEDED:
            return USERDB_BADEMAIL;
        default:
            return USERDB_ERROR;
    }
    
    memset(emaildomain_copy, 0, USERDB_EMAILLEN + 3);
    
    for ( i = 0; i < domainlen; i++ )
    {
        if ( i >= USERDB_EMAILLEN )
            return USERDB_BADEMAIL;
        else if ( domain[i] >= 'A' && domain[i] <= 'Z' )
            emaildomain_copy[i + 2] = domain[i] + ('a' - 'A');
        else
            emaildomain_copy[i + 2] = domain[i];
    }
    
    for ( i = 2; i < (domainlen + 2); i++ )
        if ( emaildomain_copy[i] == '.' )
            emaildomain_copy[i] = '\0';
    
    emaildomain_divide = emaildomain_copy + domainlen + 1;
    
    while ( *emaildomain_divide != '\0' )
        emaildomain_divide--;
    
    *start = (emaildomain_divide + 1);
    
    return USERDB_OK;
}

int userdb_splitemaildomainnext(char *prev, char **next)
{
char *prev_divide;

    if ( !prev || !next )
        return USERDB_BADPARAMETERS;
    
    prev_divide = prev - 2;
    
    if ( *prev_divide == '\0' )
    {
        *next = NULL;
        return USERDB_OK;
    }
    
    while ( *prev_divide != '\0' )
        prev_divide--;
    
    *next = (prev_divide + 1);
    
    return USERDB_OK;
}

int userdb_buildemaildomain(userdb_maildomain *maildomain, char **emaildomain)
{
unsigned int i, j;
static char emaildomain_build[USERDB_EMAILLEN + 1];
userdb_maildomain *tempmaildomain;

    if ( !maildomain || !emaildomain )
        return USERDB_BADPARAMETERS;
    
    i = 0;
    memset(emaildomain_build, 0, USERDB_EMAILLEN + 1);
    
    for ( tempmaildomain = maildomain; tempmaildomain; tempmaildomain = tempmaildomain->parent )
    {
        if ( !(tempmaildomain->maildomainname) || userdb_zerolengthstring(tempmaildomain->maildomainname->content) )
            return USERDB_ERROR;
        
        if ( i )
        {
            if ( i >= USERDB_EMAILLEN )
                return USERDB_ERROR;
            
            emaildomain_build[i] = '.';
            i++;
        }
        
        for ( j = 0; tempmaildomain->maildomainname->content[j]; j++ )
        {
            if ( i >= USERDB_EMAILLEN )
                return USERDB_ERROR;
            
            emaildomain_build[i] = tempmaildomain->maildomainname->content[j];
            i++;
        }
    }
    
    *emaildomain = emaildomain_build;
    return USERDB_OK;
}

int userdb_isnumericstring(char *in)
{
unsigned int i;

    if ( !in )
        return USERDB_BADPARAMETERS;
    else if ( userdb_zerolengthstring(in) )
        return USERDB_ERROR;
    
    for ( i = 0; in[i]; i++ )
        if ( in[i] < '0' || in[i] > '9' )
            return USERDB_ERROR;
    
    return USERDB_OK;
}

int userdb_containsnowildcards(char *in)
{
unsigned int i;

    if ( !in )
        return USERDB_BADPARAMETERS;
    
    for ( i = 0; in[i]; i++ )
        if ( in[i] == '?' || in[i] == '*' )
            return USERDB_ERROR;
    
    return USERDB_OK;
}

int userdb_ismatchall(char *in)
{
unsigned int i;

    if ( !in )
        return USERDB_BADPARAMETERS;
    else if ( userdb_zerolengthstring(in) )
        return USERDB_ERROR;
    
    for ( i = 0; in[i]; i++ )
        if ( in[i] != '*' )
            return USERDB_ERROR;
    
    return USERDB_OK;
}

int userdb_nickonstaffserver(nick *nn)
{
    if ( !nn )
        return USERDB_BADPARAMETERS;
    else if ( ((nn->numeric >> 18) == userdb_staffservernumeric) )
        return USERDB_OK;
    else
        return USERDB_ERROR;
}

int userdb_getusertagdatastring(unsigned short stringsize, unsigned char **stringdata)
{
unsigned char *newstring;

    if ( !stringdata )
        return USERDB_BADPARAMETERS;
    else if ( stringsize > USERDB_MAXUSERTAGSTRINGLEN )
        return USERDB_BADPARAMETERS;
    
    newstring = (unsigned char *)malloc((stringsize + 1));
    assert(newstring != NULL);
    memset(newstring, 0, (stringsize + 1));
    *stringdata = newstring;
    return USERDB_OK;
}

int userdb_strlen(char *in, unsigned long inlen, unsigned long *outlen)
{
unsigned long len;

    if ( !in || !outlen )
        return USERDB_BADPARAMETERS;
    
    len = 0;
    
    if ( inlen )
    {
        while ( in[len] )
        {
            len++;
            
            if ( len >= inlen )
                return USERDB_LENGTHEXCEEDED;
        }
    }
    else
    {
        while ( in[len] )
            len++;
    }
    
    *outlen = len;
    return USERDB_OK;
}

int userdb_processflagstring(flag_t inputflags, const flag *flagslist, char *internalflagslist, char *automaticflagslist, char *flagstring, char *rejectflag, flag_t *outputflags)
{
flag_t flags;
unsigned int i, j, found;

    if ( !flagslist || !flagstring || !rejectflag || !outputflags )
        return USERDB_BADPARAMETERS;
    
    for ( i = 0; flagstring[i]; i++ )
    {
        found = 0;
        
        if ( flagstring[i] == '+' || flagstring[i] == '-' )
            continue;
        
        if ( internalflagslist )
        {
            for ( j = 0; internalflagslist[j]; j++ )
            {
                if ( internalflagslist[j] == flagstring[i] )
                {
                    *rejectflag = flagstring[i];
                    return USERDB_INTERNALFLAG;
                }
            }
        }
        
        if ( automaticflagslist )
        {
            for ( j = 0; automaticflagslist[j]; j++ )
            {
                if ( automaticflagslist[j] == flagstring[i] )
                {
                    *rejectflag = flagstring[i];
                    return USERDB_AUTOMATICFLAG;
                }
            }
        }
        
        for ( j = 0; flagslist[j].flagchar; j++ )
        {
            if ( flagslist[j].flagchar == flagstring[i] )
            {
                found = 1;
                break;
            }
        }
        
        if ( !found )
        {
            *rejectflag = flagstring[i];
            return USERDB_UNKNOWNFLAG;
        }
    }
    
    flags = inputflags;
    
    switch ( setflags(&flags, 0xFFFF, flagstring, flagslist, REJECT_UNKNOWN) )
    {
        case REJECT_NONE:
            break;
        default:
            return USERDB_ERROR;
    }
    
    *outputflags = flags;
    return USERDB_OK;
}

userdb_language *userdb_malloclanguage()
{
userdb_language *language;

    language = (userdb_language *)malloc(sizeof(userdb_language));
    assert(language != NULL);
    memset(language, 0, sizeof(userdb_language));
    return language;
}

void userdb_freelanguage(userdb_language *language)
{
    if ( !language )
        return;
    
    if ( language->languagecode )
        freesstring(language->languagecode);
    
    if ( language->languagename )
        freesstring(language->languagename);
    
    memset(language, 0xFF, sizeof(userdb_language));
    free(language);
}

userdb_user *userdb_mallocuser()
{
userdb_user *user;

    user = (userdb_user *)malloc(sizeof(userdb_user));
    assert(user != NULL);
    memset(user, 0, sizeof(userdb_user));
    return user;
}

void userdb_freeuser(userdb_user *user)
{
    if ( !user )
        return;
    
    if ( user->username )
        freesstring(user->username);
    
    if ( user->emaillocal )
        freesstring(user->emaillocal);
    
    if ( user->suspensiondata )
        userdb_freesuspension(user->suspensiondata);
    
    memset(user, 0xFF, sizeof(userdb_user));
    free(user);
}

userdb_suspension *userdb_mallocsuspension()
{
userdb_suspension *suspension;

    suspension = (userdb_suspension *)malloc(sizeof(userdb_suspension));
    assert(suspension != NULL);
    memset(suspension, 0, sizeof(userdb_suspension));
    return suspension;
}

void userdb_freesuspension(userdb_suspension *suspension)
{
    if ( !suspension )
        return;
    
    if ( suspension->reason )
        freesstring(suspension->reason);
    
    memset(suspension, 0xFF, sizeof(userdb_suspension));
    free(suspension);
}

userdb_usertaginfo *userdb_mallocusertaginfo()
{
userdb_usertaginfo *usertaginfo;

    usertaginfo = (userdb_usertaginfo *)malloc(sizeof(userdb_usertaginfo));
    assert(usertaginfo != NULL);
    memset(usertaginfo, 0, sizeof(userdb_usertaginfo));
    return usertaginfo;
}

void userdb_freeusertaginfo(userdb_usertaginfo *usertaginfo)
{
    if ( !usertaginfo )
        return;
    
    if ( usertaginfo->tagname )
        freesstring(usertaginfo->tagname);
    
    memset(usertaginfo, 0xFF, sizeof(userdb_usertaginfo));
    free(usertaginfo);
}

userdb_usertagdata *userdb_mallocusertagdata()
{
userdb_usertagdata *usertagdata;

    usertagdata = (userdb_usertagdata *)malloc(sizeof(userdb_usertagdata));
    assert(usertagdata != NULL);
    memset(usertagdata, 0, sizeof(userdb_usertagdata));
    return usertagdata;
}

void userdb_freeusertagdata(userdb_usertagdata *usertagdata)
{
    if ( !usertagdata )
        return;
    
    if ( usertagdata->taginfo )
    {
        switch ( usertagdata->taginfo->tagtype )
        {
            case USERDB_TAGTYPE_STRING:
                
                if ( (usertagdata->tagdata).tagstring.stringdata )
                    free((usertagdata->tagdata).tagstring.stringdata);
                
                break;
            default:
                break;
        }
    }
    
    memset(usertagdata, 0xFF, sizeof(userdb_usertagdata));
    free(usertagdata);
}

userdb_maildomain *userdb_mallocmaildomain()
{
userdb_maildomain *maildomain;

    maildomain = (userdb_maildomain *)malloc(sizeof(userdb_maildomain));
    assert(maildomain != NULL);
    memset(maildomain, 0, sizeof(userdb_maildomain));
    return maildomain;
}

void userdb_freemaildomain(userdb_maildomain *maildomain)
{
    if ( !maildomain )
        return;
    
    if ( maildomain->maildomainname )
        freesstring(maildomain->maildomainname);
    
    memset(maildomain, 0xFF, sizeof(userdb_maildomain));
    free(maildomain);
}

userdb_maildomainconfig *userdb_mallocmaildomainconfig()
{
userdb_maildomainconfig *config;

    config = (userdb_maildomainconfig *)malloc(sizeof(userdb_maildomainconfig));
    assert(config != NULL);
    memset(config, 0, sizeof(userdb_maildomainconfig));
    return config;
}

void userdb_freemaildomainconfig(userdb_maildomainconfig *config)
{
    if ( !config )
        return;
    
    if ( config->configname )
        freesstring(config->configname);
    
    memset(config, 0xFF, sizeof(userdb_maildomainconfig));
    free(config);
}
