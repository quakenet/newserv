/*
 * UserDB
 */

#include "./userdb.h"

int userdb_registerusertag(unsigned short tagid, unsigned short tagtype, char *tagname, userdb_usertaginfo **usertaginfo)
{
userdb_usertaginfo *newusertaginfo;

    if ( !userdb_databaseloaded() )
        return USERDB_DBUNAVAILABLE;
    else if ( !tagid || !tagtype || !tagname || userdb_zerolengthstring(tagname) || !usertaginfo )
        return USERDB_BADPARAMETERS;
    
    switch ( tagtype )
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
            return USERDB_BADPARAMETERS;
    }
    
    switch ( userdb_findusertaginfo(tagid, usertaginfo) )
    {
        case USERDB_OK:
            
            if ( ((*(usertaginfo))->tagtype) != tagtype )
                return USERDB_MISMATCH;
            else if ( strncmp(tagname, (*(usertaginfo))->tagname->content, USERDB_USERTAGNAMELEN) )
                return USERDB_MISMATCH;
            
            return USERDB_ALREADYEXISTS;
        case USERDB_NOTFOUND:
            break;
        default:
            return USERDB_ERROR;
    }
    
    newusertaginfo = userdb_mallocusertaginfo();
    newusertaginfo->tagid = tagid;
    newusertaginfo->tagtype = tagtype;
    newusertaginfo->tagname = getsstring(tagname, USERDB_USERTAGNAMELEN);
    
    userdb_addusertaginfotohash(newusertaginfo);
    userdb_addusertaginfotodb(newusertaginfo);
    
    *usertaginfo = newusertaginfo;
    return USERDB_OK;
}

int userdb_unregisterusertag(userdb_usertaginfo *usertaginfo)
{
    if ( !userdb_databaseloaded() )
        return USERDB_DBUNAVAILABLE;
    else if ( !usertaginfo )
        return USERDB_BADPARAMETERS;
    
    userdb_clearallusertagdatabyinfo(usertaginfo);
    userdb_deleteusertaginfofromhash(usertaginfo);
    userdb_deleteusertaginfofromdb(usertaginfo);
    userdb_freeusertaginfo(usertaginfo);
    return USERDB_OK;
}

int userdb_getusertagdatavaluebydata(userdb_usertagdata *usertagdata, unsigned short *tagtype, unsigned short *tagsize, void **data)
{
    if ( !userdb_databaseloaded() )
        return USERDB_DBUNAVAILABLE;
    else if ( !usertagdata || !tagtype || !tagsize || !data )
        return USERDB_BADPARAMETERS;
    else if ( !usertagdata->taginfo || !usertagdata->userinfo )
        return USERDB_ERROR;
    else if ( UDBRIsDeleted(usertagdata->userinfo) )
        return USERDB_BADPARAMETERS;
    
    switch ( usertagdata->taginfo->tagtype )
    {
        case USERDB_TAGTYPE_CHAR:
            *tagsize = sizeof(unsigned char);
            *data = &((usertagdata->tagdata).tagchar);
            break;
        case USERDB_TAGTYPE_SHORT:
            *tagsize = sizeof(unsigned short);
            *data = &((usertagdata->tagdata).tagshort);
            break;
        case USERDB_TAGTYPE_INT:
            *tagsize = sizeof(unsigned int);
            *data = &((usertagdata->tagdata).tagint);
            break;
        case USERDB_TAGTYPE_LONG:
            *tagsize = sizeof(unsigned long);
            *data = &((usertagdata->tagdata).taglong);
            break;
        case USERDB_TAGTYPE_LONGLONG:
            *tagsize = sizeof(unsigned long long);
            *data = &((usertagdata->tagdata).taglonglong);
            break;
        case USERDB_TAGTYPE_FLOAT:
            *tagsize = sizeof(float);
            *data = &((usertagdata->tagdata).tagfloat);
            break;
        case USERDB_TAGTYPE_DOUBLE:
            *tagsize = sizeof(double);
            *data = &((usertagdata->tagdata).tagdouble);
            break;
        case USERDB_TAGTYPE_STRING:
            *tagsize = ((usertagdata->tagdata).tagstring.stringsize);
            *data = ((usertagdata->tagdata).tagstring.stringdata);
            break;
        default:
            return USERDB_ERROR;
    }
    
    *tagtype = usertagdata->taginfo->tagtype;
    return USERDB_OK;
}

int userdb_getusertagdatavaluebyinfo(userdb_usertaginfo *usertaginfo, userdb_user *user, unsigned short *tagtype, unsigned short *tagsize, void **data, userdb_usertagdata **usertagdata)
{
    if ( !userdb_databaseloaded() )
        return USERDB_DBUNAVAILABLE;
    else if ( !usertaginfo || !user || !tagtype || !tagsize || !data || !usertagdata || UDBRIsDeleted(user) )
        return USERDB_BADPARAMETERS;
    
    switch ( userdb_findusertagdata(usertaginfo, user, usertagdata) )
    {
        case USERDB_OK:
            break;
        case USERDB_NOTFOUND:
            return USERDB_NOTFOUND;
        default:
            return USERDB_ERROR;
    }
    
    return userdb_getusertagdatavaluebydata((*usertagdata), tagtype, tagsize, data);
}

int userdb_setusertagdatavaluebydata(userdb_usertagdata *usertagdata, void *data, unsigned short datalen)
{
    if ( !userdb_databaseloaded() )
        return USERDB_DBUNAVAILABLE;
    else if ( !usertagdata || !data )
        return USERDB_BADPARAMETERS;
    else if ( !usertagdata->taginfo || !usertagdata->userinfo )
        return USERDB_ERROR;
    else if ( (usertagdata->taginfo->tagtype == USERDB_TAGTYPE_STRING) && (datalen > USERDB_MAXUSERTAGSTRINGLEN) )
        return USERDB_BADPARAMETERS;
    else if ( UDBRIsDeleted(usertagdata->userinfo) )
        return USERDB_BADPARAMETERS;
    
    switch ( usertagdata->taginfo->tagtype )
    {
        case USERDB_TAGTYPE_CHAR:
            
            if ( (usertagdata->tagdata).tagchar == *((unsigned char *)data) )
                return USERDB_NOOP;
            
            (usertagdata->tagdata).tagchar = *((unsigned char *)data);
            break;
        case USERDB_TAGTYPE_SHORT:
            
            if ( (usertagdata->tagdata).tagshort == *((unsigned short *)data) )
                return USERDB_NOOP;
            
            (usertagdata->tagdata).tagshort = *((unsigned short *)data);
            break;
        case USERDB_TAGTYPE_INT:
            
            if ( (usertagdata->tagdata).tagint == *((unsigned int *)data) )
                return USERDB_NOOP;
            
            (usertagdata->tagdata).tagint = *((unsigned int *)data);
            break;
        case USERDB_TAGTYPE_LONG:
            
            if ( (usertagdata->tagdata).taglong == *((unsigned long *)data) )
                return USERDB_NOOP;
            
            (usertagdata->tagdata).taglong = *((unsigned long *)data);
            break;
        case USERDB_TAGTYPE_LONGLONG:
            
            if ( (usertagdata->tagdata).taglonglong == *((unsigned long long *)data) )
                return USERDB_NOOP;
            
            (usertagdata->tagdata).taglonglong = *((unsigned long long *)data);
            break;
        case USERDB_TAGTYPE_FLOAT:
            
            if ( (usertagdata->tagdata).tagfloat == *((float *)data) )
                return USERDB_NOOP;
            
            (usertagdata->tagdata).tagfloat = *((float *)data);
            break;
        case USERDB_TAGTYPE_DOUBLE:
            
            if ( (usertagdata->tagdata).tagdouble == *((double *)data) )
                return USERDB_NOOP;
            
            (usertagdata->tagdata).tagdouble = *((double *)data);
            break;
        case USERDB_TAGTYPE_STRING:
            
            if ( (usertagdata->tagdata).tagstring.stringdata )
            {
                if ( (usertagdata->tagdata).tagstring.stringsize == datalen )
                {
                    if ( !(memcmp((usertagdata->tagdata).tagstring.stringdata, data, datalen)) )
                        return USERDB_NOOP;
                    
                    memset((usertagdata->tagdata).tagstring.stringdata, 0, datalen);
                }
                else
                {
                    free((usertagdata->tagdata).tagstring.stringdata);
                    (usertagdata->tagdata).tagstring.stringdata = NULL;
                    (usertagdata->tagdata).tagstring.stringsize = 0;
                }
            }
            
            if ( !((usertagdata->tagdata).tagstring.stringdata) )
            {
                (usertagdata->tagdata).tagstring.stringsize = datalen;
                
                switch ( userdb_getusertagdatastring((usertagdata->tagdata).tagstring.stringsize, &((usertagdata->tagdata).tagstring.stringdata)) )
                {
                    case USERDB_OK:
                        break;
                    default:
                        Error("userdb", ERR_WARNING, "String allocation failed for usertagdata (%hu,%lu) in userdb_setusertagdatavaluebydata", usertagdata->taginfo->tagid, usertagdata->userinfo->userid);
                        (usertagdata->tagdata).tagstring.stringdata = NULL;
                        (usertagdata->tagdata).tagstring.stringsize = 0;
                        return USERDB_ERROR;
                }
            }
            
            memcpy((usertagdata->tagdata).tagstring.stringdata, data, (usertagdata->tagdata).tagstring.stringsize);
            break;
        default:
            return USERDB_ERROR;
    }
    
    userdb_modifyusertagdataindb(usertagdata);
    return USERDB_OK;
}

int userdb_setusertagdatavaluebyinfo(userdb_usertaginfo *usertaginfo, userdb_user *user, void *data, unsigned short datalen, userdb_usertagdata **usertagdata)
{
userdb_usertagdata *newusertagdata;

    if ( !userdb_databaseloaded() )
        return USERDB_DBUNAVAILABLE;
    else if ( !usertaginfo || !user || !data || !usertagdata || UDBRIsDeleted(user) )
        return USERDB_BADPARAMETERS;
    
    switch ( userdb_findusertagdata(usertaginfo, user, usertagdata) )
    {
        case USERDB_OK:
            break;
        case USERDB_NOTFOUND:
            newusertagdata = userdb_mallocusertagdata();
            newusertagdata->taginfo = usertaginfo;
            newusertagdata->userinfo = user;
            userdb_addusertagdatatohash(newusertagdata);
            userdb_addusertagdatatodb(newusertagdata);
            *usertagdata = newusertagdata;
            break;
        default:
            return USERDB_ERROR;
    }
    
    return userdb_setusertagdatavaluebydata((*usertagdata), data, datalen);
}

int userdb_clearusertagdatavaluebydata(userdb_usertagdata *usertagdata)
{
    if ( !userdb_databaseloaded() )
        return USERDB_DBUNAVAILABLE;
    else if ( !usertagdata )
        return USERDB_BADPARAMETERS;
    
    userdb_deleteusertagdatafromhash(usertagdata);
    userdb_deleteusertagdatafromdb(usertagdata);
    userdb_freeusertagdata(usertagdata);
    return USERDB_OK;
}

int userdb_clearusertagdatavaluebyinfo(userdb_usertaginfo *usertaginfo, userdb_user *user)
{
userdb_usertagdata *usertagdata;

    if ( !userdb_databaseloaded() )
        return USERDB_DBUNAVAILABLE;
    else if ( !usertaginfo || !user || UDBRIsDeleted(user) )
        return USERDB_BADPARAMETERS;
    
    switch ( userdb_findusertagdata(usertaginfo, user, &usertagdata) )
    {
        case USERDB_OK:
            break;
        case USERDB_NOTFOUND:
            return USERDB_NOTFOUND;
        default:
            return USERDB_ERROR;
    }
    
    return userdb_clearusertagdatavaluebydata(usertagdata);
}

int userdb_clearallusertagdatabyinfo(userdb_usertaginfo *usertaginfo)
{
    if ( !usertaginfo )
        return USERDB_BADPARAMETERS;
    
    userdb_deleteallusertagdatabyinfofromhash(usertaginfo);
    userdb_deleteallusertagdatabyinfofromdb(usertaginfo);
    return USERDB_OK;
}

int userdb_clearallusertagdatabyuser(userdb_user *user)
{
    if ( !user )
        return USERDB_BADPARAMETERS;
    
    userdb_deleteallusertagdatabyuserfromhash(user);
    userdb_deleteallusertagdatabyuserfromdb(user);
    return USERDB_OK;
}
