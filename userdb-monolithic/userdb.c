/*
 * UserDB
 */

#include "./userdb.h"

/* Module status */
unsigned long userdb_moduleloaded;

/* Database status */
unsigned long userdb_databasestatus;
unsigned long userdb_languagedatabaseaddcount, userdb_userdatabaseaddcount, userdb_maildomainconfigdatabaseaddcount;
unsigned long userdb_languagedatabasemodifycount, userdb_userdatabasemodifycount, userdb_maildomainconfigdatabasemodifycount;
unsigned long userdb_languagedatabasedeletecount, userdb_userdatabasedeletecount, userdb_maildomainconfigdatabasedeletecount;

/* Unique number control */
unsigned long userdb_lastlanguageid, userdb_lastuserid, userdb_lastmaildomainconfigid;

/* Replication control */
#ifdef USERDB_ENABLEREPLICATION
replicator_group *userdb_repl_group;
replicator_object *userdb_repl_obj_language;
replicator_object *userdb_repl_obj_user;
replicator_object *userdb_repl_obj_maildomainconfig;
#endif
unsigned long long userdb_lastlanguagetransactionid, userdb_lastusertransactionid, userdb_lastmaildomainconfigtransactionid;

/* Auth extension */
int userdb_authnameextnum;

/* Email address regex */
#define USERDB_EMAILREGEX               "^([a-zA-Z0-9_\\-\\.]+)@([a-zA-Z0-9_\\-\\.]+)\\.([a-zA-Z]{2,5})$"
pcre *userdb_emailregex;
pcre_extra *userdb_emailregexextra;

/* Hash tables */
userdb_language *userdb_languagehashtable_id[USERDB_LANGUAGEHASHSIZE];
userdb_language *userdb_languagehashtable_code[USERDB_LANGUAGEHASHSIZE];
userdb_user *userdb_userhashtable_id[USERDB_USERHASHSIZE];
userdb_user *userdb_userhashtable_username[USERDB_USERHASHSIZE];
userdb_user *userdb_usersuspendedhashtable[USERDB_USERSUSPENDEDHASHSIZE];
userdb_usertaginfo *userdb_usertaginfohashtable[USERDB_USERTAGINFOHASHSIZE];
userdb_maildomain *userdb_maildomainhashtable_name[USERDB_MAILDOMAINHASHSIZE];
userdb_maildomainconfig *userdb_maildomainconfighashtable_id[USERDB_MAILDOMAINCONFIGHASHSIZE];
userdb_maildomainconfig *userdb_maildomainconfighashtable_name[USERDB_MAILDOMAINCONFIGHASHSIZE];

#ifdef USERDB_ENABLEREPLICATION
userdb_language *userdb_languagedeletedhashtable[USERDB_LANGUAGEDELETEDHASHSIZE];
userdb_user *userdb_userdeletedhashtable[USERDB_USERDELETEDHASHSIZE];
userdb_maildomainconfig *userdb_maildomainconfigdeletedhashtable[USERDB_MAILDOMAINCONFIGDELETEDHASHSIZE];
#endif

/* Access control */
long userdb_staffservernumeric;

void userdb_statushook(int hooknum, void *arg)
{
unsigned int i = 0, tempcount = 0;
unsigned int languagecount = 0, usercount = 0, maildomaincount = 0, maildomainconfigcount = 0;
unsigned int usedlanguageid = 0, usedlanguagecode = 0;
unsigned int useduserid = 0, usedusername = 0, usedsuspendeduser = 0;
unsigned int usedmaildomain = 0;
unsigned int usedmaildomainconfigid = 0, usedmaildomainconfigname = 0;
unsigned int maxlanguageid = 0, maxlanguagecode = 0;
unsigned int maxuserid = 0, maxusername = 0, maxsuspendeduser = 0;
unsigned int maxmaildomain = 0;
unsigned int maxmaildomainconfigid = 0, maxmaildomainconfigname = 0;
#ifdef USERDB_ENABLEREPLICATION
unsigned int useddeletedlanguage = 0, useddeleteduser = 0, useddeletedmaildomainconfig = 0;
unsigned int maxdeletedlanguage = 0, maxdeleteduser = 0, maxdeletedmaildomainconfig = 0;
#endif
char buf[513];
userdb_language *language = NULL;
userdb_user *user = NULL;
userdb_maildomain *maildomain = NULL;
userdb_maildomainconfig *maildomainconfig = NULL;

    if ( ((long)arg) < 20 )
        return;
    
    for ( i = 0; i < USERDB_LANGUAGEHASHSIZE; i++ )
    {
        if ( userdb_languagehashtable_id[i] )
        {
            tempcount = 0;
            usedlanguageid++;
            
            for ( language = userdb_languagehashtable_id[i]; language; language = language->nextbyid )
            {
                languagecount++;
                tempcount++;
            }
            
            if ( maxlanguageid < tempcount )
                maxlanguageid = tempcount;
        }
    }
    
    for ( i = 0; i < USERDB_LANGUAGEHASHSIZE; i++ )
    {
        if ( userdb_languagehashtable_code[i] )
        {
            tempcount = 0;
            usedlanguagecode++;
            
            for ( language = userdb_languagehashtable_code[i]; language; language = language->nextbycode )
                tempcount++;
            
            if ( maxlanguagecode < tempcount )
                maxlanguagecode = tempcount;
        }
    }
    
#ifdef USERDB_ENABLEREPLICATION
    for ( i = 0; i < USERDB_LANGUAGEDELETEDHASHSIZE; i++ )
    {
        if ( userdb_languagedeletedhashtable[i] )
        {
            tempcount = 0;
            useddeletedlanguage++;
            
            for ( language = userdb_languagedeletedhashtable[i]; language; language = language->nextbyid )
                tempcount++;
            
            if ( maxdeletedlanguage < tempcount )
                maxdeletedlanguage = tempcount;
        }
    }
#endif
    
    for ( i = 0; i < USERDB_USERHASHSIZE; i++ )
    {
        if ( userdb_userhashtable_id[i] )
        {
            tempcount = 0;
            useduserid++;
            
            for ( user = userdb_userhashtable_id[i]; user; user = user->nextbyid )
            {
                usercount++;
                tempcount++;
            }
            
            if ( maxuserid < tempcount )
                maxuserid = tempcount;
        }
    }
    
    for ( i = 0; i < USERDB_USERHASHSIZE; i++ )
    {
        if ( userdb_userhashtable_username[i] )
        {
            tempcount = 0;
            usedusername++;
            
            for ( user = userdb_userhashtable_username[i]; user; user = user->nextbyname )
                tempcount++;
            
            if ( maxusername < tempcount )
                maxusername = tempcount;
        }
    }
    
    for ( i = 0; i < USERDB_USERSUSPENDEDHASHSIZE; i++ )
    {
        if ( userdb_usersuspendedhashtable[i] )
        {
            tempcount = 0;
            usedsuspendeduser++;
            
            for ( user = userdb_usersuspendedhashtable[i]; user; user = user->suspensiondata->nextbyid )
                tempcount++;
            
            if ( maxsuspendeduser < tempcount )
                maxsuspendeduser = tempcount;
        }
    }
    
#ifdef USERDB_ENABLEREPLICATION
    for ( i = 0; i < USERDB_USERDELETEDHASHSIZE; i++ )
    {
        if ( userdb_userdeletedhashtable[i] )
        {
            tempcount = 0;
            useddeleteduser++;
            
            for ( user = userdb_userdeletedhashtable[i]; user; user = user->nextbyid )
                tempcount++;
            
            if ( maxdeleteduser < tempcount )
                maxdeleteduser = tempcount;
        }
    }
#endif
    
    for ( i = 0; i < USERDB_MAILDOMAINHASHSIZE; i++ )
    {
        if ( userdb_maildomainhashtable_name[i] )
        {
            tempcount = 0;
            usedmaildomain++;
            
            for ( maildomain = userdb_maildomainhashtable_name[i]; maildomain; maildomain = maildomain->nextbyname )
            {
                maildomaincount++;
                tempcount++;
            }
            
            if ( maxmaildomain < tempcount )
                maxmaildomain = tempcount;
        }
    }
    
    for ( i = 0; i < USERDB_MAILDOMAINCONFIGHASHSIZE; i++ )
    {
        if ( userdb_maildomainconfighashtable_id[i] )
        {
            tempcount = 0;
            usedmaildomainconfigid++;
            
            for ( maildomainconfig = userdb_maildomainconfighashtable_id[i]; maildomainconfig; maildomainconfig = maildomainconfig->nextbyid )
            {
                maildomainconfigcount++;
                tempcount++;
            }
            
            if ( maxmaildomainconfigid < tempcount )
                maxmaildomainconfigid = tempcount;
        }
    }
    
    for ( i = 0; i < USERDB_MAILDOMAINCONFIGHASHSIZE; i++ )
    {
        if ( userdb_maildomainconfighashtable_name[i] )
        {
            tempcount = 0;
            usedmaildomainconfigname++;
            
            for ( maildomainconfig = userdb_maildomainconfighashtable_name[i]; maildomainconfig; maildomainconfig = maildomainconfig->nextbyname )
                tempcount++;
            
            if ( maxmaildomainconfigname < tempcount )
                maxmaildomainconfigname = tempcount;
        }
    }
    
#ifdef USERDB_ENABLEREPLICATION
    for ( i = 0; i < USERDB_MAILDOMAINCONFIGDELETEDHASHSIZE; i++ )
    {
        if ( userdb_maildomainconfigdeletedhashtable[i] )
        {
            tempcount = 0;
            useddeletedmaildomainconfig++;
            
            for ( maildomainconfig = userdb_maildomainconfigdeletedhashtable[i]; maildomainconfig; maildomainconfig = maildomainconfig->nextbyid )
                tempcount++;
            
            if ( maxdeletedmaildomainconfig < tempcount )
                maxdeletedmaildomainconfig = tempcount;
        }
    }
#endif
    
    triggerhook(HOOK_CORE_STATSREPLY, "UserDB  : ------------------------------------------------");
    snprintf(buf, sizeof(buf), "UserDB  : %7u registered languages", languagecount);
    buf[sizeof(buf) - 1] = '\0';
    triggerhook(HOOK_CORE_STATSREPLY, buf);
    triggerhook(HOOK_CORE_STATSREPLY, "UserDB  : Language Hash Tables:");
    snprintf(buf, sizeof(buf), "UserDB  : Language ID's                (Hash: %7u/%7u, Chain: %7u)", usedlanguageid, USERDB_LANGUAGEHASHSIZE, maxlanguageid);
    buf[sizeof(buf) - 1] = '\0';
    triggerhook(HOOK_CORE_STATSREPLY, buf);
    snprintf(buf, sizeof(buf), "UserDB  : Language Codes               (Hash: %7u/%7u, Chain: %7u)", usedlanguagecode, USERDB_LANGUAGEHASHSIZE, maxlanguagecode);
    buf[sizeof(buf) - 1] = '\0';
    triggerhook(HOOK_CORE_STATSREPLY, buf);
#ifdef USERDB_ENABLEREPLICATION
    snprintf(buf, sizeof(buf), "UserDB  : Deleted Languages            (Hash: %7u/%7u, Chain: %7u)", useddeletedlanguage, USERDB_LANGUAGEDELETEDHASHSIZE, maxdeletedlanguage);
    buf[sizeof(buf) - 1] = '\0';
    triggerhook(HOOK_CORE_STATSREPLY, buf);
#endif
    
    triggerhook(HOOK_CORE_STATSREPLY, "UserDB  : ------------------------------------------------");
    snprintf(buf, sizeof(buf), "UserDB  : %7u registered accounts", usercount);
    buf[sizeof(buf) - 1] = '\0';
    triggerhook(HOOK_CORE_STATSREPLY, buf);
    triggerhook(HOOK_CORE_STATSREPLY, "UserDB  : User Hash Tables:");
    snprintf(buf, sizeof(buf), "UserDB  : User IDs                     (Hash: %7u/%7u, Chain: %7u)", useduserid, USERDB_USERHASHSIZE, maxuserid);
    buf[sizeof(buf) - 1] = '\0';
    triggerhook(HOOK_CORE_STATSREPLY, buf);
    snprintf(buf, sizeof(buf), "UserDB  : Usernames                    (Hash: %7u/%7u, Chain: %7u)", usedusername, USERDB_USERHASHSIZE, maxusername);
    buf[sizeof(buf) - 1] = '\0';
    triggerhook(HOOK_CORE_STATSREPLY, buf);
    snprintf(buf, sizeof(buf), "UserDB  : Suspended Users              (Hash: %7u/%7u, Chain: %7u)", usedsuspendeduser, USERDB_USERSUSPENDEDHASHSIZE, maxsuspendeduser);
    buf[sizeof(buf) - 1] = '\0';
    triggerhook(HOOK_CORE_STATSREPLY, buf);
#ifdef USERDB_ENABLEREPLICATION
    snprintf(buf, sizeof(buf), "UserDB  : Deleted Users                (Hash: %7u/%7u, Chain: %7u)", useddeleteduser, USERDB_USERDELETEDHASHSIZE, maxdeleteduser);
    buf[sizeof(buf) - 1] = '\0';
    triggerhook(HOOK_CORE_STATSREPLY, buf);
#endif
    
    triggerhook(HOOK_CORE_STATSREPLY, "UserDB  : ------------------------------------------------");
    snprintf(buf, sizeof(buf), "UserDB  : %7u mail domains", maildomaincount);
    buf[sizeof(buf) - 1] = '\0';
    triggerhook(HOOK_CORE_STATSREPLY, buf);
    triggerhook(HOOK_CORE_STATSREPLY, "UserDB  : Mail Domain Hash Tables:");
    snprintf(buf, sizeof(buf), "UserDB  : Mail Domain names            (Hash: %7u/%7u, Chain: %7u)", usedmaildomain, USERDB_MAILDOMAINHASHSIZE, maxmaildomain);
    buf[sizeof(buf) - 1] = '\0';
    triggerhook(HOOK_CORE_STATSREPLY, buf);
    
    triggerhook(HOOK_CORE_STATSREPLY, "UserDB  : ------------------------------------------------");
    snprintf(buf, sizeof(buf), "UserDB  : %7u registered mail domain configs", maildomainconfigcount);
    buf[sizeof(buf) - 1] = '\0';
    triggerhook(HOOK_CORE_STATSREPLY, buf);
    triggerhook(HOOK_CORE_STATSREPLY, "UserDB  : Mail Domain Config Hash Tables:");
    snprintf(buf, sizeof(buf), "UserDB  : Mail Domain Config IDs       (Hash: %7u/%7u, Chain: %7u)", usedmaildomainconfigid, USERDB_MAILDOMAINCONFIGHASHSIZE, maxmaildomainconfigid);
    buf[sizeof(buf) - 1] = '\0';
    triggerhook(HOOK_CORE_STATSREPLY, buf);
    snprintf(buf, sizeof(buf), "UserDB  : Mail Domain Config Names     (Hash: %7u/%7u, Chain: %7u)", usedmaildomainconfigname, USERDB_MAILDOMAINCONFIGHASHSIZE, maxmaildomainconfigname);
    buf[sizeof(buf) - 1] = '\0';
    triggerhook(HOOK_CORE_STATSREPLY, buf);
#ifdef USERDB_ENABLEREPLICATION
    snprintf(buf, sizeof(buf), "UserDB  : Deleted Mail Domain Configs  (Hash: %7u/%7u, Chain: %7u)", useddeletedmaildomainconfig, USERDB_MAILDOMAINCONFIGDELETEDHASHSIZE, maxdeletedmaildomainconfig);
    buf[sizeof(buf) - 1] = '\0';
    triggerhook(HOOK_CORE_STATSREPLY, buf);
#endif
    triggerhook(HOOK_CORE_STATSREPLY, "UserDB  : ------------------------------------------------");
}

int userdb_moduleready()
{
    return userdb_moduleloaded;
}

void _init()
{
const char *error;
int erroffset;
sstring *staffserverconf;
#ifdef USERDB_ENABLEREPLICATION
sstring *roleconf, *sourceconf;
unsigned int role;
long source;
replicator_node *node;
char buf[10];
long nodenumeric;
FILE *nodelist;
#endif

    userdb_lastlanguageid = 0;
    userdb_lastlanguagetransactionid = 0;
    
    userdb_lastuserid = 0;
    userdb_lastusertransactionid = 0;
    
    userdb_lastmaildomainconfigid = 0;
    userdb_lastmaildomainconfigtransactionid = 0;
    
    userdb_authnameextnum = -1;
    
    userdb_moduleloaded = 0;
    userdb_databasestatus = USERDB_DATABASESTATE_INIT;
    userdb_languagedatabaseaddcount = 0;
    userdb_languagedatabasemodifycount = 0;
    userdb_languagedatabasedeletecount = 0;
    userdb_userdatabaseaddcount = 0;
    userdb_userdatabasemodifycount = 0;
    userdb_userdatabasedeletecount = 0;
    userdb_maildomainconfigdatabaseaddcount = 0;
    userdb_maildomainconfigdatabasemodifycount = 0;
    userdb_maildomainconfigdatabasedeletecount = 0;
    
#ifdef USERDB_ENABLEREPLICATION
    userdb_repl_group = NULL;
    userdb_repl_obj_language = NULL;
    userdb_repl_obj_user = NULL;
    userdb_repl_obj_maildomainconfig = NULL;
#endif
    
    userdb_staffservernumeric = 0;
    
    userdb_emailregex = NULL;
    userdb_emailregexextra = NULL;
    
    staffserverconf = getcopyconfigitem("userdb", "staffserver", "?", 10);
    
    if ( !ircd_strcmp("?", staffserverconf->content) )
    {
        freesstring(staffserverconf);
        Error("userdb", ERR_FATAL, "Staff server numeric is not set in config file.");
        return;
    }
    
    userdb_staffservernumeric = numerictolong(staffserverconf->content, 2);
    freesstring(staffserverconf);
    
#ifdef USERDB_ENABLEREPLICATION
    roleconf = getcopyconfigitem("userdb", "role", "?", 10);
    
    if ( !ircd_strcmp("master", roleconf->content) )
    {
        Error("userdb", ERR_WARNING, "This instance is configured as the master userdb newserv instance.");
        Error("userdb", ERR_WARNING, "Please do not run experimental code on this newserv instance.");
        role = REPLICATOR_ROLE_MASTER;
    }
    else if ( !ircd_strcmp("replica", roleconf->content) )
    {
        role = REPLICATOR_ROLE_REPLICA;
    }
    else if ( !ircd_strcmp("slave", roleconf->content) )
    {
        role = REPLICATOR_ROLE_SLAVE;
    }
    else
    {
        freesstring(roleconf);
        Error("userdb", ERR_FATAL, "Could not determine userdb replication role.");
        return;
    }
    
    freesstring(roleconf);
    
    if ( role != REPLICATOR_ROLE_MASTER )
    {
        sourceconf = getcopyconfigitem("userdb", "source", "?", 10);

        if ( !ircd_strcmp("?", sourceconf->content) )
        {
            freesstring(sourceconf);
            Error("userdb", ERR_FATAL, "Source replicator numeric is not set in config file.");
            return;
        }
        
        source = numerictolong(sourceconf->content, 2);
        freesstring(sourceconf);
        
        if ( replicator_findnodebynumeric(source, &node) != REPLICATOR_OK )
        {
            Error("userdb", ERR_FATAL, "Source replication node does not exist.");
            return;
        }
    }
#endif
    
    userdb_authnameextnum = registerauthnameext("userdb");
    
    if ( userdb_authnameextnum == -1 )
    {
        Error("userdb", ERR_FATAL, "Could not register an authname extension.");
        return;
    }
    
    userdb_emailregex = pcre_compile(USERDB_EMAILREGEX, PCRE_CASELESS, &error, &erroffset, NULL);
    
    if ( !userdb_emailregex )
    {
        Error("userdb", ERR_FATAL, "Failed to compile email regex");
        return;
    }
    
    userdb_emailregexextra = pcre_study(userdb_emailregex, 0, &error);
    
#ifdef USERDB_ENABLEREPLICATION
    if ( replicator_creategroup("udb", role, &userdb_repl_group) != REPLICATOR_OK )
    {
        Error("userdb", ERR_FATAL, "Failed to register userdb replication group");
        userdb_repl_group = NULL;
        return;
    }
    
    if ( replicator_createobject(userdb_repl_group, "l", &userdb_languagedatabaseaddcount, &userdb_languagedatabasemodifycount, &userdb_languagedatabasedeletecount, userdb_languagerequest, userdb_languagereceive, userdb_languagecleanup, &userdb_lastlanguagetransactionid, &userdb_repl_obj_language) != REPLICATOR_OK )
    {
        Error("userdb", ERR_FATAL, "Failed to register userdb language replication object");
        userdb_repl_obj_language = NULL;
        return;
    }
    
    if ( replicator_createobject(userdb_repl_group, "u", &userdb_userdatabaseaddcount, &userdb_userdatabasemodifycount, &userdb_userdatabasedeletecount, userdb_userrequest, userdb_userreceive, userdb_usercleanup, &userdb_lastusertransactionid, &userdb_repl_obj_user) != REPLICATOR_OK )
    {
        Error("userdb", ERR_FATAL, "Failed to register userdb user replication object");
        userdb_repl_obj_user = NULL;
        return;
    }
    
    if ( replicator_createobject(userdb_repl_group, "m", &userdb_maildomainconfigdatabaseaddcount, &userdb_maildomainconfigdatabasemodifycount, &userdb_maildomainconfigdatabasedeletecount, userdb_maildomainconfigrequest, userdb_maildomainconfigreceive, userdb_maildomainconfigcleanup, &userdb_lastmaildomainconfigtransactionid, &userdb_repl_obj_maildomainconfig) != REPLICATOR_OK )
    {
        Error("userdb", ERR_FATAL, "Failed to register userdb maildomainconfig replication object");
        userdb_repl_obj_maildomainconfig = NULL;
        return;
    }
    
    if ( role != REPLICATOR_ROLE_MASTER )
    {
        if ( replicator_setgroupsourcenodebydata(userdb_repl_group, node) != REPLICATOR_OK )
        {
            Error("userdb", ERR_FATAL, "Failed to set replication source node");
            return;
        }
    }
    
    nodelist = fopen("./userdb.conf", "r");
    
    if ( !nodelist )
    {
        Error("userdb", ERR_FATAL, "Could not open node list.");
        return;
    }
    
    while ( !feof(nodelist) )
    {
        fgets(buf, 5, nodelist);
        
        if ( feof(nodelist) )
            break;
        
        if ( buf[0] == '\n' || buf[0] == '\0' || buf[1] == '\n' || buf[1] == '\0' || buf[2] != '\n' || buf[3] != '\0' )
        {
            Error("userdb", ERR_FATAL, "Invalid numeric in config file.");
            fclose(nodelist);
            return;
        }
        
        buf[2] = '\0';
        nodenumeric = numerictolong(buf, 2);
        
        if ( replicator_createnodestatesforgroupbynumeric(userdb_repl_group, nodenumeric) != REPLICATOR_OK )
        {
            Error("userdb", ERR_FATAL, "Error creating node states for node %s (%ld).", buf, nodenumeric);
            fclose(nodelist);
            return;
        }
        else
        {
            Error("userdb", ERR_INFO, "Node %s (%ld) registered", buf, nodenumeric);
        }
    }
    
    fclose(nodelist);
    nodelist = NULL;
#endif
    
    userdb_inithashtables();
    userdb_initdatabase();
    
    registerhook(HOOK_CORE_STATSREQUEST, &userdb_statushook);
    userdb_moduleloaded = 1;
}

void _fini()
{
unsigned int i, j;
userdb_language *templanguage;
userdb_user *tempuser;
userdb_usertaginfo *tempusertaginfo;
userdb_usertagdata *tempusertagdata;
userdb_maildomain *tempmaildomain;
userdb_maildomainconfig *tempmaildomainconfig;

#ifdef USERDB_ENABLEREPLICATION
    if ( userdb_repl_group && replicator_deletegroupbydata(userdb_repl_group) != REPLICATOR_OK )
        Error("userdb", ERR_FATAL, "Failed to unregister userdb replication group");
#endif
    
    deregisterhook(HOOK_CORE_STATSREQUEST, &userdb_statushook);
    
    if ( userdb_authnameextnum != -1 )
    {
        releaseauthnameext(userdb_authnameextnum);
        userdb_authnameextnum = -1;
    }
    
    if ( userdb_emailregexextra )
        pcre_free(userdb_emailregexextra);
    
    if ( userdb_emailregex )
        pcre_free(userdb_emailregex);
    
    for ( i = 0; i < USERDB_LANGUAGEHASHSIZE; i++ )
    {
        while ( userdb_languagehashtable_id[i] )
        {
            templanguage = userdb_languagehashtable_id[i]->nextbyid;
            userdb_freelanguage(userdb_languagehashtable_id[i]);
            userdb_languagehashtable_id[i] = templanguage;
        }
    }
    
#ifdef USERDB_ENABLEREPLICATION
    for ( i = 0; i < USERDB_LANGUAGEDELETEDHASHSIZE; i++ )
    {
        while ( userdb_languagedeletedhashtable[i] )
        {
            templanguage = userdb_languagedeletedhashtable[i]->nextbyid;
            userdb_freelanguage(userdb_languagedeletedhashtable[i]);
            userdb_languagedeletedhashtable[i] = templanguage;
        }
    }
#endif
    
    for ( i = 0; i < USERDB_USERHASHSIZE; i++ )
    {
        while ( userdb_userhashtable_id[i] )
        {
            tempuser = userdb_userhashtable_id[i]->nextbyid;
            userdb_freeuser(userdb_userhashtable_id[i]);
            userdb_userhashtable_id[i] = tempuser;
        }
    }
    
#ifdef USERDB_ENABLEREPLICATION
    for ( i = 0; i < USERDB_USERDELETEDHASHSIZE; i++ )
    {
        while ( userdb_userdeletedhashtable[i] )
        {
            tempuser = userdb_userdeletedhashtable[i]->nextbyid;
            userdb_freeuser(userdb_userdeletedhashtable[i]);
            userdb_userdeletedhashtable[i] = tempuser;
        }
    }
#endif
    
    for ( i = 0; i < USERDB_USERTAGINFOHASHSIZE; i++ )
    {
        while ( userdb_usertaginfohashtable[i] )
        {
            for ( j = 0; j < USERDB_USERTAGDATAHASHSIZE; j++ )
            {
                while ( userdb_usertaginfohashtable[i]->usertags[j] )
                {
                    tempusertagdata = userdb_usertaginfohashtable[i]->usertags[j]->nextbytag;
                    userdb_freeusertagdata(userdb_usertaginfohashtable[i]->usertags[j]);
                    userdb_usertaginfohashtable[i]->usertags[j] = tempusertagdata;
                }
            }
            
            tempusertaginfo = userdb_usertaginfohashtable[i]->nextbyid;
            userdb_freeusertaginfo(userdb_usertaginfohashtable[i]);
            userdb_usertaginfohashtable[i] = tempusertaginfo;
        }
    }
    
    for ( i = 0; i < USERDB_MAILDOMAINHASHSIZE; i++ )
    {
        while ( userdb_maildomainhashtable_name[i] )
        {
            tempmaildomain = userdb_maildomainhashtable_name[i]->nextbyname;
            userdb_freemaildomain(userdb_maildomainhashtable_name[i]);
            userdb_maildomainhashtable_name[i] = tempmaildomain;
        }
    }
    
    for ( i = 0; i < USERDB_MAILDOMAINCONFIGHASHSIZE; i++ )
    {
        while ( userdb_maildomainconfighashtable_id[i] )
        {
            tempmaildomainconfig = userdb_maildomainconfighashtable_id[i]->nextbyid;
            userdb_freemaildomainconfig(userdb_maildomainconfighashtable_id[i]);
            userdb_maildomainconfighashtable_id[i] = tempmaildomainconfig;
        }
    }
    
#ifdef USERDB_ENABLEREPLICATION
    for ( i = 0; i < USERDB_MAILDOMAINCONFIGDELETEDHASHSIZE; i++ )
    {
        while ( userdb_maildomainconfigdeletedhashtable[i] )
        {
            tempmaildomainconfig = userdb_maildomainconfigdeletedhashtable[i]->nextbyid;
            userdb_freemaildomainconfig(userdb_maildomainconfigdeletedhashtable[i]);
            userdb_maildomainconfigdeletedhashtable[i] = tempmaildomainconfig;
        }
    }
#endif
}
